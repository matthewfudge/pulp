#pragma once

/// @file message_collector.hpp
/// SPSC MIDI accumulator for UI-thread → audio-thread MIDI delivery.
///
/// `MidiMessageCollector` lets the UI / virtual-keyboard / scripting
/// thread push timestamped MIDI events into a lock-free queue that the
/// audio callback drains into the current block's `MidiBuffer` at the
/// correct sample offsets.
///
/// Capacity is a compile-time parameter so the underlying queue
/// allocates no heap memory in the audio thread. The default of 256
/// covers typical UI input rates (mouse + scripting) at any
/// block size; larger consumers (sequencers playing back patterns)
/// should pick a higher capacity at construction.
///
/// Usage:
/// @code
///   pulp::midi::MidiMessageCollector<> collector;
///
///   // UI thread:
///   collector.push_now(MidiEvent::note_on(0, 60, 100), now_seconds);
///
///   // Audio thread (each block):
///   collector.drain_into(output_buffer, block_start_seconds,
///                        block_end_seconds, sample_rate);
/// @endcode

#include <pulp/midi/buffer.hpp>
#include <pulp/midi/message.hpp>
#include <pulp/runtime/spsc_queue.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace pulp::midi {

/// Timestamped MIDI event in the collector queue. The collector
/// holds events as `choc::midi::ShortMessage` + absolute timestamp
/// in seconds; the audio thread converts to sample-offset on drain.
struct TimestampedShortMessage {
    choc::midi::ShortMessage message{};
    double timestamp_seconds = 0.0;
};

template<std::size_t Capacity = 256>
class MidiMessageCollector {
public:
    MidiMessageCollector() = default;

    /// UI / producer thread — push a MidiEvent timestamped at `now_seconds`.
    /// Returns false when the queue is full (caller decides whether to
    /// drop, log, or back off — `push_now` never blocks).
    bool push_now(const MidiEvent& event, double now_seconds) {
        TimestampedShortMessage entry{event.message, now_seconds};
        return queue_.try_push(entry);
    }

    /// Convenience overload using `MidiEvent::timestamp` directly.
    bool push_now(const MidiEvent& event) {
        return push_now(event, event.timestamp);
    }

    /// Audio / consumer thread — drain queued events into `out` at the
    /// correct sample offsets within the current processing block.
    ///
    /// Events whose timestamps are <= @p block_start_seconds land at
    /// sample 0 (catch-up). Events between block_start and block_end
    /// land at the sample offset that matches their timestamp. Events
    /// strictly after `block_end_seconds` stay queued for the next
    /// block.
    ///
    /// `block_size_samples` and `sample_rate` together define the block
    /// length: block_end_seconds = block_start_seconds + block_size_samples / sample_rate.
    ///
    /// Returns the number of events drained into @p out this call.
    std::size_t drain_into(MidiBuffer& out,
                           double block_start_seconds,
                           int block_size_samples,
                           double sample_rate) {
        if (block_size_samples <= 0 || sample_rate <= 0.0) return 0;
        const double block_duration = block_size_samples / sample_rate;
        const double block_end_seconds = block_start_seconds + block_duration;

        std::size_t drained = 0;

        auto deliver = [&](const TimestampedShortMessage& entry) {
            int sample_offset = 0;
            if (entry.timestamp_seconds > block_start_seconds) {
                const double offset_seconds = entry.timestamp_seconds - block_start_seconds;
                // Round to nearest sample — truncation would lose
                // ~1 sample on every tick whose product is not exactly
                // representable in binary float.
                sample_offset = static_cast<int>(std::lround(offset_seconds * sample_rate));
                sample_offset = std::clamp(sample_offset, 0, block_size_samples - 1);
            }
            MidiEvent event{entry.message, sample_offset, entry.timestamp_seconds};
            out.add(event);
        };

        // Walk the consumer-owned `pending_` ring first: any deferred-
        // future entry whose timestamp now fits the current block is
        // delivered. Entries that are still in the future stay put so
        // a later block can consume them. (Pulled out of the queue so
        // the consumer thread never writes back to the SPSC FIFO —
        // Codex P1 on #2843.)
        for (auto& slot : pending_) {
            if (slot.has_value() && slot->timestamp_seconds < block_end_seconds) {
                deliver(*slot);
                slot.reset();
                ++drained;
            }
        }

        // Always drain the queue regardless of whether pending events
        // remain in the future — the queue may hold *earlier-than-pending*
        // events when the producer pushes out of timestamp order, and
        // those must not be starved (Codex P1 on #2845).
        while (true) {
            auto popped = queue_.try_pop();
            if (!popped) break;
            const auto& entry = *popped;
            if (entry.timestamp_seconds < block_end_seconds) {
                deliver(entry);
                ++drained;
                continue;
            }
            // Future event: stash in the consumer-owned pending ring.
            if (!stash_pending(entry)) {
                // Ring overflow — record + drop. Producer should slow
                // down or grow the pending ring.
                ++dropped_overflow_;
            }
        }
        return drained;
    }

    /// Approximate number of events currently queued (does NOT count
    /// the deferred-future entries in the pending ring).
    std::size_t size_approx() const { return queue_.size_approx(); }

    /// Number of future events the consumer has dropped because the
    /// pending ring overflowed. Monotonically increasing; useful as
    /// a diagnostic counter. Reset to 0 only by `reset_dropped_overflow()`.
    std::size_t dropped_overflow() const { return dropped_overflow_; }
    void reset_dropped_overflow() { dropped_overflow_ = 0; }

    /// Compile-time capacity of the producer queue.
    static constexpr std::size_t capacity() { return Capacity; }

    /// Compile-time size of the consumer-owned pending ring. Large
    /// enough to absorb out-of-order producer bursts in practical
    /// usage (UI + scripting timing). If a workload routinely overflows,
    /// pick a larger value at instantiation.
    static constexpr std::size_t pending_capacity() { return kPendingSlots; }

private:
    static constexpr std::size_t kPendingSlots = 8;

    pulp::runtime::SpscQueue<TimestampedShortMessage, Capacity> queue_;
    /// Consumer-owned pending ring for popped-but-future events.
    /// Linear scan; N=8 keeps the per-drain overhead negligible.
    std::array<std::optional<TimestampedShortMessage>, kPendingSlots> pending_{};
    std::size_t dropped_overflow_ = 0;

    /// Place @p entry in an empty pending slot. If all slots are
    /// occupied, evict the slot with the LARGEST timestamp (latest
    /// future event) so the ring keeps the soonest-due events. Returns
    /// true if @p entry was stored; false if the ring already held
    /// only earlier-due events and @p entry was rejected.
    bool stash_pending(const TimestampedShortMessage& entry) {
        // First pass: empty slot.
        for (auto& slot : pending_) {
            if (!slot.has_value()) {
                slot = entry;
                return true;
            }
        }
        // All occupied — find the latest-due slot.
        std::size_t latest_idx = 0;
        double latest_ts = pending_[0]->timestamp_seconds;
        for (std::size_t i = 1; i < pending_.size(); ++i) {
            if (pending_[i]->timestamp_seconds > latest_ts) {
                latest_ts = pending_[i]->timestamp_seconds;
                latest_idx = i;
            }
        }
        // If the new entry is sooner than the latest pending, evict.
        if (entry.timestamp_seconds < latest_ts) {
            pending_[latest_idx] = entry;
            // The evicted (later) entry is the one we lose.
            return false; // signals an overflow drop
        }
        return false; // new entry rejected entirely (it's the latest)
    }
};

} // namespace pulp::midi

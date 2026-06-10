#pragma once

/// @file realtime_pitch_time_processor.hpp
/// Realtime pitch shifting and time-scale modification for coherent
/// channel groups, built on SpectralFrameEngine (STFT/WOLA),
/// MultichannelPhaseCoordinator (Laroche-Dolson phase propagation with
/// identity peak locking) and SpectralEnvelopeShifter (formant
/// preservation / shifting).
///
/// Pitch shifting is realized as time-scale modification followed by
/// resampling (Laroche & Dolson, "New Phase-Vocoder Techniques for
/// Pitch-Shifting, Harmonizing and Other Exotic Effects," WASPAA 1999):
/// the synthesis hop tracks `pitch_ratio * analysis_hop` through a
/// fractional accumulator, and an internal Catmull-Rom reader maps each
/// output sample to its stretched-stream position through the producer's
/// own frame map (input frame position -> synthesized start), so
/// process() emits exactly as many samples as it consumes with a fixed,
/// exactly-reported latency at every ratio — the reader cannot drift
/// against the synthesis hops by construction.
///
/// Two modes, chosen at prepare():
///   - `realtime_pitch`: equal-length process(); pitch in semitones,
///     duration preserved. Latency = fft_size + analysis_hop, exact and
///     block-size independent.
///   - `time_stretch`: pull-style feed()/read_stretched(); time ratio
///     independent of pitch (pitch fixed at 0 in this mode). Output
///     availability is tracked exactly; `achieved_time_ratio()` reports
///     the hop-quantized ratio actually applied for test assertions.
///
/// Geometry is derived from the quality mode and the COLA conditions of
/// the Hann window only (quality: 4096/512 at a 96 ms latency budget;
/// low_latency: 1024/256 at 26.7 ms @ 48 kHz) — constants come from the
/// algorithm's own geometry, not from any reference product.
///
/// No allocation or locks after prepare().

#include <pulp/signal/latency_aware_control_smoother.hpp>
#include <pulp/signal/multichannel_phase_coordinator.hpp>
#include <pulp/signal/spectral_envelope_shifter.hpp>
#include <pulp/signal/spectral_frame_engine.hpp>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pulp::signal {

enum class PitchTimeQuality { quality, low_latency };
enum class PitchTimeMode { realtime_pitch, time_stretch };
enum class FormantMode { follow, preserve };

struct RealtimePitchTimeConfig {
    PitchTimeMode mode = PitchTimeMode::realtime_pitch;
    PitchTimeQuality quality = PitchTimeQuality::quality;
    int channels = 1;
    int max_block = 4096;
    float max_pitch_semitones = 12.0f;     // sizing bound, |pitch| clamp
    float max_time_ratio = 2.0f;           // sizing bound for time_stretch
    float pitch_smoothing_seconds = 0.03f; // semitone-domain attack/release
    FormantMode formant_mode = FormantMode::follow;
    int true_envelope_iterations = 3;      // formant-path quality
};

class RealtimePitchTimeProcessor {
public:
    RealtimePitchTimeProcessor() = default;

    void prepare(double sample_rate, const RealtimePitchTimeConfig& config) {
        assert(sample_rate > 0.0);
        assert(config.channels >= 1);
        config_ = config;
        sample_rate_ = sample_rate;

        assert(config.channels <= kMaxChannels);
        const bool quality = config.quality == PitchTimeQuality::quality;
        fft_size_ = quality ? 4096 : 1024;
        analysis_hop_ = quality ? 512 : 256;

        const float max_ratio = std::exp2(config.max_pitch_semitones / 12.0f);
        const float max_stretch =
            config.mode == PitchTimeMode::time_stretch
                ? std::max(config.max_time_ratio, 1.0f)
                : max_ratio;

        SpectralFrameEngineConfig engine_config;
        engine_config.fft_size = fft_size_;
        engine_config.analysis_hop = analysis_hop_;
        engine_config.channels = config.channels;
        engine_config.max_block = std::max(config.max_block, analysis_hop_);
        engine_config.max_synthesis_hop =
            static_cast<int>(std::ceil(max_stretch * analysis_hop_)) + 1;
        engine_.prepare(engine_config);

        coordinator_.prepare(fft_size_, config.channels);

        SpectralEnvelopeShifterConfig env_config;
        env_config.fft_size = fft_size_;
        env_config.true_envelope_iterations = config.true_envelope_iterations;
        envelope_.prepare(env_config);

        LatencyAwareControlSmoother::Config smoother_config;
        smoother_config.domain = LatencyAwareControlSmoother::Domain::semitone;
        smoother_config.attack_seconds = config.pitch_smoothing_seconds;
        smoother_config.release_seconds = config.pitch_smoothing_seconds;
        pitch_smoother_.prepare(sample_rate, smoother_config);
        formant_smoother_.prepare(sample_rate, smoother_config);

        // Stretched-stream ring: must span the read-to-write gap
        // (latency * ratio) plus one engine drain burst and one block.
        const float span = static_cast<float>(fft_size_ + 2 * analysis_hop_
                                              + engine_config.max_block)
                           * std::max(max_stretch, 1.0f)
                         + static_cast<float>(fft_size_) + 64.0f;
        ring_size_ = 1;
        while (ring_size_ < static_cast<int>(span)) ring_size_ <<= 1;
        ring_mask_ = ring_size_ - 1;
        stretch_ring_.assign(static_cast<size_t>(config.channels) * ring_size_, 0.0f);

        drain_buf_.assign(static_cast<size_t>(config.channels)
                          * engine_config.max_synthesis_hop * 4, 0.0f);
        drain_ptrs_.resize(static_cast<size_t>(config.channels));
        reset();
    }

    /// Fixed pipeline delay of process() in realtime_pitch mode.
    int latency_samples() const { return fft_size_ + analysis_hop_ + kReadGuard; }
    int fft_size() const { return fft_size_; }

    void set_pitch_semitones(float semitones) {
        pitch_smoother_.set_target(
            std::clamp(semitones, -config_.max_pitch_semitones, config_.max_pitch_semitones));
    }

    void set_formant_semitones(float semitones) {
        formant_smoother_.set_target(
            std::clamp(semitones, -config_.max_pitch_semitones, config_.max_pitch_semitones));
    }

    void set_formant_mode(FormantMode mode) { config_.formant_mode = mode; }

    /// time_stretch mode only; > 1 lengthens. Takes effect at the next frame.
    void set_time_ratio(float ratio) {
        time_ratio_ = std::clamp(ratio, 1.0f / config_.max_time_ratio, config_.max_time_ratio);
    }

    /// Hop-quantized stretch actually applied so far (time_stretch mode).
    double achieved_time_ratio() const {
        return frames_done_ > 0
                   ? static_cast<double>(synth_accum_int_)
                         / (static_cast<double>(frames_done_) * analysis_hop_)
                   : static_cast<double>(time_ratio_);
    }

    /// realtime_pitch mode: consume and produce exactly `num_samples`.
    void process(const float* const* in, float* const* out, int num_samples) {
        assert(config_.mode == PitchTimeMode::realtime_pitch);
        assert(num_samples <= config_.max_block);

        feed_engine(in, num_samples);
        pitch_smoother_.advance(num_samples);
        formant_smoother_.advance(num_samples);

        // Catmull-Rom read of the stretched stream, positioned through
        // the producer's own frame map: frame f covers input starting at
        // f * hop and was synthesized starting at stretched position
        // S_f, so the stretched position playing input time t is the
        // piecewise-linear interpolation through the (f * hop -> S_f)
        // pairs. Output sample T plays input time T - latency exactly —
        // no open-loop ratio integration, hence no drift against the hop
        // accumulator, exact latency at every ratio, and the local read
        // slope (the pitch ratio) is consistent with the synthesis hops
        // by construction. The reader trails the producer by under two
        // frames, which the latency guard plus drain cadence always
        // covers in realtime use.
        const auto lat = static_cast<std::int64_t>(latency_samples());
        const auto hop = static_cast<std::int64_t>(analysis_hop_);
        for (int i = 0; i < num_samples; ++i) {
            if (out_count_ < lat) {
                for (int ch = 0; ch < config_.channels; ++ch) out[ch][i] = 0.0f;
            } else {
                const std::int64_t target_in = out_count_ - lat;
                const std::int64_t f = target_in / hop;
                if (f + 1 < frames_done_) {
                    const std::int64_t s0 = frame_starts_[static_cast<size_t>(f & kFrameMapMask)];
                    const std::int64_t s1 =
                        frame_starts_[static_cast<size_t>((f + 1) & kFrameMapMask)];
                    const double frac =
                        static_cast<double>(target_in - f * hop) / static_cast<double>(hop);
                    read_pos_ = static_cast<double>(s0)
                              + frac * static_cast<double>(s1 - s0);
                    read_fractional(out, i);
                } else {
                    for (int ch = 0; ch < config_.channels; ++ch) out[ch][i] = 0.0f;
                }
            }
            ++out_count_;
        }
    }

    /// time_stretch mode: push input; stretched output accumulates.
    void feed(const float* const* in, int num_samples) {
        assert(config_.mode == PitchTimeMode::time_stretch);
        feed_engine(in, num_samples);
        pitch_smoother_.advance(num_samples);
        formant_smoother_.advance(num_samples);
    }

    /// time_stretch mode: stretched samples ready to read.
    int available_stretched() const {
        return static_cast<int>(stretch_written_ - stretch_read_);
    }

    /// time_stretch mode: pop stretched samples (caller respects
    /// available_stretched(); excess is zero-filled without advancing).
    void read_stretched(float* const* out, int num_samples) {
        for (int i = 0; i < num_samples; ++i) {
            const bool valid = stretch_read_ < stretch_written_;
            const auto idx = static_cast<size_t>(stretch_read_ & ring_mask_);
            for (int ch = 0; ch < config_.channels; ++ch)
                out[ch][i] = valid
                                 ? stretch_ring_[static_cast<size_t>(ch) * ring_size_ + idx]
                                 : 0.0f;
            if (valid) ++stretch_read_;
        }
    }

    void reset() {
        engine_.reset();
        coordinator_.reset();
        std::fill(stretch_ring_.begin(), stretch_ring_.end(), 0.0f);
        pitch_smoother_.set_immediate(pitch_smoother_.target());
        formant_smoother_.set_immediate(formant_smoother_.target());
        synth_accum_ = 0.0;
        synth_accum_int_ = 0;
        frames_done_ = 0;
        stretch_written_ = 0;
        stretch_read_ = 0;
        read_pos_ = 0.0;
        std::fill(std::begin(frame_starts_), std::end(frame_starts_),
                  static_cast<std::int64_t>(0));
        out_count_ = 0;
        input_count_ = 0;
    }

private:
    // Push input through analysis; per frame: phase-propagate, apply the
    // formant correction, synthesize at the accumulated hop, and drain
    // the engine's final samples into the stretched ring.
    void feed_engine(const float* const* in, int num_samples) {
        int done = 0;
        while (done < num_samples) {
            const int run = std::min(num_samples - done, analysis_hop_);
            offset_in_block_ = done + run;
            engine_.analyze(advance_ptrs(in, done), run,
                            [this](std::complex<float>* const* frames, int bins) {
                                handle_frame(frames, bins);
                            });
            done += run;
            input_count_ += run;
        }
    }

    void handle_frame(std::complex<float>* const* frames, int bins) {
        // Controls evaluated at the frame boundary. The smoothers still
        // sit at the block start here (they advance after feeding), so
        // the frame's in-block position is a positive forward offset.
        const float pitch_ratio =
            config_.mode == PitchTimeMode::realtime_pitch
                ? pitch_smoother_.ratio_at(offset_in_block_)
                : 1.0f;
        const float formant_ratio = formant_smoother_.ratio_at(offset_in_block_);
        const float stretch = config_.mode == PitchTimeMode::time_stretch
                                  ? time_ratio_
                                  : pitch_ratio;

        // Integer synthesis hop from the fractional accumulator, so the
        // average hop tracks stretch * analysis_hop exactly.
        synth_accum_ += static_cast<double>(stretch) * analysis_hop_;
        int hop = static_cast<int>(std::llround(synth_accum_) - synth_accum_int_);
        hop = std::clamp(hop, 1, static_cast<int>(std::ceil(
                                     static_cast<double>(stretch) * analysis_hop_))
                                 + 1);
        // Record this frame's stretched start position for the reader's
        // input-time -> stretched-position map.
        frame_starts_[static_cast<size_t>(frames_done_ & kFrameMapMask)] = synth_accum_int_;
        synth_accum_int_ += hop;
        ++frames_done_;

        coordinator_.process_group(frames, bins, analysis_hop_, hop, 0.0f);

        // Formant path: warp = pitch_ratio / formant_ratio in preserve
        // mode (cancels the resampler's envelope scaling), 1 / formant_ratio
        // in follow mode. warp == 1 is an exact bypass inside the shifter.
        const float warp =
            (config_.formant_mode == FormantMode::preserve ? pitch_ratio : 1.0f)
            / formant_ratio;
        envelope_.process_group(frames, config_.channels, bins, warp);

        engine_.synthesize_frame(frames, hop);
        drain_engine();
    }

    void drain_engine() {
        int avail = engine_.available_output();
        while (avail > 0) {
            const int chunk = std::min(avail, static_cast<int>(
                drain_buf_.size() / static_cast<size_t>(config_.channels)));
            for (int ch = 0; ch < config_.channels; ++ch)
                drain_ptrs_[static_cast<size_t>(ch)] =
                    drain_buf_.data() + static_cast<size_t>(ch)
                    * (drain_buf_.size() / static_cast<size_t>(config_.channels));
            engine_.read_output(drain_ptrs_.data(), chunk);
            for (int ch = 0; ch < config_.channels; ++ch) {
                const float* src = drain_ptrs_[static_cast<size_t>(ch)];
                float* ring = stretch_ring_.data() + static_cast<size_t>(ch) * ring_size_;
                for (int i = 0; i < chunk; ++i)
                    ring[static_cast<size_t>((stretch_written_ + i) & ring_mask_)] = src[i];
            }
            stretch_written_ += chunk;
            avail -= chunk;
        }
    }

    // Catmull-Rom interpolation of the stretched ring at read_pos_.
    void read_fractional(float* const* out, int i) {
        if (stretch_written_ < 4) {
            for (int ch = 0; ch < config_.channels; ++ch) out[ch][i] = 0.0f;
            return;
        }
        const auto i1 = static_cast<std::int64_t>(read_pos_);
        const float t = static_cast<float>(read_pos_ - static_cast<double>(i1));
        const std::int64_t last = stretch_written_ - 1;
        const auto clamp_idx = [&](std::int64_t p) {
            return static_cast<size_t>(std::clamp<std::int64_t>(p, 0, last) & ring_mask_);
        };
        for (int ch = 0; ch < config_.channels; ++ch) {
            const float* ring = stretch_ring_.data() + static_cast<size_t>(ch) * ring_size_;
            const float p0 = ring[clamp_idx(i1 - 1)];
            const float p1 = ring[clamp_idx(i1)];
            const float p2 = ring[clamp_idx(i1 + 1)];
            const float p3 = ring[clamp_idx(i1 + 2)];
            const float a = 0.5f * (-p0 + 3.0f * p1 - 3.0f * p2 + p3);
            const float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
            const float c = 0.5f * (p2 - p0);
            out[ch][i] = ((a * t + b) * t + c) * t + p1;
        }
    }

    const float* const* advance_ptrs(const float* const* in, int offset) {
        for (int ch = 0; ch < config_.channels; ++ch)
            in_ptrs_scratch_[ch] = in[ch] + offset;
        return in_ptrs_scratch_;
    }

    static constexpr int kReadGuard = 8;   // frame-map + cubic-interp headroom
    static constexpr int kFrameMapMask = 63;

    RealtimePitchTimeConfig config_;
    double sample_rate_ = 48000.0;
    int fft_size_ = 4096;
    int analysis_hop_ = 512;
    float time_ratio_ = 1.0f;

    SpectralFrameEngine engine_;
    MultichannelPhaseCoordinator coordinator_;
    SpectralEnvelopeShifter envelope_;
    LatencyAwareControlSmoother pitch_smoother_;
    LatencyAwareControlSmoother formant_smoother_;

    int ring_size_ = 0;
    int ring_mask_ = 0;
    std::vector<float> stretch_ring_;
    std::vector<float> drain_buf_;
    std::vector<float*> drain_ptrs_;
    static constexpr int kMaxChannels = 64;
    const float* in_ptrs_scratch_[kMaxChannels] = {};

    double synth_accum_ = 0.0;
    std::int64_t synth_accum_int_ = 0;
    std::int64_t frames_done_ = 0;
    std::int64_t stretch_written_ = 0;
    std::int64_t stretch_read_ = 0;
    double read_pos_ = 0.0;
    std::int64_t frame_starts_[kFrameMapMask + 1] = {};
    std::int64_t out_count_ = 0;
    std::int64_t input_count_ = 0;
    int offset_in_block_ = 0;
};

} // namespace pulp::signal

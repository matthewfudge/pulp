#pragma once

/// @file parameter_edit.hpp
/// Host-automatable UI parameter editing.
///
/// A UI control that wants its moves recorded by a DAW's automation must
/// bracket each interaction in a host gesture: begin → write values →
/// end. Without the begin/end, hosts do not record the automation, and
/// some format adapters re-push the host's own parameter value into the
/// store every audio block, fighting a raw UI write. `ParameterEdit`
/// encapsulates that lifecycle for a custom view that drives one or more
/// parameters from a single gesture (an XY pad, a 2D handle, a multi-param
/// macro), so the view does not hand-roll the gesture bookkeeping.
///
/// It also caches the in-flight value of each parameter so the view can
/// display exactly what the user is doing during the drag, independent of
/// when (or whether) the host echoes the change back through the store.
///
/// Widget authors usually do not need this directly — `bind_parameter()`
/// (pulp/view/parameter_binding.hpp) wires the standard widgets to a store
/// parameter, gestures included. `ParameterEdit` is the lower-level
/// primitive for bespoke controls.
///
/// UI-thread only; not real-time-safe and not thread-safe.

#include <pulp/state/store.hpp>
#include <array>
#include <cstddef>

namespace pulp::state {

class ParameterEdit {
public:
    /// Maximum number of parameters a single gesture can drive.
    static constexpr std::size_t kMaxParams = 8;

    explicit ParameterEdit(StateStore& store) : store_(&store) {}

    ParameterEdit(const ParameterEdit&) = delete;
    ParameterEdit& operator=(const ParameterEdit&) = delete;

    /// End any in-flight gesture so a gesture is never left open if the
    /// view is destroyed mid-drag.
    ~ParameterEdit() { finish(); }

    /// Begin a gesture over `ids`. Opens a host gesture for each parameter
    /// and seeds the in-flight cache from the store's current values. A
    /// no-op (after finishing any prior gesture) if already active with the
    /// same set is not assumed — call finish() first if reusing.
    void begin(std::initializer_list<ParamID> ids) {
        finish();
        count_ = 0;
        for (ParamID id : ids) {
            if (count_ >= kMaxParams) break;
            ids_[count_] = id;
            values_[count_] = store_->get_value(id);
            store_->begin_gesture(id);
            ++count_;
        }
        active_ = count_ > 0;
    }

    /// Convenience single-parameter begin.
    void begin(ParamID id) { begin({id}); }

    /// Write a raw value to a parameter inside the gesture and update the
    /// in-flight cache. Silently ignores ids not in the active gesture.
    void set(ParamID id, float value) {
        for (std::size_t i = 0; i < count_; ++i) {
            if (ids_[i] == id) {
                values_[i] = value;
                store_->set_value(id, value);
                return;
            }
        }
    }

    /// The value the UI should display for `id`: the in-flight cache while
    /// a gesture is active, otherwise `fallback` (typically the live store
    /// value). Lets the control follow the cursor during a drag even when
    /// the host echo lags or briefly overwrites the store.
    float display_value(ParamID id, float fallback) const {
        if (active_) {
            for (std::size_t i = 0; i < count_; ++i)
                if (ids_[i] == id) return values_[i];
        }
        return fallback;
    }

    bool active() const { return active_; }

    /// End the gesture (closing each parameter's host gesture). Safe to
    /// call when inactive.
    void finish() {
        if (!active_) return;
        for (std::size_t i = 0; i < count_; ++i)
            store_->end_gesture(ids_[i]);
        active_ = false;
        count_ = 0;
    }

private:
    StateStore* store_;
    std::array<ParamID, kMaxParams> ids_{};
    std::array<float, kMaxParams> values_{};
    std::size_t count_ = 0;
    bool active_ = false;
};

} // namespace pulp::state

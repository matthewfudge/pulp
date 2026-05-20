#pragma once

// Sample-accurate parameter-event queue used for per-block automation.
//
// Ordering contract: events are sorted by sample_offset ascending before
// being handed to consumers. Callers that append events unordered must call
// sort() before passing the queue on.

#include <pulp/state/parameter.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace pulp::state {

struct ParameterEvent {
    ParamID param_id = 0;
    int32_t sample_offset = 0; // 0..num_samples-1 within the current block
    float value = 0.0f;        // plain parameter domain
};

class ParameterEventQueue {
public:
    ParameterEventQueue() = default;

    void push(const ParameterEvent& e) { events_.push_back(e); }
    void push(ParameterEvent&& e) { events_.push_back(std::move(e)); }

    void clear() { events_.clear(); }
    bool empty() const { return events_.empty(); }
    std::size_t size() const { return events_.size(); }

    void sort() {
        std::sort(events_.begin(), events_.end(),
            [](const ParameterEvent& a, const ParameterEvent& b) {
                return a.sample_offset < b.sample_offset;
            });
    }

    auto begin() { return events_.begin(); }
    auto end() { return events_.end(); }
    auto begin() const { return events_.begin(); }
    auto end() const { return events_.end(); }

    const std::vector<ParameterEvent>& events() const { return events_; }

private:
    std::vector<ParameterEvent> events_;
};

} // namespace pulp::state

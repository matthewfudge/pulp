// state_inspector.cpp — Parameter state monitoring and editing

#include <pulp/inspect/state_inspector.hpp>

namespace pulp::inspect {

struct StateInspector::SharedState {
    std::mutex mutex;
    std::vector<ParamChange> changes;
    std::atomic<bool> alive{true};
};

StateInspector::StateInspector(StateStore& store)
    : store_(store)
    , shared_state_(std::make_shared<SharedState>())
{

    auto state = shared_state_;  // copy shared_ptr for the lambda
    store_.add_listener([state](ParamID id, float value) {
        if (!state->alive.load(std::memory_order_acquire)) return;

        ParamChange change{id, value, std::chrono::steady_clock::now()};
        std::lock_guard lock(state->mutex);
        state->changes.push_back(change);
        if (state->changes.size() > kMaxChanges)
            state->changes.erase(state->changes.begin());
    });
}

StateInspector::~StateInspector() {
    // Signal the shared state to stop accepting changes.
    // In-flight callbacks will see alive=false and return.
    if (shared_state_) shared_state_->alive.store(false, std::memory_order_release);
}

std::vector<StateInspector::ParamSnapshot> StateInspector::all_params() const {
    std::vector<ParamSnapshot> result;
    auto params = store_.all_params();
    result.reserve(params.size());

    for (auto& info : params) {
        ParamSnapshot snap;
        snap.id = info.id;
        snap.name = info.name;
        snap.unit = info.unit;
        snap.value = store_.get_value(info.id);
        snap.normalized = store_.get_normalized(info.id);
        snap.modulated = store_.get_modulated(info.id);
        snap.default_value = info.range.default_value;
        snap.min = info.range.min;
        snap.max = info.range.max;
        snap.step = info.range.step;
        snap.display_value = info.to_string ? info.to_string(snap.value) : "";
        result.push_back(std::move(snap));
    }

    return result;
}

std::vector<StateInspector::ParamChange> StateInspector::recent_changes() const {
    if (!shared_state_) return {};
    std::lock_guard lock(shared_state_->mutex);
    return shared_state_->changes;
}

void StateInspector::set_param(ParamID id, float value) {
    store_.set_value(id, value);
}

} // namespace pulp::inspect

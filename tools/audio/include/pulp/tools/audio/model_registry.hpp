#pragma once

// The generic model registry now lives in pulp::runtime (so plugins/apps can link it).
// tools/audio keeps the audio *catalog* (the data) + these compatibility aliases.

#include <pulp/runtime/model_registry.hpp>

#include <string_view>
#include <vector>

namespace pulp::tools::audio {

using RegisteredModel = pulp::runtime::ModelEntry;
using pulp::runtime::resolve_checkpoint_url;

/// The audio model catalog (CLAP etc.). Data lives here; the mechanism is generic.
const std::vector<RegisteredModel>& registered_models();

inline const RegisteredModel* find_registered_model(std::string_view model_id) {
    return pulp::runtime::find_model(registered_models(), model_id);
}

}  // namespace pulp::tools::audio

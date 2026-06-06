#pragma once

// Generic model store/state primitive (promoted from tools/audio). Owns the on-disk
// layout under PULP_HOME (`<home>/<subsystem>/model-state.json` for the active model,
// `<home>/<subsystem>/models/<id>.json` for per-model install metadata), the
// installed/active queries, list (over a caller-supplied registry), and activate.
// Parameterized by `subsystem` (e.g. "audio", "magenta") so multiple consumers share
// one mechanism. The downloader (install) lands in a follow-up (MM-PR2).

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <pulp/runtime/model_registry.hpp>

namespace pulp::runtime {

struct InstalledModelRecord {
    std::filesystem::path metadata_path;
    bool metadata_found = false;
    std::string model_id;
    std::string backend;
    std::string checkpoint_ref;
    std::filesystem::path resolved_checkpoint_path;
    bool checkpoint_exists = false;

    [[nodiscard]] bool loadable() const { return metadata_found && checkpoint_exists; }
};

struct ListedModel {
    ModelEntry model;
    std::string status = "not_installed";  // installed | missing_checkpoint | not_installed
    bool active = false;
    std::filesystem::path resolved_checkpoint_path;
};

struct ModelListResult {
    std::filesystem::path pulp_home;
    std::string active_model_id;
    std::vector<ListedModel> models;
    std::string error;
};

struct ActivateModelResult {
    bool ok = false;
    std::filesystem::path state_path;
    std::string active_model_id;
    std::string backend;
    std::string checkpoint_ref;
    std::filesystem::path resolved_checkpoint_path;
    std::string error;
};

// PULP_HOME resolution: explicit override → $PULP_HOME → $HOME/.pulp (USERPROFILE on
// Windows). NOTE (path policy, see plan): a sandboxed AUv3/iOS host may not have HOME
// access — callers in those contexts should pass an explicit container path.
std::filesystem::path resolve_pulp_home(const std::filesystem::path& override_path = {});

std::filesystem::path model_state_path(std::string_view subsystem,
                                       const std::filesystem::path& pulp_home_override = {});
std::filesystem::path model_install_path(std::string_view subsystem, std::string_view model_id,
                                         const std::filesystem::path& pulp_home_override = {});
InstalledModelRecord read_installed_model(std::string_view subsystem, std::string_view model_id,
                                          const std::filesystem::path& pulp_home_override = {});
std::string read_active_model_id(std::string_view subsystem,
                                 const std::filesystem::path& pulp_home_override = {});
ModelListResult list_models(const std::vector<ModelEntry>& registry, std::string_view subsystem,
                            const std::filesystem::path& pulp_home_override = {});
ActivateModelResult activate_model(const std::vector<ModelEntry>& registry, std::string_view subsystem,
                                   std::string_view model_id,
                                   const std::filesystem::path& pulp_home_override = {});

std::string to_json(const ModelListResult& result);
std::string to_json(const ActivateModelResult& result);

}  // namespace pulp::runtime

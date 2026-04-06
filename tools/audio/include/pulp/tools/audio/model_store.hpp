#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <pulp/tools/audio/model_registry.hpp>

namespace pulp::tools::audio {

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
    RegisteredModel model;
    std::string status = "not_installed";
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

std::filesystem::path resolve_pulp_home(const std::filesystem::path& override_path = {});
std::filesystem::path audio_model_state_path(const std::filesystem::path& pulp_home_override = {});
std::filesystem::path audio_model_install_path(std::string_view model_id,
                                               const std::filesystem::path& pulp_home_override = {});
InstalledModelRecord read_installed_model(std::string_view model_id,
                                          const std::filesystem::path& pulp_home_override = {});
std::string read_active_model_id(const std::filesystem::path& pulp_home_override = {});
ModelListResult list_models(const std::filesystem::path& pulp_home_override = {});
ActivateModelResult activate_model(std::string_view model_id,
                                   const std::filesystem::path& pulp_home_override = {});

std::string to_json(const ModelListResult& result);
std::string to_json(const ActivateModelResult& result);

} // namespace pulp::tools::audio

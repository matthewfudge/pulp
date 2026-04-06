#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace pulp::tools::audio {

struct ModelStatus {
    std::filesystem::path state_path;
    bool state_file_found = false;
    std::string configured_model_id;
    std::string backend;
    std::string checkpoint_ref;
    std::filesystem::path resolved_checkpoint_path;
    bool checkpoint_exists = false;
    std::string message;

    [[nodiscard]] bool loadable() const {
        return !configured_model_id.empty() && checkpoint_exists;
    }
};

struct BundleResult {
    int rank = 0;
    double score = 0.0;
    std::string source_file;
    double source_duration_ms = 0.0;
    double start_ms = 0.0;
    double end_ms = 0.0;
    std::string excerpt_file;
};

struct BundleReadResult {
    std::filesystem::path bundle_path;
    std::filesystem::path manifest_path;
    std::filesystem::path model_path;
    std::filesystem::path ranked_results_path;
    bool ok = false;
    int bundle_version = 0;
    std::string tool;
    std::string requested_model_id;
    std::string loaded_model_id;
    std::string backend;
    std::size_t result_count = 0;
    std::vector<BundleResult> results;
    std::string error;
};

ModelStatus query_model_status(const std::filesystem::path& pulp_home_override = {});
BundleReadResult read_excerpt_bundle(const std::filesystem::path& bundle_path);

std::string to_json(const ModelStatus& status);
std::string to_json(const BundleReadResult& bundle);

} // namespace pulp::tools::audio

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <pulp/tools/audio/service.hpp>

namespace pulp::tools::audio {

struct ExcerptFindRequest {
    std::string text;
    std::filesystem::path input_path;
    std::string model_id;
    bool recursive = false;
    std::size_t top_k = 5;
    uint64_t window_ms = 1500;
    uint64_t hop_ms = 250;
    double min_score = 0.0;
    std::size_t max_candidates_per_file = 3;
    std::filesystem::path bundle_out;
    bool dry_run = false;
};

struct ExcerptFindResult {
    bool ok = false;
    std::filesystem::path bundle_path;
    std::string query;
    std::string requested_model_id;
    std::string loaded_model_id;
    std::string backend;
    std::filesystem::path resolved_checkpoint_path;
    std::vector<BundleResult> results;
    std::size_t scanned_file_count = 0;
    std::vector<std::string> skipped_files;
    std::string error;
};

ExcerptFindResult run_excerpt_find(const ExcerptFindRequest& request,
                                   const std::filesystem::path& pulp_home_override = {});

std::string to_json(const ExcerptFindResult& result);

} // namespace pulp::tools::audio

#pragma once

#include <filesystem>
#include <string>

namespace pulp::cli {

namespace fs = std::filesystem;

struct DesignBindingInput {
    fs::path cwd_root;
    fs::path build_dir;
    fs::path script_path;
    fs::path script_root;
    fs::path build_dir_cache_root;
    fs::path binary_build_dir;
    fs::path binary_root;
    bool build_dir_explicit = false;
    bool script_explicit = false;
};

struct DesignBindingResult {
    bool ok = false;
    fs::path root;
    fs::path build_dir;
    fs::path script_path;
    std::string root_reason;
    std::string build_reason;
    std::string script_reason;
    std::string error;
};

std::string design_binding_autobind_error();
DesignBindingResult resolve_design_binding(const DesignBindingInput& input);

}  // namespace pulp::cli

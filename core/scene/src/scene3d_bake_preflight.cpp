#include <pulp/scene/bake_preflight.hpp>
#include <pulp/scene/sidecar.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace {

void print_usage(const char* argv0) {
    std::cerr << "Usage: " << argv0
              << " [--require-runtime-evidence|--require-runtime-evidence-url] "
                 "<scene.pulp3d.json>\n";
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not open file");
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

const char* bool_text(bool value) {
    return value ? "true" : "false";
}

void print_feature_rows(const char* label,
                        const std::vector<pulp::scene::UnsupportedFeature>& features) {
    for (const auto& feature : features) {
        std::cout << label << ": " << feature.feature;
        if (!feature.node_path.empty()) {
            std::cout << " node=" << feature.node_path;
        }
        if (!feature.reason.empty()) {
            std::cout << " reason=" << feature.reason;
        }
        std::cout << "\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3 || std::string_view(argv[1]) == "--help" ||
        std::string_view(argv[1]) == "-h") {
        print_usage(argv[0]);
        return argc == 2 ? 0 : 64;
    }

    pulp::scene::BakePreflightOptions options;
    int path_arg = 1;
    if (std::string_view(argv[1]) == "--require-runtime-evidence") {
        if (argc != 3) {
            print_usage(argv[0]);
            return 64;
        }
        options.require_runtime_evidence = true;
        path_arg = 2;
    } else if (std::string_view(argv[1]) == "--require-runtime-evidence-url") {
        if (argc != 3) {
            print_usage(argv[0]);
            return 64;
        }
        options.require_runtime_evidence_url = true;
        path_arg = 2;
    } else if (argc != 2) {
        print_usage(argv[0]);
        return 64;
    }

    const auto path = std::filesystem::path(argv[path_arg]);
    std::string json;
    try {
        json = read_file(path);
    } catch (const std::exception& e) {
        std::cerr << "pulp-scene3d-bake-preflight: " << path << ": "
                  << e.what() << "\n";
        return 1;
    }

    const auto parse_result = pulp::scene::sidecar_from_json(json);
    if (!parse_result.success) {
        std::cerr << "pulp-scene3d-bake-preflight: " << path << ": "
                  << parse_result.error << "\n";
        return 1;
    }

    const auto report =
        pulp::scene::analyze_bake_preflight(parse_result.sidecar, options);
    std::cout << "bake_readiness="
              << pulp::scene::bake_preflight_readiness(report) << "\n";
    std::cout << "export_blocked=" << bool_text(report.export_blocked) << "\n";
    std::cout << "texture_encoding_blocked="
              << bool_text(report.texture_encoding_blocked) << "\n";
    std::cout << "native_runtime_has_gaps="
              << bool_text(report.native_runtime_has_gaps) << "\n";
    std::cout << "has_error_diagnostics="
              << bool_text(report.has_error_diagnostics) << "\n";
    std::cout << "runtime_evidence_missing="
              << bool_text(report.runtime_evidence_missing) << "\n";
    std::cout << "runtime_evidence_url_invalid="
              << bool_text(report.runtime_evidence_url_invalid) << "\n";
    std::cout << "export_blockers=" << report.export_blockers.size()
              << " texture_encoding_blockers="
              << report.texture_encoding_blockers.size()
              << " native_runtime_gaps=" << report.native_runtime_gaps.size()
              << " diagnostics=" << report.diagnostics.size() << "\n";

    print_feature_rows("export_blocker", report.export_blockers);
    print_feature_rows("texture_encoding_blocker",
                       report.texture_encoding_blockers);
    print_feature_rows("native_runtime_gap", report.native_runtime_gaps);

    return report.export_blocked ? 2 : 0;
}

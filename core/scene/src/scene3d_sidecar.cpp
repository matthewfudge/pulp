#include <pulp/scene/gltf_loader.hpp>
#include <pulp/scene/sidecar.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void print_usage(const char* argv0) {
    std::cerr << "Usage: " << argv0
              << " [--source VALUE] [--exporter VALUE]"
                 " [--exported-at VALUE] [--runtime-evidence URL]"
                 " <scene.glb|scene.gltf>\n";
}

bool consume_option(int& index,
                    int argc,
                    char** argv,
                    std::string_view name,
                    std::string& destination) {
    if (std::string_view(argv[index]) != name) {
        return false;
    }
    if (index + 1 >= argc) {
        std::cerr << "missing value for " << name << "\n";
        return false;
    }
    destination = argv[index + 1];
    index += 2;
    return true;
}

bool require_non_empty(std::string_view option_name,
                       const std::string& value) {
    if (!value.empty()) {
        return true;
    }
    std::cerr << option_name << " must be non-empty\n";
    return false;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || std::string_view(argv[1]) == "--help" ||
        std::string_view(argv[1]) == "-h") {
        print_usage(argv[0]);
        return argc == 2 ? 0 : 64;
    }

    pulp::scene::SidecarBuildOptions options;
    options.provenance.exporter = "pulp-scene3d-sidecar";

    std::filesystem::path scene_path;
    int index = 1;
    while (index < argc) {
        std::string_view arg(argv[index]);
        if (arg == "--source") {
            if (!consume_option(index,
                                argc,
                                argv,
                                "--source",
                                options.provenance.source)) {
                return 64;
            }
            continue;
        }
        if (arg == "--exporter") {
            if (!consume_option(index,
                                argc,
                                argv,
                                "--exporter",
                                options.provenance.exporter)) {
                return 64;
            }
            continue;
        }
        if (arg == "--exported-at") {
            if (!consume_option(index,
                                argc,
                                argv,
                                "--exported-at",
                                options.provenance.exported_at)) {
                return 64;
            }
            continue;
        }
        if (arg == "--runtime-evidence") {
            if (!consume_option(index,
                                argc,
                                argv,
                                "--runtime-evidence",
                                options.provenance.runtime_evidence)) {
                return 64;
            }
            continue;
        }
        if (!scene_path.empty()) {
            print_usage(argv[0]);
            return 64;
        }
        scene_path = std::filesystem::path(argv[index]);
        ++index;
    }

    if (scene_path.empty()) {
        print_usage(argv[0]);
        return 64;
    }

    if (options.provenance.source.empty()) {
        options.provenance.source = scene_path.string();
    }
    if (!require_non_empty("--exporter", options.provenance.exporter) ||
        !require_non_empty("--exported-at", options.provenance.exported_at)) {
        return 64;
    }
    options.source_path = scene_path.string();

    const auto result = pulp::scene::load_gltf_scene(scene_path);
    if (!result.success) {
        std::cerr << "pulp-scene3d-sidecar: " << result.error << "\n";
        for (const auto& diagnostic : result.scene.diagnostics) {
            std::cerr << pulp::scene::diagnostic_severity_name(diagnostic.severity)
                      << ": " << diagnostic.code << ": "
                      << diagnostic.message << "\n";
        }
        return 2;
    }

    const auto sidecar =
        pulp::scene::build_sidecar_from_scene(result.scene, options);
    std::cout << pulp::scene::sidecar_to_json(sidecar) << "\n";
    return pulp::scene::has_error_diagnostics(sidecar.diagnostics) ? 1 : 0;
}

#pragma once

#include <pulp/scene/scene_data.hpp>

#include <string>
#include <vector>

namespace pulp::scene {

struct SidecarProvenance {
    std::string source;
    std::string exporter;
    std::string exported_at;
    std::string runtime_evidence;
};

struct UnsupportedFeature {
    std::string feature;
    std::string reason;
    std::string node_path;
};

struct RuntimeHint {
    std::string key;
    std::string value;
};

struct SidecarData {
    SidecarProvenance provenance;
    std::vector<Diagnostic> diagnostics;
    std::vector<UnsupportedFeature> unsupported_features;
    std::vector<RuntimeHint> runtime_hints;
};

struct SidecarBuildOptions {
    SidecarProvenance provenance;
    std::string source_path;
    bool include_validation_diagnostics = true;
};

struct SidecarParseResult {
    bool success = false;
    SidecarData sidecar;
    std::string error;
};

enum class BakeUnsupportedFeature {
    shader_material,
    raw_shader_material,
    postprocessing,
    render_target,
    arbitrary_js_animation,
    physics,
    event_handler,
    texture_encoding_missing,
};

SidecarData build_sidecar_from_scene(const SceneData& scene,
                                     const SidecarBuildOptions& options = {});
void append_bake_unsupported_feature(SidecarData& sidecar,
                                     BakeUnsupportedFeature feature,
                                     const std::string& node_path = {},
                                     const std::string& source_path = {});
std::string sidecar_to_json(const SidecarData& sidecar);
SidecarParseResult sidecar_from_json(const std::string& json);

} // namespace pulp::scene

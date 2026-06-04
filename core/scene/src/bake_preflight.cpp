#include <pulp/scene/bake_preflight.hpp>

#include <string>

namespace pulp::scene {
namespace {

bool is_export_blocker(const std::string& feature) {
    return feature == "ShaderMaterial" ||
           feature == "RawShaderMaterial" ||
           feature == "Postprocessing" ||
           feature == "RenderTarget" ||
           feature == "ArbitraryJSAnimation" ||
           feature == "Physics" ||
           feature == "EventHandler";
}

bool is_texture_encoding_blocker(const std::string& feature) {
    return feature == "TextureEncoding";
}

bool has_prefix(const std::string& value, const char* prefix) {
    return value.rfind(prefix, 0) == 0;
}

bool is_runtime_evidence_url(const std::string& value) {
    const bool has_scheme =
        has_prefix(value, "https://") || has_prefix(value, "http://");
    if (!has_scheme) {
        return false;
    }
    const auto scheme_end = value.find("://");
    if (scheme_end == std::string::npos) {
        return false;
    }
    const auto host_start = scheme_end + 3u;
    const auto path_start = value.find('/', host_start);
    return path_start != std::string::npos && path_start > host_start &&
           path_start + 1u < value.size();
}

bool is_native_runtime_gap(const std::string& feature) {
    return feature == "TexturePayload" ||
           has_prefix(feature, "TextureFormat:") ||
           feature == "TransformAnimation" ||
           has_prefix(feature, "AnimationPath:") ||
           feature == "MaterialTexture:normalTangents" ||
           feature == "MaterialTexture:normalScale" ||
           feature == "MaterialTexture:occlusionStrength" ||
           feature == "MaterialTextureTransform:nonBaseColor" ||
           feature == "MaterialTexcoord:nonBaseColor" ||
           has_prefix(feature, "MaterialExtension:") ||
           has_prefix(feature, "PrimitiveMode:") ||
           has_prefix(feature, "PunctualLight:") ||
           has_prefix(feature, "Camera:") ||
           feature == "MorphWeights" ||
           feature == "MorphTargets" ||
           feature == "Skinning" ||
           feature == "GpuInstancing";
}

} // namespace

BakePreflightReport analyze_bake_preflight(
    const SidecarData& sidecar,
    const BakePreflightOptions& options) {
    BakePreflightReport report;
    report.diagnostics = sidecar.diagnostics;
    report.has_error_diagnostics = has_error_diagnostics(sidecar.diagnostics);
    report.runtime_evidence_missing =
        (options.require_runtime_evidence ||
         options.require_runtime_evidence_url) &&
        sidecar.provenance.runtime_evidence.empty();
    report.runtime_evidence_url_invalid =
        options.require_runtime_evidence_url &&
        !sidecar.provenance.runtime_evidence.empty() &&
        !is_runtime_evidence_url(sidecar.provenance.runtime_evidence);

    for (const auto& feature : sidecar.unsupported_features) {
        if (is_export_blocker(feature.feature)) {
            report.export_blockers.push_back(feature);
            continue;
        }
        if (is_texture_encoding_blocker(feature.feature)) {
            report.texture_encoding_blockers.push_back(feature);
            continue;
        }
        if (is_native_runtime_gap(feature.feature)) {
            report.native_runtime_gaps.push_back(feature);
        }
    }

    report.texture_encoding_blocked = !report.texture_encoding_blockers.empty();
    report.native_runtime_has_gaps = !report.native_runtime_gaps.empty();
    report.export_blocked = report.has_error_diagnostics ||
                            !report.export_blockers.empty() ||
                            report.texture_encoding_blocked ||
                            report.runtime_evidence_missing ||
                            report.runtime_evidence_url_invalid;
    return report;
}

const char* bake_preflight_readiness(const BakePreflightReport& report) {
    if (report.export_blocked) {
        return "blocked";
    }
    if (report.native_runtime_has_gaps) {
        return "native_gaps";
    }
    return "clean";
}

} // namespace pulp::scene

#include <pulp/scene/sidecar.hpp>

#include <choc/text/choc_JSON.h>

#include <cmath>
#include <cstdint>
#include <exception>
#include <initializer_list>
#include <iterator>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace pulp::scene {
namespace {

inline constexpr int64_t kSidecarSchemaVersion = 1;

bool has_exact_keys(const choc::value::ValueView& object,
                    std::initializer_list<const char*> expected) {
    std::set<std::string> actual;
    for (uint32_t i = 0; i < object.size(); ++i) {
        actual.insert(std::string(object.getObjectMemberAt(i).name));
    }
    std::set<std::string> expected_set;
    for (const auto* key : expected) {
        expected_set.insert(std::string(key));
    }
    return actual == expected_set;
}

bool has_one_of_exact_keys(const choc::value::ValueView& object,
                           std::initializer_list<const char*> first,
                           std::initializer_list<const char*> second) {
    return has_exact_keys(object, first) || has_exact_keys(object, second);
}

bool string_member(const choc::value::ValueView& object,
                   const char* key,
                   std::string& out) {
    if (!object.hasObjectMember(key) || !object[key].isString()) {
        return false;
    }
    out = object[key].toString();
    return true;
}

bool non_empty_string_member(const choc::value::ValueView& object,
                             const char* key,
                             std::string& out) {
    return string_member(object, key, out) && !out.empty();
}

bool diagnostic_path_member(const choc::value::ValueView& object,
                            std::string& out) {
    if (object.hasObjectMember("path")) {
        return string_member(object, "path", out);
    }
    return string_member(object, "source_path", out);
}

bool severity_from_string(const std::string& value,
                          Diagnostic::Severity& severity) {
    if (value == "warning") {
        severity = Diagnostic::Severity::warning;
        return true;
    }
    if (value == "error") {
        severity = Diagnostic::Severity::error;
        return true;
    }
    if (value == "info") {
        severity = Diagnostic::Severity::info;
        return true;
    }
    return false;
}

choc::value::Value provenance_to_json(const SidecarProvenance& provenance) {
    auto out = choc::value::createObject("");
    out.addMember("source", provenance.source);
    out.addMember("exporter", provenance.exporter);
    out.addMember("exported_at", provenance.exported_at);
    out.addMember("runtime_evidence", provenance.runtime_evidence);
    return out;
}

choc::value::Value diagnostics_to_json(const std::vector<Diagnostic>& diagnostics) {
    auto out = choc::value::createEmptyArray();
    for (const auto& diagnostic : diagnostics) {
        auto item = choc::value::createObject("");
        item.addMember("severity", std::string(diagnostic_severity_name(diagnostic.severity)));
        item.addMember("code", diagnostic.code);
        item.addMember("message", diagnostic.message);
        item.addMember("path", diagnostic.source_path);
        out.addArrayElement(item);
    }
    return out;
}

choc::value::Value unsupported_features_to_json(
    const std::vector<UnsupportedFeature>& unsupported_features) {
    auto out = choc::value::createEmptyArray();
    for (const auto& feature : unsupported_features) {
        auto item = choc::value::createObject("");
        item.addMember("feature", feature.feature);
        item.addMember("reason", feature.reason);
        item.addMember("node_path", feature.node_path);
        out.addArrayElement(item);
    }
    return out;
}

choc::value::Value runtime_hints_to_json(const std::vector<RuntimeHint>& runtime_hints) {
    auto out = choc::value::createEmptyArray();
    for (const auto& hint : runtime_hints) {
        auto item = choc::value::createObject("");
        item.addMember("key", hint.key);
        item.addMember("value", hint.value);
        out.addArrayElement(item);
    }
    return out;
}

bool parse_provenance(const choc::value::ValueView& root, SidecarData& sidecar) {
    if (!root.hasObjectMember("provenance")) {
        return true;
    }
    auto provenance = root["provenance"];
    if (!provenance.isObject()) {
        return false;
    }
    if (!has_exact_keys(provenance,
                        {"source",
                         "exporter",
                         "exported_at",
                         "runtime_evidence"})) {
        return false;
    }
    return non_empty_string_member(provenance,
                                   "source",
                                   sidecar.provenance.source) &&
           non_empty_string_member(provenance,
                                   "exporter",
                                   sidecar.provenance.exporter) &&
           non_empty_string_member(provenance,
                                   "exported_at",
                                   sidecar.provenance.exported_at) &&
           string_member(provenance,
                         "runtime_evidence",
                         sidecar.provenance.runtime_evidence);
}

bool parse_diagnostics(const choc::value::ValueView& root, SidecarData& sidecar) {
    if (!root.hasObjectMember("diagnostics")) {
        return true;
    }
    auto diagnostics = root["diagnostics"];
    if (!diagnostics.isArray()) {
        return false;
    }
    for (uint32_t i = 0; i < diagnostics.size(); ++i) {
        auto item = diagnostics[i];
        if (!item.isObject()) {
            return false;
        }
        if (!has_one_of_exact_keys(item,
                                   {"severity", "code", "message", "path"},
                                   {"severity",
                                    "code",
                                    "message",
                                    "source_path"})) {
            return false;
        }
        std::string severity;
        Diagnostic diagnostic;
        if (!string_member(item, "severity", severity) ||
            !severity_from_string(severity, diagnostic.severity) ||
            !string_member(item, "code", diagnostic.code) ||
            !string_member(item, "message", diagnostic.message) ||
            !diagnostic_path_member(item, diagnostic.source_path)) {
            return false;
        }
        sidecar.diagnostics.push_back(std::move(diagnostic));
    }
    return true;
}

bool parse_unsupported_features(const choc::value::ValueView& root,
                                SidecarData& sidecar) {
    if (!root.hasObjectMember("unsupported_features")) {
        return true;
    }
    auto unsupported_features = root["unsupported_features"];
    if (!unsupported_features.isArray()) {
        return false;
    }
    for (uint32_t i = 0; i < unsupported_features.size(); ++i) {
        auto item = unsupported_features[i];
        if (!item.isObject()) {
            return false;
        }
        if (!has_exact_keys(item, {"feature", "reason", "node_path"})) {
            return false;
        }
        UnsupportedFeature feature;
        if (!string_member(item, "feature", feature.feature) ||
            !string_member(item, "reason", feature.reason) ||
            !string_member(item, "node_path", feature.node_path)) {
            return false;
        }
        sidecar.unsupported_features.push_back(std::move(feature));
    }
    return true;
}

bool parse_runtime_hints(const choc::value::ValueView& root, SidecarData& sidecar) {
    if (!root.hasObjectMember("runtime_hints")) {
        return true;
    }
    auto runtime_hints = root["runtime_hints"];
    if (!runtime_hints.isArray()) {
        return false;
    }
    for (uint32_t i = 0; i < runtime_hints.size(); ++i) {
        auto item = runtime_hints[i];
        if (!item.isObject()) {
            return false;
        }
        if (!has_exact_keys(item, {"key", "value"})) {
            return false;
        }
        RuntimeHint hint;
        if (!string_member(item, "key", hint.key) ||
            !string_member(item, "value", hint.value)) {
            return false;
        }
        sidecar.runtime_hints.push_back(std::move(hint));
    }
    return true;
}

struct BakeUnsupportedDescriptor {
    const char* feature = "";
    const char* code = "";
    const char* reason = "";
};

BakeUnsupportedDescriptor bake_descriptor(BakeUnsupportedFeature feature) {
    switch (feature) {
        case BakeUnsupportedFeature::shader_material:
            return BakeUnsupportedDescriptor{
                "ShaderMaterial",
                "bake.unsupported_shader_material",
                "THREE.ShaderMaterial has no glTF metallic-roughness representation; convert to MeshStandardMaterial or keep this scene Live-only.",
            };
        case BakeUnsupportedFeature::raw_shader_material:
            return BakeUnsupportedDescriptor{
                "RawShaderMaterial",
                "bake.unsupported_raw_shader_material",
                "THREE.RawShaderMaterial has no portable glTF representation; convert to a supported glTF material or keep this scene Live-only.",
            };
        case BakeUnsupportedFeature::postprocessing:
            return BakeUnsupportedDescriptor{
                "Postprocessing",
                "bake.unsupported_postprocessing",
                "Postprocessing and EffectComposer passes are screen-space runtime behavior and are not representable in glTF.",
            };
        case BakeUnsupportedFeature::render_target:
            return BakeUnsupportedDescriptor{
                "RenderTarget",
                "bake.unsupported_render_target",
                "Render targets are runtime framebuffer state and cannot be exported as static glTF asset data.",
            };
        case BakeUnsupportedFeature::arbitrary_js_animation:
            return BakeUnsupportedDescriptor{
                "ArbitraryJSAnimation",
                "bake.unsupported_arbitrary_js_animation",
                "Arbitrary JavaScript animation cannot be baked unless it is reduced to glTF animation channels.",
            };
        case BakeUnsupportedFeature::physics:
            return BakeUnsupportedDescriptor{
                "Physics",
                "bake.unsupported_physics",
                "Physics simulation is runtime behavior and is outside the static glTF bake subset.",
            };
        case BakeUnsupportedFeature::event_handler:
            return BakeUnsupportedDescriptor{
                "EventHandler",
                "bake.unsupported_event_handler",
                "Pointer, keyboard, and custom event handlers do not bake into glTF; keep the scene Live-only or rebuild interaction natively.",
            };
        case BakeUnsupportedFeature::texture_encoding_missing:
            return BakeUnsupportedDescriptor{
                "TextureEncoding",
                "bake.texture_encoding_missing",
                "Texture export needs a native toDataURL/toBlob image-encoding path or pre-encoded texture buffers before GLTFExporter can emit textured GLBs.",
            };
    }
    return BakeUnsupportedDescriptor{};
}

bool native_renderer_texture_format_supported(const std::string& mime_type) {
    return mime_type.empty() ||
        mime_type == "image/png" ||
        mime_type == "image/jpeg";
}

bool material_has_deferred_non_base_color_texture_transform(
    const SceneData& scene,
    uint32_t material_index,
    const MaterialData& material);
bool material_has_deferred_non_base_color_texcoord1(
    const SceneData& scene,
    uint32_t material_index,
    const MaterialData& material);
bool material_has_deferred_normal_scale(const SceneData& scene,
                                        uint32_t material_index,
                                        const MaterialData& material);
bool material_has_deferred_occlusion_strength(const SceneData& scene,
                                              uint32_t material_index,
                                              const MaterialData& material);

uint32_t effective_texture_texcoord(
    uint32_t texture_texcoord,
    const TextureTransformData& texture_transform) {
    if (texture_transform.enabled &&
        texture_transform.texcoord_override != invalid_scene_index) {
        return texture_transform.texcoord_override;
    }
    return texture_texcoord;
}

bool primitive_supports_texture_route(
    const PrimitiveData& primitive,
    uint32_t texture_texcoord,
    const TextureTransformData& texture_transform) {
    const uint32_t texcoord =
        effective_texture_texcoord(texture_texcoord, texture_transform);
    const size_t vertex_count = primitive.positions.size() / 3u;
    if (texcoord == 0u) {
        return primitive.texcoord0.size() >= vertex_count * 2u;
    }
    if (texcoord == 1u) {
        return primitive.texcoord1.size() >= vertex_count * 2u;
    }
    return false;
}

bool primitive_can_derive_tangent_basis(
    const PrimitiveData& primitive,
    uint32_t texture_texcoord,
    const TextureTransformData& texture_transform) {
    const size_t vertex_count = primitive.positions.size() / 3u;
    if (vertex_count == 0u ||
        primitive.normals.size() < vertex_count * 3u ||
        primitive.indices.size() < 3u ||
        primitive.indices.size() % 3u != 0u ||
        !primitive_supports_texture_route(primitive,
                                          texture_texcoord,
                                          texture_transform)) {
        return false;
    }
    if (texture_transform.enabled &&
        (std::abs(texture_transform.scale[0]) <= 0.000001f ||
         std::abs(texture_transform.scale[1]) <= 0.000001f)) {
        return false;
    }

    const uint32_t texcoord =
        effective_texture_texcoord(texture_texcoord, texture_transform);
    const auto& uvs = texcoord == 1u ? primitive.texcoord1 : primitive.texcoord0;
    for (size_t index = 0; index + 2u < primitive.indices.size(); index += 3u) {
        const uint32_t i0 = primitive.indices[index + 0u];
        const uint32_t i1 = primitive.indices[index + 1u];
        const uint32_t i2 = primitive.indices[index + 2u];
        if (i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count) {
            return false;
        }
        const float s1 = uvs[i1 * 2u + 0u] - uvs[i0 * 2u + 0u];
        const float t1 = uvs[i1 * 2u + 1u] - uvs[i0 * 2u + 1u];
        const float s2 = uvs[i2 * 2u + 0u] - uvs[i0 * 2u + 0u];
        const float t2 = uvs[i2 * 2u + 1u] - uvs[i0 * 2u + 1u];
        if (std::abs(s1 * t2 - s2 * t1) > 0.000001f) {
            return true;
        }
    }

    return false;
}

bool primitive_has_normal_texture_basis(
    const PrimitiveData& primitive,
    uint32_t texture_texcoord,
    const TextureTransformData& texture_transform) {
    const size_t vertex_count = primitive.positions.size() / 3u;
    return primitive.tangents.size() >= vertex_count * 4u ||
           primitive_can_derive_tangent_basis(primitive,
                                              texture_texcoord,
                                              texture_transform);
}

bool material_texture_route_supported_by_primitives(
    const SceneData& scene,
    uint32_t material_index,
    uint32_t texture_texcoord,
    const TextureTransformData& texture_transform) {
    bool material_used = false;
    for (const auto& mesh : scene.meshes) {
        for (const auto& primitive : mesh.primitives) {
            if (primitive.material != material_index) {
                continue;
            }
            material_used = true;
            if (!primitive_supports_texture_route(primitive,
                                                  texture_texcoord,
                                                  texture_transform)) {
                return false;
            }
        }
    }
    return material_used;
}

bool material_normal_texture_basis_supported_by_primitives(
    const SceneData& scene,
    uint32_t material_index,
    uint32_t texture_texcoord,
    const TextureTransformData& texture_transform) {
    bool material_used = false;
    for (const auto& mesh : scene.meshes) {
        for (const auto& primitive : mesh.primitives) {
            if (primitive.material != material_index) {
                continue;
            }
            material_used = true;
            if (!primitive_has_normal_texture_basis(primitive,
                                                    texture_texcoord,
                                                    texture_transform)) {
                return false;
            }
        }
    }
    return material_used;
}

bool material_has_deferred_non_base_color_texture_transform(
    const SceneData& scene,
    uint32_t material_index,
    const MaterialData& material) {
    return (is_valid_scene_index(material.metallic_roughness_texture,
                                 scene.textures.size()) &&
            material.metallic_roughness_transform.enabled &&
            !material_texture_route_supported_by_primitives(
                scene,
                material_index,
                material.metallic_roughness_texcoord,
                material.metallic_roughness_transform)) ||
           (is_valid_scene_index(material.normal_texture,
                                 scene.textures.size()) &&
            material.normal_transform.enabled &&
            !material_texture_route_supported_by_primitives(
                scene,
                material_index,
                material.normal_texcoord,
                material.normal_transform)) ||
           (is_valid_scene_index(material.occlusion_texture,
                                 scene.textures.size()) &&
            material.occlusion_transform.enabled &&
            !material_texture_route_supported_by_primitives(
                scene,
                material_index,
                material.occlusion_texcoord,
                material.occlusion_transform)) ||
           (is_valid_scene_index(material.emissive_texture,
                                 scene.textures.size()) &&
            material.emissive_transform.enabled &&
            !material_texture_route_supported_by_primitives(
                scene,
                material_index,
                material.emissive_texcoord,
                material.emissive_transform));
}

bool material_has_deferred_non_base_color_texcoord1(
    const SceneData& scene,
    uint32_t material_index,
    const MaterialData& material) {
    return (is_valid_scene_index(material.metallic_roughness_texture,
                                 scene.textures.size()) &&
            !material_texture_route_supported_by_primitives(
                scene,
                material_index,
                material.metallic_roughness_texcoord,
                material.metallic_roughness_transform)) ||
           (is_valid_scene_index(material.normal_texture,
                                 scene.textures.size()) &&
            !material_texture_route_supported_by_primitives(
                scene,
                material_index,
                material.normal_texcoord,
                material.normal_transform)) ||
           (is_valid_scene_index(material.occlusion_texture,
                                 scene.textures.size()) &&
            !material_texture_route_supported_by_primitives(
                scene,
                material_index,
                material.occlusion_texcoord,
                material.occlusion_transform)) ||
           (is_valid_scene_index(material.emissive_texture,
                                 scene.textures.size()) &&
            !material_texture_route_supported_by_primitives(
                scene,
                material_index,
                material.emissive_texcoord,
                material.emissive_transform));
}

bool material_has_deferred_normal_scale(const SceneData& scene,
                                        uint32_t material_index,
                                        const MaterialData& material) {
    return is_valid_scene_index(material.normal_texture, scene.textures.size()) &&
           material.normal_scale != 1.0f &&
           !material_normal_texture_basis_supported_by_primitives(
               scene,
               material_index,
               material.normal_texcoord,
               material.normal_transform);
}

bool material_has_deferred_occlusion_strength(const SceneData& scene,
                                              uint32_t material_index,
                                              const MaterialData& material) {
    return is_valid_scene_index(material.occlusion_texture,
                                scene.textures.size()) &&
           material.occlusion_strength != 1.0f &&
           !material_texture_route_supported_by_primitives(
               scene,
               material_index,
               material.occlusion_texcoord,
               material.occlusion_transform);
}

bool material_is_used_by_primitive_without_normal_texture_basis(
    const SceneData& scene,
    uint32_t material_index,
    uint32_t texture_texcoord,
    const TextureTransformData& texture_transform) {
    for (const auto& mesh : scene.meshes) {
        for (const auto& primitive : mesh.primitives) {
            if (primitive.material == material_index &&
                !primitive_has_normal_texture_basis(primitive,
                                                    texture_texcoord,
                                                    texture_transform)) {
                return true;
            }
        }
    }
    return false;
}

std::string node_path_for_node(const SceneData& scene, uint32_t node_index) {
    if (!is_valid_scene_index(node_index, scene.nodes.size())) {
        return {};
    }
    const auto& node = scene.nodes[node_index];
    if (!node.name.empty()) {
        return node.name;
    }
    return "node[" + std::to_string(node_index) + "]";
}

std::string light_path_for_light(const SceneData& scene, uint32_t light_index) {
    if (!is_valid_scene_index(light_index, scene.lights.size())) {
        return {};
    }
    const auto& light = scene.lights[light_index];
    if (!light.name.empty()) {
        return light.name;
    }
    return "light[" + std::to_string(light_index) + "]";
}

std::string camera_path_for_camera(const SceneData& scene, uint32_t camera_index) {
    if (!is_valid_scene_index(camera_index, scene.cameras.size())) {
        return {};
    }
    const auto& camera = scene.cameras[camera_index];
    if (!camera.name.empty()) {
        return camera.name;
    }
    return "camera[" + std::to_string(camera_index) + "]";
}

bool node_has_deferred_camera_transform(const NodeData& node) {
    constexpr float epsilon = 0.0001f;
    if (std::abs(node.scale[0] - 1.0f) > epsilon ||
        std::abs(node.scale[1] - 1.0f) > epsilon ||
        std::abs(node.scale[2] - 1.0f) > epsilon) {
        return true;
    }
    if (!node.has_matrix_transform) {
        return false;
    }
    if (std::abs(node.matrix[3]) > epsilon ||
        std::abs(node.matrix[7]) > epsilon ||
        std::abs(node.matrix[11]) > epsilon ||
        std::abs(node.matrix[15] - 1.0f) > epsilon) {
        return true;
    }

    auto length3 = [](float x, float y, float z) {
        return std::sqrt(x * x + y * y + z * z);
    };
    auto dot3 = [](float ax, float ay, float az, float bx, float by, float bz) {
        return ax * bx + ay * by + az * bz;
    };
    const float right_length = length3(node.matrix[0], node.matrix[1], node.matrix[2]);
    const float up_length = length3(node.matrix[4], node.matrix[5], node.matrix[6]);
    const float depth_length = length3(node.matrix[8], node.matrix[9], node.matrix[10]);
    if (std::abs(right_length - 1.0f) > epsilon ||
        std::abs(up_length - 1.0f) > epsilon ||
        std::abs(depth_length - 1.0f) > epsilon) {
        return true;
    }
    return std::abs(dot3(node.matrix[0],
                         node.matrix[1],
                         node.matrix[2],
                         node.matrix[4],
                         node.matrix[5],
                         node.matrix[6])) > epsilon ||
           std::abs(dot3(node.matrix[0],
                         node.matrix[1],
                         node.matrix[2],
                         node.matrix[8],
                         node.matrix[9],
                         node.matrix[10])) > epsilon ||
           std::abs(dot3(node.matrix[4],
                         node.matrix[5],
                         node.matrix[6],
                         node.matrix[8],
                         node.matrix[9],
                         node.matrix[10])) > epsilon;
}

bool node_has_deferred_light_transform(const NodeData& node) {
    return node_has_deferred_camera_transform(node);
}

void append_unique_unsupported(SidecarData& sidecar,
                               std::set<std::tuple<std::string, std::string>>& seen,
                               UnsupportedFeature feature) {
    auto key = std::make_tuple(feature.feature, feature.node_path);
    if (seen.insert(key).second) {
        sidecar.unsupported_features.push_back(std::move(feature));
    }
}

void append_texture_diagnostics(const SceneData& scene,
                                SidecarData& sidecar,
                                std::set<std::tuple<std::string, std::string>>& seen) {
    for (size_t texture_index = 0; texture_index < scene.textures.size(); ++texture_index) {
        const auto& texture = scene.textures[texture_index];
        const std::string texture_path = texture.name.empty()
            ? "texture[" + std::to_string(texture_index) + "]"
            : texture.name;
        if (texture.encoded_bytes.empty()) {
            append_unique_unsupported(sidecar,
                                      seen,
                                      UnsupportedFeature{
                                          "TexturePayload",
                                          "Texture has no encoded bytes available for native upload.",
                                          texture_path,
                                      });
        }
        if (!native_renderer_texture_format_supported(texture.mime_type)) {
            append_unique_unsupported(sidecar,
                                      seen,
                                      UnsupportedFeature{
                                          "TextureFormat:" + texture.mime_type,
                                          "Native renderer slice currently accepts PNG/JPEG texture payloads only.",
                                          texture_path,
                                      });
        }
    }
}

void append_animation_diagnostics(const SceneData& scene,
                                  SidecarData& sidecar,
                                  std::set<std::tuple<std::string, std::string>>& seen) {
    for (size_t animation_index = 0; animation_index < scene.animations.size(); ++animation_index) {
        const auto& animation = scene.animations[animation_index];
        const std::string animation_path = animation.name.empty()
            ? "animation[" + std::to_string(animation_index) + "]"
            : animation.name;
        append_unique_unsupported(sidecar,
                                  seen,
                                  UnsupportedFeature{
                                      "TransformAnimation",
                                      "Animation data is parsed, but native render-time playback is not implemented in this slice.",
                                      animation_path,
                                  });
    }
}

void append_material_diagnostics(const SceneData& scene,
                                 SidecarData& sidecar,
                                 std::set<std::tuple<std::string, std::string>>& seen) {
    for (size_t material_index = 0; material_index < scene.materials.size(); ++material_index) {
        const auto& material = scene.materials[material_index];
        const std::string material_path = material.name.empty()
            ? "material[" + std::to_string(material_index) + "]"
            : material.name;
        if (is_valid_scene_index(material.normal_texture,
                                 scene.textures.size()) &&
            material_is_used_by_primitive_without_normal_texture_basis(
                scene,
                static_cast<uint32_t>(material_index),
                material.normal_texcoord,
                material.normal_transform)) {
            append_unique_unsupported(sidecar,
                                      seen,
                                      UnsupportedFeature{
                                          "MaterialTexture:normalTangents",
                                          "Normal texture is parsed, but at least one primitive using this material lacks authored or derivable tangent data required by the current native renderer path.",
                                          material_path,
                                      });
        }
        if (material_has_deferred_non_base_color_texture_transform(scene,
                                                                   static_cast<uint32_t>(material_index),
                                                                   material)) {
            append_unique_unsupported(sidecar,
                                      seen,
                                      UnsupportedFeature{
                                          "MaterialTextureTransform:nonBaseColor",
                                          "Texture transforms for non-base-color material slots are parsed and keyed, but at least one slot requests a texture coordinate route outside the current native renderer floor.",
                                          material_path,
                                      });
        }
        if (material_has_deferred_non_base_color_texcoord1(
                scene,
                static_cast<uint32_t>(material_index),
                material)) {
            append_unique_unsupported(sidecar,
                                      seen,
                                      UnsupportedFeature{
                                          "MaterialTexcoord:nonBaseColor",
                                          "Non-base-color texture coordinate routing is parsed and keyed, but at least one slot requests a texture coordinate set outside the current native renderer floor.",
                                          material_path,
                                      });
        }
        if (material_has_deferred_normal_scale(
                scene,
                static_cast<uint32_t>(material_index),
                material)) {
            append_unique_unsupported(sidecar,
                                      seen,
                                      UnsupportedFeature{
                                          "MaterialTexture:normalScale",
                                          "Normal texture scale is parsed and keyed, but it remains deferred when the normal texture cannot be applied on the current native renderer route.",
                                          material_path,
                                      });
        }
        if (material_has_deferred_occlusion_strength(
                scene,
                static_cast<uint32_t>(material_index),
                material)) {
            append_unique_unsupported(sidecar,
                                      seen,
                                      UnsupportedFeature{
                                          "MaterialTexture:occlusionStrength",
                                          "Occlusion texture strength is parsed and keyed, but it remains deferred when the occlusion texture cannot be applied on the current native renderer route.",
                                          material_path,
                                      });
        }
        for (const auto& extension : material.advanced_material_extensions) {
            append_unique_unsupported(sidecar,
                                      seen,
                                      UnsupportedFeature{
                                          "MaterialExtension:" + extension,
                                          "Advanced glTF material extension is parsed for diagnostics, but this native renderer slice currently renders only the metallic-roughness floor.",
                                          material_path,
                                      });
        }
    }
}

void append_light_diagnostics(const SceneData& scene,
                              SidecarData& sidecar,
                              std::set<std::tuple<std::string, std::string>>& seen) {
    std::vector<bool> referenced(scene.lights.size(), false);
    std::vector<std::pair<uint32_t, uint32_t>> directional_references;
    std::vector<std::pair<uint32_t, uint32_t>> point_references;
    std::vector<std::pair<uint32_t, uint32_t>> spot_references;
    for (uint32_t node_index = 0;
         node_index < static_cast<uint32_t>(scene.nodes.size());
         ++node_index) {
        const auto& node = scene.nodes[node_index];
        if (is_valid_scene_index(node.light, scene.lights.size())) {
            referenced[node.light] = true;
            const auto& light = scene.lights[node.light];
            if (node_has_deferred_light_transform(node)) {
                append_unique_unsupported(
                    sidecar,
                    seen,
                    UnsupportedFeature{
                        "PunctualLight:nodeTransform",
                        "Light node transform is preserved, but the current native renderer consumes only translation, rotation, and rigid matrix light bases.",
                        node_path_for_node(scene, node_index),
                    });
            }
            if (light.type == LightData::Type::directional) {
                directional_references.push_back({node.light, node_index});
            } else if (light.type == LightData::Type::point) {
                point_references.push_back({node.light, node_index});
            } else if (light.type == LightData::Type::spot) {
                spot_references.push_back({node.light, node_index});
            }
        }
    }

    auto append_additional_references =
        [&](const std::vector<std::pair<uint32_t, uint32_t>>& references,
            const char* feature,
            const char* reason) {
            for (size_t i = 1; i < references.size(); ++i) {
                const auto light_index = references[i].first;
                const auto node_index = references[i].second;
                const bool duplicated_light_record =
                    references[i].first == references[0].first;
                const std::string path = duplicated_light_record
                    ? node_path_for_node(scene, node_index)
                    : light_path_for_light(scene, light_index);
                append_unique_unsupported(
                    sidecar,
                    seen,
                    UnsupportedFeature{
                        feature,
                        reason,
                        path,
                    });
            }
        };

    append_additional_references(
        directional_references,
        "PunctualLight:additionalDirectional",
        "Additional node-attached directional light occurrence is parsed, but the current native renderer slice consumes only the first directional light occurrence.");
    append_additional_references(
        point_references,
        "PunctualLight:additionalPoint",
        "Additional node-attached point light occurrence is parsed, but the current native renderer slice consumes only the first node-attached point light occurrence.");
    append_additional_references(
        spot_references,
        "PunctualLight:additionalSpot",
        "Additional node-attached spot light occurrence is parsed, but the current native renderer slice consumes only the first node-attached spot light occurrence.");

    bool directional_fallback_consumed = directional_references.empty();
    for (uint32_t light_index = 0;
         light_index < static_cast<uint32_t>(scene.lights.size());
         ++light_index) {
        const auto& light = scene.lights[light_index];
        const bool is_directional = light.type == LightData::Type::directional;
        const bool is_point = light.type == LightData::Type::point;
        const bool is_spot = light.type == LightData::Type::spot;
        if (!is_directional && !is_point && !is_spot) {
            continue;
        }

        const std::string light_path = light_path_for_light(scene, light_index);
        if (is_directional && !referenced[light_index]) {
            if (directional_fallback_consumed) {
                directional_fallback_consumed = false;
                continue;
            }
            const bool node_directional_consumed =
                !directional_references.empty();
            append_unique_unsupported(
                sidecar,
                seen,
                UnsupportedFeature{
                    node_directional_consumed
                        ? "PunctualLight:unattachedDirectional"
                        : "PunctualLight:additionalDirectional",
                    node_directional_consumed
                        ? "Directional light is parsed, but the current native renderer slice already consumes a node-attached directional light occurrence."
                        : "Additional unattached directional light is parsed, but the current native renderer slice consumes only the first directional light occurrence.",
                    light_path,
                });
        } else if (!referenced[light_index]) {
            append_unique_unsupported(
                sidecar,
                seen,
                UnsupportedFeature{
                    is_point ? "PunctualLight:unattachedPoint"
                             : "PunctualLight:unattachedSpot",
                    is_point
                        ? "Point light is parsed, but the current native renderer slice only consumes node-attached point lights."
                        : "Spot light is parsed, but the current native renderer slice only consumes node-attached spot lights.",
                    light_path,
                });
        }
    }
}

void append_camera_diagnostics(const SceneData& scene,
                               SidecarData& sidecar,
                               std::set<std::tuple<std::string, std::string>>& seen) {
    std::vector<bool> referenced(scene.cameras.size(), false);
    std::vector<std::pair<uint32_t, uint32_t>> camera_references;
    for (uint32_t node_index = 0;
         node_index < static_cast<uint32_t>(scene.nodes.size());
         ++node_index) {
        const auto& node = scene.nodes[node_index];
        if (is_valid_scene_index(node.camera, scene.cameras.size())) {
            referenced[node.camera] = true;
            camera_references.push_back({node.camera, node_index});
            if (node_has_deferred_camera_transform(node)) {
                append_unique_unsupported(
                    sidecar,
                    seen,
                    UnsupportedFeature{
                        "Camera:nodeTransform",
                        "Camera node transform is preserved, but the current native renderer consumes only translation, rotation, and rigid matrix camera bases.",
                        node_path_for_node(scene, node_index),
                    });
            }
        }
    }

    auto append_additional_camera = [&](uint32_t camera_index,
                                        const std::string& path) {
        if (!is_valid_scene_index(camera_index, scene.cameras.size())) {
            return;
        }
        const auto& camera = scene.cameras[camera_index];
        append_unique_unsupported(
            sidecar,
            seen,
            UnsupportedFeature{
                camera.projection == CameraData::Projection::orthographic
                    ? "Camera:additionalOrthographic"
                    : "Camera:additionalPerspective",
                "Additional camera occurrence is preserved in SceneData, but the current native renderer proof consumes only the first camera occurrence.",
                path,
            });
    };

    for (size_t i = 1; i < camera_references.size(); ++i) {
        const auto camera_index = camera_references[i].first;
        const auto node_index = camera_references[i].second;
        const bool duplicated_camera_record =
            camera_references[i].first == camera_references[0].first;
        append_additional_camera(
            camera_index,
            duplicated_camera_record
                ? node_path_for_node(scene, node_index)
                : camera_path_for_camera(scene, camera_index));
    }

    bool consumed_camera_seen = false;
    for (uint32_t camera_index = 0;
         camera_index < static_cast<uint32_t>(scene.cameras.size());
         ++camera_index) {
        const std::string camera_path = camera_path_for_camera(scene, camera_index);
        if (!consumed_camera_seen) {
            consumed_camera_seen = true;
            continue;
        }

        if (!referenced[camera_index]) {
            append_additional_camera(camera_index, camera_path);
        }
    }
}

void append_scene_unsupported_features(const SceneData& scene,
                                       SidecarData& sidecar,
                                       std::set<std::tuple<std::string, std::string>>& seen) {
    for (const auto& feature : scene.unsupported_features) {
        append_unique_unsupported(sidecar,
                                  seen,
                                  UnsupportedFeature{
                                      feature.feature,
                                      feature.reason,
                                      feature.node_path,
                                  });
    }
}

void append_runtime_hints_from_scene(const SceneData& scene, SidecarData& sidecar) {
    for (size_t node_index = 0; node_index < scene.nodes.size(); ++node_index) {
        const auto& node = scene.nodes[node_index];
        if (is_valid_scene_index(node.camera, scene.cameras.size())) {
            sidecar.runtime_hints.push_back(RuntimeHint{
                "preferredCamera",
                node_path_for_node(scene, static_cast<uint32_t>(node_index)),
            });
            break;
        }
    }
    if (!scene.lights.empty()) {
        sidecar.runtime_hints.push_back(RuntimeHint{
            "lightCount",
            std::to_string(scene.lights.size()),
        });
    }
    if (!scene.animations.empty()) {
        sidecar.runtime_hints.push_back(RuntimeHint{
            "animationCount",
            std::to_string(scene.animations.size()),
        });
    }
}

} // namespace

SidecarData build_sidecar_from_scene(const SceneData& scene,
                                     const SidecarBuildOptions& options) {
    SidecarData sidecar;
    sidecar.provenance = options.provenance;
    sidecar.diagnostics = scene.diagnostics;

    if (options.include_validation_diagnostics) {
        auto validation = validate_scene_data(scene, options.source_path);
        sidecar.diagnostics.insert(sidecar.diagnostics.end(),
                                   std::make_move_iterator(validation.begin()),
                                   std::make_move_iterator(validation.end()));
    }

    std::set<std::tuple<std::string, std::string>> unsupported_seen;
    append_texture_diagnostics(scene, sidecar, unsupported_seen);
    append_animation_diagnostics(scene, sidecar, unsupported_seen);
    append_material_diagnostics(scene, sidecar, unsupported_seen);
    append_light_diagnostics(scene, sidecar, unsupported_seen);
    append_camera_diagnostics(scene, sidecar, unsupported_seen);
    append_scene_unsupported_features(scene, sidecar, unsupported_seen);
    append_runtime_hints_from_scene(scene, sidecar);
    return sidecar;
}

void append_bake_unsupported_feature(SidecarData& sidecar,
                                     BakeUnsupportedFeature feature,
                                     const std::string& node_path,
                                     const std::string& source_path) {
    const auto descriptor = bake_descriptor(feature);
    sidecar.unsupported_features.push_back(UnsupportedFeature{
        descriptor.feature,
        descriptor.reason,
        node_path,
    });
    append_diagnostic(sidecar.diagnostics,
                      Diagnostic::Severity::warning,
                      descriptor.code,
                      descriptor.reason,
                      source_path);
}

std::string sidecar_to_json(const SidecarData& sidecar) {
    auto root = choc::value::createObject("");
    root.addMember("schema_version", kSidecarSchemaVersion);
    root.addMember("provenance", provenance_to_json(sidecar.provenance));
    root.addMember("diagnostics", diagnostics_to_json(sidecar.diagnostics));
    root.addMember("unsupported_features",
                   unsupported_features_to_json(sidecar.unsupported_features));
    root.addMember("runtime_hints", runtime_hints_to_json(sidecar.runtime_hints));
    return choc::json::toString(root, true);
}

SidecarParseResult sidecar_from_json(const std::string& json) {
    SidecarParseResult result;
    try {
        auto root = choc::json::parse(json);
        if (!root.isObject()) {
            result.error = "Sidecar JSON root must be an object.";
            return result;
        }
        if (!has_exact_keys(root,
                            {"schema_version",
                             "provenance",
                             "diagnostics",
                             "unsupported_features",
                             "runtime_hints"})) {
            result.error = "Sidecar JSON contains an invalid field shape.";
            return result;
        }
        if (!root.hasObjectMember("schema_version") ||
            !root["schema_version"].isInt() ||
            root["schema_version"].getInt64() != kSidecarSchemaVersion) {
            result.error = "Unsupported or missing sidecar schema_version.";
            return result;
        }
        if (!parse_provenance(root, result.sidecar) ||
            !parse_diagnostics(root, result.sidecar) ||
            !parse_unsupported_features(root, result.sidecar) ||
            !parse_runtime_hints(root, result.sidecar)) {
            result.error = "Sidecar JSON contains an invalid field shape.";
            return result;
        }
        result.success = true;
        return result;
    } catch (const std::exception& e) {
        result.error = e.what();
        return result;
    } catch (...) {
        result.error = "Unknown sidecar JSON parse error.";
        return result;
    }
}

} // namespace pulp::scene

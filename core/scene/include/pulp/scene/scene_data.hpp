#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace pulp::scene {

inline constexpr uint32_t invalid_scene_index = std::numeric_limits<uint32_t>::max();

struct Diagnostic {
    enum class Severity {
        info,
        warning,
        error
    };

    Severity severity = Severity::info;
    std::string code;
    std::string message;
    std::string source_path;
};

struct UnsupportedFeatureData {
    std::string feature;
    std::string reason;
    std::string node_path;
};

struct TextureData {
    std::string name;
    std::string mime_type;
    std::vector<uint8_t> encoded_bytes;
};

struct TextureSamplerData {
    enum class Filter {
        unspecified,
        nearest,
        linear,
        nearest_mipmap_nearest,
        linear_mipmap_nearest,
        nearest_mipmap_linear,
        linear_mipmap_linear
    };

    enum class Wrap {
        clamp_to_edge,
        mirrored_repeat,
        repeat
    };

    std::string name;
    Filter mag_filter = Filter::unspecified;
    Filter min_filter = Filter::unspecified;
    Wrap wrap_s = Wrap::repeat;
    Wrap wrap_t = Wrap::repeat;
};

struct TextureTransformData {
    bool enabled = false;
    float offset[2] = {0.0f, 0.0f};
    float scale[2] = {1.0f, 1.0f};
    float rotation = 0.0f;
    uint32_t texcoord_override = invalid_scene_index;
};

struct MaterialData {
    enum class AlphaMode {
        opaque,
        mask,
        blend
    };

    std::string name;
    uint32_t base_color_texture = invalid_scene_index;
    uint32_t metallic_roughness_texture = invalid_scene_index;
    uint32_t normal_texture = invalid_scene_index;
    uint32_t occlusion_texture = invalid_scene_index;
    uint32_t emissive_texture = invalid_scene_index;
    uint32_t base_color_sampler = invalid_scene_index;
    uint32_t metallic_roughness_sampler = invalid_scene_index;
    uint32_t normal_sampler = invalid_scene_index;
    uint32_t occlusion_sampler = invalid_scene_index;
    uint32_t emissive_sampler = invalid_scene_index;
    uint32_t base_color_texcoord = 0;
    uint32_t metallic_roughness_texcoord = 0;
    uint32_t normal_texcoord = 0;
    uint32_t occlusion_texcoord = 0;
    uint32_t emissive_texcoord = 0;
    TextureTransformData base_color_transform;
    TextureTransformData metallic_roughness_transform;
    TextureTransformData normal_transform;
    TextureTransformData occlusion_transform;
    TextureTransformData emissive_transform;
    std::vector<std::string> advanced_material_extensions;
    float base_color_factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float metallic_factor = 1.0f;
    float roughness_factor = 1.0f;
    float normal_scale = 1.0f;
    float occlusion_strength = 1.0f;
    float emissive_factor[3] = {0.0f, 0.0f, 0.0f};
    float emissive_strength = 1.0f;
    float alpha_cutoff = 0.5f;
    AlphaMode alpha_mode = AlphaMode::opaque;
    bool double_sided = false;
    bool unlit = false;
};

struct CameraData {
    enum class Projection {
        perspective,
        orthographic
    };

    std::string name;
    Projection projection = Projection::perspective;
    float aspect_ratio = 0.0f;
    float yfov = 0.0f;
    float znear = 0.1f;
    float zfar = 0.0f;
    float xmag = 0.0f;
    float ymag = 0.0f;
};

struct LightData {
    enum class Type {
        directional,
        point,
        spot
    };

    std::string name;
    Type type = Type::directional;
    float color[3] = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 0.0f;
    float inner_cone_angle = 0.0f;
    float outer_cone_angle = 0.7853982f;
};

struct AnimationSamplerData {
    enum class Interpolation {
        linear,
        step,
        cubic_spline
    };

    std::vector<float> input_times;
    std::vector<float> output_values;
    uint32_t output_components = 0;
    Interpolation interpolation = Interpolation::linear;
};

struct AnimationChannelData {
    enum class Path {
        translation,
        rotation,
        scale
    };

    uint32_t node = invalid_scene_index;
    uint32_t sampler = invalid_scene_index;
    Path path = Path::translation;
};

struct AnimationData {
    std::string name;
    std::vector<AnimationSamplerData> samplers;
    std::vector<AnimationChannelData> channels;
};

struct PrimitiveData {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> tangents;
    std::vector<float> texcoord0;
    std::vector<float> texcoord1;
    std::vector<float> color0;
    std::vector<uint32_t> indices;
    uint32_t material = invalid_scene_index;
};

struct MeshData {
    std::string name;
    std::vector<PrimitiveData> primitives;
};

struct NodeData {
    std::string name;
    uint32_t mesh = invalid_scene_index;
    uint32_t camera = invalid_scene_index;
    uint32_t light = invalid_scene_index;
    std::vector<uint32_t> children;
    bool has_matrix_transform = false;
    float matrix[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    float translation[3] = {0.0f, 0.0f, 0.0f};
    float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float scale[3] = {1.0f, 1.0f, 1.0f};
};

struct SceneData {
    std::vector<NodeData> nodes;
    std::vector<uint32_t> root_nodes;
    std::vector<MeshData> meshes;
    std::vector<MaterialData> materials;
    std::vector<TextureData> textures;
    std::vector<TextureSamplerData> texture_samplers;
    std::vector<CameraData> cameras;
    std::vector<LightData> lights;
    std::vector<AnimationData> animations;
    std::vector<UnsupportedFeatureData> unsupported_features;
    std::vector<Diagnostic> diagnostics;

    bool empty() const {
        return nodes.empty() && meshes.empty();
    }
};

inline bool is_valid_scene_index(uint32_t index, size_t count) {
    return index != invalid_scene_index && static_cast<size_t>(index) < count;
}

inline const char* diagnostic_severity_name(Diagnostic::Severity severity) {
    switch (severity) {
        case Diagnostic::Severity::info: return "info";
        case Diagnostic::Severity::warning: return "warning";
        case Diagnostic::Severity::error: return "error";
    }
    return "error";
}

inline void append_diagnostic(std::vector<Diagnostic>& diagnostics,
                              Diagnostic::Severity severity,
                              std::string code,
                              std::string message,
                              std::string source_path = {}) {
    diagnostics.push_back(Diagnostic{
        severity,
        std::move(code),
        std::move(message),
        std::move(source_path),
    });
}

inline bool has_error_diagnostics(const std::vector<Diagnostic>& diagnostics) {
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.severity == Diagnostic::Severity::error) {
            return true;
        }
    }
    return false;
}

inline std::vector<Diagnostic> validate_scene_data(const SceneData& scene,
                                                   std::string source_path = {}) {
    std::vector<Diagnostic> diagnostics;

    for (uint32_t root : scene.root_nodes) {
        if (!is_valid_scene_index(root, scene.nodes.size())) {
            append_diagnostic(diagnostics,
                              Diagnostic::Severity::error,
                              "scene.root_node_out_of_range",
                              "Scene root node index is outside the node table.",
                              source_path);
        }
    }

    for (size_t node_index = 0; node_index < scene.nodes.size(); ++node_index) {
        const auto& node = scene.nodes[node_index];
        if (node.mesh != invalid_scene_index &&
            !is_valid_scene_index(node.mesh, scene.meshes.size())) {
            append_diagnostic(diagnostics,
                              Diagnostic::Severity::error,
                              "scene.node_mesh_out_of_range",
                              "Node references a mesh index outside the mesh table.",
                              source_path);
        }
        if (node.camera != invalid_scene_index &&
            !is_valid_scene_index(node.camera, scene.cameras.size())) {
            append_diagnostic(diagnostics,
                              Diagnostic::Severity::error,
                              "scene.node_camera_out_of_range",
                              "Node references a camera index outside the camera table.",
                              source_path);
        }
        if (node.light != invalid_scene_index &&
            !is_valid_scene_index(node.light, scene.lights.size())) {
            append_diagnostic(diagnostics,
                              Diagnostic::Severity::error,
                              "scene.node_light_out_of_range",
                              "Node references a light index outside the light table.",
                              source_path);
        }
        for (uint32_t child : node.children) {
            if (!is_valid_scene_index(child, scene.nodes.size())) {
                append_diagnostic(diagnostics,
                                  Diagnostic::Severity::error,
                                  "scene.node_child_out_of_range",
                                  "Node references a child index outside the node table.",
                                  source_path);
            }
        }
    }

    for (const auto& material : scene.materials) {
        if (material.base_color_texture != invalid_scene_index &&
            !is_valid_scene_index(material.base_color_texture, scene.textures.size())) {
            append_diagnostic(diagnostics,
                              Diagnostic::Severity::error,
                              "scene.material_texture_out_of_range",
                              "Material references a base-color texture outside the texture table.",
                              source_path);
        }
        if (material.metallic_roughness_texture != invalid_scene_index &&
            !is_valid_scene_index(material.metallic_roughness_texture,
                                  scene.textures.size())) {
            append_diagnostic(diagnostics,
                              Diagnostic::Severity::error,
                              "scene.material_texture_out_of_range",
                              "Material references a metallic-roughness texture outside the texture table.",
                              source_path);
        }
        if (material.normal_texture != invalid_scene_index &&
            !is_valid_scene_index(material.normal_texture, scene.textures.size())) {
            append_diagnostic(diagnostics,
                              Diagnostic::Severity::error,
                              "scene.material_texture_out_of_range",
                              "Material references a normal texture outside the texture table.",
                              source_path);
        }
        if (material.occlusion_texture != invalid_scene_index &&
            !is_valid_scene_index(material.occlusion_texture, scene.textures.size())) {
            append_diagnostic(diagnostics,
                              Diagnostic::Severity::error,
                              "scene.material_texture_out_of_range",
                              "Material references an occlusion texture outside the texture table.",
                              source_path);
        }
        if (material.emissive_texture != invalid_scene_index &&
            !is_valid_scene_index(material.emissive_texture, scene.textures.size())) {
            append_diagnostic(diagnostics,
                              Diagnostic::Severity::error,
                              "scene.material_texture_out_of_range",
                              "Material references an emissive texture outside the texture table.",
                              source_path);
        }
        if (material.base_color_sampler != invalid_scene_index &&
            !is_valid_scene_index(material.base_color_sampler, scene.texture_samplers.size())) {
            append_diagnostic(diagnostics,
                              Diagnostic::Severity::error,
                              "scene.material_sampler_out_of_range",
                              "Material references a base-color texture sampler outside the sampler table.",
                              source_path);
        }
        if (material.metallic_roughness_sampler != invalid_scene_index &&
            !is_valid_scene_index(material.metallic_roughness_sampler,
                                  scene.texture_samplers.size())) {
            append_diagnostic(diagnostics,
                              Diagnostic::Severity::error,
                              "scene.material_sampler_out_of_range",
                              "Material references a metallic-roughness texture sampler outside the sampler table.",
                              source_path);
        }
        if (material.normal_sampler != invalid_scene_index &&
            !is_valid_scene_index(material.normal_sampler, scene.texture_samplers.size())) {
            append_diagnostic(diagnostics,
                              Diagnostic::Severity::error,
                              "scene.material_sampler_out_of_range",
                              "Material references a normal texture sampler outside the sampler table.",
                              source_path);
        }
        if (material.occlusion_sampler != invalid_scene_index &&
            !is_valid_scene_index(material.occlusion_sampler, scene.texture_samplers.size())) {
            append_diagnostic(diagnostics,
                              Diagnostic::Severity::error,
                              "scene.material_sampler_out_of_range",
                              "Material references an occlusion texture sampler outside the sampler table.",
                              source_path);
        }
        if (material.emissive_sampler != invalid_scene_index &&
            !is_valid_scene_index(material.emissive_sampler, scene.texture_samplers.size())) {
            append_diagnostic(diagnostics,
                              Diagnostic::Severity::error,
                              "scene.material_sampler_out_of_range",
                              "Material references an emissive texture sampler outside the sampler table.",
                              source_path);
        }
    }

    for (const auto& animation : scene.animations) {
        for (const auto& channel : animation.channels) {
            if (!is_valid_scene_index(channel.node, scene.nodes.size())) {
                append_diagnostic(diagnostics,
                                  Diagnostic::Severity::error,
                                  "scene.animation_node_out_of_range",
                                  "Animation channel references a node outside the node table.",
                                  source_path);
            }
            if (!is_valid_scene_index(channel.sampler, animation.samplers.size())) {
                append_diagnostic(diagnostics,
                                  Diagnostic::Severity::error,
                                  "scene.animation_sampler_out_of_range",
                                  "Animation channel references a sampler outside the animation sampler table.",
                                  source_path);
            }
        }
        for (const auto& sampler : animation.samplers) {
            if (sampler.input_times.empty()) {
                append_diagnostic(diagnostics,
                                  Diagnostic::Severity::error,
                                  "scene.animation_sampler_missing_input",
                                  "Animation sampler must contain input keyframe times.",
                                  source_path);
            }
            if (sampler.output_components == 0u || sampler.output_values.empty() ||
                sampler.output_values.size() % sampler.output_components != 0u) {
                append_diagnostic(diagnostics,
                                  Diagnostic::Severity::error,
                                  "scene.animation_sampler_invalid_output",
                                  "Animation sampler output values must be grouped by component count.",
                                  source_path);
                continue;
            }

            const size_t output_key_count =
                sampler.output_values.size() / sampler.output_components;
            const size_t expected_key_count =
                sampler.interpolation == AnimationSamplerData::Interpolation::cubic_spline
                    ? sampler.input_times.size() * 3u
                    : sampler.input_times.size();
            if (output_key_count != expected_key_count) {
                append_diagnostic(diagnostics,
                                  Diagnostic::Severity::error,
                                  "scene.animation_sampler_count_mismatch",
                                  "Animation sampler input and output keyframe counts do not match.",
                                  source_path);
            }
        }
    }

    for (const auto& mesh : scene.meshes) {
        if (mesh.primitives.empty()) {
            append_diagnostic(diagnostics,
                              Diagnostic::Severity::warning,
                              "scene.mesh_without_primitives",
                              "Mesh has no primitives.",
                              source_path);
        }

        for (const auto& primitive : mesh.primitives) {
            if (primitive.positions.empty() || primitive.positions.size() % 3u != 0u) {
                append_diagnostic(diagnostics,
                                  Diagnostic::Severity::error,
                                  "scene.primitive_invalid_positions",
                                  "Primitive positions must contain at least one XYZ vertex.",
                                  source_path);
                continue;
            }

            const size_t vertex_count = primitive.positions.size() / 3u;
            if (!primitive.normals.empty() && primitive.normals.size() != vertex_count * 3u) {
                append_diagnostic(diagnostics,
                                  Diagnostic::Severity::error,
                                  "scene.primitive_normal_count_mismatch",
                                  "Primitive normal count must match position vertex count.",
                                  source_path);
            }
            if (!primitive.tangents.empty() && primitive.tangents.size() != vertex_count * 4u) {
                append_diagnostic(diagnostics,
                                  Diagnostic::Severity::error,
                                  "scene.primitive_tangent_count_mismatch",
                                  "Primitive tangent count must match position vertex count with XYZW tangent data.",
                                  source_path);
            }
            if (!primitive.texcoord0.empty() && primitive.texcoord0.size() != vertex_count * 2u) {
                append_diagnostic(diagnostics,
                                  Diagnostic::Severity::error,
                                  "scene.primitive_texcoord_count_mismatch",
                                  "Primitive TEXCOORD_0 count must match position vertex count.",
                                  source_path);
            }
            if (!primitive.texcoord1.empty() && primitive.texcoord1.size() != vertex_count * 2u) {
                append_diagnostic(diagnostics,
                                  Diagnostic::Severity::error,
                                  "scene.primitive_texcoord_count_mismatch",
                                  "Primitive TEXCOORD_1 count must match position vertex count.",
                                  source_path);
            }
            if (!primitive.color0.empty() && primitive.color0.size() != vertex_count * 4u) {
                append_diagnostic(diagnostics,
                                  Diagnostic::Severity::error,
                                  "scene.primitive_color_count_mismatch",
                                  "Primitive COLOR_0 count must match position vertex count with RGBA data.",
                                  source_path);
            }
            if (primitive.indices.empty()) {
                append_diagnostic(diagnostics,
                                  Diagnostic::Severity::error,
                                  "scene.primitive_missing_indices",
                                  "Primitive must contain indexed triangle data for the first native renderer slice.",
                                  source_path);
            }
            for (uint32_t index : primitive.indices) {
                if (static_cast<size_t>(index) >= vertex_count) {
                    append_diagnostic(diagnostics,
                                      Diagnostic::Severity::error,
                                      "scene.primitive_index_out_of_range",
                                      "Primitive index is outside the vertex table.",
                                      source_path);
                    break;
                }
            }
            if (primitive.material != invalid_scene_index &&
                !is_valid_scene_index(primitive.material, scene.materials.size())) {
                append_diagnostic(diagnostics,
                                  Diagnostic::Severity::error,
                                  "scene.primitive_material_out_of_range",
                                  "Primitive references a material outside the material table.",
                                  source_path);
            }
        }
    }

    return diagnostics;
}

} // namespace pulp::scene

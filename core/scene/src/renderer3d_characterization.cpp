#include <pulp/scene/renderer3d_characterization.hpp>

#include <pulp/scene/render_packet.hpp>

#include <cmath>
#include <cstddef>
#include <unordered_set>

namespace pulp::scene {
namespace {

bool scene_filter_requires_mipmaps(TextureSamplerData::Filter filter) {
    switch (filter) {
        case TextureSamplerData::Filter::nearest_mipmap_nearest:
        case TextureSamplerData::Filter::linear_mipmap_nearest:
        case TextureSamplerData::Filter::nearest_mipmap_linear:
        case TextureSamplerData::Filter::linear_mipmap_linear:
            return true;
        case TextureSamplerData::Filter::unspecified:
        case TextureSamplerData::Filter::nearest:
        case TextureSamplerData::Filter::linear:
            return false;
    }
    return false;
}

bool scene_filter_is_linear(TextureSamplerData::Filter filter) {
    switch (filter) {
        case TextureSamplerData::Filter::linear:
        case TextureSamplerData::Filter::linear_mipmap_nearest:
        case TextureSamplerData::Filter::linear_mipmap_linear:
            return true;
        case TextureSamplerData::Filter::unspecified:
        case TextureSamplerData::Filter::nearest:
        case TextureSamplerData::Filter::nearest_mipmap_nearest:
        case TextureSamplerData::Filter::nearest_mipmap_linear:
            return false;
    }
    return false;
}

void record_sampler_intent(const TextureSamplerData& sampler,
                           Renderer3DCharacterization& out) {
    out.texture_sampler_routed = true;
    out.texture_sampler_clamp_s =
        out.texture_sampler_clamp_s ||
        sampler.wrap_s == TextureSamplerData::Wrap::clamp_to_edge;
    out.texture_sampler_clamp_t =
        out.texture_sampler_clamp_t ||
        sampler.wrap_t == TextureSamplerData::Wrap::clamp_to_edge;
    out.texture_sampler_linear =
        out.texture_sampler_linear ||
        scene_filter_is_linear(sampler.mag_filter) ||
        scene_filter_is_linear(sampler.min_filter);
    out.texture_mipmap_filter_downgrade_intended =
        out.texture_mipmap_filter_downgrade_intended ||
        scene_filter_requires_mipmaps(sampler.min_filter);
}

bool has_transform_animation(const SceneData& scene) {
    for (const auto& animation : scene.animations) {
        for (const auto& channel : animation.channels) {
            switch (channel.path) {
                case AnimationChannelData::Path::translation:
                case AnimationChannelData::Path::rotation:
                case AnimationChannelData::Path::scale:
                    return true;
            }
        }
    }
    return false;
}

bool apply_initial_animation_pose(SceneData& scene) {
    bool applied = false;
    for (const auto& animation : scene.animations) {
        for (const auto& channel : animation.channels) {
            if (!is_valid_scene_index(channel.node, scene.nodes.size()) ||
                !is_valid_scene_index(channel.sampler,
                                      animation.samplers.size())) {
                continue;
            }
            const auto& sampler = animation.samplers[channel.sampler];
            const uint32_t components = sampler.output_components;
            if (components == 0u || sampler.output_values.empty()) {
                continue;
            }
            const size_t value_offset =
                sampler.interpolation ==
                        AnimationSamplerData::Interpolation::cubic_spline
                    ? components
                    : 0u;
            if (value_offset + components > sampler.output_values.size()) {
                continue;
            }

            auto& node = scene.nodes[channel.node];
            node.has_matrix_transform = false;
            const float* values = sampler.output_values.data() + value_offset;
            switch (channel.path) {
                case AnimationChannelData::Path::translation:
                    if (components < 3u) {
                        continue;
                    }
                    node.translation[0] = values[0];
                    node.translation[1] = values[1];
                    node.translation[2] = values[2];
                    applied = true;
                    break;
                case AnimationChannelData::Path::rotation:
                    if (components < 4u) {
                        continue;
                    }
                    node.rotation[0] = values[0];
                    node.rotation[1] = values[1];
                    node.rotation[2] = values[2];
                    node.rotation[3] = values[3];
                    applied = true;
                    break;
                case AnimationChannelData::Path::scale:
                    if (components < 3u) {
                        continue;
                    }
                    node.scale[0] = values[0];
                    node.scale[1] = values[1];
                    node.scale[2] = values[2];
                    applied = true;
                    break;
            }
        }
    }
    return applied;
}

uint32_t effective_texcoord(uint32_t texture_texcoord,
                            const TextureTransformData& texture_transform) {
    if (texture_transform.enabled &&
        texture_transform.texcoord_override != invalid_scene_index) {
        return texture_transform.texcoord_override;
    }
    return texture_texcoord;
}

bool has_texcoord_route(const PrimitiveData& primitive, uint32_t texcoord) {
    const size_t vertex_count = primitive.positions.size() / 3u;
    if (texcoord == 0u) {
        return primitive.texcoord0.size() >= vertex_count * 2u;
    }
    if (texcoord == 1u) {
        return primitive.texcoord1.size() >= vertex_count * 2u;
    }
    return false;
}

bool texture_uv_route_supported(const PrimitiveData& primitive,
                                uint32_t texture_texcoord,
                                const TextureTransformData& texture_transform) {
    return has_texcoord_route(
        primitive,
        effective_texcoord(texture_texcoord, texture_transform));
}

bool texture_uses_texcoord1(const PrimitiveData& primitive,
                            uint32_t texture_texcoord,
                            const TextureTransformData& texture_transform) {
    const uint32_t texcoord =
        effective_texcoord(texture_texcoord, texture_transform);
    return texcoord == 1u && has_texcoord_route(primitive, texcoord);
}

bool has_texture(const SceneData& scene, uint32_t texture) {
    return is_valid_scene_index(texture, scene.textures.size());
}

bool has_sampler(const SceneData& scene, uint32_t sampler) {
    return is_valid_scene_index(sampler, scene.texture_samplers.size());
}

void record_non_base_texture_intent(
    const SceneData& scene,
    const PrimitiveData& primitive,
    uint32_t texture,
    uint32_t sampler,
    uint32_t texcoord,
    const TextureTransformData& transform,
    Renderer3DPrimitiveIntent& primitive_out,
    Renderer3DCharacterization& out) {
    if (!has_texture(scene, texture)) {
        return;
    }

    primitive_out.non_base_color_texture_routed = true;
    out.non_base_color_texture_routed = true;
    const bool texcoord1 = texture_uses_texcoord1(primitive,
                                                  texcoord,
                                                  transform);
    primitive_out.non_base_color_texcoord1_routed =
        primitive_out.non_base_color_texcoord1_routed || texcoord1;
    out.non_base_color_texcoord1_routed =
        out.non_base_color_texcoord1_routed || texcoord1;
    if (transform.enabled) {
        primitive_out.non_base_color_texture_transform_routed = true;
        out.non_base_color_texture_transform_routed = true;
    }
    if (!texture_uv_route_supported(primitive, texcoord, transform)) {
        primitive_out.non_base_color_texcoord_deferred = true;
        out.non_base_color_texcoord_deferred = true;
    }
    if (has_sampler(scene, sampler)) {
        record_sampler_intent(scene.texture_samplers[sampler], out);
    }
}

void record_unsupported_features(const SceneData& scene,
                                 Renderer3DCharacterization& out) {
    for (const auto& feature : scene.unsupported_features) {
        if (feature.feature == "Skinning") {
            out.skinning_deferred = true;
        } else if (feature.feature == "MorphTargets" ||
                   feature.feature == "MorphWeights") {
            out.morph_target_deferred = true;
        } else if (feature.feature == "GpuInstancing") {
            out.gpu_instancing_deferred = true;
        }
    }
}

bool matrix_is_rigid_transform(const NodeData& node) {
    if (!node.has_matrix_transform) {
        return true;
    }
    constexpr float epsilon = 0.0001f;
    if (std::abs(node.matrix[3]) > epsilon ||
        std::abs(node.matrix[7]) > epsilon ||
        std::abs(node.matrix[11]) > epsilon ||
        std::abs(node.matrix[15] - 1.0f) > epsilon) {
        return false;
    }

    auto length3 = [](float x, float y, float z) {
        return std::sqrt(x * x + y * y + z * z);
    };
    auto dot3 = [](float ax, float ay, float az, float bx, float by, float bz) {
        return ax * bx + ay * by + az * bz;
    };
    const float right = length3(node.matrix[0], node.matrix[1], node.matrix[2]);
    const float up = length3(node.matrix[4], node.matrix[5], node.matrix[6]);
    const float depth = length3(node.matrix[8], node.matrix[9], node.matrix[10]);
    return std::abs(right - 1.0f) <= epsilon &&
           std::abs(up - 1.0f) <= epsilon &&
           std::abs(depth - 1.0f) <= epsilon &&
           std::abs(dot3(node.matrix[0],
                         node.matrix[1],
                         node.matrix[2],
                         node.matrix[4],
                         node.matrix[5],
                         node.matrix[6])) <= epsilon &&
           std::abs(dot3(node.matrix[0],
                         node.matrix[1],
                         node.matrix[2],
                         node.matrix[8],
                         node.matrix[9],
                         node.matrix[10])) <= epsilon &&
           std::abs(dot3(node.matrix[4],
                         node.matrix[5],
                         node.matrix[6],
                         node.matrix[8],
                         node.matrix[9],
                         node.matrix[10])) <= epsilon;
}

bool node_has_non_identity_transform(const NodeData& node) {
    constexpr float epsilon = 0.00001f;
    auto differs = [](float value, float expected) {
        return std::abs(value - expected) > epsilon;
    };
    return node.has_matrix_transform ||
           differs(node.translation[0], 0.0f) ||
           differs(node.translation[1], 0.0f) ||
           differs(node.translation[2], 0.0f) ||
           differs(node.rotation[0], 0.0f) ||
           differs(node.rotation[1], 0.0f) ||
           differs(node.rotation[2], 0.0f) ||
           differs(node.rotation[3], 1.0f) ||
           differs(node.scale[0], 1.0f) ||
           differs(node.scale[1], 1.0f) ||
           differs(node.scale[2], 1.0f);
}

bool node_has_deferred_camera_transform(const NodeData& node) {
    constexpr float epsilon = 0.00001f;
    return !matrix_is_rigid_transform(node) ||
           std::abs(node.scale[0] - 1.0f) > epsilon ||
           std::abs(node.scale[1] - 1.0f) > epsilon ||
           std::abs(node.scale[2] - 1.0f) > epsilon;
}

void record_light_intent(const SceneData& scene,
                         const std::vector<TransformedNode>& transformed_nodes,
                         Renderer3DCharacterization& out) {
    bool has_point = false;
    bool has_spot = false;
    uint32_t point_range_count = 0;
    uint32_t spot_range_count = 0;
    bool has_spot_cone = false;
    for (const auto& light : scene.lights) {
        has_point = has_point || light.type == LightData::Type::point;
        has_spot = has_spot || light.type == LightData::Type::spot;
        if (light.type == LightData::Type::point && light.range > 0.0f) {
            ++point_range_count;
        } else if (light.type == LightData::Type::spot) {
            if (light.range > 0.0f) {
                ++spot_range_count;
            }
            has_spot_cone =
                has_spot_cone ||
                light.inner_cone_angle > 0.0f ||
                light.outer_cone_angle != 0.7853982f;
        }
    }
    for (const auto& node : scene.nodes) {
        out.light_node_transform_deferred =
            out.light_node_transform_deferred ||
            (is_valid_scene_index(node.light, scene.lights.size()) &&
             node_has_deferred_camera_transform(node));
    }

    for (const auto& transformed : transformed_nodes) {
        if (!is_valid_scene_index(transformed.node, scene.nodes.size())) {
            continue;
        }
        const auto& node = scene.nodes[transformed.node];
        if (!is_valid_scene_index(node.light, scene.lights.size())) {
            continue;
        }
        const auto& light = scene.lights[node.light];
        if (light.type == LightData::Type::directional &&
            !out.directional_light_applied) {
            out.directional_light_applied = true;
            out.directional_light_transform_applied =
                node_has_non_identity_transform(node);
        } else if (light.type == LightData::Type::point &&
                   !out.point_light_applied) {
            out.point_light_applied = true;
            out.punctual_light_range_applied =
                out.punctual_light_range_applied || light.range > 0.0f;
        } else if (light.type == LightData::Type::spot &&
                   !out.spot_light_applied) {
            out.spot_light_applied = true;
            out.punctual_light_range_applied =
                out.punctual_light_range_applied || light.range > 0.0f;
        }
    }

    if (!out.directional_light_applied) {
        for (const auto& light : scene.lights) {
            if (light.type == LightData::Type::directional) {
                out.directional_light_applied = true;
                break;
            }
        }
    }
    out.point_light_deferred = has_point && !out.point_light_applied;
    out.spot_light_deferred = has_spot && !out.spot_light_applied;
    out.punctual_light_range_deferred =
        (point_range_count > 0u && !out.point_light_applied) ||
        (spot_range_count > 0u && !out.spot_light_applied);
    out.spot_light_cone_deferred =
        has_spot_cone && !out.spot_light_applied;
}

void record_camera_transform_intent(const TransformedNode& transformed,
                                    Renderer3DCharacterization& out) {
    constexpr float epsilon = 0.00001f;
    out.camera_node_translation_applied =
        std::abs(transformed.world_transform.m[12]) > epsilon ||
        std::abs(transformed.world_transform.m[13]) > epsilon ||
        std::abs(transformed.world_transform.m[14]) > epsilon;
    out.camera_node_rotation_applied =
        std::abs(transformed.world_transform.m[0] - 1.0f) > epsilon ||
        std::abs(transformed.world_transform.m[1]) > epsilon ||
        std::abs(transformed.world_transform.m[2]) > epsilon ||
        std::abs(transformed.world_transform.m[4]) > epsilon ||
        std::abs(transformed.world_transform.m[5] - 1.0f) > epsilon ||
        std::abs(transformed.world_transform.m[6]) > epsilon ||
        std::abs(transformed.world_transform.m[8]) > epsilon ||
        std::abs(transformed.world_transform.m[9]) > epsilon ||
        std::abs(transformed.world_transform.m[10] - 1.0f) > epsilon;
}

void record_camera_intent(const SceneData& scene,
                          const std::vector<TransformedNode>& transformed_nodes,
                          Renderer3DCharacterization& out) {
    uint32_t consumed_camera = invalid_scene_index;
    for (const auto& node : scene.nodes) {
        out.camera_node_transform_deferred =
            out.camera_node_transform_deferred ||
            (is_valid_scene_index(node.camera, scene.cameras.size()) &&
             node_has_deferred_camera_transform(node));
    }

    auto apply_camera = [&scene, &out, &consumed_camera](uint32_t camera_index) {
        if (!is_valid_scene_index(camera_index, scene.cameras.size()) ||
            consumed_camera != invalid_scene_index) {
            return false;
        }
        const auto& camera = scene.cameras[camera_index];
        consumed_camera = camera_index;
        out.perspective_camera_applied =
            camera.projection == CameraData::Projection::perspective;
        out.orthographic_camera_applied =
            camera.projection == CameraData::Projection::orthographic;
        out.camera_aspect_ratio_applied = camera.aspect_ratio > 0.0f;
        out.camera_depth_range_applied =
            camera.znear > 0.0f || camera.zfar > 0.0f;
        return true;
    };

    for (const auto& transformed : transformed_nodes) {
        if (!is_valid_scene_index(transformed.node, scene.nodes.size())) {
            continue;
        }
        const auto& node = scene.nodes[transformed.node];
        if (apply_camera(node.camera)) {
            if (!node_has_deferred_camera_transform(node)) {
                record_camera_transform_intent(transformed, out);
            }
            break;
        }
    }
    if (consumed_camera == invalid_scene_index) {
        for (uint32_t i = 0; i < static_cast<uint32_t>(scene.cameras.size());
             ++i) {
            if (apply_camera(i)) {
                break;
            }
        }
    }

    for (uint32_t i = 0; i < static_cast<uint32_t>(scene.cameras.size()); ++i) {
        const auto& camera = scene.cameras[i];
        out.camera_aspect_ratio_deferred =
            out.camera_aspect_ratio_deferred ||
            (camera.aspect_ratio > 0.0f &&
             !(i == consumed_camera && out.camera_aspect_ratio_applied));
        out.camera_depth_range_deferred =
            out.camera_depth_range_deferred ||
            ((camera.znear > 0.0f || camera.zfar > 0.0f) &&
             !(i == consumed_camera && out.camera_depth_range_applied));
    }
}

} // namespace

Renderer3DCharacterization characterize_renderer3d_scene(
    const SceneData& scene) {
    Renderer3DCharacterization out;
    out.transform_animation_deferred = has_transform_animation(scene);
    record_unsupported_features(scene, out);

    SceneData render_scene = scene;
    out.transform_animation_initial_pose_applied =
        apply_initial_animation_pose(render_scene);

    const auto packet = build_render_packet(render_scene);
    out.diagnostics = packet.diagnostics;
    if (packet.has_errors()) {
        out.error =
            "Renderer3D characterization: SceneData render-packet build failed";
        return out;
    }
    if (packet.primitives.empty()) {
        out.error =
            "Renderer3D characterization: SceneData has no renderable indexed primitive";
        append_diagnostic(out.diagnostics,
                          Diagnostic::Severity::warning,
                          "scene.renderer3d_no_renderable_primitives",
                          "SceneData has no indexed primitives that can be characterized for Renderer3D.");
        return out;
    }

    record_light_intent(render_scene, packet.transformed_nodes, out);
    record_camera_intent(render_scene, packet.transformed_nodes, out);

    out.scene_data_consumed = true;
    out.primitive_count = static_cast<uint32_t>(packet.primitives.size());
    std::unordered_set<uint32_t> pipeline_keys;
    uint32_t blended_count = 0;

    for (const auto& render_primitive : packet.primitives) {
        if (!is_valid_scene_index(render_primitive.mesh,
                                  render_scene.meshes.size())) {
            continue;
        }
        const auto& mesh = render_scene.meshes[render_primitive.mesh];
        if (!is_valid_scene_index(render_primitive.primitive,
                                  mesh.primitives.size())) {
            continue;
        }
        const auto& primitive = mesh.primitives[render_primitive.primitive];
        const MaterialData* material = nullptr;
        if (is_valid_scene_index(primitive.material,
                                 render_scene.materials.size())) {
            material = &render_scene.materials[primitive.material];
        }

        Renderer3DPrimitiveIntent primitive_out;
        primitive_out.node = render_primitive.node;
        primitive_out.mesh = render_primitive.mesh;
        primitive_out.primitive = render_primitive.primitive;
        primitive_out.material = material == nullptr
            ? invalid_scene_index
            : primitive.material;
        primitive_out.material_fallback = material == nullptr;

        const size_t vertex_count = primitive.positions.size() / 3u;
        primitive_out.vertex_color =
            primitive.color0.size() >= vertex_count * 4u;
        primitive_out.geometry_normals =
            primitive.normals.size() >= vertex_count * 3u;
        primitive_out.tangent_attributes =
            primitive.tangents.size() >= vertex_count * 4u;
        out.vertex_color_applied =
            out.vertex_color_applied || primitive_out.vertex_color;
        out.geometry_normals_applied =
            out.geometry_normals_applied || primitive_out.geometry_normals;
        out.tangent_attributes_available =
            out.tangent_attributes_available ||
            primitive_out.tangent_attributes;

        if (material != nullptr) {
            out.base_color_factor_applied = true;
            out.metallic_roughness_factor_applied = true;
            primitive_out.unlit = material->unlit;
            primitive_out.alpha_mask =
                material->alpha_mode == MaterialData::AlphaMode::mask;
            primitive_out.alpha_blend =
                material->alpha_mode == MaterialData::AlphaMode::blend;
            primitive_out.double_sided = material->double_sided;
            out.unlit_material_applied =
                out.unlit_material_applied || primitive_out.unlit;
            out.alpha_mask_applied =
                out.alpha_mask_applied || primitive_out.alpha_mask;
            out.alpha_blend_applied =
                out.alpha_blend_applied || primitive_out.alpha_blend;
            out.double_sided_material_applied =
                out.double_sided_material_applied ||
                primitive_out.double_sided;
            if (primitive_out.alpha_blend) {
                out.alpha_blend_depth_write_disabled = true;
                ++blended_count;
            }

            primitive_out.base_color_texture_routed =
                has_texture(render_scene, material->base_color_texture);
            out.base_color_texture_routed =
                out.base_color_texture_routed ||
                primitive_out.base_color_texture_routed;
            primitive_out.base_color_transform_routed =
                primitive_out.base_color_texture_routed &&
                material->base_color_transform.enabled;
            primitive_out.base_color_texcoord1_routed =
                primitive_out.base_color_texture_routed &&
                texture_uses_texcoord1(primitive,
                                       material->base_color_texcoord,
                                       material->base_color_transform);
            out.base_color_transform_routed =
                out.base_color_transform_routed ||
                primitive_out.base_color_transform_routed;
            out.base_color_texcoord1_routed =
                out.base_color_texcoord1_routed ||
                primitive_out.base_color_texcoord1_routed;
            if (primitive_out.base_color_texture_routed &&
                has_sampler(render_scene, material->base_color_sampler)) {
                primitive_out.base_color_sampler_routed = true;
                record_sampler_intent(
                    render_scene.texture_samplers[material->base_color_sampler],
                    out);
            }

            out.emissive_factor_applied =
                out.emissive_factor_applied ||
                material->emissive_factor[0] > 0.0f ||
                material->emissive_factor[1] > 0.0f ||
                material->emissive_factor[2] > 0.0f;
            out.emissive_strength_applied =
                out.emissive_strength_applied ||
                (material->emissive_strength != 1.0f &&
                 (material->emissive_factor[0] > 0.0f ||
                  material->emissive_factor[1] > 0.0f ||
                  material->emissive_factor[2] > 0.0f));
            out.advanced_material_extension_deferred =
                out.advanced_material_extension_deferred ||
                !material->advanced_material_extensions.empty();

            const bool metallic_roughness_texture =
                has_texture(render_scene,
                            material->metallic_roughness_texture);
            out.metallic_roughness_texture_routed =
                out.metallic_roughness_texture_routed ||
                metallic_roughness_texture;
            record_non_base_texture_intent(render_scene,
                                           primitive,
                                           material->metallic_roughness_texture,
                                           material->metallic_roughness_sampler,
                                           material->metallic_roughness_texcoord,
                                           material->metallic_roughness_transform,
                                           primitive_out,
                                           out);

            const bool normal_texture =
                has_texture(render_scene, material->normal_texture);
            out.normal_texture_routed =
                out.normal_texture_routed || normal_texture;
            primitive_out.normal_texture_requires_tangents =
                normal_texture && !primitive_out.tangent_attributes;
            out.normal_texture_requires_tangents =
                out.normal_texture_requires_tangents ||
                primitive_out.normal_texture_requires_tangents;
            out.normal_scale_routed =
                out.normal_scale_routed ||
                (normal_texture && material->normal_scale != 1.0f);
            record_non_base_texture_intent(render_scene,
                                           primitive,
                                           material->normal_texture,
                                           material->normal_sampler,
                                           material->normal_texcoord,
                                           material->normal_transform,
                                           primitive_out,
                                           out);

            const bool occlusion_texture =
                has_texture(render_scene, material->occlusion_texture);
            out.occlusion_texture_routed =
                out.occlusion_texture_routed || occlusion_texture;
            out.occlusion_strength_routed =
                out.occlusion_strength_routed ||
                (occlusion_texture && material->occlusion_strength != 1.0f);
            record_non_base_texture_intent(render_scene,
                                           primitive,
                                           material->occlusion_texture,
                                           material->occlusion_sampler,
                                           material->occlusion_texcoord,
                                           material->occlusion_transform,
                                           primitive_out,
                                           out);

            const bool emissive_texture =
                has_texture(render_scene, material->emissive_texture);
            out.emissive_texture_routed =
                out.emissive_texture_routed || emissive_texture;
            record_non_base_texture_intent(render_scene,
                                           primitive,
                                           material->emissive_texture,
                                           material->emissive_sampler,
                                           material->emissive_texcoord,
                                           material->emissive_transform,
                                           primitive_out,
                                           out);
        }

        const uint32_t pipeline_key =
            (primitive_out.alpha_blend ? 1u : 0u) |
            (primitive_out.double_sided ? 2u : 0u);
        if (!pipeline_keys.insert(pipeline_key).second) {
            ++out.pipeline_cache_hit_count;
        }
        out.primitives.push_back(primitive_out);
    }

    out.pipeline_cache_entry_count =
        static_cast<uint32_t>(pipeline_keys.size());
    out.alpha_blend_sorted = blended_count > 1u;
    out.success = out.scene_data_consumed && !has_error_diagnostics(out.diagnostics);
    return out;
}

} // namespace pulp::scene

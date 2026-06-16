#pragma once

#include <pulp/scene/scene_data.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace pulp::scene {

struct Renderer3DPrimitiveIntent {
    uint32_t node = invalid_scene_index;
    uint32_t mesh = invalid_scene_index;
    uint32_t primitive = invalid_scene_index;
    uint32_t material = invalid_scene_index;
    bool material_fallback = false;
    bool base_color_texture_routed = false;
    bool base_color_sampler_routed = false;
    bool base_color_transform_routed = false;
    bool base_color_texcoord1_routed = false;
    bool non_base_color_texture_routed = false;
    bool non_base_color_texture_transform_routed = false;
    bool non_base_color_texcoord1_routed = false;
    bool non_base_color_texcoord_deferred = false;
    bool unlit = false;
    bool alpha_mask = false;
    bool alpha_blend = false;
    bool double_sided = false;
    bool vertex_color = false;
    bool geometry_normals = false;
    bool tangent_attributes = false;
    bool normal_texture_requires_tangents = false;
};

struct Renderer3DCharacterization {
    bool success = false;
    bool scene_data_consumed = false;
    bool transform_animation_initial_pose_applied = false;
    bool transform_animation_deferred = false;
    bool base_color_texture_routed = false;
    bool texture_sampler_routed = false;
    bool texture_sampler_clamp_s = false;
    bool texture_sampler_clamp_t = false;
    bool texture_sampler_linear = false;
    bool texture_mipmap_filter_downgrade_intended = false;
    bool base_color_transform_routed = false;
    bool base_color_texcoord1_routed = false;
    bool base_color_factor_applied = false;
    bool unlit_material_applied = false;
    bool alpha_mask_applied = false;
    bool alpha_blend_applied = false;
    bool alpha_blend_depth_write_disabled = false;
    bool alpha_blend_sorted = false;
    bool vertex_color_applied = false;
    bool geometry_normals_applied = false;
    bool metallic_roughness_factor_applied = false;
    bool metallic_roughness_texture_routed = false;
    bool double_sided_material_applied = false;
    bool emissive_factor_applied = false;
    bool emissive_strength_applied = false;
    bool emissive_texture_routed = false;
    bool directional_light_applied = false;
    bool directional_light_transform_applied = false;
    bool light_node_transform_deferred = false;
    bool point_light_applied = false;
    bool point_light_deferred = false;
    bool spot_light_applied = false;
    bool spot_light_deferred = false;
    bool punctual_light_range_applied = false;
    bool punctual_light_range_deferred = false;
    bool spot_light_cone_deferred = false;
    bool perspective_camera_applied = false;
    bool orthographic_camera_applied = false;
    bool camera_node_translation_applied = false;
    bool camera_node_rotation_applied = false;
    bool camera_aspect_ratio_applied = false;
    bool camera_aspect_ratio_deferred = false;
    bool camera_depth_range_applied = false;
    bool camera_depth_range_deferred = false;
    bool camera_node_transform_deferred = false;
    bool tangent_attributes_available = false;
    bool normal_texture_routed = false;
    bool normal_texture_requires_tangents = false;
    bool normal_scale_routed = false;
    bool occlusion_texture_routed = false;
    bool occlusion_strength_routed = false;
    bool non_base_color_texture_routed = false;
    bool non_base_color_texture_transform_routed = false;
    bool non_base_color_texcoord1_routed = false;
    bool non_base_color_texcoord_deferred = false;
    bool advanced_material_extension_deferred = false;
    bool skinning_deferred = false;
    bool morph_target_deferred = false;
    bool gpu_instancing_deferred = false;
    uint32_t primitive_count = 0;
    uint32_t pipeline_cache_entry_count = 0;
    uint32_t pipeline_cache_hit_count = 0;
    std::vector<Renderer3DPrimitiveIntent> primitives;
    std::vector<Diagnostic> diagnostics;
    std::string error;
};

Renderer3DCharacterization characterize_renderer3d_scene(
    const SceneData& scene);

} // namespace pulp::scene

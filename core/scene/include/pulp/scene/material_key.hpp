#pragma once

#include <pulp/scene/scene_data.hpp>

#include <cstdint>
#include <vector>

namespace pulp::scene {

enum class MaterialFeature : uint32_t {
    normals = 1u << 0u,
    texcoord0 = 1u << 1u,
    indexed = 1u << 2u,
    base_color_texture = 1u << 3u,
    unlit = 1u << 4u,
    double_sided = 1u << 5u,
    alpha_blend = 1u << 6u,
    material_fallback = 1u << 7u,
    metallic_roughness_texture = 1u << 8u,
    normal_texture = 1u << 9u,
    occlusion_texture = 1u << 10u,
    emissive_texture = 1u << 11u,
    alpha_mask = 1u << 12u,
    tangents = 1u << 13u,
    texcoord1 = 1u << 14u,
    color0 = 1u << 15u,
    base_color_texture_transform = 1u << 16u,
    non_base_color_texture_transform = 1u << 17u,
    non_base_color_texcoord1 = 1u << 18u,
    normal_scale = 1u << 19u,
    occlusion_strength = 1u << 20u,
};

struct MaterialKey {
    uint32_t feature_mask = 0;
    uint32_t material = invalid_scene_index;

    bool has(MaterialFeature feature) const {
        return (feature_mask & static_cast<uint32_t>(feature)) != 0u;
    }
};

MaterialKey derive_material_key(const SceneData& scene,
                                const PrimitiveData& primitive);
std::vector<const char*> material_feature_names(const MaterialKey& key);

} // namespace pulp::scene

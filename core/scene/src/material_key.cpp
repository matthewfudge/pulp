#include <pulp/scene/material_key.hpp>

namespace pulp::scene {
namespace {

void set_feature(uint32_t& mask, MaterialFeature feature) {
    mask |= static_cast<uint32_t>(feature);
}

void append_feature_name(const MaterialKey& key,
                         MaterialFeature feature,
                         const char* name,
                         std::vector<const char*>& names) {
    if (key.has(feature)) {
        names.push_back(name);
    }
}

} // namespace

MaterialKey derive_material_key(const SceneData& scene,
                                const PrimitiveData& primitive) {
    MaterialKey key;

    if (!primitive.normals.empty()) {
        set_feature(key.feature_mask, MaterialFeature::normals);
    }
    if (!primitive.tangents.empty()) {
        set_feature(key.feature_mask, MaterialFeature::tangents);
    }
    if (!primitive.texcoord0.empty()) {
        set_feature(key.feature_mask, MaterialFeature::texcoord0);
    }
    if (!primitive.texcoord1.empty()) {
        set_feature(key.feature_mask, MaterialFeature::texcoord1);
    }
    if (!primitive.color0.empty()) {
        set_feature(key.feature_mask, MaterialFeature::color0);
    }
    if (!primitive.indices.empty()) {
        set_feature(key.feature_mask, MaterialFeature::indexed);
    }

    if (!is_valid_scene_index(primitive.material, scene.materials.size())) {
        set_feature(key.feature_mask, MaterialFeature::material_fallback);
        return key;
    }

    key.material = primitive.material;
    const auto& material = scene.materials[primitive.material];
    if (material.unlit) {
        set_feature(key.feature_mask, MaterialFeature::unlit);
    }
    if (material.double_sided) {
        set_feature(key.feature_mask, MaterialFeature::double_sided);
    }
    if (material.base_color_factor[3] < 1.0f) {
        set_feature(key.feature_mask, MaterialFeature::alpha_blend);
    }
    if (material.alpha_mode == MaterialData::AlphaMode::blend) {
        set_feature(key.feature_mask, MaterialFeature::alpha_blend);
    }
    if (material.alpha_mode == MaterialData::AlphaMode::mask) {
        set_feature(key.feature_mask, MaterialFeature::alpha_mask);
    }
    if (is_valid_scene_index(material.base_color_texture, scene.textures.size())) {
        set_feature(key.feature_mask, MaterialFeature::base_color_texture);
        if (material.base_color_transform.enabled) {
            set_feature(key.feature_mask,
                        MaterialFeature::base_color_texture_transform);
        }
    }
    if (is_valid_scene_index(material.metallic_roughness_texture, scene.textures.size())) {
        set_feature(key.feature_mask, MaterialFeature::metallic_roughness_texture);
        if (material.metallic_roughness_transform.enabled) {
            set_feature(key.feature_mask,
                        MaterialFeature::non_base_color_texture_transform);
        }
        if (material.metallic_roughness_texcoord > 0u) {
            set_feature(key.feature_mask,
                        MaterialFeature::non_base_color_texcoord1);
        }
    }
    if (is_valid_scene_index(material.normal_texture, scene.textures.size())) {
        set_feature(key.feature_mask, MaterialFeature::normal_texture);
        if (material.normal_transform.enabled) {
            set_feature(key.feature_mask,
                        MaterialFeature::non_base_color_texture_transform);
        }
        if (material.normal_texcoord > 0u) {
            set_feature(key.feature_mask,
                        MaterialFeature::non_base_color_texcoord1);
        }
        if (material.normal_scale != 1.0f) {
            set_feature(key.feature_mask, MaterialFeature::normal_scale);
        }
    }
    if (is_valid_scene_index(material.occlusion_texture, scene.textures.size())) {
        set_feature(key.feature_mask, MaterialFeature::occlusion_texture);
        if (material.occlusion_transform.enabled) {
            set_feature(key.feature_mask,
                        MaterialFeature::non_base_color_texture_transform);
        }
        if (material.occlusion_texcoord > 0u) {
            set_feature(key.feature_mask,
                        MaterialFeature::non_base_color_texcoord1);
        }
        if (material.occlusion_strength != 1.0f) {
            set_feature(key.feature_mask, MaterialFeature::occlusion_strength);
        }
    }
    if (is_valid_scene_index(material.emissive_texture, scene.textures.size())) {
        set_feature(key.feature_mask, MaterialFeature::emissive_texture);
        if (material.emissive_transform.enabled) {
            set_feature(key.feature_mask,
                        MaterialFeature::non_base_color_texture_transform);
        }
        if (material.emissive_texcoord > 0u) {
            set_feature(key.feature_mask,
                        MaterialFeature::non_base_color_texcoord1);
        }
    }

    return key;
}

std::vector<const char*> material_feature_names(const MaterialKey& key) {
    std::vector<const char*> names;
    append_feature_name(key, MaterialFeature::normals, "normals", names);
    append_feature_name(key, MaterialFeature::texcoord0, "texcoord0", names);
    append_feature_name(key, MaterialFeature::indexed, "indexed", names);
    append_feature_name(key,
                        MaterialFeature::base_color_texture,
                        "base_color_texture",
                        names);
    append_feature_name(key, MaterialFeature::unlit, "unlit", names);
    append_feature_name(key, MaterialFeature::double_sided, "double_sided", names);
    append_feature_name(key, MaterialFeature::alpha_blend, "alpha_blend", names);
    append_feature_name(key,
                        MaterialFeature::material_fallback,
                        "material_fallback",
                        names);
    append_feature_name(key,
                        MaterialFeature::metallic_roughness_texture,
                        "metallic_roughness_texture",
                        names);
    append_feature_name(key,
                        MaterialFeature::normal_texture,
                        "normal_texture",
                        names);
    append_feature_name(key,
                        MaterialFeature::occlusion_texture,
                        "occlusion_texture",
                        names);
    append_feature_name(key,
                        MaterialFeature::emissive_texture,
                        "emissive_texture",
                        names);
    append_feature_name(key, MaterialFeature::alpha_mask, "alpha_mask", names);
    append_feature_name(key, MaterialFeature::tangents, "tangents", names);
    append_feature_name(key, MaterialFeature::texcoord1, "texcoord1", names);
    append_feature_name(key, MaterialFeature::color0, "color0", names);
    append_feature_name(key,
                        MaterialFeature::base_color_texture_transform,
                        "base_color_texture_transform",
                        names);
    append_feature_name(key,
                        MaterialFeature::non_base_color_texture_transform,
                        "non_base_color_texture_transform",
                        names);
    append_feature_name(key,
                        MaterialFeature::non_base_color_texcoord1,
                        "non_base_color_texcoord1",
                        names);
    append_feature_name(key, MaterialFeature::normal_scale, "normal_scale", names);
    append_feature_name(key,
                        MaterialFeature::occlusion_strength,
                        "occlusion_strength",
                        names);
    return names;
}

} // namespace pulp::scene

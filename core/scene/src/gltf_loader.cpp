#include <pulp/scene/gltf_loader.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace pulp::scene {
namespace {

std::string fastgltf_error_string(fastgltf::Error error) {
    std::string out(fastgltf::getErrorName(error));
    auto message = fastgltf::getErrorMessage(error);
    if (!message.empty()) {
        out += ": ";
        out += message;
    }
    return out;
}

void append_loader_error(SceneData& scene,
                         std::string code,
                         std::string message,
                         const std::filesystem::path& path) {
    append_diagnostic(scene.diagnostics,
                      Diagnostic::Severity::error,
                      std::move(code),
                      std::move(message),
                      path.string());
}

void append_loader_warning(SceneData& scene,
                           std::string code,
                           std::string message,
                           const std::filesystem::path& path) {
    append_diagnostic(scene.diagnostics,
                      Diagnostic::Severity::warning,
                      std::move(code),
                      std::move(message),
                      path.string());
}

void append_loader_unsupported(SceneData& scene,
                               std::string feature,
                               std::string reason,
                               std::string node_path,
                               const std::filesystem::path& path,
                               std::string diagnostic_code) {
    scene.unsupported_features.push_back(UnsupportedFeatureData{
        feature,
        reason,
        node_path,
    });
    append_loader_warning(scene,
                          std::move(diagnostic_code),
                          std::move(reason),
                          path);
}

template <typename Source>
bool copy_source_bytes(const Source&, std::vector<uint8_t>&) {
    return false;
}

bool copy_source_bytes(const fastgltf::sources::Array& source,
                       std::vector<uint8_t>& out) {
    out.resize(source.bytes.size());
    std::transform(source.bytes.begin(),
                   source.bytes.end(),
                   out.begin(),
                   [](std::byte byte) {
                       return static_cast<uint8_t>(byte);
                   });
    return true;
}

bool copy_source_bytes(const fastgltf::sources::Vector& source,
                       std::vector<uint8_t>& out) {
    out.resize(source.bytes.size());
    std::transform(source.bytes.begin(),
                   source.bytes.end(),
                   out.begin(),
                   [](std::byte byte) {
                       return static_cast<uint8_t>(byte);
                   });
    return true;
}

bool copy_source_bytes(const fastgltf::sources::ByteView& source,
                       std::vector<uint8_t>& out) {
    out.resize(source.bytes.size());
    std::transform(source.bytes.begin(),
                   source.bytes.end(),
                   out.begin(),
                   [](std::byte byte) {
                       return static_cast<uint8_t>(byte);
                   });
    return true;
}

bool copy_buffer_view_bytes(const fastgltf::Asset& asset,
                            size_t buffer_view_index,
                            std::vector<uint8_t>& out) {
    if (buffer_view_index >= asset.bufferViews.size()) {
        return false;
    }

    const auto& view = asset.bufferViews[buffer_view_index];
    if (view.bufferIndex >= asset.buffers.size()) {
        return false;
    }

    std::vector<uint8_t> buffer_bytes;
    const auto& buffer = asset.buffers[view.bufferIndex];
    const bool copied = std::visit(
        [&](const auto& source) {
            return copy_source_bytes(source, buffer_bytes);
        },
        buffer.data);
    if (!copied || view.byteOffset + view.byteLength > buffer_bytes.size()) {
        return false;
    }

    out.assign(buffer_bytes.begin() + static_cast<std::ptrdiff_t>(view.byteOffset),
               buffer_bytes.begin() + static_cast<std::ptrdiff_t>(view.byteOffset + view.byteLength));
    return true;
}

std::string mime_type_name(fastgltf::MimeType type) {
    switch (type) {
        case fastgltf::MimeType::JPEG: return "image/jpeg";
        case fastgltf::MimeType::PNG: return "image/png";
        case fastgltf::MimeType::KTX2: return "image/ktx2";
        case fastgltf::MimeType::DDS: return "image/vnd-ms.dds";
        case fastgltf::MimeType::WEBP: return "image/webp";
        case fastgltf::MimeType::GltfBuffer: return "model/gltf-buffer";
        case fastgltf::MimeType::OctetStream: return "application/octet-stream";
        case fastgltf::MimeType::None: break;
    }
    return {};
}

std::string primitive_type_name(fastgltf::PrimitiveType type) {
    switch (type) {
        case fastgltf::PrimitiveType::Points: return "Points";
        case fastgltf::PrimitiveType::Lines: return "Lines";
        case fastgltf::PrimitiveType::LineLoop: return "LineLoop";
        case fastgltf::PrimitiveType::LineStrip: return "LineStrip";
        case fastgltf::PrimitiveType::Triangles: return "Triangles";
        case fastgltf::PrimitiveType::TriangleStrip: return "TriangleStrip";
        case fastgltf::PrimitiveType::TriangleFan: return "TriangleFan";
    }
    return "Unknown";
}

void load_textures(const fastgltf::Asset& asset,
                   SceneData& scene,
                   const std::filesystem::path& path) {
    scene.textures.reserve(asset.images.size());
    for (const auto& image : asset.images) {
        TextureData texture;
        texture.name = std::string(image.name);

        bool copied = false;
        std::visit(
            [&](const auto& source) {
                using Source = std::decay_t<decltype(source)>;
                if constexpr (std::is_same_v<Source, fastgltf::sources::BufferView>) {
                    texture.mime_type = mime_type_name(source.mimeType);
                    copied = copy_buffer_view_bytes(asset, source.bufferViewIndex, texture.encoded_bytes);
                } else if constexpr (std::is_same_v<Source, fastgltf::sources::Array> ||
                                     std::is_same_v<Source, fastgltf::sources::Vector> ||
                                     std::is_same_v<Source, fastgltf::sources::ByteView>) {
                    texture.mime_type = mime_type_name(source.mimeType);
                    copied = copy_source_bytes(source, texture.encoded_bytes);
                }
            },
            image.data);

        if (!copied) {
            append_loader_warning(scene,
                                  "gltf.image_unsupported_source",
                                  "Image source is not embedded in the GLB buffer; external images are not loaded in this native slice.",
                                  path);
        }

        scene.textures.push_back(std::move(texture));
    }
}

TextureSamplerData::Filter sampler_filter_from_gltf(fastgltf::Filter filter) {
    switch (filter) {
        case fastgltf::Filter::Nearest:
            return TextureSamplerData::Filter::nearest;
        case fastgltf::Filter::Linear:
            return TextureSamplerData::Filter::linear;
        case fastgltf::Filter::NearestMipMapNearest:
            return TextureSamplerData::Filter::nearest_mipmap_nearest;
        case fastgltf::Filter::LinearMipMapNearest:
            return TextureSamplerData::Filter::linear_mipmap_nearest;
        case fastgltf::Filter::NearestMipMapLinear:
            return TextureSamplerData::Filter::nearest_mipmap_linear;
        case fastgltf::Filter::LinearMipMapLinear:
            return TextureSamplerData::Filter::linear_mipmap_linear;
    }
    return TextureSamplerData::Filter::unspecified;
}

TextureSamplerData::Wrap sampler_wrap_from_gltf(fastgltf::Wrap wrap) {
    switch (wrap) {
        case fastgltf::Wrap::ClampToEdge:
            return TextureSamplerData::Wrap::clamp_to_edge;
        case fastgltf::Wrap::MirroredRepeat:
            return TextureSamplerData::Wrap::mirrored_repeat;
        case fastgltf::Wrap::Repeat:
            return TextureSamplerData::Wrap::repeat;
    }
    return TextureSamplerData::Wrap::repeat;
}

void load_texture_samplers(const fastgltf::Asset& asset, SceneData& scene) {
    scene.texture_samplers.reserve(asset.samplers.size());
    for (const auto& gltf_sampler : asset.samplers) {
        TextureSamplerData sampler;
        sampler.name = std::string(gltf_sampler.name);
        if (gltf_sampler.magFilter.has_value()) {
            sampler.mag_filter = sampler_filter_from_gltf(gltf_sampler.magFilter.value());
        }
        if (gltf_sampler.minFilter.has_value()) {
            sampler.min_filter = sampler_filter_from_gltf(gltf_sampler.minFilter.value());
        }
        sampler.wrap_s = sampler_wrap_from_gltf(gltf_sampler.wrapS);
        sampler.wrap_t = sampler_wrap_from_gltf(gltf_sampler.wrapT);
        scene.texture_samplers.push_back(std::move(sampler));
    }
}

uint32_t material_texture_image_index(const fastgltf::Asset& asset,
                                      size_t texture_index) {
    if (texture_index < asset.textures.size() &&
        asset.textures[texture_index].imageIndex.has_value()) {
        return static_cast<uint32_t>(asset.textures[texture_index].imageIndex.value());
    }
    return invalid_scene_index;
}

uint32_t material_texture_sampler_index(const fastgltf::Asset& asset,
                                        size_t texture_index) {
    if (texture_index < asset.textures.size() &&
        asset.textures[texture_index].samplerIndex.has_value()) {
        return static_cast<uint32_t>(asset.textures[texture_index].samplerIndex.value());
    }
    return invalid_scene_index;
}

template <typename TextureInfo>
TextureTransformData texture_transform_from_gltf(const TextureInfo& texture_info) {
    TextureTransformData transform;
    if (!texture_info.transform) {
        return transform;
    }

    transform.enabled = true;
    transform.offset[0] = static_cast<float>(texture_info.transform->uvOffset.x());
    transform.offset[1] = static_cast<float>(texture_info.transform->uvOffset.y());
    transform.scale[0] = static_cast<float>(texture_info.transform->uvScale.x());
    transform.scale[1] = static_cast<float>(texture_info.transform->uvScale.y());
    transform.rotation = static_cast<float>(texture_info.transform->rotation);
    if (texture_info.transform->texCoordIndex.has_value()) {
        transform.texcoord_override =
            static_cast<uint32_t>(texture_info.transform->texCoordIndex.value());
    }
    return transform;
}

MaterialData::AlphaMode alpha_mode_from_gltf(fastgltf::AlphaMode alpha_mode) {
    switch (alpha_mode) {
        case fastgltf::AlphaMode::Opaque:
            return MaterialData::AlphaMode::opaque;
        case fastgltf::AlphaMode::Mask:
            return MaterialData::AlphaMode::mask;
        case fastgltf::AlphaMode::Blend:
            return MaterialData::AlphaMode::blend;
    }
    return MaterialData::AlphaMode::opaque;
}

void append_advanced_material_extension(MaterialData& material,
                                        SceneData& scene,
                                        std::string extension,
                                        const std::filesystem::path& path) {
    const std::string reason =
        extension +
        " material data is parsed by glTF, but native shading for this material extension is not implemented in this slice.";
    material.advanced_material_extensions.push_back(std::move(extension));
    append_loader_warning(scene,
                          "gltf.material_extension_deferred",
                          reason,
                          path);
}

void load_materials(const fastgltf::Asset& asset,
                    SceneData& scene,
                    const std::filesystem::path& path) {
    scene.materials.reserve(asset.materials.size());
    for (const auto& gltf_material : asset.materials) {
        MaterialData material;
        material.name = std::string(gltf_material.name);
        material.double_sided = gltf_material.doubleSided;
        material.unlit = gltf_material.unlit;
        material.metallic_factor = static_cast<float>(gltf_material.pbrData.metallicFactor);
        material.roughness_factor = static_cast<float>(gltf_material.pbrData.roughnessFactor);
        material.emissive_factor[0] = static_cast<float>(gltf_material.emissiveFactor.x());
        material.emissive_factor[1] = static_cast<float>(gltf_material.emissiveFactor.y());
        material.emissive_factor[2] = static_cast<float>(gltf_material.emissiveFactor.z());
        material.emissive_strength = static_cast<float>(gltf_material.emissiveStrength);
        material.alpha_cutoff = static_cast<float>(gltf_material.alphaCutoff);
        material.alpha_mode = alpha_mode_from_gltf(gltf_material.alphaMode);
        for (size_t i = 0; i < 4; ++i) {
            material.base_color_factor[i] = gltf_material.pbrData.baseColorFactor[i];
        }

        if (gltf_material.pbrData.baseColorTexture.has_value()) {
            material.base_color_texture = material_texture_image_index(
                asset,
                gltf_material.pbrData.baseColorTexture->textureIndex);
            material.base_color_sampler = material_texture_sampler_index(
                asset,
                gltf_material.pbrData.baseColorTexture->textureIndex);
            material.base_color_texcoord =
                static_cast<uint32_t>(gltf_material.pbrData.baseColorTexture->texCoordIndex);
            material.base_color_transform =
                texture_transform_from_gltf(*gltf_material.pbrData.baseColorTexture);
        }
        if (gltf_material.pbrData.metallicRoughnessTexture.has_value()) {
            material.metallic_roughness_texture = material_texture_image_index(
                asset,
                gltf_material.pbrData.metallicRoughnessTexture->textureIndex);
            material.metallic_roughness_sampler = material_texture_sampler_index(
                asset,
                gltf_material.pbrData.metallicRoughnessTexture->textureIndex);
            material.metallic_roughness_texcoord =
                static_cast<uint32_t>(
                    gltf_material.pbrData.metallicRoughnessTexture->texCoordIndex);
            material.metallic_roughness_transform =
                texture_transform_from_gltf(*gltf_material.pbrData.metallicRoughnessTexture);
        }
        if (gltf_material.normalTexture.has_value()) {
            material.normal_texture = material_texture_image_index(
                asset,
                gltf_material.normalTexture->textureIndex);
            material.normal_sampler = material_texture_sampler_index(
                asset,
                gltf_material.normalTexture->textureIndex);
            material.normal_scale = static_cast<float>(gltf_material.normalTexture->scale);
            material.normal_texcoord =
                static_cast<uint32_t>(gltf_material.normalTexture->texCoordIndex);
            material.normal_transform =
                texture_transform_from_gltf(*gltf_material.normalTexture);
        }
        if (gltf_material.occlusionTexture.has_value()) {
            material.occlusion_texture = material_texture_image_index(
                asset,
                gltf_material.occlusionTexture->textureIndex);
            material.occlusion_sampler = material_texture_sampler_index(
                asset,
                gltf_material.occlusionTexture->textureIndex);
            material.occlusion_strength =
                static_cast<float>(gltf_material.occlusionTexture->strength);
            material.occlusion_texcoord =
                static_cast<uint32_t>(gltf_material.occlusionTexture->texCoordIndex);
            material.occlusion_transform =
                texture_transform_from_gltf(*gltf_material.occlusionTexture);
        }
        if (gltf_material.emissiveTexture.has_value()) {
            material.emissive_texture = material_texture_image_index(
                asset,
                gltf_material.emissiveTexture->textureIndex);
            material.emissive_sampler = material_texture_sampler_index(
                asset,
                gltf_material.emissiveTexture->textureIndex);
            material.emissive_texcoord =
                static_cast<uint32_t>(gltf_material.emissiveTexture->texCoordIndex);
            material.emissive_transform =
                texture_transform_from_gltf(*gltf_material.emissiveTexture);
        }

        if (gltf_material.clearcoat) {
            append_advanced_material_extension(material,
                                               scene,
                                               "KHR_materials_clearcoat",
                                               path);
        }
        if (gltf_material.transmission) {
            append_advanced_material_extension(material,
                                               scene,
                                               "KHR_materials_transmission",
                                               path);
        }
        if (gltf_material.sheen) {
            append_advanced_material_extension(material,
                                               scene,
                                               "KHR_materials_sheen",
                                               path);
        }
        if (gltf_material.specular) {
            append_advanced_material_extension(material,
                                               scene,
                                               "KHR_materials_specular",
                                               path);
        }
        if (gltf_material.volume) {
            append_advanced_material_extension(material,
                                               scene,
                                               "KHR_materials_volume",
                                               path);
        }
        if (gltf_material.anisotropy) {
            append_advanced_material_extension(material,
                                               scene,
                                               "KHR_materials_anisotropy",
                                               path);
        }
        if (gltf_material.iridescence) {
            append_advanced_material_extension(material,
                                               scene,
                                               "KHR_materials_iridescence",
                                               path);
        }
        if (gltf_material.diffuseTransmission) {
            append_advanced_material_extension(
                material,
                scene,
                "KHR_materials_diffuse_transmission",
                path);
        }
        if (gltf_material.ior != 1.5f) {
            append_advanced_material_extension(material,
                                               scene,
                                               "KHR_materials_ior",
                                               path);
        }
        if (gltf_material.dispersion != 0.0f) {
            append_advanced_material_extension(material,
                                               scene,
                                               "KHR_materials_dispersion",
                                               path);
        }

        scene.materials.push_back(std::move(material));
    }
}

void load_cameras(const fastgltf::Asset& asset, SceneData& scene) {
    scene.cameras.reserve(asset.cameras.size());
    for (const auto& gltf_camera : asset.cameras) {
        CameraData camera;
        camera.name = std::string(gltf_camera.name);
        if (const auto* perspective =
                std::get_if<fastgltf::Camera::Perspective>(&gltf_camera.camera)) {
            camera.projection = CameraData::Projection::perspective;
            camera.aspect_ratio = perspective->aspectRatio.has_value()
                ? static_cast<float>(perspective->aspectRatio.value())
                : 0.0f;
            camera.yfov = static_cast<float>(perspective->yfov);
            camera.znear = static_cast<float>(perspective->znear);
            camera.zfar = perspective->zfar.has_value()
                ? static_cast<float>(perspective->zfar.value())
                : 0.0f;
        } else if (const auto* orthographic =
                       std::get_if<fastgltf::Camera::Orthographic>(&gltf_camera.camera)) {
            camera.projection = CameraData::Projection::orthographic;
            camera.xmag = static_cast<float>(orthographic->xmag);
            camera.ymag = static_cast<float>(orthographic->ymag);
            camera.znear = static_cast<float>(orthographic->znear);
            camera.zfar = static_cast<float>(orthographic->zfar);
        }
        scene.cameras.push_back(std::move(camera));
    }
}

LightData::Type light_type_from_gltf(fastgltf::LightType type) {
    switch (type) {
        case fastgltf::LightType::Directional:
            return LightData::Type::directional;
        case fastgltf::LightType::Point:
            return LightData::Type::point;
        case fastgltf::LightType::Spot:
            return LightData::Type::spot;
    }
    return LightData::Type::directional;
}

void load_lights(const fastgltf::Asset& asset, SceneData& scene) {
    scene.lights.reserve(asset.lights.size());
    for (const auto& gltf_light : asset.lights) {
        LightData light;
        light.name = std::string(gltf_light.name);
        light.type = light_type_from_gltf(gltf_light.type);
        light.color[0] = static_cast<float>(gltf_light.color.x());
        light.color[1] = static_cast<float>(gltf_light.color.y());
        light.color[2] = static_cast<float>(gltf_light.color.z());
        light.intensity = static_cast<float>(gltf_light.intensity);
        light.range = gltf_light.range.has_value()
            ? static_cast<float>(gltf_light.range.value())
            : 0.0f;
        light.inner_cone_angle = gltf_light.innerConeAngle.has_value()
            ? static_cast<float>(gltf_light.innerConeAngle.value())
            : 0.0f;
        light.outer_cone_angle = gltf_light.outerConeAngle.has_value()
            ? static_cast<float>(gltf_light.outerConeAngle.value())
            : 0.7853982f;
        scene.lights.push_back(std::move(light));
    }
}

AnimationSamplerData::Interpolation interpolation_from_gltf(
    fastgltf::AnimationInterpolation interpolation) {
    switch (interpolation) {
        case fastgltf::AnimationInterpolation::Linear:
            return AnimationSamplerData::Interpolation::linear;
        case fastgltf::AnimationInterpolation::Step:
            return AnimationSamplerData::Interpolation::step;
        case fastgltf::AnimationInterpolation::CubicSpline:
            return AnimationSamplerData::Interpolation::cubic_spline;
    }
    return AnimationSamplerData::Interpolation::linear;
}

std::optional<AnimationChannelData::Path> animation_path_from_gltf(
    fastgltf::AnimationPath path) {
    switch (path) {
        case fastgltf::AnimationPath::Translation:
            return AnimationChannelData::Path::translation;
        case fastgltf::AnimationPath::Rotation:
            return AnimationChannelData::Path::rotation;
        case fastgltf::AnimationPath::Scale:
            return AnimationChannelData::Path::scale;
        case fastgltf::AnimationPath::Weights:
            return std::nullopt;
    }
    return std::nullopt;
}

std::string animation_path_name(fastgltf::AnimationPath path) {
    switch (path) {
        case fastgltf::AnimationPath::Translation: return "translation";
        case fastgltf::AnimationPath::Rotation: return "rotation";
        case fastgltf::AnimationPath::Scale: return "scale";
        case fastgltf::AnimationPath::Weights: return "weights";
    }
    return "unknown";
}

bool copy_animation_input_times(const fastgltf::Asset& asset,
                                size_t accessor_index,
                                AnimationSamplerData& sampler) {
    if (accessor_index >= asset.accessors.size()) {
        return false;
    }
    const auto& accessor = asset.accessors[accessor_index];
    sampler.input_times.reserve(accessor.count);
    fastgltf::iterateAccessor<float>(
        asset,
        accessor,
        [&](float value) {
            sampler.input_times.push_back(value);
        });
    return sampler.input_times.size() == accessor.count;
}

bool copy_animation_output_values(const fastgltf::Asset& asset,
                                  size_t accessor_index,
                                  AnimationChannelData::Path path,
                                  AnimationSamplerData& sampler) {
    if (accessor_index >= asset.accessors.size()) {
        return false;
    }
    const auto& accessor = asset.accessors[accessor_index];
    switch (path) {
        case AnimationChannelData::Path::translation:
        case AnimationChannelData::Path::scale:
            sampler.output_components = 3;
            sampler.output_values.reserve(accessor.count * 3u);
            fastgltf::iterateAccessor<fastgltf::math::fvec3>(
                asset,
                accessor,
                [&](fastgltf::math::fvec3 value) {
                    sampler.output_values.push_back(value.x());
                    sampler.output_values.push_back(value.y());
                    sampler.output_values.push_back(value.z());
                });
            return sampler.output_values.size() == accessor.count * 3u;
        case AnimationChannelData::Path::rotation:
            sampler.output_components = 4;
            sampler.output_values.reserve(accessor.count * 4u);
            fastgltf::iterateAccessor<fastgltf::math::fvec4>(
                asset,
                accessor,
                [&](fastgltf::math::fvec4 value) {
                    sampler.output_values.push_back(value.x());
                    sampler.output_values.push_back(value.y());
                    sampler.output_values.push_back(value.z());
                    sampler.output_values.push_back(value.w());
                });
            return sampler.output_values.size() == accessor.count * 4u;
    }
    return false;
}

void load_animations(const fastgltf::Asset& asset,
                     SceneData& scene,
                     const std::filesystem::path& path) {
    scene.animations.reserve(asset.animations.size());
    for (const auto& gltf_animation : asset.animations) {
        AnimationData animation;
        animation.name = std::string(gltf_animation.name);
        const std::string animation_path = animation.name.empty()
            ? "animation[" + std::to_string(scene.animations.size()) + "]"
            : animation.name;
        std::vector<uint32_t> sampler_remap(gltf_animation.samplers.size(),
                                            invalid_scene_index);

        for (const auto& gltf_channel : gltf_animation.channels) {
            if (gltf_channel.samplerIndex >= gltf_animation.samplers.size()) {
                append_loader_warning(scene,
                                      "gltf.animation_sampler_out_of_range",
                                      "Animation channel references a sampler outside the animation sampler table.",
                                      path);
                continue;
            }
            if (!gltf_channel.nodeIndex.has_value()) {
                append_loader_warning(scene,
                                      "gltf.animation_missing_target_node",
                                      "Animation channel is missing a target node.",
                                      path);
                continue;
            }

            const auto path_kind = animation_path_from_gltf(gltf_channel.path);
            if (!path_kind.has_value()) {
                const auto path_name = animation_path_name(gltf_channel.path);
                append_loader_unsupported(
                    scene,
                    "AnimationPath:" + path_name,
                    "Animation channel path " + path_name +
                        " is valid glTF, but the current native runtime slice only preserves TRS channels.",
                    animation_path,
                    path,
                    "gltf.animation_path_unsupported");
                continue;
            }

            uint32_t local_sampler = sampler_remap[gltf_channel.samplerIndex];
            if (local_sampler == invalid_scene_index) {
                const auto& gltf_sampler =
                    gltf_animation.samplers[gltf_channel.samplerIndex];
                auto sampler = AnimationSamplerData{};
                sampler.interpolation = interpolation_from_gltf(gltf_sampler.interpolation);
                const bool copied_input = copy_animation_input_times(
                    asset,
                    gltf_sampler.inputAccessor,
                    sampler);
                const bool copied_output = copy_animation_output_values(
                    asset,
                    gltf_sampler.outputAccessor,
                    path_kind.value(),
                    sampler);
                if (!copied_input || !copied_output) {
                    append_loader_warning(scene,
                                          "gltf.animation_accessor_unsupported",
                                          "Animation sampler accessors could not be decoded in this native slice.",
                                          path);
                    continue;
                }

                local_sampler = static_cast<uint32_t>(animation.samplers.size());
                animation.samplers.push_back(std::move(sampler));
                sampler_remap[gltf_channel.samplerIndex] = local_sampler;
            }

            animation.channels.push_back(AnimationChannelData{
                static_cast<uint32_t>(gltf_channel.nodeIndex.value()),
                local_sampler,
                path_kind.value(),
            });
        }

        if (!animation.channels.empty()) {
            scene.animations.push_back(std::move(animation));
        }
    }
}

bool copy_positions(const fastgltf::Asset& asset,
                    const fastgltf::Primitive& gltf_primitive,
                    PrimitiveData& primitive,
                    SceneData& scene,
                    const std::filesystem::path& path) {
    const auto* attribute = gltf_primitive.findAttribute("POSITION");
    if (attribute == gltf_primitive.attributes.end() ||
        attribute->accessorIndex >= asset.accessors.size()) {
        append_loader_error(scene,
                            "gltf.primitive_missing_position",
                            "Primitive is missing a POSITION accessor.",
                            path);
        return false;
    }

    const auto& accessor = asset.accessors[attribute->accessorIndex];
    primitive.positions.reserve(accessor.count * 3u);
    fastgltf::iterateAccessor<fastgltf::math::fvec3>(
        asset,
        accessor,
        [&](fastgltf::math::fvec3 value) {
            primitive.positions.push_back(value.x());
            primitive.positions.push_back(value.y());
            primitive.positions.push_back(value.z());
        });
    return true;
}

void copy_normals(const fastgltf::Asset& asset,
                  const fastgltf::Primitive& gltf_primitive,
                  PrimitiveData& primitive) {
    const auto* attribute = gltf_primitive.findAttribute("NORMAL");
    if (attribute == gltf_primitive.attributes.end() ||
        attribute->accessorIndex >= asset.accessors.size()) {
        return;
    }

    const auto& accessor = asset.accessors[attribute->accessorIndex];
    primitive.normals.reserve(accessor.count * 3u);
    fastgltf::iterateAccessor<fastgltf::math::fvec3>(
        asset,
        accessor,
        [&](fastgltf::math::fvec3 value) {
            primitive.normals.push_back(value.x());
            primitive.normals.push_back(value.y());
            primitive.normals.push_back(value.z());
                });
}

void copy_tangents(const fastgltf::Asset& asset,
                   const fastgltf::Primitive& gltf_primitive,
                   PrimitiveData& primitive) {
    const auto* attribute = gltf_primitive.findAttribute("TANGENT");
    if (attribute == gltf_primitive.attributes.end() ||
        attribute->accessorIndex >= asset.accessors.size()) {
        return;
    }

    const auto& accessor = asset.accessors[attribute->accessorIndex];
    primitive.tangents.reserve(accessor.count * 4u);
    fastgltf::iterateAccessor<fastgltf::math::fvec4>(
        asset,
        accessor,
        [&](fastgltf::math::fvec4 value) {
            primitive.tangents.push_back(value.x());
            primitive.tangents.push_back(value.y());
            primitive.tangents.push_back(value.z());
            primitive.tangents.push_back(value.w());
        });
}

void copy_texcoord0(const fastgltf::Asset& asset,
                    const fastgltf::Primitive& gltf_primitive,
                    PrimitiveData& primitive) {
    const auto* attribute = gltf_primitive.findAttribute("TEXCOORD_0");
    if (attribute == gltf_primitive.attributes.end() ||
        attribute->accessorIndex >= asset.accessors.size()) {
        return;
    }

    const auto& accessor = asset.accessors[attribute->accessorIndex];
    primitive.texcoord0.reserve(accessor.count * 2u);
    fastgltf::iterateAccessor<fastgltf::math::fvec2>(
        asset,
        accessor,
        [&](fastgltf::math::fvec2 value) {
            primitive.texcoord0.push_back(value.x());
            primitive.texcoord0.push_back(value.y());
                });
}

void copy_texcoord1(const fastgltf::Asset& asset,
                    const fastgltf::Primitive& gltf_primitive,
                    PrimitiveData& primitive) {
    const auto* attribute = gltf_primitive.findAttribute("TEXCOORD_1");
    if (attribute == gltf_primitive.attributes.end() ||
        attribute->accessorIndex >= asset.accessors.size()) {
        return;
    }

    const auto& accessor = asset.accessors[attribute->accessorIndex];
    primitive.texcoord1.reserve(accessor.count * 2u);
    fastgltf::iterateAccessor<fastgltf::math::fvec2>(
        asset,
        accessor,
        [&](fastgltf::math::fvec2 value) {
            primitive.texcoord1.push_back(value.x());
            primitive.texcoord1.push_back(value.y());
        });
}

void copy_color0(const fastgltf::Asset& asset,
                 const fastgltf::Primitive& gltf_primitive,
                 PrimitiveData& primitive) {
    const auto* attribute = gltf_primitive.findAttribute("COLOR_0");
    if (attribute == gltf_primitive.attributes.end() ||
        attribute->accessorIndex >= asset.accessors.size()) {
        return;
    }

    const auto& accessor = asset.accessors[attribute->accessorIndex];
    primitive.color0.reserve(accessor.count * 4u);
    fastgltf::iterateAccessor<fastgltf::math::fvec4>(
        asset,
        accessor,
        [&](fastgltf::math::fvec4 value) {
            primitive.color0.push_back(value.x());
            primitive.color0.push_back(value.y());
            primitive.color0.push_back(value.z());
            primitive.color0.push_back(value.w());
        });
}

bool copy_indices(const fastgltf::Asset& asset,
                  const fastgltf::Primitive& gltf_primitive,
                  PrimitiveData& primitive,
                  SceneData& scene,
                  const std::filesystem::path& path) {
    if (!gltf_primitive.indicesAccessor.has_value() ||
        gltf_primitive.indicesAccessor.value() >= asset.accessors.size()) {
        append_loader_error(scene,
                            "gltf.primitive_missing_indices",
                            "Primitive is missing an index accessor.",
                            path);
        return false;
    }

    const auto& accessor = asset.accessors[gltf_primitive.indicesAccessor.value()];
    primitive.indices.reserve(accessor.count);
    switch (accessor.componentType) {
        case fastgltf::ComponentType::UnsignedByte:
            fastgltf::iterateAccessor<uint8_t>(
                asset,
                accessor,
                [&](uint8_t value) {
                    primitive.indices.push_back(value);
                });
            return true;
        case fastgltf::ComponentType::UnsignedShort:
            fastgltf::iterateAccessor<uint16_t>(
                asset,
                accessor,
                [&](uint16_t value) {
                    primitive.indices.push_back(value);
                });
            return true;
        case fastgltf::ComponentType::UnsignedInt:
            fastgltf::iterateAccessor<uint32_t>(
                asset,
                accessor,
                [&](uint32_t value) {
                    primitive.indices.push_back(value);
                });
            return true;
        default:
            append_loader_error(scene,
                                "gltf.primitive_unsupported_index_type",
                                "Primitive index accessor must be unsigned byte, unsigned short, or unsigned int.",
                                path);
            return false;
    }
}

int draco_attribute_id(const fastgltf::DracoCompressedPrimitive& draco,
                       std::string_view name) {
    const auto* attribute = draco.findAttribute(name);
    if (attribute == draco.attributes.end()) {
        return -1;
    }
    return static_cast<int>(attribute->accessorIndex);
}

DracoAttributeIds draco_attribute_ids_from_gltf(
    const fastgltf::DracoCompressedPrimitive& draco) {
    DracoAttributeIds ids;
    ids.position = draco_attribute_id(draco, "POSITION");
    ids.normal = draco_attribute_id(draco, "NORMAL");
    ids.texcoord0 = draco_attribute_id(draco, "TEXCOORD_0");
    ids.texcoord1 = draco_attribute_id(draco, "TEXCOORD_1");
    ids.tangent = draco_attribute_id(draco, "TANGENT");
    ids.color0 = draco_attribute_id(draco, "COLOR_0");
    return ids;
}

bool decode_draco_primitive(const fastgltf::Asset& asset,
                            const fastgltf::Primitive& gltf_primitive,
                            PrimitiveData& primitive,
                            SceneData& scene,
                            const LoadOptions& options,
                            const std::filesystem::path& path) {
    if (!gltf_primitive.dracoCompression) {
        return false;
    }
    if (!options.allow_draco) {
        append_loader_error(scene,
                            "gltf.draco_disabled",
                            "Asset uses KHR_draco_mesh_compression, but LoadOptions::allow_draco is false.",
                            path);
        return false;
    }
    if (!options.draco_decode) {
        append_loader_error(scene,
                            "gltf.draco_not_wired",
                            "Asset uses KHR_draco_mesh_compression, but no native Draco decode callback was supplied to this loader boundary.",
                            path);
        return false;
    }

    std::vector<uint8_t> compressed_bytes;
    if (!copy_buffer_view_bytes(asset,
                                gltf_primitive.dracoCompression->bufferView,
                                compressed_bytes)) {
        append_loader_error(scene,
                            "gltf.draco_buffer_view_invalid",
                            "KHR_draco_mesh_compression references a bufferView that could not be copied.",
                            path);
        return false;
    }

    auto decoded = options.draco_decode(
        compressed_bytes.data(),
        compressed_bytes.size(),
        draco_attribute_ids_from_gltf(*gltf_primitive.dracoCompression));
    if (!decoded.decoder_available) {
        append_loader_error(scene,
                            "gltf.draco_unavailable",
                            "Asset uses KHR_draco_mesh_compression, but this build was compiled without the native Draco decoder. Rebuild with PULP_ENABLE_DRACO=ON or re-export the asset uncompressed.",
                            path);
        return false;
    }
    if (!decoded.success) {
        append_loader_error(scene,
                            "gltf.draco_decode_failed",
                            "KHR_draco_mesh_compression bytes could not be decoded by the native Draco callback.",
                            path);
        return false;
    }

    primitive.positions = std::move(decoded.positions);
    primitive.normals = std::move(decoded.normals);
    primitive.texcoord0 = std::move(decoded.texcoord0);
    primitive.texcoord1 = std::move(decoded.texcoord1);
    primitive.tangents = std::move(decoded.tangents);
    primitive.color0 = std::move(decoded.color0);
    primitive.indices = std::move(decoded.indices);
    return true;
}

void load_meshes(const fastgltf::Asset& asset,
                 SceneData& scene,
                 const LoadOptions& options,
                 const std::filesystem::path& path) {
    scene.meshes.reserve(asset.meshes.size());
    for (const auto& gltf_mesh : asset.meshes) {
        MeshData mesh;
        mesh.name = std::string(gltf_mesh.name);
        const std::string mesh_path = mesh.name.empty()
            ? "mesh[" + std::to_string(scene.meshes.size()) + "]"
            : mesh.name;
        if (!gltf_mesh.weights.empty()) {
            append_loader_unsupported(scene,
                                      "MorphWeights",
                                      "Mesh morph target default weights are parsed by glTF, but native morph target evaluation is not implemented in this slice.",
                                      mesh_path,
                                      path,
                                      "gltf.mesh_morph_weights_unsupported");
        }

        for (const auto& gltf_primitive : gltf_mesh.primitives) {
            if (!gltf_primitive.targets.empty()) {
                append_loader_unsupported(scene,
                                          "MorphTargets",
                                          "Primitive morph targets are present, but native morph target evaluation is not implemented in this slice.",
                                          mesh_path,
                                          path,
                                          "gltf.primitive_morph_targets_unsupported");
            }
            if (gltf_primitive.type != fastgltf::PrimitiveType::Triangles) {
                const auto mode = primitive_type_name(gltf_primitive.type);
                append_loader_unsupported(
                    scene,
                    "PrimitiveMode:" + mode,
                    "Primitive mode " + mode +
                        " is valid glTF, but the current native renderer slice supports indexed triangle primitives only.",
                    mesh_path,
                    path,
                    "gltf.primitive_unsupported_mode");
                continue;
            }

            PrimitiveData primitive;
            if (gltf_primitive.dracoCompression) {
                if (!decode_draco_primitive(asset,
                                            gltf_primitive,
                                            primitive,
                                            scene,
                                            options,
                                            path)) {
                    continue;
                }
            } else {
                if (!copy_positions(asset, gltf_primitive, primitive, scene, path)) {
                    continue;
                }
                copy_normals(asset, gltf_primitive, primitive);
                copy_tangents(asset, gltf_primitive, primitive);
                copy_texcoord0(asset, gltf_primitive, primitive);
                copy_texcoord1(asset, gltf_primitive, primitive);
                copy_color0(asset, gltf_primitive, primitive);
                if (!copy_indices(asset, gltf_primitive, primitive, scene, path)) {
                    continue;
                }
            }

            if (gltf_primitive.materialIndex.has_value()) {
                primitive.material = static_cast<uint32_t>(gltf_primitive.materialIndex.value());
            }

            mesh.primitives.push_back(std::move(primitive));
        }

        scene.meshes.push_back(std::move(mesh));
    }
}

void load_nodes_and_roots(const fastgltf::Asset& asset,
                          SceneData& scene,
                          const std::filesystem::path& path) {
    scene.nodes.reserve(asset.nodes.size());
    for (const auto& gltf_node : asset.nodes) {
        NodeData node;
        node.name = std::string(gltf_node.name);
        const std::string node_path = node.name.empty()
            ? "node[" + std::to_string(scene.nodes.size()) + "]"
            : node.name;
        if (gltf_node.meshIndex.has_value()) {
            node.mesh = static_cast<uint32_t>(gltf_node.meshIndex.value());
        }
        if (gltf_node.skinIndex.has_value()) {
            append_loader_unsupported(scene,
                                      "Skinning",
                                      "Node references a glTF skin, but native skinning is not implemented in this slice.",
                                      node_path,
                                      path,
                                      "gltf.node_skin_unsupported");
        }
        if (gltf_node.cameraIndex.has_value()) {
            node.camera = static_cast<uint32_t>(gltf_node.cameraIndex.value());
        }
        if (gltf_node.lightIndex.has_value()) {
            node.light = static_cast<uint32_t>(gltf_node.lightIndex.value());
        }
        if (!gltf_node.weights.empty()) {
            append_loader_unsupported(scene,
                                      "MorphWeights",
                                      "Node morph target weights are present, but native morph target evaluation is not implemented in this slice.",
                                      node_path,
                                      path,
                                      "gltf.node_morph_weights_unsupported");
        }
        if (!gltf_node.instancingAttributes.empty()) {
            append_loader_unsupported(scene,
                                      "GpuInstancing",
                                      "EXT_mesh_gpu_instancing attributes are present, but native instanced rendering is not implemented in this slice.",
                                      node_path,
                                      path,
                                      "gltf.node_instancing_unsupported");
        }
        node.children.reserve(gltf_node.children.size());
        for (size_t child : gltf_node.children) {
            node.children.push_back(static_cast<uint32_t>(child));
        }

        if (const auto* trs = std::get_if<fastgltf::TRS>(&gltf_node.transform)) {
            node.translation[0] = trs->translation.x();
            node.translation[1] = trs->translation.y();
            node.translation[2] = trs->translation.z();
            node.rotation[0] = trs->rotation.x();
            node.rotation[1] = trs->rotation.y();
            node.rotation[2] = trs->rotation.z();
            node.rotation[3] = trs->rotation.w();
            node.scale[0] = trs->scale.x();
            node.scale[1] = trs->scale.y();
            node.scale[2] = trs->scale.z();
        } else if (const auto* matrix =
                       std::get_if<fastgltf::math::fmat4x4>(&gltf_node.transform)) {
            node.has_matrix_transform = true;
            for (size_t column = 0; column < 4; ++column) {
                for (size_t row = 0; row < 4; ++row) {
                    node.matrix[column * 4u + row] = matrix->col(column)[row];
                }
            }
        }

        scene.nodes.push_back(std::move(node));
    }

    const size_t scene_index = asset.defaultScene.value_or(0);
    if (scene_index < asset.scenes.size()) {
        for (size_t root : asset.scenes[scene_index].nodeIndices) {
            scene.root_nodes.push_back(static_cast<uint32_t>(root));
        }
    }
}

void append_validation_diagnostics(SceneData& scene,
                                   const std::filesystem::path& path) {
    auto validation = validate_scene_data(scene, path.string());
    scene.diagnostics.insert(scene.diagnostics.end(),
                             std::make_move_iterator(validation.begin()),
                             std::make_move_iterator(validation.end()));
}

} // namespace

LoadResult load_gltf_scene(const std::filesystem::path& path,
                           const LoadOptions& options) {
    LoadResult result;

    auto buffer = fastgltf::GltfDataBuffer::FromPath(path);
    if (buffer.error() != fastgltf::Error::None) {
        result.error = fastgltf_error_string(buffer.error());
        append_loader_error(result.scene,
                            "gltf.read_failed",
                            result.error,
                            path);
        return result;
    }

    fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_transform |
                            fastgltf::Extensions::EXT_mesh_gpu_instancing |
                            fastgltf::Extensions::KHR_lights_punctual |
                            fastgltf::Extensions::KHR_materials_anisotropy |
                            fastgltf::Extensions::KHR_materials_clearcoat |
                            fastgltf::Extensions::KHR_materials_diffuse_transmission |
                            fastgltf::Extensions::KHR_materials_dispersion |
                            fastgltf::Extensions::KHR_materials_emissive_strength |
                            fastgltf::Extensions::KHR_materials_ior |
                            fastgltf::Extensions::KHR_materials_iridescence |
                            fastgltf::Extensions::KHR_materials_sheen |
                            fastgltf::Extensions::KHR_materials_specular |
                            fastgltf::Extensions::KHR_materials_transmission |
                            fastgltf::Extensions::KHR_materials_unlit |
                            fastgltf::Extensions::KHR_materials_volume |
                            fastgltf::Extensions::KHR_draco_mesh_compression);
    auto asset_result = parser.loadGltf(buffer.get(),
                                        path.parent_path(),
                                        fastgltf::Options::GenerateMeshIndices |
                                            fastgltf::Options::LoadExternalBuffers |
                                            fastgltf::Options::LoadExternalImages);
    if (asset_result.error() != fastgltf::Error::None) {
        result.error = fastgltf_error_string(asset_result.error());
        append_loader_error(result.scene,
                            "gltf.parse_failed",
                            result.error,
                            path);
        return result;
    }

    const auto& asset = asset_result.get();
    load_textures(asset, result.scene, path);
    load_texture_samplers(asset, result.scene);
    load_materials(asset, result.scene, path);
    load_cameras(asset, result.scene);
    load_lights(asset, result.scene);
    load_animations(asset, result.scene, path);
    load_meshes(asset, result.scene, options, path);
    load_nodes_and_roots(asset, result.scene, path);
    append_validation_diagnostics(result.scene, path);

    result.success = !has_error_diagnostics(result.scene.diagnostics);
    if (!result.success) {
        result.error = "Loaded glTF scene has structural diagnostics.";
    }
    return result;
}

} // namespace pulp::scene

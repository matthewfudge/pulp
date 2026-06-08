#include <catch2/catch_test_macros.hpp>
#include <pulp/scene/renderer3d_characterization.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

using namespace pulp::scene;

namespace {

SceneData make_two_primitive_scene() {
    SceneData scene;

    MaterialData material;
    material.name = "shared-double-sided";
    material.base_color_factor[0] = 0.85f;
    material.base_color_factor[1] = 0.45f;
    material.base_color_factor[2] = 0.20f;
    material.double_sided = true;
    scene.materials.push_back(material);

    PrimitiveData left;
    left.positions = {
        -2.0f, -1.0f, 0.0f,
        -0.4f, -1.0f, 0.0f,
        -1.2f,  1.0f, 0.0f,
    };
    left.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    left.indices = {0, 1, 2};
    left.material = 0;

    PrimitiveData right;
    right.positions = {
         0.4f, -1.0f, 0.0f,
         2.0f, -1.0f, 0.0f,
         1.2f,  1.0f, 0.0f,
    };
    right.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    right.indices = {0, 1, 2};
    right.material = 0;

    MeshData mesh;
    mesh.name = "two-triangle-mesh";
    mesh.primitives.push_back(std::move(left));
    mesh.primitives.push_back(std::move(right));
    scene.meshes.push_back(std::move(mesh));

    NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

SceneData make_sampler_route_scene() {
    SceneData scene;

    TextureData texture;
    texture.name = "encoded-base";
    texture.mime_type = "image/png";
    texture.encoded_bytes = {0x89, 0x50, 0x4e, 0x47};
    scene.textures.push_back(std::move(texture));

    TextureSamplerData sampler;
    sampler.name = "linear-mipmap-clamp";
    sampler.mag_filter = TextureSamplerData::Filter::linear;
    sampler.min_filter = TextureSamplerData::Filter::linear_mipmap_linear;
    sampler.wrap_s = TextureSamplerData::Wrap::clamp_to_edge;
    sampler.wrap_t = TextureSamplerData::Wrap::clamp_to_edge;
    scene.texture_samplers.push_back(sampler);

    MaterialData material;
    material.name = "base-texture-route";
    material.base_color_texture = 0;
    material.base_color_sampler = 0;
    material.base_color_transform.enabled = true;
    material.base_color_transform.offset[0] = 0.25f;
    material.base_color_transform.texcoord_override = 1;
    material.emissive_texture = 0;
    material.emissive_sampler = 0;
    material.emissive_texcoord = 1;
    material.emissive_transform.enabled = true;
    scene.materials.push_back(material);

    PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.texcoord1 = {
        0.25f, 0.25f,
        0.75f, 0.25f,
        0.5f, 0.75f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    MeshData mesh;
    mesh.name = "sampler-route-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

SceneData make_blended_layers_scene() {
    SceneData scene;

    MaterialData front;
    front.name = "front-blend";
    front.base_color_factor[3] = 0.45f;
    front.alpha_mode = MaterialData::AlphaMode::blend;
    front.double_sided = true;
    scene.materials.push_back(front);

    MaterialData back = front;
    back.name = "back-blend";
    scene.materials.push_back(back);

    PrimitiveData front_primitive;
    front_primitive.positions = {
        -1.0f, -1.0f,  0.35f,
         1.0f, -1.0f,  0.35f,
         0.0f,  1.0f,  0.35f,
    };
    front_primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    front_primitive.indices = {0, 1, 2};
    front_primitive.material = 0;

    PrimitiveData back_primitive;
    back_primitive.positions = {
        -1.0f, -1.0f, -0.35f,
         1.0f, -1.0f, -0.35f,
         0.0f,  1.0f, -0.35f,
    };
    back_primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    back_primitive.indices = {0, 1, 2};
    back_primitive.material = 1;

    MeshData mesh;
    mesh.name = "blended-layers";
    mesh.primitives.push_back(std::move(front_primitive));
    mesh.primitives.push_back(std::move(back_primitive));
    scene.meshes.push_back(std::move(mesh));

    NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

SceneData make_deferred_feature_scene() {
    auto scene = make_two_primitive_scene();
    scene.materials[0].advanced_material_extensions.push_back("KHR_materials_clearcoat");
    scene.unsupported_features.push_back(UnsupportedFeatureData{
        "Skinning",
        "Native skinning is deferred.",
        "SkinnedNode",
    });
    scene.unsupported_features.push_back(UnsupportedFeatureData{
        "MorphTargets",
        "Native morph targets are deferred.",
        "MorphNode",
    });
    scene.unsupported_features.push_back(UnsupportedFeatureData{
        "GpuInstancing",
        "Native GPU instancing is deferred.",
        "InstancedNode",
    });
    return scene;
}

SceneData make_animation_scene() {
    auto scene = make_two_primitive_scene();

    AnimationSamplerData sampler;
    sampler.input_times = {0.0f, 1.0f};
    sampler.output_values = {
        0.35f, 0.0f, 0.0f,
        0.75f, 0.0f, 0.0f,
    };
    sampler.output_components = 3;

    AnimationData animation;
    animation.name = "translation-initial-pose";
    animation.samplers.push_back(std::move(sampler));
    animation.channels.push_back(AnimationChannelData{
        0,
        0,
        AnimationChannelData::Path::translation,
    });
    scene.animations.push_back(std::move(animation));
    return scene;
}

SceneData make_camera_light_scene() {
    auto scene = make_two_primitive_scene();

    CameraData camera;
    camera.name = "perspective-camera";
    camera.projection = CameraData::Projection::perspective;
    camera.aspect_ratio = 1.5f;
    camera.znear = 0.25f;
    camera.zfar = 80.0f;
    scene.cameras.push_back(camera);

    LightData directional;
    directional.name = "rotated-key";
    directional.type = LightData::Type::directional;
    scene.lights.push_back(directional);

    LightData point;
    point.name = "deferred-point";
    point.type = LightData::Type::point;
    point.range = 6.0f;
    scene.lights.push_back(point);

    LightData spot;
    spot.name = "deferred-spot";
    spot.type = LightData::Type::spot;
    spot.range = 8.0f;
    spot.inner_cone_angle = 0.2f;
    spot.outer_cone_angle = 0.7f;
    scene.lights.push_back(spot);

    NodeData camera_node;
    camera_node.name = "camera";
    camera_node.camera = 0;
    camera_node.translation[2] = 3.0f;
    camera_node.rotation[1] = 0.258819f;
    camera_node.rotation[3] = 0.965926f;
    scene.nodes.push_back(std::move(camera_node));
    scene.root_nodes.push_back(static_cast<uint32_t>(scene.nodes.size() - 1u));

    NodeData light_node;
    light_node.name = "light";
    light_node.light = 0;
    light_node.rotation[0] = 0.382683f;
    light_node.rotation[3] = 0.92388f;
    scene.nodes.push_back(std::move(light_node));
    scene.root_nodes.push_back(static_cast<uint32_t>(scene.nodes.size() - 1u));

    return scene;
}

SceneData make_cycle_scene() {
    SceneData scene = make_two_primitive_scene();
    scene.nodes[0].children.push_back(1);

    NodeData child;
    child.name = "cycle";
    child.children.push_back(0);
    scene.nodes.push_back(std::move(child));
    return scene;
}

bool has_code(const std::vector<Diagnostic>& diagnostics,
              const std::string& code) {
    return std::any_of(diagnostics.begin(),
                       diagnostics.end(),
                       [&code](const Diagnostic& diagnostic) {
                           return diagnostic.code == code;
                       });
}

} // namespace

TEST_CASE("Renderer3D characterization reports render packet and pipeline intent",
          "[scene3d][renderer3d][cpu]") {
    const auto summary = characterize_renderer3d_scene(make_two_primitive_scene());

    REQUIRE(summary.success);
    REQUIRE(summary.scene_data_consumed);
    REQUIRE(summary.primitive_count == 2);
    REQUIRE(summary.primitives.size() == 2);
    CHECK(summary.pipeline_cache_entry_count == 1);
    CHECK(summary.pipeline_cache_hit_count == 1);
    CHECK(summary.base_color_factor_applied);
    CHECK(summary.metallic_roughness_factor_applied);
    CHECK(summary.double_sided_material_applied);
}

TEST_CASE("Renderer3D characterization reports texture sampler route intent",
          "[scene3d][renderer3d][cpu]") {
    const auto summary = characterize_renderer3d_scene(make_sampler_route_scene());

    REQUIRE(summary.success);
    CHECK(summary.base_color_texture_routed);
    CHECK(summary.texture_sampler_routed);
    CHECK(summary.texture_sampler_clamp_s);
    CHECK(summary.texture_sampler_clamp_t);
    CHECK(summary.texture_sampler_linear);
    CHECK(summary.texture_mipmap_filter_downgrade_intended);
    CHECK(summary.base_color_transform_routed);
    CHECK(summary.base_color_texcoord1_routed);
    CHECK(summary.emissive_texture_routed);
    CHECK(summary.non_base_color_texture_routed);
    CHECK(summary.non_base_color_texture_transform_routed);
    CHECK(summary.non_base_color_texcoord1_routed);
    REQUIRE_FALSE(summary.primitives.empty());
    CHECK(summary.primitives[0].base_color_sampler_routed);
    CHECK(summary.primitives[0].non_base_color_texture_transform_routed);
}

TEST_CASE("Renderer3D characterization reports alpha sort and deferred features",
          "[scene3d][renderer3d][cpu]") {
    const auto alpha_summary =
        characterize_renderer3d_scene(make_blended_layers_scene());
    REQUIRE(alpha_summary.success);
    CHECK(alpha_summary.alpha_blend_applied);
    CHECK(alpha_summary.alpha_blend_depth_write_disabled);
    CHECK(alpha_summary.alpha_blend_sorted);
    CHECK(alpha_summary.pipeline_cache_entry_count == 1);
    CHECK(alpha_summary.pipeline_cache_hit_count == 1);

    const auto deferred_summary =
        characterize_renderer3d_scene(make_deferred_feature_scene());
    REQUIRE(deferred_summary.success);
    CHECK(deferred_summary.advanced_material_extension_deferred);
    CHECK(deferred_summary.skinning_deferred);
    CHECK(deferred_summary.morph_target_deferred);
    CHECK(deferred_summary.gpu_instancing_deferred);
}

TEST_CASE("Renderer3D characterization applies initial transform animation pose",
          "[scene3d][renderer3d][cpu]") {
    const auto summary = characterize_renderer3d_scene(make_animation_scene());

    REQUIRE(summary.success);
    CHECK(summary.transform_animation_initial_pose_applied);
    CHECK(summary.transform_animation_deferred);
    CHECK(summary.primitive_count == 2);
}

TEST_CASE("Renderer3D characterization reports camera and light intent",
          "[scene3d][renderer3d][cpu]") {
    const auto summary = characterize_renderer3d_scene(make_camera_light_scene());

    REQUIRE(summary.success);
    CHECK(summary.directional_light_applied);
    CHECK(summary.directional_light_transform_applied);
    CHECK(summary.point_light_deferred);
    CHECK(summary.spot_light_deferred);
    CHECK(summary.punctual_light_range_deferred);
    CHECK(summary.spot_light_cone_deferred);
    CHECK(summary.perspective_camera_applied);
    CHECK(summary.camera_node_translation_applied);
    CHECK(summary.camera_node_rotation_applied);
    CHECK(summary.camera_aspect_ratio_applied);
    CHECK(summary.camera_depth_range_applied);
}

TEST_CASE("Renderer3D characterization reports graph and renderability diagnostics",
          "[scene3d][renderer3d][cpu]") {
    const auto cycle_summary = characterize_renderer3d_scene(make_cycle_scene());
    REQUIRE_FALSE(cycle_summary.success);
    CHECK(has_code(cycle_summary.diagnostics, "scene.graph_cycle"));

    SceneData empty_scene;
    const auto empty_summary = characterize_renderer3d_scene(empty_scene);
    REQUIRE_FALSE(empty_summary.success);
    CHECK(has_code(empty_summary.diagnostics,
                   "scene.renderer3d_no_renderable_primitives"));
}

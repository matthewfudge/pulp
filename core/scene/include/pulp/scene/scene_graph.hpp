#pragma once

#include <pulp/scene/scene_data.hpp>

#include <cstdint>
#include <vector>

namespace pulp::scene {

struct Mat4 {
    float m[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
};

struct TransformedPoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct RenderableNode {
    uint32_t node = invalid_scene_index;
    uint32_t mesh = invalid_scene_index;
    Mat4 world_transform;
};

struct TransformedNode {
    uint32_t node = invalid_scene_index;
    Mat4 world_transform;
};

Mat4 local_transform_for_node(const NodeData& node);
Mat4 multiply(const Mat4& a, const Mat4& b);
TransformedPoint transform_point(const Mat4& transform,
                                 float x,
                                 float y,
                                 float z);

std::vector<TransformedNode> collect_node_world_transforms(
    const SceneData& scene,
    std::vector<Diagnostic>* diagnostics = nullptr);

std::vector<RenderableNode> collect_renderable_nodes(
    const SceneData& scene,
    std::vector<Diagnostic>* diagnostics = nullptr);

} // namespace pulp::scene

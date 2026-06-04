#pragma once

#include <pulp/scene/material_key.hpp>
#include <pulp/scene/scene_graph.hpp>

#include <cstdint>
#include <vector>

namespace pulp::scene {

struct RenderPrimitive {
    uint32_t node = invalid_scene_index;
    uint32_t mesh = invalid_scene_index;
    uint32_t primitive = invalid_scene_index;
    Mat4 world_transform;
    MaterialKey material_key;
};

struct RenderPacket {
    std::vector<TransformedNode> transformed_nodes;
    std::vector<RenderPrimitive> primitives;
    std::vector<Diagnostic> diagnostics;

    bool has_errors() const {
        return has_error_diagnostics(diagnostics);
    }
};

RenderPacket build_render_packet(const SceneData& scene);

} // namespace pulp::scene

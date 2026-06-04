#pragma once

#include <pulp/scene/scene_data.hpp>

#include <cstddef>
#include <string>

namespace pulp::scene {

struct SceneStats {
    size_t nodes = 0;
    size_t root_nodes = 0;
    size_t meshes = 0;
    size_t primitives = 0;
    size_t indexed_primitives = 0;
    size_t vertices = 0;
    size_t indices = 0;
    size_t materials = 0;
    size_t textures = 0;
    size_t texture_samplers = 0;
    size_t texture_bytes = 0;
    size_t advanced_material_extensions = 0;
    size_t cameras = 0;
    size_t lights = 0;
    size_t animations = 0;
    size_t unsupported_features = 0;
    size_t diagnostics = 0;
    size_t error_diagnostics = 0;
};

SceneStats summarize_scene_data(const SceneData& scene);
std::string scene_stats_to_text(const SceneStats& stats);

} // namespace pulp::scene

#include <pulp/scene/scene_stats.hpp>

#include <sstream>

namespace pulp::scene {

SceneStats summarize_scene_data(const SceneData& scene) {
    SceneStats stats;
    stats.nodes = scene.nodes.size();
    stats.root_nodes = scene.root_nodes.size();
    stats.meshes = scene.meshes.size();
    stats.materials = scene.materials.size();
    stats.textures = scene.textures.size();
    stats.texture_samplers = scene.texture_samplers.size();
    stats.cameras = scene.cameras.size();
    stats.lights = scene.lights.size();
    stats.animations = scene.animations.size();
    stats.unsupported_features = scene.unsupported_features.size();
    stats.diagnostics = scene.diagnostics.size();

    for (const auto& mesh : scene.meshes) {
        stats.primitives += mesh.primitives.size();
        for (const auto& primitive : mesh.primitives) {
            stats.vertices += primitive.positions.size() / 3u;
            stats.indices += primitive.indices.size();
            if (!primitive.indices.empty()) {
                ++stats.indexed_primitives;
            }
        }
    }

    for (const auto& texture : scene.textures) {
        stats.texture_bytes += texture.encoded_bytes.size();
    }

    for (const auto& material : scene.materials) {
        stats.advanced_material_extensions +=
            material.advanced_material_extensions.size();
    }

    for (const auto& diagnostic : scene.diagnostics) {
        if (diagnostic.severity == Diagnostic::Severity::error) {
            ++stats.error_diagnostics;
        }
    }

    return stats;
}

std::string scene_stats_to_text(const SceneStats& stats) {
    std::ostringstream out;
    out << "nodes=" << stats.nodes
        << " roots=" << stats.root_nodes
        << " meshes=" << stats.meshes
        << " primitives=" << stats.primitives
        << " indexed_primitives=" << stats.indexed_primitives
        << " vertices=" << stats.vertices
        << " indices=" << stats.indices
        << " materials=" << stats.materials
        << " textures=" << stats.textures
        << " texture_samplers=" << stats.texture_samplers
        << " texture_bytes=" << stats.texture_bytes
        << " advanced_material_extensions=" << stats.advanced_material_extensions
        << " cameras=" << stats.cameras
        << " lights=" << stats.lights
        << " animations=" << stats.animations
        << " unsupported_features=" << stats.unsupported_features
        << " diagnostics=" << stats.diagnostics
        << " error_diagnostics=" << stats.error_diagnostics;
    return out.str();
}

} // namespace pulp::scene

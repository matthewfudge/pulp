#include <pulp/scene/render_packet.hpp>

#include <cstddef>

namespace pulp::scene {

RenderPacket build_render_packet(const SceneData& scene) {
    RenderPacket packet;

    auto validation = validate_scene_data(scene);
    packet.diagnostics.insert(packet.diagnostics.end(),
                              validation.begin(),
                              validation.end());
    if (has_error_diagnostics(packet.diagnostics)) {
        return packet;
    }

    std::vector<Diagnostic> graph_diagnostics;
    packet.transformed_nodes =
        collect_node_world_transforms(scene, &graph_diagnostics);
    packet.diagnostics.insert(packet.diagnostics.end(),
                              graph_diagnostics.begin(),
                              graph_diagnostics.end());
    if (has_error_diagnostics(packet.diagnostics)) {
        return packet;
    }

    for (const auto& transformed : packet.transformed_nodes) {
        if (!is_valid_scene_index(transformed.node, scene.nodes.size())) {
            continue;
        }

        const auto& node = scene.nodes[transformed.node];
        if (!is_valid_scene_index(node.mesh, scene.meshes.size())) {
            continue;
        }

        const auto& mesh = scene.meshes[node.mesh];
        for (size_t primitive_index = 0; primitive_index < mesh.primitives.size();
             ++primitive_index) {
            const auto& primitive = mesh.primitives[primitive_index];
            if (primitive.positions.empty() || primitive.indices.empty()) {
                continue;
            }

            packet.primitives.push_back(RenderPrimitive{
                transformed.node,
                node.mesh,
                static_cast<uint32_t>(primitive_index),
                transformed.world_transform,
                derive_material_key(scene, primitive),
            });
        }
    }

    if (packet.primitives.empty() && !scene.meshes.empty()) {
        append_diagnostic(packet.diagnostics,
                          Diagnostic::Severity::warning,
                          "scene.render_packet_empty",
                          "SceneData has no indexed primitives that can be rendered by the native renderer.");
    }

    return packet;
}

} // namespace pulp::scene

#include <pulp/scene/scene_graph.hpp>

#include <cmath>
#include <cstddef>

namespace pulp::scene {
namespace {

Mat4 identity() {
    return {};
}

void visit_node(const SceneData& scene,
                uint32_t node_index,
                const Mat4& parent_transform,
                std::vector<uint8_t>& active,
                std::vector<TransformedNode>* transformed_nodes,
                std::vector<RenderableNode>& renderables,
                std::vector<Diagnostic>* diagnostics) {
    if (!is_valid_scene_index(node_index, scene.nodes.size())) {
        if (diagnostics) {
            append_diagnostic(*diagnostics,
                              Diagnostic::Severity::error,
                              "scene.graph_node_out_of_range",
                              "Scene graph traversal reached an invalid node index.");
        }
        return;
    }
    if (active[node_index] != 0u) {
        if (diagnostics) {
            append_diagnostic(*diagnostics,
                              Diagnostic::Severity::error,
                              "scene.graph_cycle",
                              "Scene graph contains a cycle.");
        }
        return;
    }

    active[node_index] = 1u;
    const auto& node = scene.nodes[node_index];
    const auto world = multiply(parent_transform, local_transform_for_node(node));
    if (transformed_nodes != nullptr) {
        transformed_nodes->push_back(TransformedNode{
            node_index,
            world,
        });
    }
    if (is_valid_scene_index(node.mesh, scene.meshes.size())) {
        renderables.push_back(RenderableNode{
            node_index,
            node.mesh,
            world,
        });
    }

    for (uint32_t child : node.children) {
        visit_node(scene,
                   child,
                   world,
                   active,
                   transformed_nodes,
                   renderables,
                   diagnostics);
    }
    active[node_index] = 0u;
}

} // namespace

Mat4 local_transform_for_node(const NodeData& node) {
    if (node.has_matrix_transform) {
        Mat4 out;
        for (size_t i = 0; i < 16; ++i) {
            out.m[i] = node.matrix[i];
        }
        return out;
    }

    const float x = node.rotation[0];
    const float y = node.rotation[1];
    const float z = node.rotation[2];
    const float w = node.rotation[3];
    const float length = std::sqrt(x * x + y * y + z * z + w * w);
    const float qx = length > 0.0f ? x / length : 0.0f;
    const float qy = length > 0.0f ? y / length : 0.0f;
    const float qz = length > 0.0f ? z / length : 0.0f;
    const float qw = length > 0.0f ? w / length : 1.0f;

    const float xx = qx * qx;
    const float yy = qy * qy;
    const float zz = qz * qz;
    const float xy = qx * qy;
    const float xz = qx * qz;
    const float yz = qy * qz;
    const float wx = qw * qx;
    const float wy = qw * qy;
    const float wz = qw * qz;

    const float sx = node.scale[0];
    const float sy = node.scale[1];
    const float sz = node.scale[2];

    Mat4 out;
    out.m[0] = (1.0f - 2.0f * (yy + zz)) * sx;
    out.m[1] = (2.0f * (xy + wz)) * sx;
    out.m[2] = (2.0f * (xz - wy)) * sx;
    out.m[3] = 0.0f;

    out.m[4] = (2.0f * (xy - wz)) * sy;
    out.m[5] = (1.0f - 2.0f * (xx + zz)) * sy;
    out.m[6] = (2.0f * (yz + wx)) * sy;
    out.m[7] = 0.0f;

    out.m[8] = (2.0f * (xz + wy)) * sz;
    out.m[9] = (2.0f * (yz - wx)) * sz;
    out.m[10] = (1.0f - 2.0f * (xx + yy)) * sz;
    out.m[11] = 0.0f;

    out.m[12] = node.translation[0];
    out.m[13] = node.translation[1];
    out.m[14] = node.translation[2];
    out.m[15] = 1.0f;
    return out;
}

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 out;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            out.m[static_cast<size_t>(column) * 4u + static_cast<size_t>(row)] =
                a.m[0 * 4 + row] * b.m[column * 4 + 0] +
                a.m[1 * 4 + row] * b.m[column * 4 + 1] +
                a.m[2 * 4 + row] * b.m[column * 4 + 2] +
                a.m[3 * 4 + row] * b.m[column * 4 + 3];
        }
    }
    return out;
}

TransformedPoint transform_point(const Mat4& transform,
                                 float x,
                                 float y,
                                 float z) {
    return TransformedPoint{
        transform.m[0] * x + transform.m[4] * y + transform.m[8] * z + transform.m[12],
        transform.m[1] * x + transform.m[5] * y + transform.m[9] * z + transform.m[13],
        transform.m[2] * x + transform.m[6] * y + transform.m[10] * z + transform.m[14],
    };
}

std::vector<TransformedNode> collect_node_world_transforms(
    const SceneData& scene,
    std::vector<Diagnostic>* diagnostics) {
    std::vector<TransformedNode> transformed_nodes;
    std::vector<RenderableNode> ignored_renderables;
    std::vector<uint8_t> active(scene.nodes.size(), 0u);

    if (!scene.root_nodes.empty()) {
        for (uint32_t root : scene.root_nodes) {
            visit_node(scene,
                       root,
                       identity(),
                       active,
                       &transformed_nodes,
                       ignored_renderables,
                       diagnostics);
        }
        return transformed_nodes;
    }

    for (uint32_t i = 0; i < scene.nodes.size(); ++i) {
        visit_node(scene,
                   i,
                   identity(),
                   active,
                   &transformed_nodes,
                   ignored_renderables,
                   diagnostics);
    }
    return transformed_nodes;
}

std::vector<RenderableNode> collect_renderable_nodes(
    const SceneData& scene,
    std::vector<Diagnostic>* diagnostics) {
    std::vector<RenderableNode> renderables;
    std::vector<uint8_t> active(scene.nodes.size(), 0u);

    if (!scene.root_nodes.empty()) {
        for (uint32_t root : scene.root_nodes) {
            visit_node(scene,
                       root,
                       identity(),
                       active,
                       nullptr,
                       renderables,
                       diagnostics);
        }
        return renderables;
    }

    for (uint32_t i = 0; i < scene.nodes.size(); ++i) {
        visit_node(scene,
                   i,
                   identity(),
                   active,
                   nullptr,
                   renderables,
                   diagnostics);
    }
    return renderables;
}

} // namespace pulp::scene

// Signal Graph implementation
// Directed acyclic graph for audio routing between nodes.
// Topological sort determines processing order.

#include <pulp/host/signal_graph.hpp>
#include <pulp/runtime/log.hpp>
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <cstring>

namespace pulp::host {

NodeId SignalGraph::add_input_node(int channels, const std::string& name) {
    GraphNode node;
    node.id = next_id_++;
    node.type = NodeType::AudioInput;
    node.name = name;
    node.num_output_ports = channels;
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

NodeId SignalGraph::add_output_node(int channels, const std::string& name) {
    GraphNode node;
    node.id = next_id_++;
    node.type = NodeType::AudioOutput;
    node.name = name;
    node.num_input_ports = channels;
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

NodeId SignalGraph::add_plugin_node(const PluginInfo& info) {
    GraphNode node;
    node.id = next_id_++;
    node.type = NodeType::Plugin;
    node.name = info.name;
    node.num_input_ports = info.num_inputs;
    node.num_output_ports = info.num_outputs;
    node.plugin = PluginSlot::load(info);
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

NodeId SignalGraph::add_gain_node(const std::string& name) {
    GraphNode node;
    node.id = next_id_++;
    node.type = NodeType::Gain;
    node.name = name;
    node.num_input_ports = 2;
    node.num_output_ports = 2;
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

NodeId SignalGraph::add_midi_input_node(const std::string& name) {
    GraphNode node;
    node.id = next_id_++;
    node.type = NodeType::MidiInput;
    node.name = name;
    node.num_output_ports = 1;
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

NodeId SignalGraph::add_midi_output_node(const std::string& name) {
    GraphNode node;
    node.id = next_id_++;
    node.type = NodeType::MidiOutput;
    node.name = name;
    node.num_input_ports = 1;
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

bool SignalGraph::remove_node(NodeId id) {
    auto it = std::find_if(nodes_.begin(), nodes_.end(),
        [id](const GraphNode& n) { return n.id == id; });
    if (it == nodes_.end()) return false;

    // Remove all connections involving this node
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
            [id](const Connection& c) {
                return c.source_node == id || c.dest_node == id;
            }),
        connections_.end());

    nodes_.erase(it);
    return true;
}

bool SignalGraph::connect(NodeId source, PortIndex source_port,
                          NodeId dest, PortIndex dest_port) {
    // Verify nodes exist
    if (!node(source) || !node(dest)) return false;

    // Check for cycle
    if (would_create_cycle(source, dest)) return false;

    // Check for duplicate
    Connection conn{source, source_port, dest, dest_port};
    for (auto& c : connections_) {
        if (c == conn) return false;
    }

    connections_.push_back(conn);
    return true;
}

bool SignalGraph::disconnect(NodeId source, PortIndex source_port,
                             NodeId dest, PortIndex dest_port) {
    Connection target{source, source_port, dest, dest_port};
    auto it = std::find(connections_.begin(), connections_.end(), target);
    if (it == connections_.end()) return false;
    connections_.erase(it);
    return true;
}

const GraphNode* SignalGraph::node(NodeId id) const {
    for (auto& n : nodes_) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

bool SignalGraph::has_path(NodeId from, NodeId to) const {
    // BFS to check reachability
    std::unordered_set<NodeId> visited;
    std::queue<NodeId> queue;
    queue.push(from);

    while (!queue.empty()) {
        auto current = queue.front();
        queue.pop();
        if (current == to) return true;
        if (visited.count(current)) continue;
        visited.insert(current);

        for (auto& c : connections_) {
            if (c.source_node == current) {
                queue.push(c.dest_node);
            }
        }
    }
    return false;
}

bool SignalGraph::would_create_cycle(NodeId source, NodeId dest) const {
    // Adding source→dest creates a cycle if there's already a path dest→source
    return has_path(dest, source);
}

std::vector<NodeId> SignalGraph::processing_order() const {
    // Kahn's algorithm for topological sort
    std::unordered_map<NodeId, int> in_degree;
    for (auto& n : nodes_) in_degree[n.id] = 0;

    for (auto& c : connections_) {
        in_degree[c.dest_node]++;
    }

    std::queue<NodeId> queue;
    for (auto& [id, deg] : in_degree) {
        if (deg == 0) queue.push(id);
    }

    std::vector<NodeId> order;
    while (!queue.empty()) {
        auto current = queue.front();
        queue.pop();
        order.push_back(current);

        for (auto& c : connections_) {
            if (c.source_node == current) {
                if (--in_degree[c.dest_node] == 0) {
                    queue.push(c.dest_node);
                }
            }
        }
    }

    return order;
}

bool SignalGraph::prepare(double sample_rate, int max_block_size) {
    for (auto& n : nodes_) {
        if (n.plugin) {
            if (!n.plugin->prepare(sample_rate, max_block_size)) {
                runtime::log_error("SignalGraph: failed to prepare plugin '{}'", n.name);
                return false;
            }
        }
    }
    return true;
}

void SignalGraph::release() {
    for (auto& n : nodes_) {
        if (n.plugin) n.plugin->release();
    }
}

void SignalGraph::process(audio::BufferView<float>& output,
                          const audio::BufferView<const float>& input,
                          int num_samples) {
    auto order = processing_order();

    // Process each node in topological order
    // TODO: implement intermediate buffer management for proper routing
    // For now, this is a placeholder that processes the graph linearly
    for (auto id : order) {
        auto* n = node(id);
        if (!n || !n->plugin) continue;

        midi::MidiBuffer midi_in, midi_out;
        n->plugin->process(output, input, midi_in, midi_out, num_samples);
    }
}

void SignalGraph::clear() {
    connections_.clear();
    nodes_.clear();
    next_id_ = 1;
}

} // namespace pulp::host

#pragma once

// Signal Graph for Pulp Host
// A directed acyclic graph of audio processing nodes for routing audio
// between plugin slots, I/O, and utility nodes (gain, mix, split).
//
// Usage:
//   SignalGraph graph;
//   auto input = graph.add_input_node(2);
//   auto slot = graph.add_plugin_node(plugin_info);
//   auto output = graph.add_output_node(2);
//   graph.connect(input, 0, slot, 0);  // input port 0 → slot port 0
//   graph.connect(slot, 0, output, 0);
//   graph.prepare(48000, 512);
//   graph.process(output_buffer, input_buffer, num_samples);

#include <pulp/host/plugin_slot.hpp>
#include <pulp/audio/buffer.hpp>
#include <pulp/midi/buffer.hpp>
#include <memory>
#include <vector>
#include <string>
#include <cstdint>

namespace pulp::host {

// ── Node types ──────────────────────────────────────────────────────────

enum class NodeType {
    AudioInput,    // System audio input
    AudioOutput,   // System audio output
    Plugin,        // Plugin slot (VST3/AU/CLAP)
    Gain,          // Simple gain utility
    MidiInput,     // System MIDI input
    MidiOutput,    // System MIDI output
};

using NodeId = uint32_t;
using PortIndex = uint32_t;

// ── Connection ──────────────────────────────────────────────────────────

struct Connection {
    NodeId source_node;
    PortIndex source_port;
    NodeId dest_node;
    PortIndex dest_port;

    bool operator==(const Connection& o) const {
        return source_node == o.source_node && source_port == o.source_port
            && dest_node == o.dest_node && dest_port == o.dest_port;
    }
};

// ── Graph Node ──────────────────────────────────────────────────────────

struct GraphNode {
    NodeId id;
    NodeType type;
    std::string name;
    int num_input_ports = 0;
    int num_output_ports = 0;

    // For Plugin nodes, the loaded plugin slot
    std::unique_ptr<PluginSlot> plugin;
};

// ── Signal Graph ────────────────────────────────────────────────────────

class SignalGraph {
public:
    SignalGraph() = default;

    // Add nodes — returns the node ID
    NodeId add_input_node(int channels, const std::string& name = "Input");
    NodeId add_output_node(int channels, const std::string& name = "Output");
    NodeId add_plugin_node(const PluginInfo& info);
    NodeId add_gain_node(const std::string& name = "Gain");
    NodeId add_midi_input_node(const std::string& name = "MIDI In");
    NodeId add_midi_output_node(const std::string& name = "MIDI Out");

    // Remove a node and all its connections
    bool remove_node(NodeId id);

    // Connect two nodes (port-to-port)
    bool connect(NodeId source, PortIndex source_port,
                 NodeId dest, PortIndex dest_port);

    // Disconnect
    bool disconnect(NodeId source, PortIndex source_port,
                    NodeId dest, PortIndex dest_port);

    // Query
    const GraphNode* node(NodeId id) const;
    const std::vector<GraphNode>& nodes() const { return nodes_; }
    const std::vector<Connection>& connections() const { return connections_; }

    // Check if connecting would create a cycle
    bool would_create_cycle(NodeId source, NodeId dest) const;

    // Compute processing order (topological sort)
    std::vector<NodeId> processing_order() const;

    // Lifecycle
    bool prepare(double sample_rate, int max_block_size);
    void release();

    // Process one block of audio through the graph
    void process(audio::BufferView<float>& output,
                 const audio::BufferView<const float>& input,
                 int num_samples);

    // Clear all nodes and connections
    void clear();

private:
    std::vector<GraphNode> nodes_;
    std::vector<Connection> connections_;
    NodeId next_id_ = 1;

    bool has_path(NodeId from, NodeId to) const;
};

} // namespace pulp::host

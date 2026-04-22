// CLAP adapter inbound/outbound MIDI event coverage (issue-pending).
//
// This suite pins the behaviour added alongside
// feature/clap-midi-cc-coverage:
//
//   Inbound:
//     - CLAP_EVENT_MIDI    — CC, pitch bend, channel/poly aftertouch,
//                            program change decoded into MidiBuffer.
//     - CLAP_EVENT_NOTE_EXPRESSION
//                           — mapped to channel aftertouch / pitch
//                             bend / CC when the plugin opted in to
//                             MPE; dropped otherwise.
//     - CLAP_EVENT_NOTE_CHOKE
//                           — synthesised as a zero-velocity note-off
//                             at the event's sample offset.
//     - CLAP_EVENT_MIDI2    — routed straight to the UMP sidecar when
//                             the plugin opted in to UMP.
//   Outbound:
//     - midi_out → CLAP_EVENT_MIDI on out_events.
//     - midi_out sysex → CLAP_EVENT_MIDI_SYSEX on out_events.
//
// The tests build an in-memory clap_input_events_t / clap_output_events_t
// with vtables, drive pulp::format::clap_adapter::clap_process(), and
// assert that the TestProcessor saw the MIDI event (or that the output
// stream captured the right bytes). No real DAW or shared library is
// needed.

#include <catch2/catch_test_macros.hpp>

#include <pulp/format/clap_adapter.hpp>
#include <pulp/format/clap_entry.hpp>
#include <pulp/format/processor.hpp>
#include <pulp/midi/buffer.hpp>
#include <pulp/midi/message.hpp>

#include <cstring>
#include <memory>
#include <vector>

using namespace pulp;
using namespace pulp::format;

namespace {

// ── Event-stream helpers ────────────────────────────────────────────────

// Heterogeneous event stream that copies the full CLAP struct payload into
// a contiguous byte vector, so the `clap_input_events_t` get() vtable can
// hand back stable pointers to each event header.
class InputEventList {
public:
    template <typename Event>
    void push(const Event& e) {
        const auto offset = storage_.size();
        storage_.resize(offset + sizeof(Event));
        std::memcpy(storage_.data() + offset, &e, sizeof(Event));
        offsets_.push_back(offset);
    }

    clap_input_events_t make_vtable() {
        vtable_.ctx = this;
        vtable_.size = [](const clap_input_events_t* list) -> uint32_t {
            auto* self = static_cast<InputEventList*>(list->ctx);
            return static_cast<uint32_t>(self->offsets_.size());
        };
        vtable_.get = [](const clap_input_events_t* list, uint32_t idx)
            -> const clap_event_header_t* {
            auto* self = static_cast<InputEventList*>(list->ctx);
            return reinterpret_cast<const clap_event_header_t*>(
                self->storage_.data() + self->offsets_[idx]);
        };
        return vtable_;
    }

private:
    std::vector<std::uint8_t> storage_;
    std::vector<std::size_t> offsets_;
    clap_input_events_t vtable_{};
};

// Collects the pushed CLAP events as raw byte copies so the test can walk
// back over them after clap_process() returns. Sysex bodies are copied
// into `sysex_bodies_` and the stored event header's `buffer` pointer is
// rebased to point at that copy, mirroring what a real host's try_push()
// does — otherwise the adapter's local MidiBuffer data would be freed by
// the time the test inspects the event.
class OutputEventList {
public:
    clap_output_events_t make_vtable() {
        vtable_.ctx = this;
        vtable_.try_push = [](const clap_output_events_t* list,
                              const clap_event_header_t* hdr) -> bool {
            auto* self = static_cast<OutputEventList*>(list->ctx);
            std::vector<std::uint8_t> buf(hdr->size);
            std::memcpy(buf.data(), hdr, hdr->size);
            if (hdr->type == CLAP_EVENT_MIDI_SYSEX) {
                auto* sx = reinterpret_cast<const clap_event_midi_sysex_t*>(hdr);
                std::vector<std::uint8_t> body(sx->buffer, sx->buffer + sx->size);
                self->sysex_bodies_.push_back(std::move(body));
                auto* stored = reinterpret_cast<clap_event_midi_sysex_t*>(buf.data());
                stored->buffer = self->sysex_bodies_.back().data();
            }
            self->events_.push_back(std::move(buf));
            return true;
        };
        return vtable_;
    }

    std::size_t size() const { return events_.size(); }
    const clap_event_header_t* at(std::size_t i) const {
        return reinterpret_cast<const clap_event_header_t*>(events_[i].data());
    }

    // Filter by type.
    template <typename Event>
    std::vector<const Event*> by_type(uint16_t type) const {
        std::vector<const Event*> out;
        for (const auto& buf : events_) {
            auto* h = reinterpret_cast<const clap_event_header_t*>(buf.data());
            if (h->type == type) {
                out.push_back(reinterpret_cast<const Event*>(buf.data()));
            }
        }
        return out;
    }

private:
    std::vector<std::vector<std::uint8_t>> events_;
    // Owns the copied sysex payloads so their pointers stay valid for the
    // lifetime of the OutputEventList. Must be declared before events_ is
    // resized via push_back to avoid invalidation — plain vector<vector>
    // reallocates on growth, so we reserve() in the ctor.
    std::vector<std::vector<std::uint8_t>> sysex_bodies_;
    clap_output_events_t vtable_{};

public:
    OutputEventList() { sysex_bodies_.reserve(16); events_.reserve(64); }
};

// ── Test processors ─────────────────────────────────────────────────────

// Captures midi_in so the test can assert what the CLAP adapter handed up.
class CapturingProcessor : public Processor {
public:
    bool opts_mpe = false;
    bool opts_ump = false;

    // Mutable state captured each time process() runs.
    mutable midi::MidiBuffer captured_midi;
    mutable std::vector<midi::UmpEvent> captured_ump;
    mutable bool had_mpe_input = false;
    mutable bool had_ump_input = false;

    PluginDescriptor descriptor() const override {
        PluginDescriptor d;
        d.name = "CapturingCLAP";
        d.manufacturer = "PulpTest";
        d.bundle_id = "com.pulp.test.clap.capture";
        d.version = "1.0.0";
        d.accepts_midi = true;
        d.supports_mpe = opts_mpe;
        d.supports_ump = opts_ump;
        return d;
    }
    void define_parameters(state::StateStore&) override {}
    void prepare(const PrepareContext&) override {}
    void process(audio::BufferView<float>&,
                 const audio::BufferView<const float>&,
                 midi::MidiBuffer& midi_in,
                 midi::MidiBuffer&,
                 const ProcessContext&) override {
        captured_midi.clear();
        for (const auto& ev : midi_in) captured_midi.add(ev);
        for (const auto& se : midi_in.sysex()) {
            captured_midi.add_sysex(se.data, se.sample_offset, se.timestamp);
        }
        had_mpe_input = (mpe_input() != nullptr);
        had_ump_input = (ump_input() != nullptr);
        captured_ump.clear();
        if (auto* ump = ump_input()) {
            for (const auto& e : *ump) captured_ump.push_back(e);
        }
    }
};

// Emits a pre-programmed set of MIDI events on midi_out so we can test
// the outbound bridge.
class EmittingProcessor : public Processor {
public:
    std::vector<midi::MidiEvent> to_emit;
    std::vector<midi::MidiBuffer::SysexEvent> sysex_to_emit;

    PluginDescriptor descriptor() const override {
        PluginDescriptor d;
        d.name = "EmittingCLAP";
        d.manufacturer = "PulpTest";
        d.bundle_id = "com.pulp.test.clap.emit";
        d.version = "1.0.0";
        d.produces_midi = true;
        return d;
    }
    void define_parameters(state::StateStore&) override {}
    void prepare(const PrepareContext&) override {}
    void process(audio::BufferView<float>&,
                 const audio::BufferView<const float>&,
                 midi::MidiBuffer&,
                 midi::MidiBuffer& midi_out,
                 const ProcessContext&) override {
        for (const auto& ev : to_emit) midi_out.add(ev);
        for (const auto& se : sysex_to_emit) {
            midi_out.add_sysex(se.data, se.sample_offset, se.timestamp);
        }
    }
};

// Processor factory hooks have to be raw function pointers. Route through
// singletons so the tests can configure the active processor before the
// adapter instantiates one.
CapturingProcessor* g_capturing = nullptr;
EmittingProcessor*  g_emitting  = nullptr;
bool g_pending_opts_mpe = false;
bool g_pending_opts_ump = false;
std::vector<midi::MidiEvent> g_pending_emit;
std::vector<midi::MidiBuffer::SysexEvent> g_pending_sysex;

std::unique_ptr<Processor> make_capturing() {
    auto up = std::make_unique<CapturingProcessor>();
    g_capturing = up.get();
    if (g_pending_opts_mpe) up->opts_mpe = true;
    if (g_pending_opts_ump) up->opts_ump = true;
    return up;
}

std::unique_ptr<Processor> make_emitting() {
    auto up = std::make_unique<EmittingProcessor>();
    g_emitting = up.get();
    if (!g_pending_emit.empty()) up->to_emit = g_pending_emit;
    if (!g_pending_sysex.empty()) up->sysex_to_emit = g_pending_sysex;
    return up;
}

// ── Driver: spin up a PulpClapPlugin, run one process block ─────────────

struct Harness {
    static constexpr uint32_t kFrames = 64;
    static constexpr int      kChannels = 2;

    clap_adapter::PulpClapPlugin plugin;
    std::vector<float> in_left, in_right;
    std::vector<float> out_left, out_right;
    const float* in_ptrs[2]{};
    float* out_ptrs[2]{};
    clap_audio_buffer_t audio_in{}, audio_out{};

    explicit Harness(ProcessorFactory factory) {
        plugin.factory = factory;
        plugin.plugin.plugin_data = &plugin;
        REQUIRE(clap_adapter::clap_init(&plugin.plugin));
        REQUIRE(clap_adapter::clap_activate(&plugin.plugin, 48000.0, 32, kFrames));
        in_left.assign(kFrames, 0.0f);
        in_right.assign(kFrames, 0.0f);
        out_left.assign(kFrames, 0.0f);
        out_right.assign(kFrames, 0.0f);
        in_ptrs[0] = in_left.data();
        in_ptrs[1] = in_right.data();
        out_ptrs[0] = out_left.data();
        out_ptrs[1] = out_right.data();
        audio_in.data32 = const_cast<float**>(in_ptrs);
        audio_in.channel_count = kChannels;
        audio_out.data32 = out_ptrs;
        audio_out.channel_count = kChannels;
    }

    ~Harness() {
        clap_adapter::clap_deactivate(&plugin.plugin);
    }

    clap_process_status run(InputEventList& in_list,
                            OutputEventList* out_list = nullptr) {
        clap_input_events_t in_vt = in_list.make_vtable();
        clap_output_events_t out_vt{};
        if (out_list) out_vt = out_list->make_vtable();

        clap_process_t proc{};
        proc.frames_count = kFrames;
        proc.audio_inputs = &audio_in;
        proc.audio_inputs_count = 1;
        proc.audio_outputs = &audio_out;
        proc.audio_outputs_count = 1;
        proc.in_events = &in_vt;
        proc.out_events = out_list ? &out_vt : nullptr;
        return clap_adapter::clap_process(&plugin.plugin, &proc);
    }
};

clap_event_header_t make_header(uint32_t size, uint16_t type, uint32_t time) {
    clap_event_header_t h{};
    h.size = size;
    h.type = type;
    h.time = time;
    h.space_id = CLAP_CORE_EVENT_SPACE_ID;
    h.flags = 0;
    return h;
}

} // namespace

// ── Inbound: CLAP_EVENT_MIDI ────────────────────────────────────────────

TEST_CASE("CLAP_EVENT_MIDI decodes CC into MidiBuffer",
          "[clap][midi][issue-pending]") {
    g_pending_opts_mpe = false;
    g_pending_opts_ump = false;
    Harness h(make_capturing);
    REQUIRE(g_capturing != nullptr);

    InputEventList events;
    clap_event_midi_t ev{};
    ev.header = make_header(sizeof(ev), CLAP_EVENT_MIDI, 17);
    ev.port_index = 0;
    ev.data[0] = 0xB1;  // CC on channel 1
    ev.data[1] = 74;    // Brightness
    ev.data[2] = 99;
    events.push(ev);

    REQUIRE(h.run(events) == CLAP_PROCESS_CONTINUE);
    REQUIRE(g_capturing->captured_midi.size() == 1);
    const auto& got = g_capturing->captured_midi[0];
    REQUIRE(got.is_cc());
    REQUIRE(got.channel() == 1);
    REQUIRE(got.cc_number() == 74);
    REQUIRE(got.cc_value() == 99);
    REQUIRE(got.sample_offset == 17);
}

TEST_CASE("CLAP_EVENT_MIDI decodes pitch bend, program change, poly AT, channel AT",
          "[clap][midi][issue-pending]") {
    g_pending_opts_mpe = false;
    g_pending_opts_ump = false;
    Harness h(make_capturing);

    InputEventList events;
    // Pitch bend: channel 2, value 0x3FFF (roughly +max).
    clap_event_midi_t pb{};
    pb.header = make_header(sizeof(pb), CLAP_EVENT_MIDI, 4);
    pb.data[0] = 0xE2;
    pb.data[1] = 0x7F;  // LSB
    pb.data[2] = 0x7F;  // MSB
    events.push(pb);
    // Program change.
    clap_event_midi_t pc{};
    pc.header = make_header(sizeof(pc), CLAP_EVENT_MIDI, 5);
    pc.data[0] = 0xC3;
    pc.data[1] = 42;
    pc.data[2] = 0;
    events.push(pc);
    // Poly aftertouch: key 60 value 77 on channel 4.
    clap_event_midi_t poly{};
    poly.header = make_header(sizeof(poly), CLAP_EVENT_MIDI, 6);
    poly.data[0] = 0xA4;
    poly.data[1] = 60;
    poly.data[2] = 77;
    events.push(poly);
    // Channel pressure: value 33 on channel 5.
    clap_event_midi_t chat{};
    chat.header = make_header(sizeof(chat), CLAP_EVENT_MIDI, 7);
    chat.data[0] = 0xD5;
    chat.data[1] = 33;
    chat.data[2] = 0;
    events.push(chat);

    REQUIRE(h.run(events) == CLAP_PROCESS_CONTINUE);
    REQUIRE(g_capturing->captured_midi.size() == 4);

    const auto& gp = g_capturing->captured_midi[0];
    REQUIRE(gp.is_pitch_bend());
    REQUIRE(gp.channel() == 2);
    REQUIRE(gp.sample_offset == 4);
    REQUIRE(gp.data()[1] == 0x7F);
    REQUIRE(gp.data()[2] == 0x7F);

    const auto& gpc = g_capturing->captured_midi[1];
    REQUIRE(gpc.is_program_change());
    REQUIRE(gpc.channel() == 3);
    REQUIRE(gpc.data()[1] == 42);

    const auto& gpoly = g_capturing->captured_midi[2];
    REQUIRE(gpoly.channel() == 4);
    REQUIRE(gpoly.data()[0] == 0xA4);
    REQUIRE(gpoly.data()[1] == 60);
    REQUIRE(gpoly.data()[2] == 77);

    const auto& gchat = g_capturing->captured_midi[3];
    REQUIRE(gchat.channel() == 5);
    REQUIRE(gchat.data()[0] == 0xD5);
    REQUIRE(gchat.data()[1] == 33);
}

// ── Inbound: CLAP_EVENT_NOTE_CHOKE ──────────────────────────────────────

TEST_CASE("CLAP_EVENT_NOTE_CHOKE synthesises a zero-velocity note-off",
          "[clap][midi][issue-pending]") {
    g_pending_opts_mpe = false;
    g_pending_opts_ump = false;
    Harness h(make_capturing);

    InputEventList events;
    clap_event_note_t ev{};
    ev.header = make_header(sizeof(ev), CLAP_EVENT_NOTE_CHOKE, 23);
    ev.note_id = -1;
    ev.port_index = 0;
    ev.channel = 3;
    ev.key = 64;
    ev.velocity = 0.0;  // spec-ignored
    events.push(ev);

    REQUIRE(h.run(events) == CLAP_PROCESS_CONTINUE);
    REQUIRE(g_capturing->captured_midi.size() == 1);
    const auto& got = g_capturing->captured_midi[0];
    REQUIRE(got.is_note_off());
    REQUIRE(got.channel() == 3);
    REQUIRE(got.note() == 64);
    REQUIRE(got.velocity() == 0);
    REQUIRE(got.sample_offset == 23);
}

// ── Inbound: CLAP_EVENT_NOTE_EXPRESSION ─────────────────────────────────

TEST_CASE("CLAP_EVENT_NOTE_EXPRESSION pressure → channel-AT when MPE opted in",
          "[clap][midi][issue-pending]") {
    g_pending_opts_mpe = true;
    g_pending_opts_ump = false;
    Harness h(make_capturing);

    InputEventList events;
    clap_event_note_expression_t ev{};
    ev.header = make_header(sizeof(ev), CLAP_EVENT_NOTE_EXPRESSION, 11);
    ev.expression_id = CLAP_NOTE_EXPRESSION_PRESSURE;
    ev.note_id = -1;
    ev.port_index = 0;
    ev.channel = 2;
    ev.key = 72;
    ev.value = 0.5;   // → roughly 64/127
    events.push(ev);

    REQUIRE(h.run(events) == CLAP_PROCESS_CONTINUE);
    // After the MPE tracker runs, midi_in still contains the synthesised
    // channel-AT message (we deliberately don't consume it). Check for it.
    bool found = false;
    for (const auto& got : g_capturing->captured_midi) {
        if (got.data()[0] == 0xD2) {
            REQUIRE(got.sample_offset == 11);
            // 0.5 → 64 (rounded).
            REQUIRE(got.data()[1] == 64);
            found = true;
        }
    }
    REQUIRE(found);
}

TEST_CASE("CLAP_EVENT_NOTE_EXPRESSION dropped when MPE not opted in",
          "[clap][midi][issue-pending]") {
    g_pending_opts_mpe = false;
    g_pending_opts_ump = false;
    Harness h(make_capturing);

    InputEventList events;
    clap_event_note_expression_t ev{};
    ev.header = make_header(sizeof(ev), CLAP_EVENT_NOTE_EXPRESSION, 3);
    ev.expression_id = CLAP_NOTE_EXPRESSION_PRESSURE;
    ev.note_id = -1;
    ev.port_index = 0;
    ev.channel = 0;
    ev.key = 60;
    ev.value = 0.5;
    events.push(ev);

    REQUIRE(h.run(events) == CLAP_PROCESS_CONTINUE);
    // Plugin opted out of MPE — adapter should drop the expression.
    REQUIRE(g_capturing->captured_midi.empty());
}

// ── Inbound: CLAP_EVENT_MIDI2 ───────────────────────────────────────────

#if defined(CLAP_VERSION_GE) && CLAP_VERSION_GE(1, 1, 0)
TEST_CASE("CLAP_EVENT_MIDI2 routed straight to UMP sidecar when opted in",
          "[clap][midi][issue-pending]") {
    g_pending_opts_mpe = false;
    g_pending_opts_ump = true;
    Harness h(make_capturing);

    InputEventList events;
    // Build a MIDI 2.0 note-on UMP packet; velocity = 0xFFFF.
    auto packet = midi::UmpPacket::note_on_2(/*group*/0, /*channel*/1,
                                              /*note*/60, /*vel16*/0xFFFF);
    clap_event_midi2_t ev{};
    ev.header = make_header(sizeof(ev), CLAP_EVENT_MIDI2, 13);
    ev.port_index = 0;
    ev.data[0] = packet.words[0];
    ev.data[1] = packet.words[1];
    ev.data[2] = 0;
    ev.data[3] = 0;
    events.push(ev);

    REQUIRE(h.run(events) == CLAP_PROCESS_CONTINUE);
    REQUIRE(g_capturing->had_ump_input);
    REQUIRE(g_capturing->captured_ump.size() == 1);
    const auto& got = g_capturing->captured_ump[0];
    REQUIRE(got.sample_offset == 13);
    REQUIRE(got.packet.message_type() == midi::UmpMessageType::Midi2ChannelVoice);
    REQUIRE(got.packet.channel() == 1);
    REQUIRE(got.packet.note_number() == 60);
    REQUIRE(got.packet.velocity_16() == 0xFFFF);
}
#endif

// ── Outbound: midi_out → CLAP_EVENT_MIDI / SYSEX ────────────────────────

TEST_CASE("midi_out CC + pitch-bend surface on CLAP out_events",
          "[clap][midi][issue-pending]") {
    g_pending_emit.clear();
    g_pending_sysex.clear();
    auto cc = midi::MidiEvent::cc(/*ch*/0, /*num*/7, /*val*/100);
    cc.sample_offset = 12;
    auto pb = midi::MidiEvent::pitch_bend(/*ch*/0, /*val*/12345);
    pb.sample_offset = 20;
    g_pending_emit = {cc, pb};

    Harness h(make_emitting);
    InputEventList in;
    OutputEventList out;
    REQUIRE(h.run(in, &out) == CLAP_PROCESS_CONTINUE);

    auto midis = out.by_type<clap_event_midi_t>(CLAP_EVENT_MIDI);
    REQUIRE(midis.size() == 2);

    // Sort guarantee — sample_offset ascending.
    REQUIRE(midis[0]->header.time == 12);
    REQUIRE(midis[0]->data[0] == 0xB0);
    REQUIRE(midis[0]->data[1] == 7);
    REQUIRE(midis[0]->data[2] == 100);

    REQUIRE(midis[1]->header.time == 20);
    REQUIRE(midis[1]->data[0] == 0xE0);
    // pitch bend low = value & 0x7F, high = (value >> 7) & 0x7F
    REQUIRE(midis[1]->data[1] == (12345 & 0x7F));
    REQUIRE(midis[1]->data[2] == ((12345 >> 7) & 0x7F));
}

TEST_CASE("midi_out sysex surfaces on CLAP out_events",
          "[clap][midi][issue-pending]") {
    g_pending_emit.clear();
    g_pending_sysex.clear();
    midi::MidiBuffer::SysexEvent se;
    se.data = {0xF0, 0x7D, 0x01, 0x02, 0xF7};
    se.sample_offset = 5;
    g_pending_sysex = {se};

    Harness h(make_emitting);
    InputEventList in;
    OutputEventList out;
    REQUIRE(h.run(in, &out) == CLAP_PROCESS_CONTINUE);

    auto sysexes = out.by_type<clap_event_midi_sysex_t>(CLAP_EVENT_MIDI_SYSEX);
    REQUIRE(sysexes.size() == 1);
    REQUIRE(sysexes[0]->header.time == 5);
    REQUIRE(sysexes[0]->size == 5);
    REQUIRE(sysexes[0]->buffer[0] == 0xF0);
    REQUIRE(sysexes[0]->buffer[4] == 0xF7);
}

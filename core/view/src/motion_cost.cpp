/// @file motion_cost.cpp
/// Implementation of motion cost attribution. The Coordinator drives
/// the attributor via three hooks called from `on_tick`:
///   note_tick_begin() — capture frame / t_seconds, reset per-tick state
///   note_trace_activity() — record which trace_ids emitted this tick
///   emit_frame() — call the probe + fan out a CostSample
///
/// All state is guarded by a single mutex; sink dispatch happens
/// outside the lock so sinks can re-enter the attributor safely.

#include <pulp/view/motion_cost.hpp>

#include <algorithm>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace pulp::view::motion {

namespace {

std::string escape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned int>(c));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

void append_provenance(std::ostringstream& ss, const Provenance& p) {
    ss << "{";
    ss << "\"source_kind\":\"" << escape_json(p.source_kind) << "\",";
    ss << "\"source_id\":\""   << escape_json(p.source_id)   << "\",";
    ss << "\"source_file\":\"" << escape_json(p.source_file) << "\",";
    ss << "\"source_line\":"   << p.source_line;
    ss << "}";
}

}  // namespace

std::string serialize_cost_sample(const CostSample& s) {
    std::ostringstream ss;
    ss << "{";
    ss << "\"kind\":\"cost\",";
    ss << "\"frame\":" << s.frame << ",";
    ss << "\"t\":" << s.t_seconds << ",";
    ss << "\"render_pass_duration_ms\":" << s.render_pass_duration_ms << ",";
    ss << "\"dirty_rect_area_px\":" << s.dirty_rect_area_px << ",";
    ss << "\"dirty_rect_count\":" << s.dirty_rect_count << ",";
    ss << "\"active_trace_ids\":[";
    for (std::size_t i = 0; i < s.active_trace_ids.size(); ++i) {
        if (i) ss << ",";
        ss << s.active_trace_ids[i];
    }
    ss << "],";
    ss << "\"active_provenance\":[";
    for (std::size_t i = 0; i < s.active_provenance.size(); ++i) {
        if (i) ss << ",";
        append_provenance(ss, s.active_provenance[i]);
    }
    ss << "]";
    ss << "}";
    return ss.str();
}

// ── CostAttributor::State ────────────────────────────────────────────

struct CostAttributor::State {
    mutable std::mutex mtx;
    bool enabled = false;
    CostProbe probe;
    std::map<int, CostSink> sinks;
    int next_sink_id = 1;
    std::size_t emitted_count = 0;

    // Per-tick scratch (reset in note_tick_begin).
    std::uint64_t cur_frame = 0;
    double cur_t = 0.0;
    std::vector<int> active_ids;
    std::map<int, Provenance> provenance;
};

CostAttributor& CostAttributor::instance() {
    static CostAttributor inst;
    return inst;
}

CostAttributor::CostAttributor() : state_(std::make_unique<State>()) {}
CostAttributor::~CostAttributor() = default;

void CostAttributor::set_enabled(bool on) {
    std::lock_guard<std::mutex> lock(state_->mtx);
    state_->enabled = on;
}

bool CostAttributor::enabled() const noexcept {
    std::lock_guard<std::mutex> lock(state_->mtx);
    return state_->enabled;
}

void CostAttributor::set_probe(CostProbe probe) {
    std::lock_guard<std::mutex> lock(state_->mtx);
    state_->probe = std::move(probe);
}

int CostAttributor::add_sink(CostSink sink) {
    std::lock_guard<std::mutex> lock(state_->mtx);
    const int id = state_->next_sink_id++;
    state_->sinks.emplace(id, std::move(sink));
    return id;
}

void CostAttributor::remove_sink(int id) {
    std::lock_guard<std::mutex> lock(state_->mtx);
    state_->sinks.erase(id);
}

void CostAttributor::clear_sinks() {
    std::lock_guard<std::mutex> lock(state_->mtx);
    state_->sinks.clear();
}

void CostAttributor::reset() {
    std::lock_guard<std::mutex> lock(state_->mtx);
    state_->enabled = false;
    state_->probe = nullptr;
    state_->sinks.clear();
    state_->next_sink_id = 1;
    state_->emitted_count = 0;
    state_->cur_frame = 0;
    state_->cur_t = 0.0;
    state_->active_ids.clear();
    state_->provenance.clear();
}

std::size_t CostAttributor::emitted_sample_count() const noexcept {
    std::lock_guard<std::mutex> lock(state_->mtx);
    return state_->emitted_count;
}

void CostAttributor::note_tick_begin(std::uint64_t frame, double t_seconds) {
    std::lock_guard<std::mutex> lock(state_->mtx);
    state_->cur_frame = frame;
    state_->cur_t = t_seconds;
    state_->active_ids.clear();
    state_->provenance.clear();
}

void CostAttributor::note_trace_activity(int trace_id) {
    if (trace_id <= 0) return;  // 0 = publish channel, ignore.
    std::lock_guard<std::mutex> lock(state_->mtx);
    auto& ids = state_->active_ids;
    if (std::find(ids.begin(), ids.end(), trace_id) == ids.end()) {
        ids.push_back(trace_id);
    }
}

void CostAttributor::note_provenance(int trace_id, const Provenance& prov) {
    if (trace_id <= 0) return;
    std::lock_guard<std::mutex> lock(state_->mtx);
    state_->provenance[trace_id] = prov;
}

void CostAttributor::emit_frame() {
    CostSample sample;
    std::vector<CostSink> sinks_snapshot;
    CostProbe probe_snapshot;
    {
        std::lock_guard<std::mutex> lock(state_->mtx);
        if (!state_->enabled || state_->sinks.empty()) {
            // Still clear per-tick state for next frame.
            state_->active_ids.clear();
            state_->provenance.clear();
            return;
        }
        sample.frame = state_->cur_frame;
        sample.t_seconds = state_->cur_t;
        sample.active_trace_ids = state_->active_ids;
        std::sort(sample.active_trace_ids.begin(),
                  sample.active_trace_ids.end());
        sample.active_provenance.reserve(sample.active_trace_ids.size());
        for (int id : sample.active_trace_ids) {
            auto it = state_->provenance.find(id);
            if (it != state_->provenance.end()) {
                sample.active_provenance.push_back(it->second);
            } else {
                sample.active_provenance.push_back({});
            }
        }
        probe_snapshot = state_->probe;
        for (const auto& [sid, sink] : state_->sinks) {
            (void)sid;
            sinks_snapshot.push_back(sink);
        }
        state_->emitted_count++;
        // Reset per-tick state under the lock so re-entrancy from a
        // sink can't observe last-tick activity.
        state_->active_ids.clear();
        state_->provenance.clear();
    }

    // Probe call happens outside the lock — keep it cheap, but
    // implementations are free to do whatever they need without
    // contending with sink registration.
    if (probe_snapshot) {
        RenderCostSnapshot snap = probe_snapshot();
        sample.render_pass_duration_ms = snap.render_pass_duration_ms;
        sample.dirty_rect_area_px = snap.dirty_rect_area_px;
        sample.dirty_rect_count = snap.dirty_rect_count;
    }

    for (const auto& sink : sinks_snapshot) {
        if (sink) sink(sample);
    }
}

// ── Sinks ────────────────────────────────────────────────────────────

CostAttributor::CostSink make_cost_buffer_sink(std::vector<CostSample>* buf) {
    return [buf](const CostSample& s) {
        if (buf) buf->push_back(s);
    };
}

CostAttributor::CostSink make_cost_sink(std::string path) {
    // The file is opened lazily inside a shared_ptr-captured state so
    // the destructor closes the stream when the sink is dropped.
    struct File {
        std::ofstream out;
        bool header_written = false;
    };
    auto state = std::make_shared<File>();
    auto p = std::make_shared<std::string>(std::move(path));
    return [state, p](const CostSample& s) {
        if (!state->header_written) {
            state->out.open(*p, std::ios::out | std::ios::trunc);
            if (!state->out.is_open()) return;
            state->out << "{\"motion_cost_version\":"
                       << kCostSchemaVersion << "}\n";
            state->header_written = true;
        }
        if (!state->out.is_open()) return;
        state->out << serialize_cost_sample(s) << "\n";
        state->out.flush();
    };
}

std::vector<CostSample> load_cost_stream(const std::string& path) {
    std::vector<CostSample> out;
    std::ifstream in(path);
    if (!in.is_open()) return out;

    std::string line;
    bool header_seen = false;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (!header_seen) {
            // Header must be on the first non-empty line.
            // Defensive: if it isn't there, abandon parsing — unknown
            // format.
            if (line.find("\"motion_cost_version\"") == std::string::npos) {
                return {};
            }
            // Only schema version 1 supported today.
            if (line.find("\"motion_cost_version\":1") == std::string::npos) {
                return {};
            }
            header_seen = true;
            continue;
        }

        // Hand-rolled minimal parser for the fields we emit. Robust
        // JSON parsing isn't needed for round-tripping our own
        // output; if callers need that, they should plumb choc::json
        // at the inspector layer.
        CostSample s;
        auto find_num = [&](const std::string& key, double& dst) {
            auto pos = line.find("\"" + key + "\":");
            if (pos == std::string::npos) return;
            pos += key.size() + 3;
            char* end = nullptr;
            dst = std::strtod(line.c_str() + pos, &end);
        };
        double frame_d = 0;
        find_num("frame", frame_d);
        s.frame = static_cast<std::uint64_t>(frame_d);
        find_num("t", s.t_seconds);
        find_num("render_pass_duration_ms", s.render_pass_duration_ms);
        find_num("dirty_rect_area_px", s.dirty_rect_area_px);
        double drc = 0;
        find_num("dirty_rect_count", drc);
        s.dirty_rect_count = static_cast<int>(drc);

        // active_trace_ids
        {
            auto pos = line.find("\"active_trace_ids\":[");
            if (pos != std::string::npos) {
                pos += sizeof("\"active_trace_ids\":[") - 1;
                auto end = line.find(']', pos);
                std::string inside = line.substr(pos, end - pos);
                std::stringstream ss(inside);
                std::string tok;
                while (std::getline(ss, tok, ',')) {
                    if (tok.empty()) continue;
                    s.active_trace_ids.push_back(std::atoi(tok.c_str()));
                }
            }
        }

        // active_provenance — minimal scan: pull each {...} object.
        {
            auto pos = line.find("\"active_provenance\":[");
            if (pos != std::string::npos) {
                pos += sizeof("\"active_provenance\":[") - 1;
                while (pos < line.size() && line[pos] != ']') {
                    if (line[pos] != '{') { ++pos; continue; }
                    auto obj_end = line.find('}', pos);
                    if (obj_end == std::string::npos) break;
                    std::string obj = line.substr(pos, obj_end - pos + 1);
                    Provenance p;
                    auto pull_str = [&](const std::string& k, std::string& dst) {
                        auto kp = obj.find("\"" + k + "\":\"");
                        if (kp == std::string::npos) return;
                        kp += k.size() + 4;
                        auto kq = obj.find('"', kp);
                        if (kq == std::string::npos) return;
                        dst = obj.substr(kp, kq - kp);
                    };
                    pull_str("source_kind", p.source_kind);
                    pull_str("source_id", p.source_id);
                    pull_str("source_file", p.source_file);
                    auto lp = obj.find("\"source_line\":");
                    if (lp != std::string::npos) {
                        p.source_line = std::atoi(obj.c_str() + lp + 14);
                    }
                    s.active_provenance.push_back(std::move(p));
                    pos = obj_end + 1;
                    if (pos < line.size() && line[pos] == ',') ++pos;
                }
            }
        }

        out.push_back(std::move(s));
    }
    return out;
}

} // namespace pulp::view::motion

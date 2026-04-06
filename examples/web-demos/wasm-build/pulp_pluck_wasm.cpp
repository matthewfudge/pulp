// PulpPluck WASM entry point
#include "pulp_pluck.hpp"
#include <pulp/format/web/wam_adapter.hpp>

static pulp::format::wam::WamProcessorBridge g_bridge(pulp::examples::create_pulp_pluck);

extern "C" {
__attribute__((used, visibility("default")))
int wam_init(double sr, int bs) { return g_bridge.initialize(sr, bs) ? 1 : 0; }
__attribute__((used, visibility("default")))
void wam_process(const float* i, float* o, int c, int f) { g_bridge.process(i, o, c, f); }
__attribute__((used, visibility("default")))
void wam_set_param(const char* id, float v) { g_bridge.set_parameter_value(id, v); }
__attribute__((used, visibility("default")))
float wam_get_param(const char* id) { return g_bridge.get_parameter_value(id); }
__attribute__((used, visibility("default")))
void wam_midi(int s, int d1, int d2, int off) { g_bridge.schedule_midi(s, d1, d2, off); }
__attribute__((used, visibility("default")))
const char* wam_descriptor() { static std::string j; j = g_bridge.descriptor().to_json(); return j.c_str(); }
}

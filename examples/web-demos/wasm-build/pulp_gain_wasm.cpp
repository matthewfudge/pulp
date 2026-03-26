// PulpGain WASM entry point — wraps PulpGain as a WAMv2 AudioWorklet module
#include "pulp_gain.hpp"
#include <pulp/format/web/wam_adapter.hpp>

// Create the global WAM bridge with PulpGain factory
static pulp::format::wam::WamProcessorBridge g_bridge(pulp::examples::create_pulp_gain);

extern "C" {

__attribute__((used, visibility("default")))
int wam_init(double sample_rate, int block_size) {
    return g_bridge.initialize(sample_rate, block_size) ? 1 : 0;
}

__attribute__((used, visibility("default")))
void wam_process(const float* input, float* output, int channels, int frames) {
    g_bridge.process(input, output, channels, frames);
}

__attribute__((used, visibility("default")))
void wam_set_param(const char* id, float value) {
    g_bridge.set_parameter_value(id, value);
}

__attribute__((used, visibility("default")))
float wam_get_param(const char* id) {
    return g_bridge.get_parameter_value(id);
}

__attribute__((used, visibility("default")))
void wam_midi(int status, int data1, int data2, int offset) {
    g_bridge.schedule_midi(status, data1, data2, offset);
}

__attribute__((used, visibility("default")))
const char* wam_descriptor() {
    static std::string json;
    json = g_bridge.descriptor().to_json();
    return json.c_str();
}

} // extern "C"

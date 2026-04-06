// Plugin Slot stub implementation
// Full implementation requires loading format-specific plugin binaries
// (VST3 via IPluginFactory, CLAP via clap_entry, AU via AudioComponent).
// This stub returns nullptr — real loading is a future deliverable.

#include <pulp/host/plugin_slot.hpp>
#include <pulp/runtime/log.hpp>

namespace pulp::host {

std::unique_ptr<PluginSlot> PluginSlot::load(const PluginInfo& info) {
    // TODO: implement format-specific plugin loading
    // - VST3: dlopen → GetPluginFactory → createInstance
    // - CLAP: dlopen → clap_entry → create_plugin
    // - AU: AudioComponentFindNext → AudioComponentInstanceNew
    // - LV2: lilv world scan → instantiate
    runtime::log_info("PluginSlot::load(): stub for '{}' ({})",
                      info.name, info.path);
    return nullptr;
}

} // namespace pulp::host

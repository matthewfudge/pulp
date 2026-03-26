#pragma once

// Plugin Scanner for Pulp Host
// Scans standard system directories for VST3, AU, CLAP, and LV2 plugins.
// Returns a list of PluginInfo descriptors that can be used to load plugins.
//
// Usage:
//   auto scanner = PluginScanner();
//   auto plugins = scanner.scan();
//   for (auto& p : plugins) { ... p.name, p.path, p.format ... }

#include <string>
#include <vector>
#include <functional>

namespace pulp::host {

// ── Plugin format identifier ────────────────────────────────────────────

enum class PluginFormat {
    VST3,
    AudioUnit,     // AU v2 (macOS)
    AudioUnitV3,   // AUv3 (macOS/iOS)
    CLAP,
    LV2,
};

// ── Plugin descriptor from scanning ─────────────────────────────────────

struct PluginInfo {
    std::string name;
    std::string manufacturer;
    std::string version;
    std::string path;          // Full path to the plugin bundle/binary
    std::string unique_id;     // Format-specific unique ID (FUID, bundle ID, etc.)
    PluginFormat format;
    bool is_instrument = false;
    bool is_effect = true;
    int num_inputs = 2;
    int num_outputs = 2;
};

// ── Scanner configuration ───────────────────────────────────────────────

struct ScanOptions {
    bool scan_vst3 = true;
    bool scan_au = true;
    bool scan_clap = true;
    bool scan_lv2 = true;
    std::vector<std::string> extra_paths;  // Additional scan directories

    // Callback for progress reporting during scan
    using ProgressCallback = std::function<void(const std::string& current_path, int scanned, int total)>;
    ProgressCallback on_progress;
};

// ── Plugin Scanner ──────────────────────────────────────────────────────

class PluginScanner {
public:
    PluginScanner() = default;

    // Scan all standard plugin directories and return found plugins.
    // This performs I/O and may take seconds on large collections.
    std::vector<PluginInfo> scan(const ScanOptions& options = {});

    // Get default scan paths for a specific format on the current platform
    static std::vector<std::string> default_paths(PluginFormat format);

    // Quick check if a specific path looks like a valid plugin bundle
    static bool is_plugin_bundle(const std::string& path, PluginFormat format);

private:
    std::vector<PluginInfo> scan_directory(const std::string& dir, PluginFormat format);
    PluginInfo scan_vst3_bundle(const std::string& path);
    PluginInfo scan_clap_bundle(const std::string& path);
    PluginInfo scan_lv2_bundle(const std::string& path);
#ifdef __APPLE__
    std::vector<PluginInfo> scan_audio_units();
#endif
};

} // namespace pulp::host

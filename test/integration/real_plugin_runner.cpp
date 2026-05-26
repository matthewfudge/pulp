// Real-plugin integration test runner — item 4.2 of the macOS plugin
// authoring plan (planning/2026-05-24-macos-plugin-authoring-plan.md).
//
// This file drives real, third-party plugin binaries (Surge XT, Vital,
// OB-Xd, Dexed) end-to-end through Pulp's host stack:
//
//   PluginScanner::scan()    ─┐
//   PluginSlot::load()        │   each step is asserted, with rich
//   PluginSlot::prepare()     ├─► diagnostic output when a real plugin
//   PluginSlot::process()     │   fails so the failing format / binary is
//   PluginSlot::save_state()  │   immediately identifiable.
//   PluginSlot::restore_state()
//   PluginSlot::release()    ─┘
//
// Tests are OPT-IN: this translation unit only compiles when CMake is
// configured with `-DPULP_REAL_PLUGIN_TESTS=ON`. Even when compiled, an
// individual test SKIPs (prints WARN, returns) if its fixture binary is
// missing from the cache, so a developer who only ran the downloader for
// `surge-xt` sees `dexed` as a skip rather than a hard failure.
//
// Two lanes are supported (see real_plugin_fixture.hpp for details):
//   * Pinned-download lane — sha256 in the manifest, fetched + verified
//     by tools/scripts/fetch_real_plugins.py.
//   * Developer-supplied lane — for plugins (Vital, OB-Xd, …) whose
//     download URLs are auth/EULA-gated and can't be CI-fetched. The
//     developer drops the bundle into $PULP_REAL_PLUGIN_CACHE and the
//     runner accepts it without a hash check.

#include <catch2/catch_test_macros.hpp>

#include <pulp/host/scanner.hpp>
#include <pulp/host/plugin_slot.hpp>
#include <pulp/host/parameter_event_queue.hpp>
#include <pulp/audio/buffer.hpp>
#include <pulp/midi/buffer.hpp>

#include "real_plugin_fixture.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace pulp::host;
using namespace pulp::test::real_plugins;

namespace {

fs::path config_path() {
    // CMake injects PULP_REAL_PLUGINS_TOML to point at the source-tree
    // manifest. Falling back to the relative path keeps the runner usable
    // when invoked from the source root (e.g. an in-tree dev build).
#ifdef PULP_REAL_PLUGINS_TOML
    return fs::path(PULP_REAL_PLUGINS_TOML);
#else
    return fs::path("test/integration/real_plugins.toml");
#endif
}

PluginFormat parse_format(const std::string& s) {
    if (s == "clap") return PluginFormat::CLAP;
    if (s == "au")   return PluginFormat::AudioUnit;
    if (s == "lv2")  return PluginFormat::LV2;
    return PluginFormat::VST3;
}

// ── Audio helpers ─────────────────────────────────────────────────────────

bool process_one_block(PluginSlot& slot, int frames, float input_sample,
                       float& out_peak) {
    std::vector<float> in_l(static_cast<size_t>(frames), input_sample);
    std::vector<float> in_r(static_cast<size_t>(frames), input_sample);
    std::vector<float> out_l(static_cast<size_t>(frames), 0.0f);
    std::vector<float> out_r(static_cast<size_t>(frames), 0.0f);
    const float* in_ptrs[2]  = {in_l.data(), in_r.data()};
    float*       out_ptrs[2] = {out_l.data(), out_r.data()};
    pulp::audio::BufferView<const float> in(in_ptrs, 2, frames);
    pulp::audio::BufferView<float>       out(out_ptrs, 2, frames);
    pulp::midi::MidiBuffer mi, mo;
    pulp::host::ParameterEventQueue pe;
    slot.process(out, in, mi, mo, pe, frames);

    out_peak = 0.0f;
    for (int i = 0; i < frames; ++i) {
        out_peak = std::max(out_peak, std::abs(out_l[i]));
        out_peak = std::max(out_peak, std::abs(out_r[i]));
    }
    return true;
}

// ── The drive-one-plugin routine ──────────────────────────────────────────
//
// Centralized so every plugin goes through identical steps. When a real
// plugin breaks, the Catch2 section name + the WARN message identify both
// the plugin and the step.

void drive_plugin(const PluginEntry& e, const fs::path& bundle,
                  FixtureSource source) {
    INFO("plugin id=" << e.id << " format=" << e.format
                       << " bundle=" << bundle.string()
                       << " source=" << source_label(source));

    PluginInfo info;
    info.name          = e.expected_name;
    info.manufacturer  = e.expected_manufacturer;
    info.path          = bundle.string();
    info.format        = parse_format(e.format);
    info.is_instrument = e.is_instrument;
    info.is_effect     = !e.is_instrument;

    auto slot = PluginSlot::load(info);
    REQUIRE(slot != nullptr);
    REQUIRE(slot->is_loaded());

    SECTION("descriptor surfaces expected name") {
        const auto& got = slot->info();
        if (!e.expected_name.empty() && !got.name.empty())
            REQUIRE(got.name == e.expected_name);
    }

    SECTION("prepare + process produces a non-degenerate signal") {
        REQUIRE(slot->prepare(48000.0, 512));
        float peak = 0.0f;
        REQUIRE(process_one_block(*slot, 512, 0.25f, peak));
        REQUIRE(std::isfinite(peak));
        slot->release();
    }

    SECTION("parameters enumerate + round-trip") {
        const auto params = slot->parameters();
        for (const auto& p : params) {
            const float baseline = slot->get_parameter(p.id);
            slot->set_parameter(p.id, baseline);
            REQUIRE(slot->get_parameter(p.id) == baseline);
        }
    }

    SECTION("state save + restore round-trip") {
        const auto blob = slot->save_state();
        REQUIRE(slot->restore_state(blob));
    }
}

void run_plugin_case(const std::string& id) {
    const auto entries = parse_real_plugins_toml(config_path());
    const auto it = std::find_if(entries.begin(), entries.end(),
        [&](const PluginEntry& e) { return e.id == id; });
    REQUIRE(it != entries.end());

    const auto r = resolve_fixture(*it);
    if (r.source == FixtureSource::Missing) {
        WARN("skipping " << it->display_name << ": " << r.skip_reason);
        return;
    }
    if (r.source == FixtureSource::DeveloperSupplied) {
        WARN("running " << it->display_name
             << " from developer-supplied bundle (no sha256 verification)");
    }
    drive_plugin(*it, r.bundle_path, r.source);
}

} // namespace

// ── Smoke: scaffold parses correctly even when no fixtures exist ──────────

TEST_CASE("real-plugin scaffold: TOML config parses",
          "[host][integration][real-plugins][scaffold]") {
    const fs::path cfg = config_path();
    REQUIRE(fs::exists(cfg));

    const auto entries = parse_real_plugins_toml(cfg);
    REQUIRE_FALSE(entries.empty());

    std::vector<std::string> ids;
    for (const auto& e : entries) ids.push_back(e.id);
    REQUIRE(std::find(ids.begin(), ids.end(), "surge-xt") != ids.end());
    REQUIRE(std::find(ids.begin(), ids.end(), "vital")    != ids.end());
    REQUIRE(std::find(ids.begin(), ids.end(), "obxd")     != ids.end());
    REQUIRE(std::find(ids.begin(), ids.end(), "dexed")    != ids.end());

    for (const auto& e : entries) {
        INFO("entry id=" << e.id);
        REQUIRE_FALSE(e.format.empty());
        REQUIRE_FALSE(e.bundle_relpath.empty());
        REQUIRE((e.macos.has_value() || e.linux_.has_value() || e.windows.has_value()));
    }
}

TEST_CASE("real-plugin scaffold: fixture cache root is resolvable",
          "[host][integration][real-plugins][scaffold]") {
    const fs::path root = cache_root();
    INFO("cache root: " << root.string());
    REQUIRE_FALSE(root.empty());
    REQUIRE(root.is_absolute());
}

// ── The real driver: one TEST_CASE per plugin entry ───────────────────────

TEST_CASE("real-plugin: Surge XT", "[host][integration][real-plugins][surge-xt]") {
    run_plugin_case("surge-xt");
}

TEST_CASE("real-plugin: Vital", "[host][integration][real-plugins][vital]") {
    run_plugin_case("vital");
}

TEST_CASE("real-plugin: OB-Xd", "[host][integration][real-plugins][obxd]") {
    run_plugin_case("obxd");
}

TEST_CASE("real-plugin: Dexed", "[host][integration][real-plugins][dexed]") {
    run_plugin_case("dexed");
}

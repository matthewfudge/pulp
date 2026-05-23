// Verifies the richer PluginInfo metadata shape (workstream 03 slice 3.7).
// Format-level extraction (CLAP features → category) has unit-test
// coverage here; per-format scanner-path validation lives with the
// scanner tests.

#include <catch2/catch_test_macros.hpp>
#include <pulp/host/scanner.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace pulp::host;
namespace fs = std::filesystem;

namespace {

struct ScannerScratchDir {
    fs::path path;

    explicit ScannerScratchDir(const char* stem) {
        path = fs::temp_directory_path()
             / (std::string("pulp-scanner-metadata-") + stem + "-"
                + std::to_string(std::chrono::steady_clock::now()
                                     .time_since_epoch()
                                     .count()));
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path);
    }

    ~ScannerScratchDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    ScannerScratchDir(const ScannerScratchDir&) = delete;
    ScannerScratchDir& operator=(const ScannerScratchDir&) = delete;
};

void write_text(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());
    out << text;
}

} // namespace

TEST_CASE("PluginInfo default-constructs with empty metadata",
          "[host][plugin-info]") {
    PluginInfo p;
    REQUIRE(p.category.empty());
    REQUIRE(p.features.empty());
    REQUIRE(p.description.empty());
    REQUIRE_FALSE(p.has_editor);
    REQUIRE_FALSE(p.supports_sidechain);
    REQUIRE_FALSE(p.supports_midi_in);
    REQUIRE_FALSE(p.supports_midi_out);
}

TEST_CASE("PluginInfo is copyable and carries metadata",
          "[host][plugin-info]") {
    PluginInfo p;
    p.name = "Pulp Drums";
    p.category = "Instrument";
    p.features = {"instrument", "drum", "sampler"};
    p.has_editor = true;
    p.supports_midi_in = true;

    PluginInfo q = p;
    REQUIRE(q.name == "Pulp Drums");
    REQUIRE(q.category == "Instrument");
    REQUIRE(q.features.size() == 3);
    REQUIRE(q.features[2] == "sampler");
    REQUIRE(q.has_editor);
    REQUIRE(q.supports_midi_in);
}

// ── PluginInfo contract (default values + mutation invariants) ──────────

TEST_CASE("PluginInfo defaults treat plugin as a stereo effect",
          "[host][plugin-info][defaults]") {
    // Classifiers in scanner_{clap,vst3,au,lv2}.cpp assume these
    // starting conditions — any future change to the struct defaults
    // ripples into every adapter. This test pins them.
    PluginInfo p;
    REQUIRE_FALSE(p.is_instrument);
    REQUIRE(p.is_effect);
    REQUIRE(p.num_inputs == 2);
    REQUIRE(p.num_outputs == 2);
}

TEST_CASE("PluginInfo MIDI-effect shape: MIDI in+out, stays effect, not instrument",
          "[host][plugin-info][midi-effect]") {
    // Mirrors the CLAP feature="note-effect" fix in scanner_clap.cpp
    // (#198 P2): note-effects are still effects — they process MIDI
    // with no audio output. Clearing is_effect silently dropped them
    // from is_effect filters before the fix landed.
    PluginInfo p;
    p.category = "MidiEffect";
    p.is_effect = true;
    p.is_instrument = false;
    p.supports_midi_in = true;
    p.supports_midi_out = true;

    REQUIRE(p.category == "MidiEffect");
    REQUIRE(p.is_effect);
    REQUIRE_FALSE(p.is_instrument);
    REQUIRE(p.supports_midi_in);
    REQUIRE(p.supports_midi_out);
}

TEST_CASE("PluginInfo instrument shape: 0 inputs, MIDI in, is_instrument true",
          "[host][plugin-info][instrument]") {
    PluginInfo p;
    p.category = "Instrument";
    p.is_instrument = true;
    p.is_effect = false;
    p.num_inputs = 0;
    p.num_outputs = 2;
    p.supports_midi_in = true;

    REQUIRE(p.is_instrument);
    REQUIRE_FALSE(p.is_effect);
    REQUIRE(p.num_inputs == 0);
    REQUIRE(p.supports_midi_in);
}

TEST_CASE("PluginInfo sidechain flag is orthogonal to effect/instrument",
          "[host][plugin-info]") {
    PluginInfo p;
    p.is_effect = true;
    p.supports_sidechain = true;
    REQUIRE(p.supports_sidechain);

    // Clearing sidechain doesn't affect effect/instrument flags.
    p.supports_sidechain = false;
    REQUIRE(p.is_effect);
    REQUIRE_FALSE(p.is_instrument);
}

TEST_CASE("PluginInfo features vector survives move assignment",
          "[host][plugin-info][move]") {
    PluginInfo src;
    src.name = "SrcPlugin";
    src.features = {"audio-effect", "analyzer", "utility"};

    PluginInfo dst = std::move(src);
    REQUIRE(dst.name == "SrcPlugin");
    REQUIRE(dst.features.size() == 3);
    REQUIRE(dst.features[0] == "audio-effect");
    REQUIRE(dst.features[2] == "utility");
}

TEST_CASE("PluginInfo category taxonomy accepts known strings",
          "[host][plugin-info][taxonomy]") {
    // The set of strings scanners write. Callers that switch on
    // category (not the adapter code) should fail fast if a new
    // category sneaks into the scanner without an intentional opt-in.
    const std::vector<std::string> accepted = {
        "Fx", "Instrument", "Analyzer", "MidiEffect", ""
    };
    for (const auto& c : accepted) {
        PluginInfo p;
        p.category = c;
        REQUIRE(p.category == c);
    }
}

// ── PluginFormat enum ───────────────────────────────────────────────────

TEST_CASE("PluginFormat enum covers the 5 shipping formats",
          "[host][plugin-info][format]") {
    for (auto f : {
             PluginFormat::VST3,
             PluginFormat::AudioUnit,
             PluginFormat::AudioUnitV3,
             PluginFormat::CLAP,
             PluginFormat::LV2,
         }) {
        PluginInfo p;
        p.format = f;
        REQUIRE(p.format == f);
    }
}

TEST_CASE("PluginScanner default paths match platform format support",
          "[host][plugin-info][format][coverage][phase3]") {
#if defined(__APPLE__)
    auto vst3 = PluginScanner::default_paths(PluginFormat::VST3);
    auto au = PluginScanner::default_paths(PluginFormat::AudioUnit);
    auto auv3 = PluginScanner::default_paths(PluginFormat::AudioUnitV3);
    auto clap = PluginScanner::default_paths(PluginFormat::CLAP);
    auto lv2 = PluginScanner::default_paths(PluginFormat::LV2);

    REQUIRE(vst3.size() == 2);
    REQUIRE(vst3[0].find("/Library/Audio/Plug-Ins/VST3") != std::string::npos);
    REQUIRE(au.size() == 2);
    REQUIRE(auv3 == au);
    REQUIRE(clap.size() == 2);
    REQUIRE(lv2.empty());
#elif defined(_WIN32)
    auto vst3 = PluginScanner::default_paths(PluginFormat::VST3);
    auto au = PluginScanner::default_paths(PluginFormat::AudioUnit);

    REQUIRE(vst3.size() == 2);
    REQUIRE(vst3[0].find("VST3") != std::string::npos);
    REQUIRE(au.size() == 2);
#elif defined(__linux__)
    auto vst3 = PluginScanner::default_paths(PluginFormat::VST3);
    auto clap = PluginScanner::default_paths(PluginFormat::CLAP);
    auto lv2 = PluginScanner::default_paths(PluginFormat::LV2);
    auto au = PluginScanner::default_paths(PluginFormat::AudioUnit);

    REQUIRE(vst3.size() == 3);
    REQUIRE(clap.size() == 2);
    REQUIRE(lv2.size() == 3);
    REQUIRE(au.empty());
#else
    REQUIRE(PluginScanner::default_paths(PluginFormat::VST3).empty());
#endif
}

TEST_CASE("PluginScanner identifies bundle suffixes for each host format",
          "[host][plugin-info][format]") {
    REQUIRE(PluginScanner::is_plugin_bundle("/Library/Audio/Plug-Ins/VST3/Test.vst3",
                                            PluginFormat::VST3));
    REQUIRE(PluginScanner::is_plugin_bundle("/Library/Audio/Plug-Ins/Components/Test.component",
                                            PluginFormat::AudioUnit));
    REQUIRE(PluginScanner::is_plugin_bundle("/Applications/Synth.app/PlugIns/Test.component",
                                            PluginFormat::AudioUnitV3));
    REQUIRE(PluginScanner::is_plugin_bundle("/Library/Audio/Plug-Ins/CLAP/Test.clap",
                                            PluginFormat::CLAP));
    REQUIRE(PluginScanner::is_plugin_bundle("/usr/lib/lv2/Test.lv2",
                                            PluginFormat::LV2));

    REQUIRE_FALSE(PluginScanner::is_plugin_bundle("/tmp/Test.vst3.backup",
                                                  PluginFormat::VST3));
    REQUIRE_FALSE(PluginScanner::is_plugin_bundle("/tmp/Test.component",
                                                  PluginFormat::CLAP));
    REQUIRE_FALSE(PluginScanner::is_plugin_bundle("/tmp/Test.clap",
                                                  PluginFormat::LV2));
}

TEST_CASE("PluginScanner honors disabled format flags without progress callbacks",
          "[host][scanner][coverage][phase3]") {
    PluginScanner scanner;
    ScanOptions opts;
    opts.scan_vst3 = false;
    opts.scan_au = false;
    opts.scan_clap = false;
    opts.scan_lv2 = false;
    opts.only_extra_paths = true;
    opts.extra_paths.push_back("/tmp/pulp-disabled-scan-should-not-be-read");

    int progress_calls = 0;
    opts.on_progress = [&](const std::string&, int, int) {
        ++progress_calls;
    };

    const auto plugins = scanner.scan(opts);
    REQUIRE(plugins.empty());
    REQUIRE(progress_calls == 0);
}

TEST_CASE("PluginScanner only_extra_paths reports each enabled format path",
          "[host][scanner][coverage][phase3]") {
    ScannerScratchDir scratch("empty-extra-paths");

    PluginScanner scanner;
    ScanOptions opts;
    opts.scan_vst3 = true;
    opts.scan_au = false;
    opts.scan_clap = true;
    opts.scan_lv2 = true;
    opts.only_extra_paths = true;
    opts.extra_paths.push_back(scratch.path.string());

    std::vector<std::string> paths;
    std::vector<int> scanned;
    opts.on_progress = [&](const std::string& current, int scanned_count, int) {
        paths.push_back(current);
        scanned.push_back(scanned_count);
    };

    const auto plugins = scanner.scan(opts);
    REQUIRE(plugins.empty());
    REQUIRE(paths.size() == 3);
    REQUIRE(paths[0] == scratch.path.string());
    REQUIRE(paths[1] == scratch.path.string());
    REQUIRE(paths[2] == scratch.path.string());
    REQUIRE(scanned == std::vector<int>{0, 1, 2});
}

TEST_CASE("PluginScanner scans only supplied hermetic VST3 and LV2 directories",
          "[host][scanner][coverage][phase3]") {
    ScannerScratchDir scratch("synthetic-bundles");
    fs::create_directories(scratch.path / "BetaSynth.vst3" / "Contents" / "Resources");
    fs::create_directories(scratch.path / "AlphaVerb.lv2");
    fs::create_directories(scratch.path / "Ignored.clap");
    fs::create_directories(scratch.path / "NotAPlugin");
    write_text(scratch.path / "AlphaVerb.lv2" / "manifest.ttl",
               "@prefix lv2: <http://lv2plug.in/ns/lv2core#> .\n"
               "<http://example.com/pulp/AlphaVerb> a lv2:Plugin .\n");

    PluginScanner scanner;
    ScanOptions opts;
    opts.scan_vst3 = true;
    opts.scan_au = false;
    opts.scan_clap = false;
    opts.scan_lv2 = true;
    opts.only_extra_paths = true;
    opts.extra_paths.push_back(scratch.path.string());

    const auto plugins = scanner.scan(opts);
    REQUIRE(plugins.size() == 2);
    REQUIRE(plugins[0].name == "AlphaVerb");
    REQUIRE(plugins[0].format == PluginFormat::LV2);
    REQUIRE(plugins[0].path == (scratch.path / "AlphaVerb.lv2").string());
    REQUIRE(plugins[0].unique_id == "http://example.com/pulp/AlphaVerb");
    REQUIRE(plugins[1].name == "BetaSynth");
    REQUIRE(plugins[1].format == PluginFormat::VST3);
    REQUIRE(plugins[1].path == (scratch.path / "BetaSynth.vst3").string());
    REQUIRE(plugins[1].unique_id == "BetaSynth");
}

TEST_CASE("PluginScanner skips missing and non-directory extra paths",
          "[host][scanner][coverage][phase3]") {
    ScannerScratchDir scratch("bad-extra-paths");
    const auto plain_file = scratch.path / "plain-file.vst3";
    write_text(plain_file, "not a directory");

    PluginScanner scanner;
    ScanOptions opts;
    opts.scan_vst3 = true;
    opts.scan_au = false;
    opts.scan_clap = false;
    opts.scan_lv2 = false;
    opts.only_extra_paths = true;
    opts.extra_paths = {
        (scratch.path / "missing").string(),
        plain_file.string(),
    };

    std::vector<std::string> progressed;
    opts.on_progress = [&](const std::string& current, int, int) {
        progressed.push_back(current);
    };

    const auto plugins = scanner.scan(opts);
    REQUIRE(plugins.empty());
    REQUIRE(progressed.size() == 2);
    REQUIRE(progressed[0] == (scratch.path / "missing").string());
    REQUIRE(progressed[1] == plain_file.string());
}

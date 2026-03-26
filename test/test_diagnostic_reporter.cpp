#include <catch2/catch_test_macros.hpp>
#include <pulp/format/diagnostic_reporter.hpp>
#include <pulp/state/store.hpp>

using namespace pulp::format;
using namespace pulp::state;

static PluginDescriptor make_desc() {
    PluginDescriptor d;
    d.name = "TestPlugin";
    d.manufacturer = "TestCo";
    d.version = "1.0.0";
    d.bundle_id = "com.testco.testplugin";
    d.category = PluginCategory::Effect;
    return d;
}

static void setup_store(StateStore& s) {
    s.add_parameter({.id = 1, .name = "Gain", .unit = "dB", .range = {-60, 12, 0}});
    s.add_parameter({.id = 2, .name = "Mix", .unit = "%", .range = {0, 100, 50}});
}

TEST_CASE("DiagnosticReporter generates text report", "[format][diagnostic]") {
    DiagnosticReporter diag;
    auto desc = make_desc();
    StateStore store;
    setup_store(store);
    store.set_value(1, -6.0f);

    diag.set_plugin_info(desc, store);
    diag.set_audio_config(44100, 512, 2, 2);

    auto report = diag.generate_report();

    REQUIRE(report.find("Pulp Diagnostic Report") != std::string::npos);
    REQUIRE(report.find("TestPlugin") != std::string::npos);
    REQUIRE(report.find("TestCo") != std::string::npos);
    REQUIRE(report.find("44100") != std::string::npos);
    REQUIRE(report.find("512") != std::string::npos);
    REQUIRE(report.find("Gain") != std::string::npos);
    REQUIRE(report.find("Effect") != std::string::npos);
}

TEST_CASE("DiagnosticReporter generates JSON report", "[format][diagnostic]") {
    DiagnosticReporter diag;
    auto desc = make_desc();
    StateStore store;
    setup_store(store);

    diag.set_plugin_info(desc, store);
    diag.set_audio_config(48000, 256, 2, 2);

    auto json = diag.generate_json();

    REQUIRE(json.find("\"name\": \"TestPlugin\"") != std::string::npos);
    REQUIRE(json.find("\"sampleRate\": 48000") != std::string::npos);
    REQUIRE(json.find("\"parameters\"") != std::string::npos);
    REQUIRE(json.find("\"Gain\"") != std::string::npos);
}

TEST_CASE("DiagnosticReporter includes format and host", "[format][diagnostic]") {
    DiagnosticReporter diag;
    auto desc = make_desc();
    StateStore store;
    setup_store(store);

    diag.set_plugin_info(desc, store);
    diag.set_audio_config(44100, 512, 2, 2);
    diag.set_format_name("CLAP");
    diag.set_host_name("Bitwig Studio 5.2");

    auto report = diag.generate_report();
    REQUIRE(report.find("Format: CLAP") != std::string::npos);
    REQUIRE(report.find("Host: Bitwig Studio 5.2") != std::string::npos);
}

TEST_CASE("DiagnosticReporter includes CPU load", "[format][diagnostic]") {
    DiagnosticReporter diag;
    auto desc = make_desc();
    StateStore store;
    setup_store(store);

    diag.set_plugin_info(desc, store);
    diag.set_audio_config(44100, 512, 2, 2);
    diag.set_cpu_load(0.15f, 0.25f);

    auto report = diag.generate_report();
    REQUIRE(report.find("CPU Load: 15") != std::string::npos);
    REQUIRE(report.find("CPU Peak: 25") != std::string::npos);
}

TEST_CASE("DiagnosticReporter custom entries", "[format][diagnostic]") {
    DiagnosticReporter diag;
    auto desc = make_desc();
    StateStore store;
    setup_store(store);

    diag.set_plugin_info(desc, store);
    diag.set_audio_config(44100, 512, 2, 2);
    diag.add_custom_entry("GPU", "Apple M1 Pro");
    diag.add_custom_entry("Driver", "CoreAudio");

    auto report = diag.generate_report();
    REQUIRE(report.find("GPU: Apple M1 Pro") != std::string::npos);
    REQUIRE(report.find("Driver: CoreAudio") != std::string::npos);
}

TEST_CASE("DiagnosticReporter captures parameter values", "[format][diagnostic]") {
    DiagnosticReporter diag;
    auto desc = make_desc();
    StateStore store;
    setup_store(store);
    store.set_value(1, -12.0f);
    store.set_value(2, 75.0f);

    diag.set_plugin_info(desc, store);
    diag.set_audio_config(44100, 512, 2, 2);

    auto report = diag.generate_report();
    REQUIRE(report.find("-12") != std::string::npos);
    REQUIRE(report.find("75") != std::string::npos);
}

TEST_CASE("DiagnosticReporter instrument type", "[format][diagnostic]") {
    DiagnosticReporter diag;
    PluginDescriptor desc;
    desc.name = "MySynth";
    desc.manufacturer = "Co";
    desc.version = "1.0";
    desc.category = PluginCategory::Instrument;

    StateStore store;
    diag.set_plugin_info(desc, store);
    diag.set_audio_config(44100, 512, 0, 2);

    auto report = diag.generate_report();
    REQUIRE(report.find("Instrument") != std::string::npos);
}

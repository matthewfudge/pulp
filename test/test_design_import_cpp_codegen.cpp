#include "fixtures/design_import_generated_cpp_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <choc/text/choc_JSON.h>
#include <pulp/platform/child_process.hpp>
#include <pulp/view/design_import.hpp>
#include <pulp/view/layout_snapshot.hpp>
#include <pulp/view/view.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace pulp::view;
namespace fs = std::filesystem;

#ifndef PULP_TEST_CXX_COMPILER
#define PULP_TEST_CXX_COMPILER ""
#endif

#ifndef PULP_REPO_ROOT
#define PULP_REPO_ROOT ""
#endif

namespace {

class TempDir {
public:
    explicit TempDir(const std::string& prefix) {
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() / (prefix + "-" + std::to_string(tick));
        fs::create_directories(path);
    }

    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    fs::path path;
};

void write_text(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path);
    REQUIRE(out.is_open());
    out << text;
    REQUIRE(out.good());
}

std::string read_text(const fs::path& path) {
    std::ifstream in(path);
    REQUIRE(in.is_open());
    std::ostringstream ss;
    ss << in.rdbuf();
    REQUIRE((in.good() || in.eof()));
    return ss.str();
}

IRNode label_node(std::string id,
                  std::string text,
                  float width,
                  float height) {
    IRNode node;
    node.type = "text";
    node.name = id;
    node.stable_anchor_id = std::move(id);
    node.text_content = std::move(text);
    node.style.width = width;
    node.style.height = height;
    return node;
}

IRNode frame_node(std::string id,
                  std::string name,
                  float width,
                  float height,
                  LayoutDirection direction) {
    IRNode node;
    node.type = "frame";
    node.name = std::move(name);
    node.stable_anchor_id = std::move(id);
    node.layout.direction = direction;
    node.style.width = width;
    node.style.height = height;
    return node;
}

std::size_t count_occurrences(std::string_view text, std::string_view needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string_view::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

DesignIR build_codegen_fixture_ir() {
    DesignIR ir;
    ir.source = DesignSource::stitch;
    ir.capture_method = "adapter_parse";
    ir.source_adapter = "phase7-codegen-test";
    ir.source_version = "1";
    ir.tokens.colors["bg.primary"] = "#112233";
    ir.tokens.colors["bg.alias"] = "#112233";
    ir.tokens.colors["semantic.surface"] = "surface-token";
    ir.tokens.dimensions["panel.width"] = 320.0f;
    ir.tokens.dimensions["panel.width.alias"] = 320.0f;
    ir.tokens.dimensions["panel.height"] = 140.0f;
    ir.tokens.strings["label.drive"] = "Drive";

    ir.root = frame_node("root", "Panel", 320.0f, 140.0f, LayoutDirection::column);
    ir.root.style.background_color = "#112233";
    ir.root.layout.gap = 10.0f;

    auto header = frame_node("header", "Header", 320.0f, 32.0f, LayoutDirection::row);
    header.layout.gap = 8.0f;
    header.children.push_back(label_node("title", "Cloud Chorus", 144.0f, 24.0f));
    header.children.push_back(label_node("badge", "Generated C++", 120.0f, 24.0f));
    ir.root.children.push_back(std::move(header));

    auto drive = frame_node("drive", "Drive", 72.0f, 72.0f, LayoutDirection::column);
    drive.audio_widget = AudioWidgetType::knob;
    drive.audio_label = "Drive";
    drive.audio_min = -60.0f;
    drive.audio_max = 0.0f;
    drive.audio_default = -15.0f;
    drive.attributes["value"] = "0.2";
    ir.root.children.push_back(std::move(drive));

    IRAssetRef asset;
    asset.asset_id = "logo";
    asset.original_uri = "logo.svg";
    asset.content_hash = "sha256:fixture";
    asset.mime = "image/svg+xml";
    ir.asset_manifest.assets.push_back(std::move(asset));
    return ir;
}

DesignIR build_phase_a_typed_control_ir() {
    DesignIR ir;
    ir.source = DesignSource::stitch;
    ir.capture_method = "phase-a-typed-ir-smoke";
    ir.source_adapter = "native-cpp-import-execution-validation";
    ir.source_version = "phase-a";
    ir.root = frame_node("phase-a-root", "Typed Control Smoke", 360.0f, 140.0f, LayoutDirection::row);
    ir.root.layout.gap = 12.0f;

    auto drive = frame_node("drive", "Drive", 72.0f, 72.0f, LayoutDirection::column);
    drive.audio_widget = AudioWidgetType::knob;
    drive.audio_label = "Drive";
    drive.audio_min = 0.0f;
    drive.audio_max = 1.0f;
    drive.audio_default = 0.4f;
    drive.attributes["value"] = "0.7";
    ir.root.children.push_back(std::move(drive));

    auto mix = frame_node("mix", "Mix", 48.0f, 96.0f, LayoutDirection::column);
    mix.audio_widget = AudioWidgetType::fader;
    mix.audio_label = "Mix";
    mix.audio_min = 0.0f;
    mix.audio_max = 1.0f;
    mix.audio_default = 0.5f;
    mix.attributes["value"] = "0.25";
    ir.root.children.push_back(std::move(mix));

    auto level = frame_node("level", "Level", 96.0f, 24.0f, LayoutDirection::column);
    level.audio_widget = AudioWidgetType::meter;
    level.audio_label = "Level";
    level.attributes["value"] = "0.62";
    level.attributes["orientation"] = "horizontal";
    ir.root.children.push_back(std::move(level));

    auto shape = frame_node("shape", "Shape", 72.0f, 72.0f, LayoutDirection::column);
    shape.audio_widget = AudioWidgetType::xy_pad;
    shape.audio_label = "Shape";
    shape.attributes["x"] = "0.3";
    shape.attributes["y"] = "0.8";
    shape.attributes["xLabel"] = "Cutoff";
    shape.attributes["yLabel"] = "Resonance";
    ir.root.children.push_back(std::move(shape));

    return ir;
}

DesignIR build_untyped_named_control_ir() {
    DesignIR ir;
    ir.source = DesignSource::stitch;
    ir.capture_method = "phase-a-negative-typed-ir-smoke";
    ir.root = frame_node("root", "Untyped Smoke", 120.0f, 80.0f, LayoutDirection::column);

    auto untyped = frame_node("gain-knob-looking-frame",
                              "GainKnob",
                              64.0f,
                              64.0f,
                              LayoutDirection::column);
    untyped.attributes["value"] = "0.5";
    ir.root.children.push_back(std::move(untyped));
    return ir;
}

DesignIR build_phase_c_chainer_one_knob_ir() {
    DesignIR ir;
    ir.source = DesignSource::jsx;
    ir.capture_method = "phase-c-chainer-one-knob";
    ir.source_adapter = "native-cpp-import-execution-validation";
    ir.source_version = "phase-c";
    ir.root = frame_node("phase-c-root", "Chainer One Knob", 80.0f, 90.0f, LayoutDirection::column);

    auto freq = frame_node("pr_2c", "freq", 52.0f, 79.0f, LayoutDirection::column);
    freq.audio_widget = AudioWidgetType::knob;
    freq.audio_label = "freq";
    freq.audio_min = 0.0f;
    freq.audio_max = 1.0f;
    freq.audio_default = 0.35f;
    freq.attributes["value"] = "0.35";
    freq.attributes["pulpRouteId"] = "chainer.knob.0.osc_freq";
    freq.attributes["pulpRouteType"] = "native_cpp";
    freq.attributes["pulpSourceFamily"] = "Knob";
    freq.attributes["pulpSourcePath"] =
        "planning/artifacts/native-ui/nv0/chainer-fixture/ChainerInstrument.jsx:550:Knob[0]";
    freq.attributes["pulpParamKey"] = "osc_freq";
    freq.attributes["pulpBindingModule"] = "OSC";
    freq.attributes["pulpBindingParam"] = "freq";
    freq.attributes["pulpEventContract"] = "onChange:set_param:osc_freq";
    freq.attributes["pulpGestureContract"] = "rotary_drag:begin/update/end";
    ir.root.children.push_back(std::move(freq));

    return ir;
}

std::string diff_messages(const LayoutTreeDiff& diff) {
    std::ostringstream out;
    for (const auto& message : diff.messages)
        out << message << '\n';
    return out.str();
}

bool compile_generated_source(const fs::path& source_path,
                              const fs::path& output_path,
                              std::string* diagnostics) {
    const fs::path compiler(PULP_TEST_CXX_COMPILER);
    if (compiler.empty() || !fs::exists(compiler)) {
        if (diagnostics != nullptr) *diagnostics = "C++ compiler path is unavailable";
        return false;
    }

    const fs::path root(PULP_REPO_ROOT);
    std::vector<std::string> include_dirs = {
        root.string(),
        (root / "core" / "view" / "include").string(),
        (root / "core" / "canvas" / "include").string(),
        (root / "core" / "runtime" / "include").string(),
        (root / "core" / "platform" / "include").string(),
        (root / "core" / "events" / "include").string(),
        (root / "core" / "state" / "include").string(),
        (root / "core" / "audio" / "include").string(),
        (root / "core" / "midi" / "include").string(),
        (root / "core" / "signal" / "include").string(),
        (root / "core" / "host" / "include").string(),
    };

    std::vector<std::string> args;
#if defined(_WIN32)
    const auto filename = compiler.filename().string();
    const bool msvc_style = filename.find("cl") != std::string::npos;
    if (msvc_style) {
        args = {"/nologo", "/std:c++20", "/EHsc"};
        for (const auto& dir : include_dirs) args.push_back("/I" + dir);
        args.push_back("/c");
        args.push_back(source_path.string());
        args.push_back("/Fo" + output_path.string());
    } else
#endif
    {
        args = {"-std=c++20"};
        for (const auto& dir : include_dirs) {
            args.push_back("-I");
            args.push_back(dir);
        }
        args.push_back("-c");
        args.push_back(source_path.string());
        args.push_back("-o");
        args.push_back(output_path.string());
    }

    auto result = pulp::platform::exec(compiler.string(), args, 30000);
    if (diagnostics != nullptr)
        *diagnostics = result.stdout_output + result.stderr_output;
    return !result.timed_out && result.exit_code == 0 && fs::exists(output_path);
}

}  // namespace

TEST_CASE("typed DesignIR smoke emits typed baked C++ controls",
          "[view][import][cpp-codegen][native-cpp-phase-a]") {
    const auto ir = build_phase_a_typed_control_ir();
    CppExportOptions opts;
    opts.header_filename = "phase_a_typed_controls.hpp";
    opts.namespace_name = "pulp::test::phase_a";

    const auto result = generate_pulp_cpp(ir, ir.asset_manifest, opts);

    REQUIRE(count_occurrences(result.source, "std::make_unique<pulp::view::Knob>()") == 1);
    REQUIRE(count_occurrences(result.source, "std::make_unique<pulp::view::Fader>()") == 1);
    REQUIRE(count_occurrences(result.source, "std::make_unique<pulp::view::Meter>()") == 1);
    REQUIRE(count_occurrences(result.source, "std::make_unique<pulp::view::XYPad>()") == 1);
    REQUIRE(result.source.find("std::make_unique<pulp::view::View>()") != std::string::npos);

    REQUIRE(result.source.find("->set_label(\"Drive\");") != std::string::npos);
    REQUIRE(result.source.find("->set_value(/* TODO: bind to param */ 0.7f);") != std::string::npos);
    REQUIRE(result.source.find("->set_default_value(0.4f);") != std::string::npos);
    REQUIRE(result.source.find("->set_label(\"Mix\");") != std::string::npos);
    REQUIRE(result.source.find("->set_value(/* TODO: bind to param */ 0.25f);") != std::string::npos);
    REQUIRE(result.source.find("->set_level(/* TODO: bind to meter */ 0.62f, 0.62f);") != std::string::npos);
    REQUIRE(result.source.find("->set_orientation(pulp::view::Meter::Orientation::horizontal);") != std::string::npos);
    REQUIRE(result.source.find("->set_x(0.3f);") != std::string::npos);
    REQUIRE(result.source.find("->set_y(0.8f);") != std::string::npos);
    REQUIRE(result.source.find("->set_x_label(\"Cutoff\");") != std::string::npos);
    REQUIRE(result.source.find("->set_y_label(\"Resonance\");") != std::string::npos);

    TempDir tmp("pulp-phase-a-typed-cpp-codegen");
    const auto header = tmp.path / "phase_a_typed_controls.hpp";
    const auto source = tmp.path / "phase_a_typed_controls.cpp";
    const auto object = tmp.path / "phase_a_typed_controls.o";
    write_text(header, result.header);
    write_text(source, result.source);

    std::string diagnostics;
    const bool compiled = compile_generated_source(source, object, &diagnostics);
    INFO(diagnostics);
    REQUIRE(compiled);
}

TEST_CASE("untyped named control-looking IR remains generic baked C++",
          "[view][import][cpp-codegen][native-cpp-phase-a]") {
    const auto ir = build_untyped_named_control_ir();
    const auto result = generate_pulp_cpp(ir, ir.asset_manifest, {});

    REQUIRE(result.source.find("set_id(\"gain-knob-looking-frame\")") != std::string::npos);
    REQUIRE(result.source.find("std::make_unique<pulp::view::Knob>()") == std::string::npos);
    REQUIRE(result.source.find("std::make_unique<pulp::view::Fader>()") == std::string::npos);
    REQUIRE(result.source.find("std::make_unique<pulp::view::Meter>()") == std::string::npos);
    REQUIRE(result.source.find("std::make_unique<pulp::view::XYPad>()") == std::string::npos);
    REQUIRE(count_occurrences(result.source, "std::make_unique<pulp::view::View>()") >= 2);
}

TEST_CASE("Chainer route overlay can lower one knob to typed C++ with binding sidecar",
          "[view][import][cpp-codegen][native-cpp-phase-c]") {
    const fs::path manifest_path =
        fs::path(PULP_REPO_ROOT) / "planning/artifacts/native-ui/nv0/reports/chainer-route-manifest.json";
    REQUIRE(fs::exists(manifest_path));
    const auto overlay = read_text(manifest_path);
    REQUIRE(overlay.find("\"schema\": \"pulp-native-ui-route-overlay-v1\"") != std::string::npos);
    REQUIRE(overlay.find("\"id\": \"chainer.knob.0.osc_freq\"") != std::string::npos);
    REQUIRE(overlay.find("\"materialized_ir_path\": \"root/1/2/0/1/0\"") != std::string::npos);
    REQUIRE(overlay.find("\"unique_knob_ir_paths\": 8") != std::string::npos);

    const auto ir = build_phase_c_chainer_one_knob_ir();
    CppExportOptions opts;
    opts.header_filename = "phase_c_chainer_one_knob.hpp";
    opts.namespace_name = "pulp::test::phase_c";

    const auto result = generate_pulp_cpp(ir, ir.asset_manifest, opts);

    REQUIRE(count_occurrences(result.source, "std::make_unique<pulp::view::Knob>()") == 1);
    REQUIRE(result.source.find("->set_anchor_id(\"pr_2c\");") != std::string::npos);
    REQUIRE(result.source.find("->set_label(\"freq\");") != std::string::npos);
    REQUIRE(result.source.find("->set_value(/* TODO: bind to param */ 0.35f);") != std::string::npos);
    REQUIRE(result.source.find("->set_default_value(0.35f);") != std::string::npos);

    REQUIRE(result.binding_manifest.find("\"schema\": \"pulp-native-cpp-binding-manifest-v1\"") != std::string::npos);
    REQUIRE(result.binding_manifest.find("\"id\": \"chainer.knob.0.osc_freq\"") != std::string::npos);
    REQUIRE(result.binding_manifest.find("\"native_primitive\": \"knob\"") != std::string::npos);
    REQUIRE(result.binding_manifest.find("\"source_family\": \"Knob\"") != std::string::npos);
    REQUIRE(result.binding_manifest.find("\"param_key\": \"osc_freq\"") != std::string::npos);
    REQUIRE(result.binding_manifest.find("\"binding_module\": \"OSC\"") != std::string::npos);
    REQUIRE(result.binding_manifest.find("\"binding_param\": \"freq\"") != std::string::npos);
    REQUIRE(result.binding_manifest.find("\"event_contract\": \"onChange:set_param:osc_freq\"") != std::string::npos);
    REQUIRE(result.binding_manifest.find("\"gesture_contract\": \"rotary_drag:begin/update/end\"") != std::string::npos);
    auto binding_manifest = choc::json::parse(result.binding_manifest);
    REQUIRE(binding_manifest["schema"].getString() == std::string("pulp-native-cpp-binding-manifest-v1"));
    REQUIRE(binding_manifest["entries"].size() == 1);
    const auto& entry = binding_manifest["entries"][0];
    REQUIRE(entry["id"].getString() == std::string("chainer.knob.0.osc_freq"));
    REQUIRE(entry["ir_path"].getString() == std::string("root/0"));
    REQUIRE(entry["native_primitive"].getString() == std::string("knob"));
    REQUIRE(entry["param_key"].getString() == std::string("osc_freq"));

    TempDir tmp("pulp-phase-c-chainer-one-knob-cpp-codegen");
    const auto header = tmp.path / "phase_c_chainer_one_knob.hpp";
    const auto source = tmp.path / "phase_c_chainer_one_knob.cpp";
    const auto object = tmp.path / "phase_c_chainer_one_knob.o";
    write_text(header, result.header);
    write_text(source, result.source);

    std::string diagnostics;
    const bool compiled = compile_generated_source(source, object, &diagnostics);
    INFO(diagnostics);
    REQUIRE(compiled);
}

TEST_CASE("baked C++ exporter emits ownable C++ source artifacts",
          "[view][import][cpp-codegen][phase-7]") {
    auto ir = build_codegen_fixture_ir();
    ir.root.children.front().children.push_back(
        label_node("escape",
                   std::string("A") + static_cast<char>(0x01) + "B" + static_cast<char>(0) + "9",
                   48.0f,
                   24.0f));
    CppExportOptions opts;
    opts.namespace_name = "pulp::test::generated_design";
    opts.header_filename = "generated_design.hpp";

    const auto result = generate_pulp_cpp(ir, ir.asset_manifest, opts);
    REQUIRE(result.header.find("build_imported_ui") != std::string::npos);
    REQUIRE(result.header.find("bake_asset_manifest") != std::string::npos);
    REQUIRE(result.source.find("namespace tokens") != std::string::npos);
    REQUIRE(result.source.find("set_background_color(tokens::") != std::string::npos);
    REQUIRE(result.source.find("kBgPrimary = pulp::view::Color::rgba8(17, 34, 51, 255)") != std::string::npos);
    REQUIRE(result.source.find("kBgAlias = pulp::view::Color::rgba8(17, 34, 51, 255)") != std::string::npos);
    REQUIRE(result.source.find("kSemanticSurface = \"surface-token\"") != std::string::npos);
    REQUIRE(result.source.find("kPanelWidthAlias = 320.0f") != std::string::npos);
    REQUIRE(result.source.find("A\\001B\\0009") != std::string::npos);
    REQUIRE(result.source.find("namespace assets") != std::string::npos);
    REQUIRE(result.source.find("assets::kLogo") != std::string::npos);
    REQUIRE(result.source.find("// auto-extracted: structural name \"Header\"") != std::string::npos);
    REQUIRE(result.source.find("std::unique_ptr<pulp::view::View> build_header()") != std::string::npos);
    REQUIRE(result.source.find("/* TODO: bind to param */") != std::string::npos);
    REQUIRE(result.source.find("->set_value(/* TODO: bind to param */ 0.2f);") != std::string::npos);
    REQUIRE(result.source.find("->set_default_value(0.75f);") != std::string::npos);
    REQUIRE(result.source.find("std::make_unique<pulp::view::Knob>()") != std::string::npos);
    REQUIRE(result.source.find("build_native_view_tree") == std::string::npos);
    REQUIRE(result.source.find("serialize_design_ir") == std::string::npos);

    TempDir tmp("pulp-design-import-cpp-codegen");
    const auto header = tmp.path / "generated_design.hpp";
    const auto source = tmp.path / "generated_design.cpp";
    const auto object = tmp.path / "generated_design.o";
    write_text(header, result.header);
    write_text(source, result.source);

    std::string diagnostics;
    const bool compiled = compile_generated_source(source, object, &diagnostics);
    INFO(diagnostics);
    REQUIRE(compiled);
}

TEST_CASE("generated C++ fixture renders layout-equivalent native tree",
          "[view][import][cpp-codegen][phase-7]") {
    auto ir = build_codegen_fixture_ir();
    auto baked_native = build_native_view_tree(ir, ir.asset_manifest);
    auto generated_cpp = pulp::test::design_import_cpp_fixture::build_imported_ui();
    REQUIRE(baked_native != nullptr);
    REQUIRE(generated_cpp != nullptr);

    baked_native->set_bounds({0, 0, 320, 140});
    generated_cpp->set_bounds({0, 0, 320, 140});
    baked_native->layout_children();
    generated_cpp->layout_children();

    const LayoutTreeSnapshotOptions options{
        .surface = "phase7-baked-cpp",
        .fixture = "generated-cpp-vs-baked-native",
        .viewport_width = 320,
        .viewport_height = 140,
    };
    const auto baked_json = dump_layout_tree(*baked_native, options);
    const auto generated_json = dump_layout_tree(*generated_cpp, options);

    LayoutTreeDiff diff;
    const bool equivalent = layout_tree_snapshots_equivalent(
        baked_json,
        generated_json,
        {},
        &diff);
    INFO(diff_messages(diff));
    INFO("baked native:\n" << baked_json);
    INFO("generated cpp:\n" << generated_json);
    REQUIRE(equivalent);

    const auto manifest = pulp::test::design_import_cpp_fixture::bake_asset_manifest();
    REQUIRE(manifest.assets.size() == 1);
    REQUIRE(manifest.assets.front().asset_id == "logo");
    REQUIRE(manifest.assets.front().content_hash == "sha256:fixture");
}

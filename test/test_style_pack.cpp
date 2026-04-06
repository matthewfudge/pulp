#include <catch2/catch_test_macros.hpp>
#include <pulp/view/widget_bridge.hpp>
#include <filesystem>
#include <string>

using namespace pulp::view;
using namespace pulp::state;

TEST_CASE("Style presets round-trip through saveStylePreset/loadStylePreset", "[view][style-pack]") {
    ScriptEngine engine;
    View root;
    root.set_bounds({0, 0, 400, 300});
    StateStore store;
    WidgetBridge bridge(engine, root, store);

    auto preset_name = std::string("test_style_pack_roundtrip");
    auto preset_path = std::filesystem::temp_directory_path() / "pulp-style-presets" / (preset_name + ".json");
    std::filesystem::remove(preset_path);

    bridge.load_script(
        "saveStylePreset('" + preset_name + "', {"
        "shader:'half4 main(float2 coord){return half4(1);}',"
        "schema:{type:'knob'},"
        "tokens:{colors:{'accent.primary':'#123456'}}"
        "});");

    REQUIRE(std::filesystem::exists(preset_path));

    auto loaded = engine.evaluate("JSON.stringify(loadStylePreset('" + preset_name + "'))").toString();
    REQUIRE(loaded.find("\"shader\"") != std::string::npos);
    REQUIRE(loaded.find("#123456") != std::string::npos);
}

TEST_CASE("Style presets sanitize names with spaces", "[view][style-pack]") {
    ScriptEngine engine;
    View root;
    root.set_bounds({0, 0, 400, 300});
    StateStore store;
    WidgetBridge bridge(engine, root, store);

    std::string display_name = "Warm Analog";
    auto preset_path = std::filesystem::temp_directory_path() / "pulp-style-presets" / "Warm_Analog.json";
    std::filesystem::remove(preset_path);

    bridge.load_script(
        "saveStylePreset('" + display_name + "', {tokens:{colors:{'accent.primary':'#abcdef'}}});");

    REQUIRE(std::filesystem::exists(preset_path));
    auto loaded = engine.evaluate("JSON.stringify(loadStylePreset('Warm Analog'))").toString();
    REQUIRE(loaded.find("#abcdef") != std::string::npos);
}

// UI Preview — validates the full rendering pipeline
// JS → WidgetBridge → View tree → layout → paint → CoreGraphics → screen

#include <pulp/view/view.hpp>
#include <pulp/view/widgets.hpp>
#include <pulp/view/theme.hpp>
#include <pulp/view/script_engine.hpp>
#include <pulp/view/widget_bridge.hpp>
#include <pulp/view/window_host.hpp>
#include <pulp/state/store.hpp>
#include <iostream>

using namespace pulp::view;
using namespace pulp::state;

int main() {
    // Set up parameters
    StateStore store;
    store.add_parameter({1, "Gain", "dB", {-60.0f, 12.0f, 0.0f}});
    store.add_parameter({2, "Mix", "%", {0.0f, 100.0f, 100.0f}});
    store.add_parameter({3, "Bypass", "", {0.0f, 1.0f, 0.0f}});

    // Create root view with dark theme
    View root;
    root.set_theme(Theme::dark());
    root.flex().direction = FlexDirection::column;
    root.flex().padding = 16;
    root.flex().gap = 12;

    // Set up scripting engine
    ScriptEngine engine;
    WidgetBridge bridge(engine, root, store);

    // Build UI from JavaScript
    bridge.load_script(R"(
        createLabel('title', 'PulpGain Preview', 0, 0, 300, 24);
        createKnob('Gain', 0, 0, 64, 64);
        createKnob('Mix', 0, 0, 64, 64);
        createToggle('Bypass', 0, 0, 60, 28);
        createFader('volume', 0, 0, 200, 20, 'horizontal');
    )");

    // Set initial values
    engine.evaluate("setValue('Gain', 0.5)");
    engine.evaluate("setValue('Mix', 0.8)");
    engine.evaluate("setValue('volume', 0.65)");

    std::cout << "UI Preview: " << root.child_count() << " widgets created\n";

    // Open window and run
    WindowOptions opts;
    opts.title = "Pulp UI Preview";
    opts.width = 360;
    opts.height = 300;

    auto window = WindowHost::create(root, opts);
    window->set_close_callback([] {
        std::cout << "Window closed\n";
    });

    std::cout << "Opening window...\n";
    window->run_event_loop();

    return 0;
}

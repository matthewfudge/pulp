// Pulp Style Designer — JS-defined design tool
// Minimal C++ host: opens a window, loads JS via ScriptEngine + WidgetBridge,
// hot-reloads on file changes. All UI defined in design-tool.js.

#include <pulp/view/view.hpp>
#include <pulp/view/widgets.hpp>
#include <pulp/view/theme.hpp>
#include <pulp/view/script_engine.hpp>
#include <pulp/view/widget_bridge.hpp>
#include <pulp/view/window_host.hpp>
#include <pulp/view/hot_reload.hpp>
#include <pulp/state/store.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <cstdlib>
#include <dispatch/dispatch.h>

using namespace pulp::view;
using namespace pulp::state;
namespace fs = std::filesystem;

static std::string read_file(const fs::path& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char* argv[]) {
    // Find the JS file
    fs::path js_path;
    if (argc > 1) {
        js_path = argv[1];
    } else {
        auto dir = fs::current_path();
        while (!dir.empty()) {
            auto candidate = dir / "examples" / "design-tool" / "design-tool.js";
            if (fs::exists(candidate)) { js_path = candidate; break; }
            auto parent = dir.parent_path();
            if (parent == dir) break;
            dir = parent;
        }
    }

    if (js_path.empty() || !fs::exists(js_path)) {
        std::cerr << "Error: design-tool.js not found.\n";
        std::cerr << "Usage: pulp-design-tool [path/to/design-tool.js]\n";
        return 1;
    }

    std::cout << "Loading: " << js_path.string() << "\n";

    // Create the view tree root
    View root;
    root.set_theme(Theme::dark());
    root.flex().direction = FlexDirection::column;

    StateStore store;
    auto make_engine = [](std::string prefix = {}) {
        auto engine = std::make_unique<ScriptEngine>();
        engine->set_log_callback([prefix = std::move(prefix)](std::string_view level, std::string_view msg) {
            if (!prefix.empty()) {
                std::cerr << prefix << "[" << level << "] " << msg << "\n";
            } else {
                std::cerr << "[" << level << "] " << msg << "\n";
            }
        });
        return engine;
    };

    auto engine = make_engine();
    auto bridge = std::make_unique<WidgetBridge>(*engine, root, store);
    if (const char* ai_cli = std::getenv("PULP_AI_CLI")) {
        bridge->set_ai_cli_command(ai_cli);
    }

    // Load library scripts first (oklch.js)
    auto js_dir = js_path.parent_path();
    auto load_library_scripts = [&](WidgetBridge& target) {
        for (auto& lib : {"oklch.js"}) {
            auto lib_path = js_dir / lib;
            if (fs::exists(lib_path)) {
                target.load_script(read_file(lib_path));
            }
        }
    };
    try {
        load_library_scripts(*bridge);
        std::cout << "Loaded: oklch.js\n";
    } catch (const std::exception& e) {
        std::cerr << "Error loading library scripts: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Error loading library scripts: unknown exception\n";
        return 1;
    }

    // Load main UI script
    auto code = read_file(js_path);
    if (code.empty()) {
        std::cerr << "Error: could not read " << js_path.string() << "\n";
        return 1;
    }
    try {
        bridge->load_script(code);
    } catch (const std::exception& e) {
        std::cerr << "Error loading design tool script: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Error loading design tool script: unknown exception\n";
        return 1;
    }

    std::cout << "UI created: " << root.child_count() << " top-level views\n";

    // Open window
    WindowOptions opts;
    opts.title = "Pulp Style Designer";
    opts.width = 1100;
    opts.height = 700;
    opts.min_width = 1000;  // Issue 5: prevent scrollbar overlap
    opts.min_height = 600;
    opts.resizable = true;
    opts.use_gpu = true;

    auto window = WindowHost::create(root, opts);
    bridge->set_repaint_callback([&window] {
        if (window) window->repaint();
    });
    window->set_close_callback([] {
        std::cout << "Window closed\n";
    });

    // Hot reload
    HotReloader reloader(js_path, [&](const std::string& new_code) {
        std::cout << "Hot reload: " << js_path.filename().string() << "\n";
        try {
            // Validate the new script in an isolated bridge first so a bad
            // reload does not tear down the live UI.
            View probe_root;
            probe_root.set_theme(root.theme());
            probe_root.flex().direction = FlexDirection::column;
            StateStore probe_store;
            auto probe_engine = make_engine("reload:");
            auto probe_bridge = std::make_unique<WidgetBridge>(*probe_engine, probe_root, probe_store);
            load_library_scripts(*probe_bridge);
            probe_bridge->load_script(new_code);

            std::unordered_map<std::string, float> saved;
            bridge->snapshot_values(saved);
            window->invalidate_input_state();
            bridge->clear();

            auto next_engine = make_engine();
            auto next_bridge = std::make_unique<WidgetBridge>(*next_engine, root, store);
            if (const char* ai_cli = std::getenv("PULP_AI_CLI")) {
                next_bridge->set_ai_cli_command(ai_cli);
            }
            load_library_scripts(*next_bridge);
            next_bridge->load_script(new_code);
            next_bridge->restore_values(saved);
            engine = std::move(next_engine);
            bridge = std::move(next_bridge);
            bridge->set_repaint_callback([&window] {
                if (window) window->repaint();
            });
            window->repaint();
        } catch (const std::exception& e) {
            std::cerr << "Hot reload failed: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "Hot reload failed: unknown exception\n";
        }
    });

    // Poll hot-reload and async bridge results on a GCD timer (main thread).
    auto* reload_timer = dispatch_source_create(
        DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(reload_timer,
        dispatch_time(DISPATCH_TIME_NOW, 0),
        100 * NSEC_PER_MSEC, 20 * NSEC_PER_MSEC);
    auto* reloader_ptr = &reloader;
    auto* bridge_slot = &bridge;
    dispatch_source_set_event_handler(reload_timer, ^{
        try {
            if (*bridge_slot) {
                (*bridge_slot)->poll_async_results();
            }
            reloader_ptr->poll_reload();
        } catch (const std::exception& e) {
            std::cerr << "Design tool event loop error: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "Design tool event loop error: unknown exception\n";
        }
    });
    dispatch_resume(reload_timer);

    std::cout << "Hot reload watching: " << js_path.parent_path().string() << "\n";
    std::cout << "Opening window...\n";

    window->run_event_loop();
    dispatch_source_cancel(reload_timer);
    return 0;
}

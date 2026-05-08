#include <pulp/view/design_import.hpp>
#include <pulp/view/design_export.hpp>
#include <pulp/view/screenshot_compare.hpp>
#include <pulp/view/screenshot.hpp>
#include <pulp/view/script_engine.hpp>
#include <pulp/view/widget_bridge.hpp>
#include <pulp/view/window_host.hpp>
#include <pulp/state/store.hpp>
#include "import_detect.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <vector>

namespace fs = std::filesystem;
using namespace pulp::view;
using namespace pulp::state;

static void print_usage() {
    std::cout << "pulp import-design — Import designs from external tools into Pulp\n\n";
    std::cout << "Usage:\n";
    std::cout << "  pulp import-design --from <source> [options]\n\n";
    std::cout << "Sources:\n";
    std::cout << "  figma    Figma export JSON or MCP data\n";
    std::cout << "  stitch   Google Stitch screen HTML or MCP data\n";
    std::cout << "  v0       v0.dev TSX/Tailwind output\n";
    std::cout << "  pencil   Pencil/OpenPencil node JSON or .pen export\n";
    std::cout << "  claude   Anthropic Claude Design — manually-exported standalone HTML\n\n";
    std::cout << "Options:\n";
    std::cout << "  --from <source>   Design source (required)\n";
    std::cout << "  --file <path>     Input file path\n";
    std::cout << "  --url <url>       Design URL (Figma file URL or v0 share link)\n";
    std::cout << "  --frame <name>    Frame/artboard to import (Figma)\n";
    std::cout << "  --screen <name>   Screen to import (Stitch)\n";
    std::cout << "  --output <path>   Output JS file (default: ui.js)\n";
    std::cout << "  --tokens <path>   Output W3C token file (default: tokens.json)\n";
    std::cout << "  --dry-run         Show generated code without writing files\n";
    std::cout << "  --no-tokens       Skip token extraction\n";
    std::cout << "  --no-comments     Omit comments from generated code\n";
    std::cout << "  --web-compat      Use DOM API instead of native Pulp API\n";
    std::cout << "  --validate        Render generated JS and validate layout. Defaults to\n";
    std::cout << "                    native-gpu so imports prove the Dawn/Skia bridge unless\n";
    std::cout << "                    --validation-renderer selects an explicit headless lane.\n";
    std::cout << "  --validation-renderer <mode>\n";
    std::cout << "                    native-gpu (default), headless-skia, headless-coregraphics,\n";
    std::cout << "                    or headless-default.\n";
    std::cout << "  --allow-cpu-validation\n";
    std::cout << "                    Alias for --validation-renderer headless-skia; use only\n";
    std::cout << "                    for CPU/headless test lanes.\n";
    std::cout << "  --reference <png> Compare render against a reference screenshot\n";
    std::cout << "  --diff <png>      Save visual diff image\n";
    std::cout << "  --render-size WxH Render dimensions (default: 340x280)\n";
    std::cout << "  --validate-click x,y\n";
    std::cout << "                    During validation, click a point and capture after/diff PNGs.\n";
    std::cout << "  --validate-click-widget <id>\n";
    std::cout << "                    During validation, click the generated widget's center and\n";
    std::cout << "                    capture after/diff PNGs.\n";
    std::cout << "  --bridge-output <path>  Path to write bridge handler scaffold (default: bridge_handlers.cpp,\n";
    std::cout << "                          only emitted for --from claude)\n";
    std::cout << "  --no-bridge-scaffold    Skip bridge handler scaffold (claude only)\n";
    std::cout << "  --classnames <path>     Output classname → style map (default: classnames.json,\n";
    std::cout << "                          only emitted for --from claude — pulp #1035)\n";
    std::cout << "  --emit classnames       Force-emit classnames.json (default on for --from claude)\n";
    std::cout << "  --no-emit-classnames    Skip classname emission (claude only)\n";
    std::cout << "  --execute-bundle  Run the bundled React app in a headless JS engine and\n";
    std::cout << "                    walk the materialized DOM (--from claude only).\n";
    std::cout << "                    Falls back to the static parser on any harness failure.\n";
    std::cout << "  --detect-only     Detect (source, format-version, parser-version) for\n";
    std::cout << "                    --file or --directory <path> against compat.json without\n";
    std::cout << "                    parsing. Prints match counts and confidence.\n";
    std::cout << "  --directory <p>   Path to a directory export (alternative to --file).\n";
    std::cout << "  --compat <path>   compat.json override (default: discover from cwd / repo root).\n";
    std::cout << "  --report-new-format\n";
    std::cout << "                    Emit a fingerprint-diff JSON suitable for hand-editing\n";
    std::cout << "                    into a new compat.json[imports/<source>/detected-formats]\n";
    std::cout << "                    entry. Implies --detect-only.\n";
    std::cout << "  --help            Show this help\n\n";
    std::cout << "Examples:\n";
    std::cout << "  pulp import-design --from figma --file design.json\n";
    std::cout << "  pulp import-design --from figma --url 'https://figma.com/design/...' --frame 'Plugin UI'\n";
    std::cout << "  pulp import-design --from stitch --file screen.html --screen 'Main'\n";
    std::cout << "  pulp import-design --from v0 --url 'https://v0.dev/t/abc123' --output my-ui.js\n";
    std::cout << "  pulp import-design --from pencil --file design.json --dry-run\n";
    std::cout << "  pulp import-design --from pencil --file design.json --validate --reference source.png\n";
    std::cout << "  pulp import-design --from claude --file design.html\n";
}

// Bridge-handler scaffold body lives in core/view/src/design_import.cpp
// (`render_claude_bridge_scaffold`) so it can be unit-tested directly
// from the design_import test target — coverage doesn't follow CLI
// subprocess invocations, so keeping the body here would leave it
// uncovered. The CLI only calls into the library function below.

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Error: cannot open file: " << path << "\n";
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool write_file(const std::string& path, const std::string& content) {
    // Create parent directories if needed
    auto parent = fs::path(path).parent_path();
    if (!parent.empty()) fs::create_directories(parent);

    std::ofstream f(path);
    if (!f.is_open()) {
        std::cerr << "Error: cannot write file: " << path << "\n";
        return false;
    }
    f << content;
    return true;
}

static bool write_binary_file(const std::string& path, const std::vector<uint8_t>& bytes) {
    auto parent = fs::path(path).parent_path();
    if (!parent.empty()) fs::create_directories(parent);

    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "Error: cannot write file: " << path << "\n";
        return false;
    }
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return f.good();
}

enum class ValidationRenderer {
    native_gpu,
    headless_default,
    headless_skia,
    headless_coregraphics,
};

static const char* validation_renderer_name(ValidationRenderer renderer) {
    switch (renderer) {
        case ValidationRenderer::native_gpu: return "native-gpu";
        case ValidationRenderer::headless_default: return "headless-default";
        case ValidationRenderer::headless_skia: return "headless-skia";
        case ValidationRenderer::headless_coregraphics: return "headless-coregraphics";
    }
    return "unknown";
}

static std::string filename_token(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (auto& c : value) {
        if (std::isalnum(static_cast<unsigned char>(c))) continue;
        c = '-';
    }

    value.erase(std::unique(value.begin(), value.end(),
        [](char a, char b) { return a == '-' && b == '-'; }), value.end());
    while (!value.empty() && value.front() == '-') value.erase(value.begin());
    while (!value.empty() && value.back() == '-') value.pop_back();
    return value.empty() ? "design" : value;
}

static bool parse_validation_renderer(const std::string& value, ValidationRenderer& out) {
    if (value == "native-gpu") {
        out = ValidationRenderer::native_gpu;
        return true;
    }
    if (value == "headless" || value == "headless-default") {
        out = ValidationRenderer::headless_default;
        return true;
    }
    if (value == "headless-skia" || value == "skia") {
        out = ValidationRenderer::headless_skia;
        return true;
    }
    if (value == "headless-coregraphics" || value == "coregraphics") {
        out = ValidationRenderer::headless_coregraphics;
        return true;
    }
    return false;
}

struct ValidationClick {
    bool enabled = false;
    bool by_widget = false;
    std::string widget_id;
    pulp::view::Point point{};
};

struct ValidationRun {
    bool ran = false;
    bool valid = false;
    std::string error;
    ValidationRenderer renderer = ValidationRenderer::native_gpu;
    bool requested_gpu = false;
    bool resolved_gpu = false;
    bool native_bridge = false;
    bool webview = false;
    std::string render_path;
    std::string click_after_path;
    std::string click_diff_path;
    bool click_requested = false;
    bool click_dispatched = false;
    std::string click_widget_id;
    pulp::view::Point click_point{};
    CompareResult click_compare;
    int64_t setup_ms = 0;
    int64_t render_ms = 0;
    int64_t click_ms = 0;
    std::vector<uint8_t> rendered_png;
};

static bool parse_point_arg(const std::string& text, pulp::view::Point& out) {
    auto comma = text.find(',');
    if (comma == std::string::npos) return false;
    try {
        out.x = std::stof(text.substr(0, comma));
        out.y = std::stof(text.substr(comma + 1));
        return true;
    } catch (...) {
        return false;
    }
}

static pulp::view::Point center_in_root(pulp::view::View& target) {
    float x = 0.0f;
    float y = 0.0f;
    for (auto* v = &target; v != nullptr; v = v->parent()) {
        auto b = v->bounds();
        x += b.x;
        y += b.y;
    }
    auto b = target.bounds();
    return {x + b.width * 0.5f, y + b.height * 0.5f};
}

static void pump_validation_frames(WidgetBridge& bridge, pulp::view::View& root, int cycles = 8) {
    for (int i = 0; i < cycles; ++i) {
        bridge.service_frame_callbacks();
        root.layout_children();
    }
}

static ScreenshotBackend screenshot_backend_for(ValidationRenderer renderer) {
    switch (renderer) {
        case ValidationRenderer::headless_skia: return ScreenshotBackend::skia;
        case ValidationRenderer::headless_coregraphics: return ScreenshotBackend::coregraphics;
        default: return ScreenshotBackend::default_backend;
    }
}

static ValidationRun run_validation(const std::string& js,
                                    ValidationRenderer renderer,
                                    int render_width,
                                    int render_height,
                                    const std::string& render_path,
                                    const ValidationClick& click) {
    ValidationRun run;
    run.ran = true;
    run.renderer = renderer;
    run.render_path = render_path;
    run.click_requested = click.enabled;
    run.click_widget_id = click.widget_id;

    auto t0 = std::chrono::steady_clock::now();

    View render_root;
    render_root.set_theme(Theme::dark());
    render_root.flex().direction = FlexDirection::column;
    render_root.set_bounds({0.0f, 0.0f,
                            static_cast<float>(render_width),
                            static_cast<float>(render_height)});

    StateStore render_store;
    ScriptEngine render_engine;

    try {
        if (renderer == ValidationRenderer::native_gpu) {
            WindowOptions wopts;
            wopts.title = "Pulp import-design native GPU validation";
            wopts.width = static_cast<uint32_t>(std::max(render_width, 1));
            wopts.height = static_cast<uint32_t>(std::max(render_height, 1));
            wopts.use_gpu = true;
            wopts.resizable = false;
            run.requested_gpu = true;

            auto window = WindowHost::create(render_root, wopts);
            if (!window) {
                run.error = "native-gpu validation requested but WindowHost::create failed";
                return run;
            }

            run.resolved_gpu = window->is_gpu();
            run.native_bridge = run.resolved_gpu &&
                window->gpu_surface() != nullptr &&
                window->dawn_device_handle() != nullptr &&
                window->dawn_queue_handle() != nullptr;

            if (!run.native_bridge) {
                run.error =
                    "native-gpu validation requested but WindowHost did not resolve "
                    "a Dawn/Skia native bridge. Pass --validation-renderer headless-skia "
                    "only for an explicit CPU/headless test lane.";
                return run;
            }

            WidgetBridge render_bridge(render_engine, render_root, render_store, window->gpu_surface());
            render_bridge.set_repaint_callback([host = window.get()] {
                if (host) host->repaint();
            });
            render_bridge.load_script(js);
            pump_validation_frames(render_bridge, render_root);
            window->repaint();

            auto t_setup = std::chrono::steady_clock::now();
            run.rendered_png = window->capture_png();
            auto t_render = std::chrono::steady_clock::now();

            if (run.rendered_png.empty()) {
                run.error = "native-gpu validation render failed";
                return run;
            }
            if (!write_binary_file(render_path, run.rendered_png)) {
                run.error = "failed to write validation screenshot";
                return run;
            }

            if (click.enabled) {
                pulp::view::Point click_point = click.point;
                if (click.by_widget) {
                    auto* target = render_bridge.widget(click.widget_id);
                    if (!target) {
                        run.error = "validation click widget not found: " + click.widget_id;
                        return run;
                    }
                    click_point = center_in_root(*target);
                }
                run.click_point = click_point;
                render_root.simulate_click(click_point);
                pump_validation_frames(render_bridge, render_root, 4);
                window->repaint();

                const auto click_stem = fs::path(render_path).replace_extension("").string();
                run.click_after_path = click_stem + "-after-click.png";
                run.click_diff_path = click_stem + "-click-diff.png";
                auto after_png = window->capture_png();
                if (after_png.empty()) {
                    run.error = "native-gpu validation after-click render failed";
                    return run;
                }
                if (!write_binary_file(run.click_after_path, after_png)) {
                    run.error = "failed to write validation after-click screenshot";
                    return run;
                }
                auto diff_png = generate_diff_image(run.rendered_png, after_png);
                if (!diff_png.empty()) {
                    (void)write_binary_file(run.click_diff_path, diff_png);
                }
                run.click_compare = compare_screenshots(run.rendered_png, after_png);
                run.click_dispatched = true;
                auto t_click = std::chrono::steady_clock::now();
                run.click_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_click - t_render).count();
            }

            run.setup_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_setup - t0).count();
            run.render_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_render - t_setup).count();
            run.valid = true;
            return run;
        }

#ifndef PULP_HAS_SKIA
        if (renderer == ValidationRenderer::headless_skia) {
            run.error =
                "headless-skia validation requested but this binary was built without PULP_HAS_SKIA";
            return run;
        }
#endif

        WidgetBridge render_bridge(render_engine, render_root, render_store);
        render_bridge.load_script(js);
        pump_validation_frames(render_bridge, render_root);

        auto t_setup = std::chrono::steady_clock::now();
        run.rendered_png = render_to_png(render_root,
            static_cast<uint32_t>(render_width),
            static_cast<uint32_t>(render_height),
            2.0f,
            screenshot_backend_for(renderer));
        auto t_render = std::chrono::steady_clock::now();

        if (run.rendered_png.empty()) {
            run.error = "headless validation render failed";
            return run;
        }
        if (!write_binary_file(render_path, run.rendered_png)) {
            run.error = "failed to write validation screenshot";
            return run;
        }

        if (click.enabled) {
            pulp::view::Point click_point = click.point;
            if (click.by_widget) {
                auto* target = render_bridge.widget(click.widget_id);
                if (!target) {
                    run.error = "validation click widget not found: " + click.widget_id;
                    return run;
                }
                click_point = center_in_root(*target);
            }
            run.click_point = click_point;
            render_root.simulate_click(click_point);
            pump_validation_frames(render_bridge, render_root, 4);

            const auto click_stem = fs::path(render_path).replace_extension("").string();
            run.click_after_path = click_stem + "-after-click.png";
            run.click_diff_path = click_stem + "-click-diff.png";
            auto after_png = render_to_png(render_root,
                static_cast<uint32_t>(render_width),
                static_cast<uint32_t>(render_height),
                2.0f,
                screenshot_backend_for(renderer));
            if (after_png.empty()) {
                run.error = "headless validation after-click render failed";
                return run;
            }
            if (!write_binary_file(run.click_after_path, after_png)) {
                run.error = "failed to write validation after-click screenshot";
                return run;
            }
            auto diff_png = generate_diff_image(run.rendered_png, after_png);
            if (!diff_png.empty()) {
                (void)write_binary_file(run.click_diff_path, diff_png);
            }
            run.click_compare = compare_screenshots(run.rendered_png, after_png);
            run.click_dispatched = true;
            auto t_click = std::chrono::steady_clock::now();
            run.click_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_click - t_render).count();
        }

        run.setup_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_setup - t0).count();
        run.render_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_render - t_setup).count();
        run.valid = true;
        return run;
    } catch (const std::exception& e) {
        run.error = e.what();
        return run;
    }
}

int main(int argc, char* argv[]) {
    std::string source_str;
    std::string input_file;
    std::string input_url;           // --url: Figma file URL or v0 share link
    std::string frame_name;          // --frame: Figma frame/artboard name
    std::string screen_name;         // --screen: Stitch screen name
    std::string output_file = "ui.js";
    std::string tokens_file = "tokens.json";
    std::string export_format = "w3c";
    std::string reference_image;     // --reference: PNG of source design for validation
    std::string diff_output;         // --diff: output path for visual diff image
    bool dry_run = false;
    bool include_tokens = true;
    bool include_comments = true;
    bool export_tokens_mode = false;
    bool validate = false;           // --validate: render + compare after import
    bool use_web_compat = false;     // --web-compat: use DOM API instead of native
    bool preview_mode = false;       // --preview: minimal widget style for design comparison
    bool debug_json = false;         // --debug: output JSON report with all metrics
    std::string debug_output;        // --debug-output: path for JSON report
    int render_width = 340;
    int render_height = 280;
    ValidationRenderer validation_renderer = ValidationRenderer::native_gpu;
    ValidationClick validation_click;
    std::string bridge_output = "bridge_handlers.cpp";  // claude scaffold output
    bool emit_bridge_scaffold = true;                    // default on for --from claude
    bool execute_bundle = false;                         // pulp #468 native-runtime path
    std::string classnames_output = "classnames.json";   // pulp #1035 — claude classname map
    bool emit_classnames = true;                          // default on for --from claude
    // pulp #1031 — versioned detect surface
    bool detect_only = false;
    bool report_new_format = false;
    std::string input_directory;                          // --directory: alternative to --file
    std::string compat_override;                          // --compat: explicit compat.json path

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--from") == 0 && i + 1 < argc) {
            source_str = argv[++i];
        } else if (std::strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            input_file = argv[++i];
        } else if (std::strcmp(argv[i], "--url") == 0 && i + 1 < argc) {
            input_url = argv[++i];
        } else if (std::strcmp(argv[i], "--frame") == 0 && i + 1 < argc) {
            frame_name = argv[++i];
        } else if (std::strcmp(argv[i], "--screen") == 0 && i + 1 < argc) {
            screen_name = argv[++i];
        } else if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (std::strcmp(argv[i], "--tokens") == 0 && i + 1 < argc) {
            tokens_file = argv[++i];
        } else if (std::strcmp(argv[i], "--dry-run") == 0) {
            dry_run = true;
        } else if (std::strcmp(argv[i], "--no-tokens") == 0) {
            include_tokens = false;
        } else if (std::strcmp(argv[i], "--no-comments") == 0) {
            include_comments = false;
        } else if (std::strcmp(argv[i], "--export-tokens") == 0) {
            export_tokens_mode = true;
        } else if (std::strcmp(argv[i], "--format") == 0 && i + 1 < argc) {
            export_format = argv[++i];
        } else if (std::strcmp(argv[i], "--web-compat") == 0) {
            use_web_compat = true;
        } else if (std::strcmp(argv[i], "--validate") == 0) {
            validate = true;
        } else if (std::strcmp(argv[i], "--validation-renderer") == 0 && i + 1 < argc) {
            std::string value = argv[++i];
            if (!parse_validation_renderer(value, validation_renderer)) {
                std::cerr << "Error: unknown validation renderer '" << value << "'\n";
                std::cerr << "Valid renderers: native-gpu, headless-skia, headless-coregraphics, headless-default\n";
                return 1;
            }
        } else if (std::strcmp(argv[i], "--allow-cpu-validation") == 0) {
            validation_renderer = ValidationRenderer::headless_skia;
        } else if (std::strcmp(argv[i], "--reference") == 0 && i + 1 < argc) {
            reference_image = argv[++i];
            validate = true;
        } else if (std::strcmp(argv[i], "--diff") == 0 && i + 1 < argc) {
            diff_output = argv[++i];
        } else if (std::strcmp(argv[i], "--render-size") == 0 && i + 1 < argc) {
            // Parse WxH
            std::string sz = argv[++i];
            auto x = sz.find('x');
            if (x != std::string::npos) {
                render_width = std::stoi(sz.substr(0, x));
                render_height = std::stoi(sz.substr(x + 1));
            }
        } else if (std::strcmp(argv[i], "--validate-click") == 0 && i + 1 < argc) {
            validation_click.enabled = true;
            validation_click.by_widget = false;
            std::string point_arg = argv[++i];
            if (!parse_point_arg(point_arg, validation_click.point)) {
                std::cerr << "Error: --validate-click expects x,y coordinates\n";
                return 1;
            }
        } else if (std::strcmp(argv[i], "--validate-click-widget") == 0 && i + 1 < argc) {
            validation_click.enabled = true;
            validation_click.by_widget = true;
            validation_click.widget_id = argv[++i];
        } else if (std::strcmp(argv[i], "--preview") == 0) {
            preview_mode = true;
        } else if (std::strcmp(argv[i], "--debug") == 0) {
            debug_json = true;
        } else if (std::strcmp(argv[i], "--debug-output") == 0 && i + 1 < argc) {
            debug_output = argv[++i];
            debug_json = true;
        } else if (std::strcmp(argv[i], "--bridge-output") == 0 && i + 1 < argc) {
            bridge_output = argv[++i];
        } else if (std::strcmp(argv[i], "--no-bridge-scaffold") == 0) {
            emit_bridge_scaffold = false;
        } else if (std::strcmp(argv[i], "--execute-bundle") == 0) {
            execute_bundle = true;
        } else if (std::strcmp(argv[i], "--classnames") == 0 && i + 1 < argc) {
            classnames_output = argv[++i];
        } else if (std::strcmp(argv[i], "--emit") == 0 && i + 1 < argc) {
            // Currently only `--emit classnames` is recognized — additional
            // emit targets (e.g. `--emit tokens`) can layer on later
            // without changing the flag shape. Unknown values are
            // silently no-ops to keep forward compatibility.
            std::string what = argv[++i];
            if (what == "classnames") emit_classnames = true;
        } else if (std::strcmp(argv[i], "--no-emit-classnames") == 0) {
            emit_classnames = false;
        } else if (std::strcmp(argv[i], "--detect-only") == 0) {
            detect_only = true;
        } else if (std::strcmp(argv[i], "--report-new-format") == 0) {
            report_new_format = true;
            detect_only = true;
        } else if (std::strcmp(argv[i], "--directory") == 0 && i + 1 < argc) {
            input_directory = argv[++i];
        } else if (std::strcmp(argv[i], "--compat") == 0 && i + 1 < argc) {
            compat_override = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
    }

    // Export-tokens mode: read a Pulp theme JSON and export as W3C tokens
    if (export_tokens_mode) {
        if (input_file.empty()) {
            // No input = export the built-in dark theme
            auto theme = Theme::dark();
            auto w3c = export_w3c_tokens(theme);
            if (dry_run) {
                std::cout << w3c;
                return 0;
            }
            if (!write_file(tokens_file, w3c)) return 1;
            std::cout << "Exported " << (theme.colors.size() + theme.dimensions.size() + theme.strings.size())
                      << " tokens → " << tokens_file << "\n";
            return 0;
        }
        // Read theme JSON → export as W3C
        auto content = read_file(input_file);
        if (content.empty()) return 1;
        auto theme = Theme::from_json(content);
        auto w3c = export_w3c_tokens(theme);
        if (dry_run) {
            std::cout << w3c;
            return 0;
        }
        if (!write_file(tokens_file, w3c)) return 1;
        std::cout << "Exported " << (theme.colors.size() + theme.dimensions.size() + theme.strings.size())
                  << " tokens → " << tokens_file << "\n";
        return 0;
    }

    // ── pulp #1031 — versioned detect-only path ─────────────────────────
    // Runs against compat.json without invoking the source parsers.
    if (detect_only) {
        namespace det = pulp::import_detect;

        std::string scan_path = input_file.empty() ? input_directory : input_file;
        if (scan_path.empty()) {
            std::cerr << "Error: --detect-only requires --file <path> or --directory <path>\n";
            return 1;
        }
        if (!fs::exists(scan_path)) {
            std::cerr << "Error: path does not exist: " << scan_path << "\n";
            return 1;
        }

        // Resolve compat.json — explicit override > walk parents > cwd.
        fs::path compat_path;
        if (!compat_override.empty()) {
            compat_path = compat_override;
        } else {
            fs::path start = fs::is_directory(scan_path)
                ? fs::path(scan_path)
                : fs::path(scan_path).parent_path();
            if (start.empty()) start = fs::current_path();
            compat_path = det::find_compat_json(start);
            if (compat_path.empty())
                compat_path = det::find_compat_json(fs::current_path());
        }
        if (compat_path.empty() || !fs::exists(compat_path)) {
            std::cerr << "Error: compat.json not found"
                         " (pass --compat <path> or run from a Pulp checkout)\n";
            return 1;
        }

        auto manifest_text = read_file(compat_path.string());
        auto manifest = det::parse_compat_json(manifest_text);
        if (!manifest) {
            std::cerr << "Error: malformed compat.json at " << compat_path << "\n";
            return 1;
        }

        auto snap = det::snapshot_input(scan_path);
        auto result = det::detect(*manifest, snap);

        if (report_new_format) {
            auto report = det::build_new_format_report(*manifest, snap, result);
            std::cout << det::render_new_format_json(report);
            return 0;
        }

        if (result.source.empty()) {
            std::cout << "no detected source for " << scan_path << "\n";
            std::cout << "  compat.json: " << compat_path.string() << " (schema "
                      << manifest->compat_schema_version << ")\n";
            return 2;  // distinct from generic failure (1)
        }

        std::cout << "detected source: " << result.source << "\n";
        std::cout << "  format-version: " << result.format_version << "\n";
        std::cout << "  parser-version: " << result.parser_version << "\n";
        std::cout << "  fingerprint match: " << result.matched_clauses
                  << "/" << result.total_clauses;
        if (!result.matched_kinds.empty()) {
            std::cout << " (";
            for (size_t i = 0; i < result.matched_kinds.size(); ++i) {
                if (i) std::cout << ", ";
                std::cout << result.matched_kinds[i];
            }
            std::cout << ")";
        }
        std::cout << "\n";
        std::cout << "  confidence: " << result.confidence_pct << "%\n";

        if (result.confidence_pct < 80) {
            std::cout << "warning: confidence below 80% — this export may be a newer\n"
                      << "         format-version than Pulp recognises. Pulp will use\n"
                      << "         the most-recent matching parser; gaps surface in\n"
                      << "         import-report.json. To file a new format detector:\n"
                      << "  pulp import-design --file " << scan_path
                      << " --report-new-format\n";
        }
        return 0;
    }

    if (source_str.empty()) {
        std::cerr << "Error: --from <source> is required\n";
        print_usage();
        return 1;
    }

    auto source = parse_design_source(source_str);
    if (!source) {
        std::cerr << "Error: unknown source '" << source_str << "'\n";
        std::cerr << "Valid sources: figma, stitch, v0, pencil\n";
        return 1;
    }

    if (input_file.empty() && input_url.empty()) {
        std::cerr << "Error: --file <path> or --url <url> is required\n";
        return 1;
    }

    // --url without --file: fetch the URL content via curl
    std::string fetched_tmp;
    if (input_file.empty() && !input_url.empty()) {
        fetched_tmp = (fs::temp_directory_path() / "pulp-import-fetched.tmp").string();
        std::string cmd = "curl -sL -o '" + fetched_tmp + "' '" + input_url + "' 2>&1";
        int rc = std::system(cmd.c_str());
        if (rc != 0) {
            std::cerr << "Error: failed to fetch URL: " << input_url << "\n";
            return 1;
        }
        input_file = fetched_tmp;
        std::cout << "Fetched " << input_url << " → " << fetched_tmp << "\n";
    }

    auto t_start = std::chrono::steady_clock::now();

    // Read input
    auto content = read_file(input_file);
    if (content.empty()) return 1;

    // Parse based on source
    DesignIR ir;
    std::string runtime_error;  // captures --execute-bundle fallback reason
    try {
        switch (*source) {
            case DesignSource::figma:  ir = parse_figma_json(content); break;
            case DesignSource::stitch: ir = parse_stitch_html(content); break;
            case DesignSource::v0:     ir = parse_v0_tsx(content); break;
            case DesignSource::pencil: ir = parse_pencil_json(content); break;
            case DesignSource::claude:
                if (execute_bundle) {
                    ClaudeRuntimeOptions ropts;
                    ropts.error_out = &runtime_error;
                    // Allow up to 16 MB for the largest realistic Claude
                    // exports (3.1 MB Spectr app + 1.1 MB react-dom +
                    // 0.1 MB react with growth headroom).
                    ropts.max_total_js_bytes = 16 * 1024 * 1024;
                    ir = parse_claude_html_with_runtime(content, ropts);
                } else {
                    ir = parse_claude_html(content);
                }
                break;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing " << design_source_name(*source) << " input: " << e.what() << "\n";
        return 1;
    }

    if (execute_bundle && !runtime_error.empty()) {
        // Surface the harness-fallback reason so users can tell when the
        // bundle eval lane bailed out vs. produced a real materialized IR.
        std::cout << "[execute-bundle] runtime fallback: " << runtime_error << "\n";
    } else if (execute_bundle) {
        std::cout << "[execute-bundle] runtime path produced the IR (no fallback)\n";
    }

    ir.source = *source;
    ir.source_file = input_url.empty() ? input_file : input_url;

    // Clean up temp file from URL fetch
    if (!fetched_tmp.empty()) fs::remove(fetched_tmp);

    // Store frame/screen selection metadata
    if (!frame_name.empty()) ir.root.attributes["frame"] = frame_name;
    if (!screen_name.empty()) ir.root.attributes["screen"] = screen_name;

    // Generate Pulp JS
    CodeGenOptions opts;
    opts.mode = use_web_compat ? CodeGenMode::web_compat : CodeGenMode::native;
    opts.include_tokens = include_tokens;
    opts.include_comments = include_comments;
    opts.preview_mode = preview_mode;
    auto js = generate_pulp_js(ir, opts);

    if (dry_run) {
        std::cout << "=== Generated Pulp JS (" << design_source_name(*source) << " → " << output_file << ") ===\n\n";
        std::cout << js;

        if (include_tokens && (!ir.tokens.colors.empty() || !ir.tokens.dimensions.empty())) {
            auto theme = ir_tokens_to_theme(ir.tokens);
            auto w3c = export_w3c_tokens(theme);
            std::cout << "\n=== W3C Design Tokens (" << tokens_file << ") ===\n\n";
            std::cout << w3c;
        }
        return 0;
    }

    auto t_codegen = std::chrono::steady_clock::now();

    // Write output files
    if (!write_file(output_file, js)) return 1;

    // Count elements by type
    size_t node_count = 0, text_count = 0, container_count = 0, widget_count = 0;
    std::function<void(const IRNode&)> count_nodes = [&](const IRNode& n) {
        node_count++;
        if (n.audio_widget != AudioWidgetType::none) widget_count++;
        else if (n.type == "text" || n.type == "label") text_count++;
        else if (!n.children.empty() || n.type == "frame") container_count++;
        for (auto& c : n.children) count_nodes(c);
    };
    count_nodes(ir.root);

    auto t_write = std::chrono::steady_clock::now();
    std::cout << "Wrote " << output_file << " (" << node_count << " elements: "
              << container_count << " containers, " << widget_count << " widgets, "
              << text_count << " labels";

    // Write tokens
    if (include_tokens && (!ir.tokens.colors.empty() || !ir.tokens.dimensions.empty() || !ir.tokens.strings.empty())) {
        auto theme = ir_tokens_to_theme(ir.tokens);
        auto w3c = export_w3c_tokens(theme);
        if (write_file(tokens_file, w3c)) {
            size_t token_count = ir.tokens.colors.size() + ir.tokens.dimensions.size() + ir.tokens.strings.size();
            std::cout << ", " << token_count << " tokens → " << tokens_file;
        }
    }

    std::cout << ")\n";

    // Bridge handler scaffold for Claude Design imports (pulp #709).
    // Only emitted for --from claude; other sources keep their existing
    // output shape unchanged.
    if (*source == DesignSource::claude && emit_bridge_scaffold) {
        const auto scaffold = render_claude_bridge_scaffold(output_file);
        if (write_file(bridge_output, scaffold)) {
            std::cout << "Wrote " << bridge_output
                      << " (bridge handler scaffold — edit add_handler() entries to wire your editor's messages)\n";
        }
    }

    // Classnames artifact for Claude Design imports (pulp #1035).
    // Spectr's `tools/extract-html-bundle/extract.mjs` emits the same
    // map by hand; pulling it into the CLI lets `@pulp/css-adapt`
    // consume the file directly without a separate Node-side pass.
    // Only emitted for --from claude; default on, opt-out via
    // --no-emit-classnames.
    if (*source == DesignSource::claude && emit_classnames) {
        auto rules = extract_claude_classnames(content);
        const auto classnames_json = serialize_claude_classnames(rules);
        if (write_file(classnames_output, classnames_json)) {
            std::cout << "Wrote " << classnames_output
                      << " (" << rules.size() << " class rule"
                      << (rules.size() == 1 ? "" : "s")
                      << " — feed to @pulp/css-adapt or dom-adapter)\n";
        }
    }

    auto design_name = fs::path(output_file).stem().string();
    auto output_dir = fs::path(output_file).parent_path();
    auto rendered_name = design_name + "-" + filename_token(design_source_name(*source)) + "-render.png";

    ValidationRun validation_run;

    // ── Validation: render generated JS and compare with reference ──────
    if (validate) {
        auto rendered_path = (output_dir.empty() ? fs::path(rendered_name) : output_dir / rendered_name).string();
        std::cout << "Validating render via " << validation_renderer_name(validation_renderer) << "...\n";
        validation_run = run_validation(js, validation_renderer, render_width, render_height,
                                        rendered_path, validation_click);
        if (!validation_run.valid) {
            std::cerr << "Validation error: " << validation_run.error << "\n";
            return 1;
        }

        std::cout << "Validation renderer: " << validation_renderer_name(validation_renderer)
                  << " (requested_gpu=" << (validation_run.requested_gpu ? "true" : "false")
                  << ", resolved_gpu=" << (validation_run.resolved_gpu ? "true" : "false")
                  << ", native_bridge=" << (validation_run.native_bridge ? "true" : "false")
                  << ", webview=false)\n";
        std::cout << "Rendered → " << rendered_path << " (" << render_width << "x" << render_height << ")\n";
        std::cout << "Validation timing: setup=" << validation_run.setup_ms
                  << "ms render=" << validation_run.render_ms << "ms";
        if (validation_run.click_requested) std::cout << " click=" << validation_run.click_ms << "ms";
        std::cout << "\n";

        if (validation_run.click_dispatched) {
            std::cout << "Clicked";
            if (!validation_run.click_widget_id.empty())
                std::cout << " widget " << validation_run.click_widget_id;
            std::cout << " at " << validation_run.click_point.x << "," << validation_run.click_point.y << "\n";
            std::cout << "After click → " << validation_run.click_after_path << "\n";
            if (!validation_run.click_diff_path.empty())
                std::cout << "Click diff → " << validation_run.click_diff_path << "\n";
            if (validation_run.click_compare.valid) {
                std::cout << "Click visual delta: "
                          << validation_run.click_compare.diff_pixels << "/"
                          << validation_run.click_compare.total_pixels
                          << " pixels differ\n";
                if (validation_run.click_compare.diff_pixels == 0) {
                    std::cout << "Validation warning: click produced no visual delta; "
                              << "for interactive imports this is a parity gap to investigate.\n";
                }
            }
        }

        // Compare with reference if provided
        if (!reference_image.empty()) {
            auto result = compare_screenshot_files(reference_image, rendered_path);
            if (!result.valid) {
                std::cerr << "Comparison error: " << result.error << "\n";
                return 1;
            }

            std::cout << "Similarity: " << static_cast<int>(result.similarity * 100) << "% ("
                      << result.diff_pixels << "/" << result.total_pixels << " pixels differ, "
                      << "mean error: " << result.mean_error << ")\n";

            if (result.passes(0.70f)) {
                std::cout << "Validation: PASS\n";
            } else {
                std::cout << "Validation: NEEDS REVIEW (similarity below 70%)\n";
            }

            // Always generate diff image when reference is provided
            // Use --diff path if given, otherwise auto-generate alongside render
            auto actual_diff_path = diff_output.empty()
                ? (fs::path(validation_run.render_path).replace_extension("").string() + "-diff.png")
                : diff_output;
            {
                auto ref_bytes = [&]() -> std::vector<uint8_t> {
                    std::ifstream f(reference_image, std::ios::binary);
                    return {std::istreambuf_iterator<char>(f), {}};
                }();
                auto diff_png = generate_diff_image(ref_bytes, validation_run.rendered_png);
                if (!diff_png.empty()) {
                    std::ofstream f(actual_diff_path, std::ios::binary);
                    f.write(reinterpret_cast<const char*>(diff_png.data()),
                            static_cast<std::streamsize>(diff_png.size()));
                    std::cout << "Diff image → " << actual_diff_path << "\n";
                }
            }
        }
    }

    // ── Debug JSON report ────────────────────────────────────────────────
    if (debug_json) {
        auto t_end = std::chrono::steady_clock::now();
        auto ms_total = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
        auto ms_codegen = std::chrono::duration_cast<std::chrono::milliseconds>(t_codegen - t_start).count();
        auto ms_write = std::chrono::duration_cast<std::chrono::milliseconds>(t_write - t_codegen).count();
        auto ms_post_codegen = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_codegen).count();
        auto ms_validation = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_write).count();

        std::ostringstream dbg;
        dbg << "{\n";
        dbg << "  \"source\": \"" << design_source_name(*source) << "\",\n";
        dbg << "  \"input_file\": \"" << input_file << "\",\n";
        dbg << "  \"output_file\": \"" << output_file << "\",\n";
        dbg << "  \"mode\": \"" << (use_web_compat ? "web_compat" : "native") << "\",\n";
        dbg << "  \"elements\": {\n";
        dbg << "    \"total\": " << node_count << ",\n";
        dbg << "    \"containers\": " << container_count << ",\n";
        dbg << "    \"widgets\": " << widget_count << ",\n";
        dbg << "    \"labels\": " << text_count << "\n";
        dbg << "  },\n";
        dbg << "  \"tokens\": {\n";
        dbg << "    \"colors\": " << ir.tokens.colors.size() << ",\n";
        dbg << "    \"dimensions\": " << ir.tokens.dimensions.size() << ",\n";
        dbg << "    \"strings\": " << ir.tokens.strings.size() << "\n";
        dbg << "  },\n";
        dbg << "  \"timing_ms\": " << ms_total << ",\n";
        dbg << "  \"timing_codegen_ms\": " << ms_codegen << ",\n";
        dbg << "  \"timing_write_ms\": " << ms_write << ",\n";
        dbg << "  \"timing_post_codegen_ms\": " << ms_post_codegen << ",\n";
        dbg << "  \"timing_validation_ms\": " << ms_validation << ",\n";
        dbg << "  \"render_size\": \"" << render_width << "x" << render_height << "\",\n";
        dbg << "  \"js_bytes\": " << js.size() << ",\n";

        // Validation results if available
        if (validate) {
            dbg << "  \"validation\": {\n";
            dbg << "    \"renderer\": \"" << validation_renderer_name(validation_run.renderer) << "\",\n";
            dbg << "    \"requested_gpu\": " << (validation_run.requested_gpu ? "true" : "false") << ",\n";
            dbg << "    \"resolved_gpu\": " << (validation_run.resolved_gpu ? "true" : "false") << ",\n";
            dbg << "    \"native_bridge\": " << (validation_run.native_bridge ? "true" : "false") << ",\n";
            dbg << "    \"webview\": false,\n";
            dbg << "    \"render_image\": \"" << validation_run.render_path << "\",\n";
            dbg << "    \"setup_ms\": " << validation_run.setup_ms << ",\n";
            dbg << "    \"render_ms\": " << validation_run.render_ms << ",\n";
            dbg << "    \"click_requested\": " << (validation_run.click_requested ? "true" : "false") << ",\n";
            dbg << "    \"click_dispatched\": " << (validation_run.click_dispatched ? "true" : "false") << ",\n";
            dbg << "    \"click_ms\": " << validation_run.click_ms << ",\n";
            dbg << "    \"click_widget\": \"" << validation_run.click_widget_id << "\",\n";
            dbg << "    \"click_point\": {\"x\": " << validation_run.click_point.x
                << ", \"y\": " << validation_run.click_point.y << "},\n";
            if (validation_run.click_dispatched) {
                dbg << "    \"click_after_image\": \"" << validation_run.click_after_path << "\",\n";
                dbg << "    \"click_diff_image\": \"" << validation_run.click_diff_path << "\",\n";
                dbg << "    \"click_diff_pixels\": " << validation_run.click_compare.diff_pixels << ",\n";
                dbg << "    \"click_total_pixels\": " << validation_run.click_compare.total_pixels << "\n";
            } else {
                dbg << "    \"click_after_image\": null,\n";
                dbg << "    \"click_diff_image\": null,\n";
                dbg << "    \"click_diff_pixels\": null,\n";
                dbg << "    \"click_total_pixels\": null\n";
            }
            dbg << "  },\n";
        }

        if (validate && !reference_image.empty()) {
            auto result = compare_screenshot_files(reference_image, validation_run.render_path);
            dbg << "  \"reference_comparison\": {\n";
            dbg << "    \"reference\": \"" << reference_image << "\",\n";
            dbg << "    \"similarity_pct\": " << static_cast<int>(result.similarity * 100) << ",\n";
            dbg << "    \"diff_pixels\": " << result.diff_pixels << ",\n";
            dbg << "    \"total_pixels\": " << result.total_pixels << ",\n";
            dbg << "    \"mean_error\": " << result.mean_error << ",\n";
            dbg << "    \"pass\": " << (result.passes(0.70f) ? "true" : "false") << "\n";
            dbg << "  },\n";
        }

        // List unprocessed/unsupported elements
        dbg << "  \"gaps\": [\n";
        bool first_gap = true;
        std::function<void(const IRNode&)> find_gaps = [&](const IRNode& n) {
            // Shapes that aren't audio widgets (not translated to Pulp widgets)
            if ((n.type == "ellipse" || n.type == "rectangle" || n.type == "path" ||
                 n.type == "polygon" || n.type == "line") &&
                n.audio_widget == AudioWidgetType::none) {
                if (!first_gap) dbg << ",\n";
                first_gap = false;
                dbg << "    {\"type\": \"" << n.type << "\", \"name\": \"" << n.name
                    << "\", \"reason\": \"shape not mapped to widget\"}";
            }
            for (auto& c : n.children) find_gaps(c);
        };
        find_gaps(ir.root);
        dbg << "\n  ]\n";

        dbg << "}\n";

        auto report = dbg.str();
        if (!debug_output.empty()) {
            write_file(debug_output, report);
            std::cout << "Debug report → " << debug_output << "\n";
        } else {
            std::cout << "\n" << report;
        }
    }

    return 0;
}

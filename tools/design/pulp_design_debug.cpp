// pulp-design-debug — headless before/after/diff runner for design chat prompts
// Reuses the live design-tool JS prompt builder and response applier so debug
// artifacts stay aligned with the actual product path.

#include <pulp/view/view.hpp>
#include <pulp/view/theme.hpp>
#include <pulp/view/script_engine.hpp>
#include <pulp/view/widget_bridge.hpp>
#include <pulp/view/screenshot.hpp>
#include <pulp/view/screenshot_compare.hpp>
#include <pulp/state/store.hpp>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

using namespace pulp::view;
using namespace pulp::state;
namespace fs = std::filesystem;

namespace {

struct Options {
    fs::path script_path;
    fs::path output_dir = fs::path("planning") / "screenshots" / "design-debug";
    std::string prompt;
    std::string target = "all";
    std::string provider = "claude";
    std::string model = "claude-sonnet-4-6";
    std::string reasoning_effort;
    std::string ai_cli;
    fs::path response_file;
    uint32_t width = 1100;
    uint32_t height = 700;
    float scale = 2.0f;
    bool debug_json = true;
};

std::string read_file(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool write_text_file(const fs::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;
    out << content;
    return out.good();
}

bool write_binary_file(const fs::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return out.good();
}

std::string json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    out += ' ';
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

std::string slugify(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    bool prev_dash = false;
    for (char c : text) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc)) {
            out.push_back(static_cast<char>(std::tolower(uc)));
            prev_dash = false;
        } else if (!prev_dash) {
            out.push_back('-');
            prev_dash = true;
        }
    }
    while (!out.empty() && out.front() == '-') out.erase(out.begin());
    while (!out.empty() && out.back() == '-') out.pop_back();
    if (out.empty()) out = "prompt";
    if (out.size() > 48) out.resize(48);
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out;
}

fs::path find_repo_root() {
    auto dir = fs::current_path();
    while (!dir.empty()) {
        if (fs::exists(dir / "CMakeLists.txt") && fs::exists(dir / "examples" / "design-tool" / "design-tool.js")) {
            return dir;
        }
        auto parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    return {};
}

fs::path find_default_script(const fs::path& repo_root) {
    auto path = repo_root / "examples" / "design-tool" / "design-tool.js";
    if (fs::exists(path)) return path;
    return {};
}

void load_design_tool(const fs::path& script_path, WidgetBridge& bridge) {
    auto js_dir = script_path.parent_path();
    auto oklch_path = js_dir / "oklch.js";
    if (fs::exists(oklch_path)) {
        bridge.load_script(read_file(oklch_path));
    }
    bridge.load_script(read_file(script_path));
}

void print_usage() {
    std::cerr << "Usage: pulp-design-debug --prompt <text> [options]\n";
    std::cerr << "  --target <id|all>            Scope prompt to a widget like k1, slider1, t1\n";
    std::cerr << "  --script <file.js>           Design tool JS file (default: examples/design-tool/design-tool.js)\n";
    std::cerr << "  --output-dir <dir>           Artifact directory (default: planning/screenshots/design-debug)\n";
    std::cerr << "  --response-file <file.txt>   Use a saved model response instead of calling AI\n";
    std::cerr << "  --provider <name>            Metadata provider name (default: claude)\n";
    std::cerr << "  --model <id>                 Metadata/model id (default: claude-sonnet-4-6)\n";
    std::cerr << "  --reasoning-effort <level>   Metadata effort (low|medium|high|xhigh)\n";
    std::cerr << "  --ai-cli <template>          Override AI CLI template; supports {prompt_file} {model} {provider} {reasoning_effort}\n";
    std::cerr << "  --width <px>                 Render width (default: 1100)\n";
    std::cerr << "  --height <px>                Render height (default: 700)\n";
    std::cerr << "  --scale <factor>             Render scale (default: 2.0)\n";
}

std::string run_command_capture(const std::string& command, int& exit_code) {
    std::array<char, 4096> buffer{};
    std::string output;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        exit_code = -1;
        return {};
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
    auto status = pclose(pipe);
#if defined(_WIN32)
    exit_code = status;
#else
    if (WIFEXITED(status)) exit_code = WEXITSTATUS(status);
    else exit_code = status;
#endif
    return output;
}

std::string now_stamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tm);
    return buf;
}

} // namespace

int main(int argc, char* argv[]) {
    Options opts;
    auto repo_root = find_repo_root();
    if (!repo_root.empty()) {
        opts.script_path = find_default_script(repo_root);
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--prompt" && i + 1 < argc) opts.prompt = argv[++i];
        else if (arg == "--target" && i + 1 < argc) opts.target = argv[++i];
        else if (arg == "--script" && i + 1 < argc) opts.script_path = argv[++i];
        else if (arg == "--output-dir" && i + 1 < argc) opts.output_dir = argv[++i];
        else if (arg == "--response-file" && i + 1 < argc) opts.response_file = argv[++i];
        else if (arg == "--provider" && i + 1 < argc) opts.provider = argv[++i];
        else if (arg == "--model" && i + 1 < argc) opts.model = argv[++i];
        else if (arg == "--reasoning-effort" && i + 1 < argc) opts.reasoning_effort = argv[++i];
        else if (arg == "--ai-cli" && i + 1 < argc) opts.ai_cli = argv[++i];
        else if (arg == "--width" && i + 1 < argc) opts.width = static_cast<uint32_t>(std::stoul(argv[++i]));
        else if (arg == "--height" && i + 1 < argc) opts.height = static_cast<uint32_t>(std::stoul(argv[++i]));
        else if (arg == "--scale" && i + 1 < argc) opts.scale = std::stof(argv[++i]);
        else if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage();
            return 1;
        }
    }

    if (opts.prompt.empty()) {
        std::cerr << "Error: --prompt is required\n";
        print_usage();
        return 1;
    }
    if (opts.script_path.empty() || !fs::exists(opts.script_path)) {
        std::cerr << "Error: design-tool.js not found\n";
        return 1;
    }
    if (!opts.response_file.empty() && !fs::exists(opts.response_file)) {
        std::cerr << "Error: response file not found: " << opts.response_file << "\n";
        return 1;
    }
    fs::create_directories(opts.output_dir);

    auto stem = now_stamp() + "-" + slugify(opts.target == "all" ? "all" : opts.target) + "-" +
                slugify(opts.model) + "-" + slugify(opts.prompt);
    auto before_path = opts.output_dir / (stem + "-before.png");
    auto after_path = opts.output_dir / (stem + "-after.png");
    auto diff_path = opts.output_dir / (stem + "-diff.png");
    auto prompt_path = opts.output_dir / (stem + "-prompt.txt");
    auto response_path = opts.output_dir / (stem + "-response.txt");
    auto report_path = opts.output_dir / (stem + "-report.json");

    View root;
    root.set_theme(Theme::dark());
    root.flex().direction = FlexDirection::column;
    root.set_bounds({0.0f, 0.0f, static_cast<float>(opts.width), static_cast<float>(opts.height)});

    StateStore store;
    ScriptEngine engine;
    WidgetBridge bridge(engine, root, store);
    if (!opts.ai_cli.empty()) {
        bridge.set_ai_cli_command(opts.ai_cli);
    } else if (const char* ai_cli = std::getenv("PULP_AI_CLI")) {
        bridge.set_ai_cli_command(ai_cli);
    }

    try {
        load_design_tool(opts.script_path, bridge);
    } catch (const std::exception& e) {
        std::cerr << "Error loading design tool: " << e.what() << "\n";
        return 1;
    }
    root.layout_children();

    try {
        if (opts.target != "all") {
            engine.invoke("setDesignDebugTarget", opts.target);
            root.layout_children();
        } else {
            engine.invoke("clearInspectedComponent");
            root.layout_children();
        }
        engine.invoke("setDesignDebugAIConfig", opts.provider, opts.model, opts.reasoning_effort);
        root.layout_children();
    } catch (const std::exception& e) {
        std::cerr << "Error setting debug state: " << e.what() << "\n";
        return 1;
    }

    auto before_png = render_to_png(root, opts.width, opts.height, opts.scale);
    if (before_png.empty() || !write_binary_file(before_path, before_png)) {
        std::cerr << "Error: failed to render before screenshot\n";
        return 1;
    }

    std::string prompt_text;
    try {
        prompt_text = engine.invoke("buildDesignChatPrompt", opts.prompt).toString();
    } catch (const std::exception& e) {
        std::cerr << "Error building design prompt: " << e.what() << "\n";
        return 1;
    }
    if (!write_text_file(prompt_path, prompt_text)) {
        std::cerr << "Error: failed to write prompt file\n";
        return 1;
    }

    std::string response_text;
    std::string command_string;
    int command_exit = 0;
    if (!opts.response_file.empty()) {
        response_text = read_file(opts.response_file);
    } else {
        auto temp_prompt = fs::temp_directory_path() / (stem + "-prompt.txt");
        if (!write_text_file(temp_prompt, prompt_text)) {
            std::cerr << "Error: failed to write temp prompt file\n";
            return 1;
        }
        try {
            command_string = engine.invoke("buildAiCliCommand",
                                           temp_prompt.string(),
                                           opts.model,
                                           opts.provider,
                                           opts.reasoning_effort).toString();
        } catch (const std::exception& e) {
            std::cerr << "Error building AI CLI command: " << e.what() << "\n";
            return 1;
        }
        response_text = run_command_capture(command_string, command_exit);
        if (command_exit != 0) {
            std::cerr << "AI command failed with exit code " << command_exit << "\n";
            if (!response_text.empty()) std::cerr << response_text << "\n";
            return 1;
        }
    }
    if (!write_text_file(response_path, response_text)) {
        std::cerr << "Error: failed to write response file\n";
        return 1;
    }

    std::string apply_summary;
    try {
        apply_summary = engine.invoke("applyDesignChatResponse", response_text).toString();
        root.layout_children();
    } catch (const std::exception& e) {
        std::cerr << "Error applying design response: " << e.what() << "\n";
        return 1;
    }

    auto after_png = render_to_png(root, opts.width, opts.height, opts.scale);
    if (after_png.empty() || !write_binary_file(after_path, after_png)) {
        std::cerr << "Error: failed to render after screenshot\n";
        return 1;
    }

    auto diff_png = generate_diff_image(before_png, after_png);
    if (!diff_png.empty()) {
        write_binary_file(diff_path, diff_png);
    }
    auto compare = compare_screenshots(before_png, after_png);

    std::string debug_state_json = "{}";
    try {
        debug_state_json = engine.invoke("getDesignDebugStateJson").toString();
    } catch (...) {
    }

    float target_x = 0.0f, target_y = 0.0f, target_w = 0.0f, target_h = 0.0f;
    bool target_found = false;
    if (opts.target != "all") {
        if (auto* target = bridge.widget(opts.target)) {
            auto b = target->bounds();
            target_x = b.x;
            target_y = b.y;
            target_w = b.width;
            target_h = b.height;
            target_found = true;
        }
    }

    std::ostringstream report;
    report << "{\n";
    report << "  \"tool\": \"pulp-design-debug\",\n";
    report << "  \"script\": \"" << json_escape(opts.script_path.string()) << "\",\n";
    report << "  \"render_backend\": \"coregraphics-headless\",\n";
    report << "  \"sksl_gpu_supported\": false,\n";
    report << "  \"warnings\": [\n";
    report << "    \"Headless design-debug currently renders through CoreGraphicsCanvas, not the live Skia GPU path. Use this report to validate prompt/response/apply flow and visual diffs, but judge final widget SkSL quality in the interactive design tool.\"\n";
    report << "  ],\n";
    report << "  \"provider\": \"" << json_escape(opts.provider) << "\",\n";
    report << "  \"model\": \"" << json_escape(opts.model) << "\",\n";
    report << "  \"reasoning_effort\": \"" << json_escape(opts.reasoning_effort) << "\",\n";
    report << "  \"target\": \"" << json_escape(opts.target) << "\",\n";
    report << "  \"target_found\": " << (target_found ? "true" : "false") << ",\n";
    report << "  \"target_bounds\": {\"x\": " << target_x << ", \"y\": " << target_y
           << ", \"width\": " << target_w << ", \"height\": " << target_h << "},\n";
    report << "  \"prompt\": \"" << json_escape(opts.prompt) << "\",\n";
    report << "  \"prompt_file\": \"" << json_escape(prompt_path.string()) << "\",\n";
    report << "  \"response_file\": \"" << json_escape(response_path.string()) << "\",\n";
    report << "  \"before_image\": \"" << json_escape(before_path.string()) << "\",\n";
    report << "  \"after_image\": \"" << json_escape(after_path.string()) << "\",\n";
    report << "  \"diff_image\": \"" << json_escape(diff_path.string()) << "\",\n";
    report << "  \"similarity_pct\": " << static_cast<int>(compare.similarity * 100.0f) << ",\n";
    report << "  \"diff_pixels\": " << compare.diff_pixels << ",\n";
    report << "  \"total_pixels\": " << compare.total_pixels << ",\n";
    report << "  \"mean_error\": " << compare.mean_error << ",\n";
    report << "  \"apply_summary\": \"" << json_escape(apply_summary) << "\",\n";
    report << "  \"debug_state\": " << debug_state_json;
    if (!command_string.empty()) {
        report << ",\n  \"ai_command\": \"" << json_escape(command_string) << "\"";
    }
    report << "\n}\n";

    if (!write_text_file(report_path, report.str())) {
        std::cerr << "Error: failed to write report file\n";
        return 1;
    }

    std::cout << "Prompt saved → " << prompt_path << "\n";
    std::cout << "Before → " << before_path << "\n";
    std::cout << "After → " << after_path << "\n";
    if (!diff_png.empty()) std::cout << "Diff → " << diff_path << "\n";
    std::cout << "Response → " << response_path << "\n";
    std::cout << "Report → " << report_path << "\n";
    std::cout << "Applied → " << apply_summary << "\n";
    std::cout << "Visual change: " << static_cast<int>(compare.similarity * 100.0f)
              << "% similarity (" << compare.diff_pixels << "/" << compare.total_pixels
              << " pixels differ)\n";
    return 0;
}

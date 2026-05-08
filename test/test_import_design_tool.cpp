#include <catch2/catch_test_macros.hpp>
#include <pulp/platform/child_process.hpp>
#include <pulp/runtime/base64.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace pulp::platform;
namespace fs = std::filesystem;

#ifndef PULP_IMPORT_DESIGN_TOOL_PATH
#define PULP_IMPORT_DESIGN_TOOL_PATH ""
#endif

namespace {

fs::path tool_binary() {
    if (const char* env = std::getenv("PULP_IMPORT_DESIGN_TOOL_PATH"); env && *env) {
        return fs::path(env);
    }
    return fs::path(PULP_IMPORT_DESIGN_TOOL_PATH);
}

bool binary_exists() {
    const auto bin = tool_binary();
    return !bin.empty() && fs::exists(bin);
}

ProcessResult run_import_design(const std::vector<std::string>& args, int timeout_ms = 30000) {
    const auto bin = tool_binary();
    if (bin.empty() || !fs::exists(bin)) {
        ProcessResult r;
        r.exit_code = -1;
        r.stderr_output = "pulp-import-design binary not at " + bin.string();
        return r;
    }
    return exec(bin.string(), args, timeout_ms);
}

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
    std::ofstream f(path);
    REQUIRE(f.is_open());
    f << text;
    REQUIRE(f.good());
}

std::string read_text(const fs::path& path) {
    std::ifstream f(path);
    REQUIRE(f.is_open());
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string json_quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '/':  out += "\\u002F"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    out += "\"";
    return out;
}

std::string manifest_entry(const std::string& uuid, const std::string& mime,
                           const std::string& contents) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(contents.data());
    const auto b64 = pulp::runtime::base64_encode(bytes, contents.size());
    std::ostringstream ss;
    ss << "\"" << uuid << "\":{"
       << "\"mime\":\"" << mime << "\","
       << "\"compressed\":false,"
       << "\"data\":\"" << b64 << "\"}";
    return ss.str();
}

std::string build_claude_envelope(const std::string& manifest_json,
                                  const std::string& template_body_html) {
    std::ostringstream ss;
    ss << "<!DOCTYPE html><html><head><title>CLI Test</title></head><body>"
       << "<script type=\"__bundler/manifest\">" << manifest_json << "</script>"
       << "<script type=\"__bundler/template\">"
       << json_quote(template_body_html)
       << "</script></body></html>";
    return ss.str();
}

} // namespace

TEST_CASE("pulp-import-design reports help and argument diagnostics",
          "[cli][import-design][tool][issue-493]") {
    if (!binary_exists()) { SUCCEED("skipped: pulp-import-design not built"); return; }

    SECTION("help exits cleanly") {
        auto r = run_import_design({"--help"});
        REQUIRE_FALSE(r.timed_out);
        REQUIRE(r.exit_code == 0);
        REQUIRE(r.stdout_output.find("pulp import-design") != std::string::npos);
        REQUIRE(r.stdout_output.find("--from <source>") != std::string::npos);
    }

    SECTION("missing source is a usage error") {
        auto r = run_import_design({});
        REQUIRE_FALSE(r.timed_out);
        REQUIRE(r.exit_code != 0);
        REQUIRE(r.stderr_output.find("--from <source> is required") != std::string::npos);
        REQUIRE(r.stdout_output.find("Usage:") != std::string::npos);
    }

    SECTION("unknown source is rejected before input parsing") {
        auto r = run_import_design({"--from", "not-a-source"});
        REQUIRE_FALSE(r.timed_out);
        REQUIRE(r.exit_code != 0);
        REQUIRE(r.stderr_output.find("unknown source 'not-a-source'") != std::string::npos);
        REQUIRE(r.stderr_output.find("Valid sources") != std::string::npos);
    }
}

TEST_CASE("pulp-import-design export-tokens dry-run emits the built-in theme",
          "[cli][import-design][tool][issue-493]") {
    if (!binary_exists()) { SUCCEED("skipped: pulp-import-design not built"); return; }

    auto r = run_import_design({"--export-tokens", "--dry-run"});

    REQUIRE_FALSE(r.timed_out);
    REQUIRE(r.exit_code == 0);
    REQUIRE(r.stdout_output.find("\"bg\"") != std::string::npos);
    REQUIRE(r.stdout_output.find("\"$type\": \"color\"") != std::string::npos);
    REQUIRE(r.stdout_output.find("\"$type\": \"dimension\"") != std::string::npos);
    REQUIRE(r.stdout_output.find("\"$type\": \"string\"") != std::string::npos);
}

TEST_CASE("pulp-import-design writes a web-compat Stitch import to nested outputs",
          "[cli][import-design][tool][issue-493]") {
    if (!binary_exists()) { SUCCEED("skipped: pulp-import-design not built"); return; }

    TempDir tmp("pulp-import-design-tool-stitch");
    const auto input = tmp.path / "screen.html";
    const auto output = tmp.path / "generated" / "ui.js";
    const auto tokens = tmp.path / "generated" / "tokens.json";

    write_text(input,
               "<!DOCTYPE html><html><body>"
               "<main class=\"panel\"><h1>Gain</h1><button>Bypass</button></main>"
               "</body></html>");

    auto r = run_import_design({"--from", "stitch",
                                "--file", input.string(),
                                "--output", output.string(),
                                "--tokens", tokens.string(),
                                "--web-compat",
                                "--no-comments",
                                "--no-tokens"});

    REQUIRE_FALSE(r.timed_out);
    REQUIRE(r.exit_code == 0);
    REQUIRE(fs::exists(output));
    REQUIRE_FALSE(fs::exists(tokens));

    const auto js = read_text(output);
    REQUIRE(js.find("setTheme('dark')") != std::string::npos);
    REQUIRE(js.find("document.createElement") != std::string::npos);
    REQUIRE(js.find("document.body.appendChild") != std::string::npos);
    REQUIRE(r.stdout_output.find(output.string()) != std::string::npos);
}

TEST_CASE("pulp-import-design execute-bundle materializes a Claude runtime import",
          "[cli][import-design][tool][claude][issue-1689]") {
    if (!binary_exists()) { SUCCEED("skipped: pulp-import-design not built"); return; }

    TempDir tmp("pulp-import-design-tool-claude-runtime");
    const auto input = tmp.path / "claude-runtime.html";

    const std::string babel_umd = R"JS(
        (function(root, factory) {
            if (typeof exports === 'object' && typeof module !== 'undefined') {
                module.exports = factory();
            } else {
                root.Babel = factory();
            }
        })(this, function() {
            return {
                transform: function(src) { return { code: src }; }
            };
        });
    )JS";

    std::ostringstream manifest;
    manifest << "{" << manifest_entry("u-babel", "text/javascript", babel_umd) << "}";

    std::string body;
    body += R"(<div id="root">Loader Shell</div>)";
    body += R"(<script src="u-babel"></script>)";
    body += R"(<script type="text/babel">
        (function() {
            var root = document.getElementById('root');
            root.textContent = '';
            for (var i = 0; i < 36; i++) {
                var cell = document.createElement('button');
                cell.id = 'runtime-cell-' + i;
                cell.setAttribute('data-pulp-role', 'runtime-cell');
                cell.textContent = i === 0 ? 'Runtime Materialized' : ('Runtime Cell ' + i);
                root.appendChild(cell);
            }
        })();
    </script>)";

    write_text(input, build_claude_envelope(manifest.str(), body));

    auto r = run_import_design({"--from", "claude",
                                "--file", input.string(),
                                "--execute-bundle",
                                "--dry-run",
                                "--no-tokens",
                                "--no-bridge-scaffold",
                                "--no-emit-classnames"},
                               60000);

    INFO("stdout: " << r.stdout_output);
    INFO("stderr: " << r.stderr_output);
    REQUIRE_FALSE(r.timed_out);
    REQUIRE(r.exit_code == 0);
    REQUIRE(r.stdout_output.find("[execute-bundle] runtime path produced the IR (no fallback)")
            != std::string::npos);
    REQUIRE(r.stdout_output.find("runtime fallback") == std::string::npos);
    REQUIRE(r.stdout_output.find("Runtime Materialized") != std::string::npos);
    REQUIRE(r.stdout_output.find("runtime-cell-35") != std::string::npos);
}

// version_diag.cpp — Implementation of `pulp doctor --versions`
//
// Issue #499 (Slice 1). Pure-logic core + narrow I/O helpers so the
// analyzer can be exercised from unit tests without a live filesystem
// or a built binary. Only `render_report` writes to std::cout.

#include "version_diag.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#  include <cstdlib>   // _dupenv_s
#endif

namespace pulp::cli::version_diag {

namespace {

// Locally scoped home-directory lookup. cli_common.cpp also has a
// user_home_dir() but we deliberately don't take that dependency —
// version_diag is linked into the unit-test binary standalone.
fs::path user_home_dir_local() {
#ifdef _WIN32
    char* buf = nullptr;
    size_t len = 0;
    if (_dupenv_s(&buf, &len, "USERPROFILE") == 0 && buf) {
        fs::path p(buf);
        std::free(buf);
        return p;
    }
    return {};
#else
    if (const char* h = std::getenv("HOME")) return fs::path(h);
    return {};
#endif
}

// Minimal TOML scalar scanner — same shape as cli_common's
// read_pulp_toml_value helper. We keep a local copy so this
// translation unit has no link dependency on cli_common.cpp, which
// would pull in the entire CLI runtime.
std::string read_toml_scalar(const fs::path& toml, const std::string& key) {
    if (!fs::exists(toml)) return {};
    std::ifstream f(toml);
    if (!f.is_open()) return {};
    std::string line;
    while (std::getline(f, line)) {
        auto pos = line.find(key);
        if (pos == std::string::npos) continue;
        auto q1 = line.find('"', pos);
        auto q2 = (q1 == std::string::npos) ? std::string::npos : line.find('"', q1 + 1);
        if (q1 != std::string::npos && q2 != std::string::npos) {
            return line.substr(q1 + 1, q2 - q1 - 1);
        }
    }
    return {};
}

// Tiny ANSI helpers so this module stays decoupled from the CLI's
// global color state. Output is safe on all platforms — a terminal
// without ANSI support just shows the escape codes as literals when
// colour is forced on, which is why we gate on isatty elsewhere in
// the CLI. For version diagnostics, the test harness will see
// uncoloured content because stdout is piped.
const char* ansi_bold()   { return "\x1b[1m"; }
const char* ansi_reset()  { return "\x1b[0m"; }
const char* ansi_green()  { return "\x1b[32m"; }
const char* ansi_yellow() { return "\x1b[33m"; }
const char* ansi_dim()    { return "\x1b[2m"; }

}  // namespace

Semver parse_semver(const std::string& s) {
    Semver out;
    out.raw = s;
    if (s.empty()) return out;

    // Strip a leading "v" for tolerance with tag-style strings.
    std::string body = (s.front() == 'v' || s.front() == 'V') ? s.substr(1) : s;

    static const std::regex triple(R"(^(\d+)\.(\d+)\.(\d+)$)");
    std::smatch m;
    if (std::regex_match(body, m, triple)) {
        out.major = std::stoi(m[1].str());
        out.minor = std::stoi(m[2].str());
        out.patch = std::stoi(m[3].str());
        out.comparable = true;
    }
    return out;
}

int compare_semver(const Semver& a, const Semver& b) {
    if (a.major != b.major) return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
    return 0;
}

Semver read_plugin_version(const fs::path& plugin_json_path) {
    Semver out;
    if (plugin_json_path.empty() || !fs::exists(plugin_json_path)) return out;

    std::ifstream f(plugin_json_path);
    if (!f.is_open()) return out;

    std::stringstream buf;
    buf << f.rdbuf();
    auto body = buf.str();

    // Cheap, dependency-free: scrape the first `"version"` field.
    // The plugin manifest is maintained in-repo and has a stable shape.
    // Use a custom delimiter on the raw string so inner `"` characters
    // don't end the literal.
    static const std::regex ver_re(R"RE("version"\s*:\s*"([^"]+)")RE");
    std::smatch m;
    if (std::regex_search(body, m, ver_re)) {
        return parse_semver(m[1].str());
    }
    return out;
}

fs::path locate_plugin_json(const fs::path& active_repo_root,
                            const fs::path& override_path) {
    if (!override_path.empty() && fs::exists(override_path)) {
        return override_path;
    }
    if (!active_repo_root.empty()) {
        auto in_repo = active_repo_root / ".claude-plugin" / "plugin.json";
        if (fs::exists(in_repo)) return in_repo;
    }
    auto home = user_home_dir_local();
    if (!home.empty()) {
        for (auto suffix : {
                fs::path{".claude"} / "plugins" / "pulp" / "plugin.json",
                fs::path{".claude-plugin"} / "pulp" / "plugin.json",
             }) {
            auto candidate = home / suffix;
            if (fs::exists(candidate)) return candidate;
        }
    }
    return {};
}

Semver read_project_cli_min_version(const fs::path& project_root) {
    if (project_root.empty()) return {};
    auto raw = read_toml_scalar(project_root / "pulp.toml", "cli_min_version");
    return parse_semver(raw);
}

std::vector<SkewFinding> VersionReport::analyze() const {
    std::vector<SkewFinding> findings;

    // Rule 1: hard skew against cli_min_version — the primary reason
    // the design doc surfaces this at all. Warn, never block.
    if (cli.comparable && project_cli_min.comparable) {
        if (compare_semver(project_cli_min, cli) > 0) {
            findings.push_back({
                SkewSeverity::Warn,
                "Project requires CLI >= v" + project_cli_min.raw +
                    " but installed CLI is v" + cli.raw +
                    " — run `pulp upgrade`",
            });
        }
    }

    // Rule 2: project SDK newer than the CLI binary. This is the
    // "fleet of projects pulled ahead of the user's machine" case.
    // Advisory only.
    if (cli.comparable && project_sdk.comparable) {
        if (compare_semver(project_sdk, cli) > 0) {
            findings.push_back({
                SkewSeverity::Warn,
                "Project SDK is v" + project_sdk.raw +
                    " but installed CLI is v" + cli.raw +
                    " — consider `pulp upgrade`",
            });
        } else {
            findings.push_back({
                SkewSeverity::Info,
                "CLI v" + cli.raw + " is compatible with project SDK v" +
                    project_sdk.raw,
            });
        }
    }

    return findings;
}

int render_report(const VersionReport& report) {
    auto line = [](const std::string& label, const std::string& value) {
        std::cout << "  " << label;
        // Pad label column to 10 chars for alignment.
        if (label.size() < 10) {
            std::cout << std::string(10 - label.size(), ' ');
        }
        std::cout << value << "\n";
    };

    std::cout << ansi_bold() << "Pulp Version Diagnostics" << ansi_reset()
              << "\n========================\n\n";

    line("CLI:",
         report.cli.raw.empty() ? std::string("(unknown)")
                                : "v" + report.cli.raw);
    if (!report.plugin.raw.empty()) {
        line("Plugin:", "v" + report.plugin.raw +
                           (report.plugin_json_path.empty()
                                ? std::string{}
                                : "   (" + report.plugin_json_path.string() + ")"));
    } else {
        line("Plugin:", "(not found — install the Claude Code plugin to enable checks)");
    }

    if (!report.project_sdk.raw.empty()) {
        line("SDK:", "v" + report.project_sdk.raw +
                        (report.project_root.empty()
                             ? std::string{}
                             : "   (" + report.project_root.string() + ")"));
    } else {
        line("SDK:", "(no active project — run this command from a Pulp project to compare)");
    }

    if (report.project_cli_min.comparable) {
        line("CLI min:", "v" + report.project_cli_min.raw +
                            "   (from pulp.toml)");
    }

    std::cout << "\n";

    auto findings = report.analyze();
    int warn_count = 0;
    if (findings.empty()) {
        std::cout << ansi_dim()
                  << "  No version information to compare."
                  << ansi_reset() << "\n";
    } else {
        std::cout << "  Skew checks:\n";
        for (auto& f : findings) {
            if (f.severity == SkewSeverity::Warn) {
                ++warn_count;
                std::cout << "    " << ansi_yellow() << "WARN" << ansi_reset()
                          << " — " << f.message << "\n";
            } else {
                std::cout << "    " << ansi_green() << "OK" << ansi_reset()
                          << "   — " << f.message << "\n";
            }
        }
    }

    std::cout << "\n";
    if (warn_count == 0) {
        std::cout << "  " << ansi_green() << "All checks passed."
                  << ansi_reset() << "\n";
    } else {
        std::cout << "  " << ansi_yellow() << warn_count
                  << " warning" << (warn_count == 1 ? "" : "s")
                  << ansi_reset()
                  << " — advisory only, commands continue to run.\n";
    }
    return warn_count > 0 ? 1 : 0;
}

}  // namespace pulp::cli::version_diag

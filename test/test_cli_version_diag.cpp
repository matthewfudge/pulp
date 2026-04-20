// test_cli_version_diag.cpp — Unit tests for `pulp doctor --versions`
//
// Issue #499 Slice 1. Exercises the pure-logic core of version_diag
// (semver parse/compare + skew analyzer) and the plugin.json scraper,
// without shelling out to the built binary. Shell-out coverage lives
// in test_cli_shellout.cpp ([cli][shellout][issue-499]) and in the
// cli-doctor-versions CTest case registered in tools/cli/CMakeLists.txt.

#include <catch2/catch_test_macros.hpp>

#include "../tools/cli/version_diag.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace pulp::cli::version_diag;

namespace {

// Scoped tempdir helper — keeps each TEST_CASE isolated without
// depending on GTest-style fixtures. Catch2 is used for every other
// unit test in this repo; matches the house style.
struct TempDir {
    fs::path path;
    TempDir() {
        auto base = fs::temp_directory_path();
        // %p gives a unique per-process token; combine with a counter
        // for parallel test isolation.
        static std::atomic<int> seq{0};
        int n = seq.fetch_add(1);
        path = base / ("pulp-version-diag-test-" + std::to_string(
                          reinterpret_cast<std::uintptr_t>(this)) + "-" +
                       std::to_string(n));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void write_file(const fs::path& p, const std::string& body) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p);
    f << body;
}

}  // namespace

TEST_CASE("parse_semver accepts clean M.N.P triples", "[version-diag][issue-499]") {
    auto v = parse_semver("0.24.0");
    REQUIRE(v.comparable);
    REQUIRE(v.major == 0);
    REQUIRE(v.minor == 24);
    REQUIRE(v.patch == 0);
    REQUIRE(v.raw == "0.24.0");
}

TEST_CASE("parse_semver tolerates a leading 'v'", "[version-diag][issue-499]") {
    auto v = parse_semver("v1.2.3");
    REQUIRE(v.comparable);
    REQUIRE(v.major == 1);
    REQUIRE(v.minor == 2);
    REQUIRE(v.patch == 3);
}

TEST_CASE("parse_semver marks non-triple strings non-comparable",
          "[version-diag][issue-499]") {
    // Untagged / dev builds must be flagged non-comparable so skew
    // analysis silently skips them — this is the "untagged builds
    // are silently skipped" rule from the design doc.
    for (auto s : {"0.24.0-dev", "main", "", "abcd", "1.2", "1.2.3.4"}) {
        auto v = parse_semver(s);
        REQUIRE_FALSE(v.comparable);
    }
}

TEST_CASE("compare_semver orders by major/minor/patch",
          "[version-diag][issue-499]") {
    auto a = parse_semver("0.24.0");
    auto b = parse_semver("0.25.0");
    auto c = parse_semver("0.24.0");
    REQUIRE(compare_semver(a, b) < 0);
    REQUIRE(compare_semver(b, a) > 0);
    REQUIRE(compare_semver(a, c) == 0);

    auto d = parse_semver("1.0.0");
    REQUIRE(compare_semver(a, d) < 0);
    REQUIRE(compare_semver(d, b) > 0);
}

TEST_CASE("read_plugin_version scrapes the version field from plugin.json",
          "[version-diag][issue-499]") {
    TempDir tmp;
    auto plugin_json = tmp.path / ".claude-plugin" / "plugin.json";
    write_file(plugin_json, R"({
        "name": "pulp",
        "version": "0.6.0",
        "description": "test"
    })");

    auto v = read_plugin_version(plugin_json);
    REQUIRE(v.comparable);
    REQUIRE(v.major == 0);
    REQUIRE(v.minor == 6);
    REQUIRE(v.patch == 0);
}

TEST_CASE("read_plugin_version returns empty on missing file",
          "[version-diag][issue-499]") {
    auto v = read_plugin_version("/nonexistent/path/plugin.json");
    REQUIRE_FALSE(v.comparable);
    REQUIRE(v.raw.empty());
}

TEST_CASE("locate_plugin_json prefers an explicit override",
          "[version-diag][issue-499]") {
    TempDir tmp;
    auto explicit_path = tmp.path / "custom-plugin.json";
    write_file(explicit_path, R"({"version": "9.9.9"})");

    auto found = locate_plugin_json(tmp.path, explicit_path);
    REQUIRE(found == explicit_path);
}

TEST_CASE("locate_plugin_json falls back to the repo .claude-plugin dir",
          "[version-diag][issue-499]") {
    TempDir tmp;
    auto repo_plugin = tmp.path / ".claude-plugin" / "plugin.json";
    write_file(repo_plugin, R"({"version": "0.6.0"})");

    auto found = locate_plugin_json(tmp.path, {});
    REQUIRE(found == repo_plugin);
}

TEST_CASE("analyze warns when cli_min_version exceeds installed CLI",
          "[version-diag][issue-499]") {
    VersionReport r;
    r.cli = parse_semver("0.20.0");
    r.project_cli_min = parse_semver("0.24.0");

    auto findings = r.analyze();
    REQUIRE_FALSE(findings.empty());
    bool saw_warn = false;
    for (auto& f : findings) {
        if (f.severity == SkewSeverity::Warn &&
            f.message.find("pulp upgrade") != std::string::npos) {
            saw_warn = true;
        }
    }
    REQUIRE(saw_warn);
}

TEST_CASE("analyze is silent when cli_min_version is satisfied",
          "[version-diag][issue-499]") {
    VersionReport r;
    r.cli = parse_semver("0.24.0");
    r.project_cli_min = parse_semver("0.20.0");
    // No project_sdk — so no "compatible" Info line either. Findings
    // should therefore be empty (no warnings, no info).
    auto findings = r.analyze();
    REQUIRE(findings.empty());
}

TEST_CASE("analyze warns when project SDK is ahead of the installed CLI",
          "[version-diag][issue-499]") {
    VersionReport r;
    r.cli = parse_semver("0.20.0");
    r.project_sdk = parse_semver("0.24.0");

    auto findings = r.analyze();
    bool saw_warn = false;
    for (auto& f : findings) {
        if (f.severity == SkewSeverity::Warn &&
            f.message.find("Project SDK is v0.24.0") != std::string::npos) {
            saw_warn = true;
        }
    }
    REQUIRE(saw_warn);
}

TEST_CASE("analyze emits a compatible Info line when CLI >= project SDK",
          "[version-diag][issue-499]") {
    VersionReport r;
    r.cli = parse_semver("0.24.0");
    r.project_sdk = parse_semver("0.24.0");

    auto findings = r.analyze();
    bool saw_ok = false;
    for (auto& f : findings) {
        if (f.severity == SkewSeverity::Info) saw_ok = true;
    }
    REQUIRE(saw_ok);
}

TEST_CASE("analyze skips skew checks silently when CLI is untagged",
          "[version-diag][issue-499]") {
    VersionReport r;
    r.cli = parse_semver("0.24.0-dev");    // non-comparable
    r.project_cli_min = parse_semver("0.25.0");
    r.project_sdk = parse_semver("0.25.0");

    auto findings = r.analyze();
    // Neither rule fires when one side is non-comparable.
    REQUIRE(findings.empty());
}

TEST_CASE("read_project_cli_min_version reads pulp.toml field",
          "[version-diag][issue-499]") {
    TempDir tmp;
    auto toml = tmp.path / "pulp.toml";
    write_file(toml, "[pulp]\nsdk_version = \"0.24.0\"\ncli_min_version = \"0.22.0\"\n");
    auto v = read_project_cli_min_version(tmp.path);
    REQUIRE(v.comparable);
    REQUIRE(v.major == 0);
    REQUIRE(v.minor == 22);
    REQUIRE(v.patch == 0);
}

TEST_CASE("read_project_cli_min_version returns empty when absent",
          "[version-diag][issue-499]") {
    TempDir tmp;
    auto toml = tmp.path / "pulp.toml";
    write_file(toml, "[pulp]\nsdk_version = \"0.24.0\"\n");
    auto v = read_project_cli_min_version(tmp.path);
    REQUIRE_FALSE(v.comparable);
}

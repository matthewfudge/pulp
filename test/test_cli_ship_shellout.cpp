// Shell-out CLI behaviour tests for `pulp ship`.
//
// Per CLAUDE.md the #295 lesson (silent empty Ed25519 signature) came
// from the CLI ship path never being exercised end-to-end. This file
// shells to the built binary for the non-destructive ship branches
// that are safe to run in CI without real signing material.

#include <catch2/catch_test_macros.hpp>
#include <pulp/platform/child_process.hpp>

#include <filesystem>
#include <string>
#include <vector>

using namespace pulp::platform;
namespace fs = std::filesystem;

namespace {

fs::path pulp_binary() {
    if (const char* env = std::getenv("PULP_CLI_PATH"); env && *env) {
        return fs::path(env);
    }
    return fs::current_path() / ".." / "tools" / "cli" / "pulp";
}

bool binary_exists() { return fs::exists(pulp_binary()); }

ProcessResult run_pulp_in(const fs::path& cwd,
                          const std::vector<std::string>& args,
                          int timeout_ms = 10000) {
    auto bin = pulp_binary();
    ProcessOptions opts;
    opts.timeout_ms = timeout_ms;
    opts.working_directory = cwd.string();
    return ChildProcess::run(bin.string(), args, opts);
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

TEST_CASE("pulp ship outside a project directory errors out",
          "[cli][shellout][ship]") {
    if (!binary_exists()) { SUCCEED("pulp binary not built"); return; }
    auto r = run_pulp_in(fs::temp_directory_path(), {"ship"});
    REQUIRE_FALSE(r.timed_out);
    REQUIRE(r.exit_code != 0);
    auto combined = r.stdout_output + r.stderr_output;
    // The handler early-exits with "not in a Pulp project directory"
    // — that wording is the contract hosts/users rely on.
    REQUIRE(contains(combined, "Pulp project"));
}

TEST_CASE("pulp ship sign outside a project directory errors cleanly",
          "[cli][shellout][ship]") {
    if (!binary_exists()) { SUCCEED("pulp binary not built"); return; }
    auto r = run_pulp_in(fs::temp_directory_path(),
                         {"ship", "sign", "--identity", "fake-id"});
    REQUIRE_FALSE(r.timed_out);
    REQUIRE(r.exit_code != 0);
    // Must not fake-succeed: before #295 was closed, similar silent
    // paths let bad CLI args write empty artifacts.
    auto combined = r.stdout_output + r.stderr_output;
    REQUIRE(contains(combined, "Pulp project"));
}

TEST_CASE("pulp ship appcast outside a project errors cleanly",
          "[cli][shellout][ship]") {
    if (!binary_exists()) { SUCCEED("pulp binary not built"); return; }
    auto r = run_pulp_in(fs::temp_directory_path(),
                         {"ship", "appcast"});
    REQUIRE_FALSE(r.timed_out);
    REQUIRE(r.exit_code != 0);
}

TEST_CASE("pulp ship notarize outside a project errors cleanly",
          "[cli][shellout][ship]") {
    if (!binary_exists()) { SUCCEED("pulp binary not built"); return; }
    auto r = run_pulp_in(fs::temp_directory_path(), {"ship", "notarize"});
    REQUIRE_FALSE(r.timed_out);
    REQUIRE(r.exit_code != 0);
}

TEST_CASE("pulp ship check outside a project errors cleanly",
          "[cli][shellout][ship]") {
    if (!binary_exists()) { SUCCEED("pulp binary not built"); return; }
    auto r = run_pulp_in(fs::temp_directory_path(), {"ship", "check"});
    REQUIRE_FALSE(r.timed_out);
    REQUIRE(r.exit_code != 0);
}

TEST_CASE("pulp ship package outside a project errors cleanly",
          "[cli][shellout][ship]") {
    if (!binary_exists()) { SUCCEED("pulp binary not built"); return; }
    auto r = run_pulp_in(fs::temp_directory_path(),
                         {"ship", "package", "--version", "1.0.0"});
    REQUIRE_FALSE(r.timed_out);
    REQUIRE(r.exit_code != 0);
}

TEST_CASE("pulp ship help (or default) enumerates every subcommand",
          "[cli][shellout][ship][help]") {
    // Run from the build tree — find_project_root() walks up to the
    // worktree root, and build/CMakeCache.txt exists since the test
    // harness itself was built. That puts us in the fallthrough help
    // branch regardless of whether we pass "help" or bogus args.
    if (!binary_exists()) { SUCCEED("pulp binary not built"); return; }
    auto r = run_pulp_in(fs::current_path(), {"ship"});
    REQUIRE_FALSE(r.timed_out);

    // If the current-path walk doesn't find the project (some CI
    // layouts drop tests outside the tree), we accept the non-zero
    // branch provided the stderr mentions the project — that's the
    // same invariant the other cases assert.
    auto combined = r.stdout_output + r.stderr_output;
    if (r.exit_code == 0) {
        // Help branch reached — every shipping subcommand must be listed.
        for (const char* sub : {"sign", "notarize", "package", "appcast", "check"}) {
            INFO("ship help missing subcommand: " << sub);
            REQUIRE(contains(r.stdout_output, sub));
        }
    } else {
        REQUIRE(contains(combined, "Pulp project"));
    }
}

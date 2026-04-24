// test_cli_validator_discovery.cpp — Unit tests for `pulp doctor --validators`
//
// Issue #743. Exercises the pure-logic discovery + healing core of
// `tools/cli/validator_discovery.{hpp,cpp}`, with a hand-rolled
// DiscoveryEnv that stubs `path_exists` / `path_owner_uid` /
// `assess_signature` so the four acceptance scenarios in the spec are
// hermetic — no host filesystem, no `spctl`, no PATH dependence.
//
// Scenarios covered (#743 acceptance list):
//   (a) healthy cask install  → ✓ for each tool, exit 0
//   (b) user-owned broken     → ✗ + auto-fixable; --fix removes it
//   (c) root-owned broken     → ✗ + sudo one-liner; --fix is a no-op
//   (d) missing validator     → ⚠ + install command

#include <catch2/catch_test_macros.hpp>

#include "../tools/cli/validator_discovery.hpp"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;
namespace vd = pulp::cli::validator_discovery;

namespace {

// Scoped tempdir — kept tiny because most scenarios don't need real
// files (the env stubs fake existence). We only materialise files for
// the healing tests where we want to assert `fs::remove` actually ran.
struct TempDir {
    fs::path path;
    TempDir() {
        auto base = fs::temp_directory_path();
        static std::atomic<int> seq{0};
        int n = seq.fetch_add(1);
        path = base / ("pulp-validator-discovery-" + std::to_string(
                          reinterpret_cast<std::uintptr_t>(this)) + "-" +
                       std::to_string(n));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

// Stub env builder. Callers register virtual existing paths, owners,
// and signature verdicts per path. Everything unregistered is treated
// as non-existent / non-owned / signature-failed.
struct StubEnv {
    std::unordered_set<std::string> existing;
    std::unordered_map<std::string, uint32_t> owners;
    std::unordered_map<std::string, bool> verdicts;
    std::unordered_map<std::string, std::string> verdict_text;
    std::unordered_map<std::string, fs::path> path_resolve;
    uint32_t current_uid = 1000;
    fs::path home = "/home/test";

    vd::DiscoveryEnv build() const {
        vd::DiscoveryEnv e;
        auto ex = existing;
        e.path_exists = [ex](const fs::path& p) {
            return ex.count(p.string()) > 0;
        };
        e.path_executable = [ex](const fs::path& p) {
            return ex.count(p.string()) > 0;
        };
        auto ow = owners;
        e.path_owner_uid = [ow](const fs::path& p) -> uint32_t {
            auto it = ow.find(p.string());
            if (it == ow.end()) return std::numeric_limits<uint32_t>::max();
            return it->second;
        };
        e.current_uid = current_uid;
        auto vv = verdicts;
        auto vt = verdict_text;
        e.assess_signature = [vv, vt](const fs::path& p,
                                      const std::string& /*tool*/,
                                      std::string& verdict_line) {
            auto it = vt.find(p.string());
            verdict_line = (it == vt.end()) ? std::string{} : it->second;
            auto vit = vv.find(p.string());
            return vit != vv.end() && vit->second;
        };
        auto pr = path_resolve;
        e.resolve_in_path = [pr](const std::string& t) -> fs::path {
            auto it = pr.find(t);
            return it == pr.end() ? fs::path{} : it->second;
        };
        e.home_dir = home;
        return e;
    }
};

const vd::ValidatorReport& find_report(
    const std::vector<vd::ValidatorReport>& rs, const std::string& name) {
    for (const auto& r : rs) {
        if (r.name == name) return r;
    }
    FAIL("no report for validator '" + name + "'");
    // unreachable — FAIL throws, but compilers want a return path.
    return rs.front();
}

}  // namespace

TEST_CASE("healthy cask install reports OK for every validator",
          "[doctor][validators][issue-743]") {
    // Scenario (a) — each validator's first-priority existing path
    // passes spctl assessment, everything else is absent.
    StubEnv env;
    // auval: /usr/bin/auval, user-owned, accepted
    env.existing.insert("/usr/bin/auval");
    env.owners["/usr/bin/auval"] = env.current_uid;
    env.verdicts["/usr/bin/auval"] = true;
    env.verdict_text["/usr/bin/auval"] =
        "/usr/bin/auval: accepted\nsource=Apple System";

    // pluginval — use the cask bundle path, not the rip target.
    env.existing.insert(
        "/Applications/pluginval.app/Contents/MacOS/pluginval");
    env.owners["/Applications/pluginval.app/Contents/MacOS/pluginval"] = 0;
    env.verdicts["/Applications/pluginval.app/Contents/MacOS/pluginval"] = true;
    env.verdict_text["/Applications/pluginval.app/Contents/MacOS/pluginval"] =
        "accepted\nsource=Notarized Developer ID";

    // clap-validator — cargo install location.
    env.existing.insert("/home/test/.cargo/bin/clap-validator");
    env.owners["/home/test/.cargo/bin/clap-validator"] = env.current_uid;
    env.verdicts["/home/test/.cargo/bin/clap-validator"] = true;
    env.verdict_text["/home/test/.cargo/bin/clap-validator"] = "accepted";

    auto reports = vd::discover_validators(env.build());
    REQUIRE(reports.size() == 3);
    for (const auto& r : reports) {
        REQUIRE(r.status == vd::ValidatorStatus::Healthy);
    }
    REQUIRE(vd::compute_exit_code(reports) == 0);
    REQUIRE_FALSE(vd::has_broken_validator(reports));

    auto text = vd::render_report(reports, /*use_color=*/false);
    REQUIRE(text.find("auval: /usr/bin/auval") != std::string::npos);
    REQUIRE(text.find("pluginval: /Applications/pluginval.app") !=
            std::string::npos);
    REQUIRE(text.find("clap-validator: /home/test/.cargo") !=
            std::string::npos);
    // The ✓ path should not print a remediation line.
    REQUIRE(text.find("fix:") == std::string::npos);
}

TEST_CASE("user-owned ripped-out-of-bundle pluginval is auto-fixable",
          "[doctor][validators][issue-743]") {
    // Scenario (b). pluginval exists at /usr/local/bin/pluginval (the
    // historical Homebrew formula path), owned by the current user,
    // spctl rejects it with the classic "invalid resource directory"
    // verdict. The cask bundle is not installed. auval + clap-validator
    // are also present and healthy so we can assert the exit code
    // flips solely on pluginval.
    StubEnv env;
    env.existing.insert("/usr/local/bin/pluginval");
    env.owners["/usr/local/bin/pluginval"] = env.current_uid;
    env.verdicts["/usr/local/bin/pluginval"] = false;
    env.verdict_text["/usr/local/bin/pluginval"] =
        "/usr/local/bin/pluginval: rejected\n"
        "source=invalid resource directory (directory or signature have been modified)";

    env.existing.insert("/usr/bin/auval");
    env.owners["/usr/bin/auval"] = 0;
    env.verdicts["/usr/bin/auval"] = true;
    env.verdict_text["/usr/bin/auval"] = "accepted";

    env.existing.insert("/home/test/.cargo/bin/clap-validator");
    env.owners["/home/test/.cargo/bin/clap-validator"] = env.current_uid;
    env.verdicts["/home/test/.cargo/bin/clap-validator"] = true;
    env.verdict_text["/home/test/.cargo/bin/clap-validator"] = "accepted";

    auto reports = vd::discover_validators(env.build());
    const auto& pv = find_report(reports, "pluginval");
    REQUIRE(pv.status == vd::ValidatorStatus::Broken);
    REQUIRE(pv.path == fs::path("/usr/local/bin/pluginval"));
    REQUIRE(pv.ownership == vd::PathOwnership::UserOwned);
    REQUIRE(pv.fixable_without_sudo);
    REQUIRE(pv.remediation == "rm /usr/local/bin/pluginval");
    REQUIRE(vd::has_broken_validator(reports));
    REQUIRE(vd::compute_exit_code(reports) == 1);

    auto text = vd::render_report(reports, false);
    REQUIRE(text.find("Auto-fixable") != std::string::npos);
    REQUIRE(text.find("invalid resource directory") != std::string::npos);

    // --fix --dry-run: report the count, don't mutate.
    auto dry = reports;
    auto outcome_dry = vd::apply_fixes(dry, /*dry_run=*/true);
    REQUIRE(outcome_dry.auto_fixed == 1);
    REQUIRE(outcome_dry.needs_sudo == 0);
    // Status should still be Broken (we only fake the apply).
    REQUIRE(find_report(dry, "pluginval").status ==
            vd::ValidatorStatus::Broken);
}

TEST_CASE("apply_fixes on user-owned broken copy actually removes the file",
          "[doctor][validators][issue-743]") {
    // End-to-end heal: materialise a real file so we can assert the
    // in-tree `apply_fixes` call runs `fs::remove`. This is the only
    // scenario that needs a real tempdir.
    TempDir tmp;
    auto fake = tmp.path / "usr-local-bin-pluginval";
    { std::ofstream f(fake); f << "not actually pluginval\n"; }

    std::vector<vd::ValidatorReport> reports;
    vd::ValidatorReport r;
    r.name = "pluginval";
    r.format = "VST3";
    r.status = vd::ValidatorStatus::Broken;
    r.path = fake;
    r.ownership = vd::PathOwnership::UserOwned;
    r.fixable_without_sudo = true;
    r.reason = "signature check failed";
    r.remediation = "rm " + fake.string();
    reports.push_back(std::move(r));

    REQUIRE(fs::exists(fake));
    auto outcome = vd::apply_fixes(reports, /*dry_run=*/false);
    REQUIRE(outcome.auto_fixed == 1);
    REQUIRE_FALSE(fs::exists(fake));
    // After a successful fix the status should flip to Missing so the
    // post-fix render advises reinstall.
    REQUIRE(reports.front().status == vd::ValidatorStatus::Missing);
    REQUIRE(reports.front().remediation.find("brew install") !=
            std::string::npos);
}

TEST_CASE("root-owned ripped-out-of-bundle pluginval needs sudo",
          "[doctor][validators][issue-743]") {
    // Scenario (c). Same shape as (b), but /usr/local/bin/pluginval is
    // owned by uid 0 (root). `apply_fixes` must not remove it even
    // with `--fix`; it must print the sudo one-liner and move on.
    StubEnv env;
    env.existing.insert("/usr/local/bin/pluginval");
    env.owners["/usr/local/bin/pluginval"] = 0;
    env.verdicts["/usr/local/bin/pluginval"] = false;
    env.verdict_text["/usr/local/bin/pluginval"] =
        "/usr/local/bin/pluginval: rejected\n"
        "source=invalid resource directory (directory or signature have been modified)";

    env.existing.insert("/usr/bin/auval");
    env.owners["/usr/bin/auval"] = 0;
    env.verdicts["/usr/bin/auval"] = true;
    env.verdict_text["/usr/bin/auval"] = "accepted";

    env.existing.insert("/home/test/.cargo/bin/clap-validator");
    env.owners["/home/test/.cargo/bin/clap-validator"] = env.current_uid;
    env.verdicts["/home/test/.cargo/bin/clap-validator"] = true;
    env.verdict_text["/home/test/.cargo/bin/clap-validator"] = "accepted";

    auto reports = vd::discover_validators(env.build());
    const auto& pv = find_report(reports, "pluginval");
    REQUIRE(pv.status == vd::ValidatorStatus::Broken);
    REQUIRE(pv.ownership == vd::PathOwnership::RootOrSystem);
    REQUIRE_FALSE(pv.fixable_without_sudo);
    REQUIRE(pv.remediation == "sudo rm /usr/local/bin/pluginval");

    auto text = vd::render_report(reports, false);
    REQUIRE(text.find("Root-owned") != std::string::npos);
    REQUIRE(text.find("sudo rm /usr/local/bin/pluginval") != std::string::npos);

    // --fix does NOT mutate root-owned broken paths.
    auto outcome = vd::apply_fixes(reports, /*dry_run=*/false);
    REQUIRE(outcome.auto_fixed == 0);
    REQUIRE(outcome.needs_sudo == 1);
    // Status must remain Broken — the file wasn't removed.
    REQUIRE(find_report(reports, "pluginval").status ==
            vd::ValidatorStatus::Broken);
}

TEST_CASE("missing validators report install command and contribute to exit",
          "[doctor][validators][issue-743]") {
    // Scenario (d). No validator binaries exist anywhere. Every report
    // is Missing with its install hint as remediation.
    StubEnv env;
    auto reports = vd::discover_validators(env.build());
    REQUIRE(reports.size() == 3);
    for (const auto& r : reports) {
        REQUIRE(r.status == vd::ValidatorStatus::Missing);
        REQUIRE_FALSE(r.remediation.empty());
    }
    REQUIRE(vd::compute_exit_code(reports) == 1);
    REQUIRE_FALSE(vd::has_broken_validator(reports));

    auto text = vd::render_report(reports, false);
    REQUIRE(text.find("auval: not installed") != std::string::npos);
    REQUIRE(text.find("pluginval: not installed") != std::string::npos);
    REQUIRE(text.find("clap-validator: not installed") != std::string::npos);
    REQUIRE(text.find("xcode-select --install") != std::string::npos);
    REQUIRE(text.find("brew install --cask pluginval") != std::string::npos);
    REQUIRE(text.find("cargo install clap-validator") != std::string::npos);
}

TEST_CASE("first-existing priority path wins even when a healthy copy is further down",
          "[doctor][validators][issue-743]") {
    // Regression guard: if the user has a broken /usr/local/bin/pluginval
    // AND a healthy /Applications/pluginval.app, the diagnostic must
    // surface the broken one — that's the path the user's shell will
    // dispatch, so that's the copy that will silently SIGKILL under
    // amfid. Masking it with the healthy cask path would defeat the
    // whole point of the check.
    StubEnv env;
    env.existing.insert("/usr/local/bin/pluginval");
    env.owners["/usr/local/bin/pluginval"] = env.current_uid;
    env.verdicts["/usr/local/bin/pluginval"] = false;
    env.verdict_text["/usr/local/bin/pluginval"] = "rejected: invalid resource directory";

    env.existing.insert(
        "/Applications/pluginval.app/Contents/MacOS/pluginval");
    env.owners["/Applications/pluginval.app/Contents/MacOS/pluginval"] = 0;
    env.verdicts["/Applications/pluginval.app/Contents/MacOS/pluginval"] = true;

    auto reports = vd::discover_validators(env.build());
    const auto& pv = find_report(reports, "pluginval");
    REQUIRE(pv.status == vd::ValidatorStatus::Broken);
    REQUIRE(pv.path == fs::path("/usr/local/bin/pluginval"));
}

TEST_CASE("apply_fixes is a no-op on a fully healthy environment",
          "[doctor][validators][issue-743]") {
    // `pulp doctor --validators --fix` on a healthy env must not mutate
    // anything and must exit 0. This is the acceptance criterion
    // "--fix is a no-op on a healthy env".
    StubEnv env;
    env.existing.insert("/usr/bin/auval");
    env.owners["/usr/bin/auval"] = env.current_uid;
    env.verdicts["/usr/bin/auval"] = true;
    env.verdict_text["/usr/bin/auval"] = "accepted";

    env.existing.insert(
        "/Applications/pluginval.app/Contents/MacOS/pluginval");
    env.owners["/Applications/pluginval.app/Contents/MacOS/pluginval"] = 0;
    env.verdicts["/Applications/pluginval.app/Contents/MacOS/pluginval"] = true;
    env.verdict_text["/Applications/pluginval.app/Contents/MacOS/pluginval"] = "accepted";

    env.existing.insert("/home/test/.cargo/bin/clap-validator");
    env.owners["/home/test/.cargo/bin/clap-validator"] = env.current_uid;
    env.verdicts["/home/test/.cargo/bin/clap-validator"] = true;
    env.verdict_text["/home/test/.cargo/bin/clap-validator"] = "accepted";

    auto reports = vd::discover_validators(env.build());
    auto outcome = vd::apply_fixes(reports, /*dry_run=*/false);
    REQUIRE(outcome.auto_fixed == 0);
    REQUIRE(outcome.needs_sudo == 0);
    REQUIRE(outcome.still_missing == 0);
    REQUIRE(outcome.healthy == 3);
    REQUIRE(vd::compute_exit_code(reports) == 0);
}

// test_design_import_designmd.cpp — DESIGN.md import test suite
//
// Covers the Phase 1 acceptance criteria from
// planning/2026-05-13-designmd-integration-plan.md. Tag set:
// [view][import][designmd][issue-1434]

#include <catch2/catch_test_macros.hpp>

#include <pulp/view/design_import.hpp>

#include "import_detect.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

using namespace pulp::view;

namespace {

#ifndef PULP_REPO_ROOT
#define PULP_REPO_ROOT "."
#endif

std::string read_fixture(const std::string& rel_path) {
    fs::path full = fs::path(PULP_REPO_ROOT) / rel_path;
    std::ifstream f(full);
    if (!f.is_open()) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string upstream_fixture() {
    return read_fixture("test/fixtures/imports/designmd/alpha/DESIGN.md");
}

std::string hand_authored_fixture() {
    return read_fixture("test/fixtures/imports/designmd/alpha/hand-authored.md");
}

bool has_diag_code(const std::vector<DesignMdDiagnostic>& diags, const std::string& code) {
    for (const auto& d : diags) if (d.code == code) return true;
    return false;
}

pulp::import_detect::ImportsManifest load_manifest() {
    auto text = read_fixture("compat.json");
    auto m = pulp::import_detect::parse_compat_json(text);
    REQUIRE(m.has_value());
    return *m;
}

}  // namespace

// (1) ── parses paws-and-paths frontmatter into populated tokens ────────
TEST_CASE("parse_designmd populates colors/typography/rounded/spacing/components on paws-and-paths",
          "[view][import][designmd][issue-1434]") {
    auto text = upstream_fixture();
    REQUIRE_FALSE(text.empty());

    auto result = parse_designmd(text);
    REQUIRE(result.had_frontmatter);
    REQUIRE(result.ir.source == DesignSource::designmd);

    // Paws & Paths defines well over 40 colors across surface/primary/etc.
    REQUIRE(result.ir.tokens.colors.size() > 40);
    // Mapped typography levels show up under typography.<level>.<field> string keys.
    bool has_typography = false;
    for (const auto& [k, _] : result.ir.tokens.strings) {
        if (k.rfind("typography.", 0) == 0) { has_typography = true; break; }
    }
    REQUIRE(has_typography);
    // Components are present.
    bool has_components = false;
    for (const auto& [k, _] : result.ir.tokens.strings) {
        if (k.rfind("components.", 0) == 0) { has_components = true; break; }
    }
    REQUIRE(has_components);
}

// (2) ── resolves nested color references in components section ────────
TEST_CASE("token references inside components resolve to primitive values",
          "[view][import][designmd][issue-1434]") {
    auto text = hand_authored_fixture();
    auto result = parse_designmd(text);
    auto it = result.ir.tokens.strings.find("components.button-primary.backgroundColor");
    REQUIRE(it != result.ir.tokens.strings.end());
    // {colors.primary} → "#1A1C1E" per the fixture
    REQUIRE(it->second == "#1A1C1E");
}

// (3) ── composite typography refs inside components are preserved ─────
TEST_CASE("composite typography refs inside components are preserved verbatim",
          "[view][import][designmd][issue-1434]") {
    auto text = hand_authored_fixture();
    auto result = parse_designmd(text);
    auto it = result.ir.tokens.strings.find("components.button-primary.typography");
    REQUIRE(it != result.ir.tokens.strings.end());
    // composite ref permitted only inside components; left as-is.
    REQUIRE(it->second == "{typography.body-md}");
}

// (4) ── group refs outside components emit a broken-ref diagnostic ────
TEST_CASE("non-component reference to a group emits broken-ref warning",
          "[view][import][designmd][issue-1434]") {
    // colors.accent → {colors.primary}: this is a valid in-group ref to a
    // primitive, which the parser resolves. The "group-ref" rejection
    // case is exercised when a value points at a bare group name
    // ("{colors}") — exercise that with a synthetic fixture.
    std::string yaml =
        "---\nname: Synthetic\ncolors:\n  primary: \"#000000\"\n  bad: \"{colors}\"\n---\n";
    auto result = parse_designmd(yaml);
    REQUIRE(has_diag_code(result.diagnostics, "broken-ref"));
}

// (5) ── DTCG export round-trip on paws-and-paths is non-empty + valid ─
TEST_CASE("export_w3c_tokens emits DTCG JSON from a parsed DESIGN.md",
          "[view][import][designmd][issue-1434]") {
    auto text = upstream_fixture();
    auto result = parse_designmd(text);
    auto theme = ir_tokens_to_theme(result.ir.tokens);
    auto dtcg = export_w3c_tokens(theme);
    REQUIRE_FALSE(dtcg.empty());
    // Surface a primary color marker — Paws & Paths sets primary to #855300.
    REQUIRE(dtcg.find("855300") != std::string::npos);
}

// (6) ── unknown section headings preserved without erroring ────────────
TEST_CASE("unknown section headings are preserved and do not error",
          "[view][import][designmd][issue-1434]") {
    auto text = hand_authored_fixture();
    auto result = parse_designmd(text);
    bool seen_iconography = false;
    for (const auto& s : result.sections) {
        if (s == "Iconography") { seen_iconography = true; break; }
    }
    REQUIRE(seen_iconography);
    // No error-severity diagnostic for the unknown section.
    for (const auto& d : result.diagnostics) {
        if (d.code == "unknown-section") {
            REQUIRE(d.severity != DesignMdSeverity::error);
        }
    }
}

// (7) ── duplicate section headings reject the file ────────────────────
TEST_CASE("duplicate ## section heading reports error-severity diagnostic",
          "[view][import][designmd][issue-1434]") {
    std::string text =
        "---\nname: Dup\ncolors:\n  primary: \"#000\"\n---\n"
        "## Colors\nFirst body.\n\n## Colors\nSecond body — duplicate!\n";
    auto result = parse_designmd(text);
    bool found = false;
    for (const auto& d : result.diagnostics) {
        if (d.code == "duplicate-section" && d.severity == DesignMdSeverity::error) {
            found = true;
            break;
        }
    }
    REQUIRE(found);
}

// (8) ── detector identifies a DESIGN.md as `designmd` source ─────────
TEST_CASE("detector recognizes upstream DESIGN.md fixture",
          "[view][import][designmd][issue-1434]") {
    auto manifest = load_manifest();
    auto snap = pulp::import_detect::snapshot_input(
        fs::path(PULP_REPO_ROOT) / "test/fixtures/imports/designmd/alpha/DESIGN.md");
    auto result = pulp::import_detect::detect(manifest, snap);
    REQUIRE(result.ok);
    REQUIRE(result.source == "designmd");
    REQUIRE(result.format_version == "alpha");
    REQUIRE(result.parser_version == "0.1");
}

// (9a) ── detector rejects Jekyll-style blog post ─────────────────────
TEST_CASE("detector rejects generic Markdown frontmatter (Jekyll decoy)",
          "[view][import][designmd][issue-1434]") {
    auto manifest = load_manifest();
    auto snap = pulp::import_detect::snapshot_input(
        fs::path(PULP_REPO_ROOT) / "test/fixtures/imports/designmd/alpha/decoys/jekyll/blog-post.md");
    auto result = pulp::import_detect::detect(manifest, snap);
    REQUIRE(result.source.empty());  // no match
}

// (9b) ── detector rejects DESIGN.md missing `name:` key ───────────────
TEST_CASE("detector rejects DESIGN.md without name: key (missing-name decoy)",
          "[view][import][designmd][issue-1434]") {
    auto manifest = load_manifest();
    auto snap = pulp::import_detect::snapshot_input(
        fs::path(PULP_REPO_ROOT) / "test/fixtures/imports/designmd/alpha/decoys/missing-name/DESIGN.md");
    auto result = pulp::import_detect::detect(manifest, snap);
    REQUIRE(result.source.empty());
}

// (9c) ── detector rejects DESIGN.md without any canonical token group ─
TEST_CASE("detector rejects DESIGN.md without token-group keys",
          "[view][import][designmd][issue-1434]") {
    auto manifest = load_manifest();
    auto snap = pulp::import_detect::snapshot_input(
        fs::path(PULP_REPO_ROOT) / "test/fixtures/imports/designmd/alpha/decoys/missing-token-groups/DESIGN.md");
    auto result = pulp::import_detect::detect(manifest, snap);
    REQUIRE(result.source.empty());
}

// (10) ── prose-only DESIGN.md emits empty tokens, no error ────────────
TEST_CASE("DESIGN.md with no frontmatter produces empty tokens + info diagnostic",
          "[view][import][designmd][issue-1434]") {
    std::string text = "# Brand Notes\n\nNo frontmatter; just prose.\n";
    auto result = parse_designmd(text);
    REQUIRE_FALSE(result.had_frontmatter);
    REQUIRE(result.ir.tokens.colors.empty());
    REQUIRE(result.ir.tokens.dimensions.empty());
    REQUIRE(has_diag_code(result.diagnostics, "no-frontmatter"));
}

// (11) ── fontFeature / fontVariation preserved verbatim ───────────────
TEST_CASE("fontFeature and fontVariation typography fields survive parse",
          "[view][import][designmd][issue-1434]") {
    auto text = hand_authored_fixture();
    auto result = parse_designmd(text);
    auto feat = result.ir.tokens.strings.find("typography.body-md.fontFeature");
    auto vari = result.ir.tokens.strings.find("typography.body-md.fontVariation");
    REQUIRE(feat != result.ir.tokens.strings.end());
    REQUIRE(vari != result.ir.tokens.strings.end());
    REQUIRE(feat->second == "ss01, cv11");
    REQUIRE(vari->second == "wght 400, slnt 0");
}

// (12) ── parse_design_source(\"designmd\") wires the enum value ──────
TEST_CASE("parse_design_source maps \"designmd\" → DesignSource::designmd",
          "[view][import][designmd][issue-1434]") {
    auto src = parse_design_source("designmd");
    REQUIRE(src.has_value());
    REQUIRE(*src == DesignSource::designmd);
    REQUIRE(std::string(design_source_name(DesignSource::designmd)) == "DESIGN.md");
}

// Tests for `extract_keyboard_shortcuts` + `serialize_detected_shortcuts`
// in design_import.cpp. Verifies the static-scan path on representative
// React patterns (inline JSX onKeyDown, window/document.addEventListener
// keydown, modifier combinations) lifted from the Spectr editor source.
//
// The extractor is regex-driven and lexical only — it does NOT evaluate
// handler bodies or resolve dynamic key references. Tests pin the
// recognized forms + verify de-dup + verify modifier normalization
// (metaKey || ctrlKey collapses to "meta", per the cross-platform idiom).

#include <catch2/catch_test_macros.hpp>
#include <pulp/view/design_import.hpp>
#include <algorithm>

using pulp::view::DetectedShortcut;
using pulp::view::extract_keyboard_shortcuts;
using pulp::view::serialize_detected_shortcuts;

TEST_CASE("extract_keyboard_shortcuts finds bare e.key === literal", "[design-import][shortcuts]") {
    auto out = extract_keyboard_shortcuts(
        R"JS(
            const onKey = (e) => { if (e.key === 'Escape') onClose(); };
            window.addEventListener('keydown', onKey);
        )JS", "editor.tsx");
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].key == "Escape");
    REQUIRE(out[0].modifiers.empty());
    REQUIRE(out[0].source_location.find("editor.tsx:") == 0);
    REQUIRE(out[0].handler_excerpt.find("onClose") != std::string::npos);
}

TEST_CASE("extract_keyboard_shortcuts handles inline onKeyDown JSX", "[design-import][shortcuts]") {
    auto out = extract_keyboard_shortcuts(
        R"JS(
            onKeyDown={e => { if (e.key === 'Enter') e.target.blur();
                              if (e.key === 'Escape') setEditName(false); }}
        )JS", "");
    REQUIRE(out.size() == 2);
    // Sorted by (key, modifiers) — Enter comes before Escape.
    REQUIRE(out[0].key == "Enter");
    REQUIRE(out[1].key == "Escape");
}

TEST_CASE("extract_keyboard_shortcuts captures meta modifier", "[design-import][shortcuts]") {
    auto out = extract_keyboard_shortcuts(
        R"JS(
            const onKey = (e) => {
                if ((e.metaKey || e.ctrlKey) && e.key === 's') save();
            };
        )JS", "");
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].key == "s");
    // metaKey || ctrlKey collapses to a single "meta" entry — that's the
    // cross-platform shortcut idiom we want native Pulp to bind once.
    REQUIRE(out[0].modifiers.size() == 1);
    REQUIRE(out[0].modifiers[0] == "meta");
}

TEST_CASE("extract_keyboard_shortcuts captures multiple modifiers", "[design-import][shortcuts]") {
    auto out = extract_keyboard_shortcuts(
        R"JS(
            if (e.shiftKey && e.altKey && e.key === 'F') openFlare();
        )JS", "");
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].key == "F");
    REQUIRE(out[0].modifiers.size() == 2);
    // collect_modifiers walks the window and adds in fixed order:
    // alt before shift (alphabetical by `add()` order in source).
    auto has = [&](const std::string& m) {
        return std::find(out[0].modifiers.begin(), out[0].modifiers.end(), m)
            != out[0].modifiers.end();
    };
    REQUIRE(has("alt"));
    REQUIRE(has("shift"));
}

TEST_CASE("extract_keyboard_shortcuts recognizes e.code variant", "[design-import][shortcuts]") {
    auto out = extract_keyboard_shortcuts(
        R"JS(
            if (e.code === 'ArrowLeft') prevTab();
        )JS", "");
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].key == "ArrowLeft");
}

TEST_CASE("extract_keyboard_shortcuts de-dupes same chord across branches", "[design-import][shortcuts]") {
    // Two checks for `e.key === 'Escape'` in different branches of the same
    // handler should produce one manifest entry, not two — the runtime
    // registers a single shortcut for the chord.
    auto out = extract_keyboard_shortcuts(
        R"JS(
            const onKey = (e) => {
                if (isEditing) { if (e.key === 'Escape') cancelEdit(); }
                else { if (e.key === 'Escape') closeOverlay(); }
            };
        )JS", "");
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].key == "Escape");
}

TEST_CASE("extract_keyboard_shortcuts returns empty on no match", "[design-import][shortcuts]") {
    auto out = extract_keyboard_shortcuts(
        R"JS(
            const x = 5;
            function helper() { return x * 2; }
        )JS", "");
    REQUIRE(out.empty());
}

TEST_CASE("extract_keyboard_shortcuts tolerates empty / whitespace input", "[design-import][shortcuts]") {
    REQUIRE(extract_keyboard_shortcuts("", "").empty());
    REQUIRE(extract_keyboard_shortcuts("   \n\t\n   ", "").empty());
}

TEST_CASE("serialize_detected_shortcuts emits stable JSON", "[design-import][shortcuts]") {
    std::vector<DetectedShortcut> shortcuts;
    DetectedShortcut a;
    a.key = "Escape";
    a.pattern = "e.key === 'Escape'";
    a.source_location = "editor.tsx:42";
    a.handler_excerpt = "onClose();";
    shortcuts.push_back(a);

    DetectedShortcut b;
    b.key = "s";
    b.modifiers = {"meta"};
    b.pattern = "e.metaKey && e.key === 's'";
    b.source_location = "editor.tsx:128";
    b.handler_excerpt = "save();";
    shortcuts.push_back(b);

    std::string json = serialize_detected_shortcuts(shortcuts);
    REQUIRE(json.find("\"key\": \"Escape\"") != std::string::npos);
    REQUIRE(json.find("\"key\": \"s\"") != std::string::npos);
    // The modifiers array — choc::json may use multi-line indentation, so
    // search for the substring `"meta"` after a `"modifiers":` key rather
    // than asserting an exact bracket-array form.
    auto mods_pos = json.find("\"modifiers\"");
    REQUIRE(mods_pos != std::string::npos);
    REQUIRE(json.find("\"meta\"", mods_pos) != std::string::npos);
    REQUIRE(json.find("\"source_location\": \"editor.tsx:42\"") != std::string::npos);
}

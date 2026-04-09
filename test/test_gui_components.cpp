#include <catch2/catch_test_macros.hpp>
#include <pulp/view/table.hpp>
#include <pulp/view/toolbar.hpp>
#include <pulp/view/concertina_panel.hpp>
#include <pulp/view/buttons.hpp>
#include <pulp/view/lasso.hpp>

using namespace pulp::view;

// ── SimpleTableModel ────────────────────────────────────────────────────

TEST_CASE("SimpleTableModel stores and retrieves data", "[gui][table]") {
    SimpleTableModel model;
    model.add_row({"Alice", "25", "Engineer"});
    model.add_row({"Bob", "30", "Designer"});
    model.add_row({"Charlie", "22", "Intern"});

    REQUIRE(model.row_count() == 3);
    REQUIRE(model.cell_text(0, 0) == "Alice");
    REQUIRE(model.cell_text(1, 1) == "30");
    REQUIRE(model.cell_text(2, 2) == "Intern");
}

TEST_CASE("SimpleTableModel sorts by column", "[gui][table]") {
    SimpleTableModel model;
    model.add_row({"Charlie", "22"});
    model.add_row({"Alice", "25"});
    model.add_row({"Bob", "30"});

    REQUIRE(model.sort(0, true));
    REQUIRE(model.cell_text(0, 0) == "Alice");
    REQUIRE(model.cell_text(1, 0) == "Bob");
    REQUIRE(model.cell_text(2, 0) == "Charlie");

    REQUIRE(model.sort(0, false));
    REQUIRE(model.cell_text(0, 0) == "Charlie");
}

TEST_CASE("SimpleTableModel out-of-bounds returns empty", "[gui][table]") {
    SimpleTableModel model;
    model.add_row({"a"});
    REQUIRE(model.cell_text(-1, 0) == "");
    REQUIRE(model.cell_text(0, 5) == "");
    REQUIRE(model.cell_text(99, 0) == "");
}

TEST_CASE("TableListBox selection", "[gui][table]") {
    TableListBox table;
    SimpleTableModel model;
    model.add_row({"Row 0"});
    model.add_row({"Row 1"});
    model.add_row({"Row 2"});

    table.set_model(&model);
    table.add_column({"Name", 200.0f});

    REQUIRE(table.selected_row() == -1);
    table.set_selected_row(1);
    REQUIRE(table.selected_row() == 1);
}

TEST_CASE("TableListBox column management", "[gui][table]") {
    TableListBox table;
    REQUIRE(table.column_count() == 0);

    table.add_column({"Name", 150.0f});
    table.add_column({"Value", 100.0f});
    REQUIRE(table.column_count() == 2);

    table.clear_columns();
    REQUIRE(table.column_count() == 0);
}

// ── Toolbar ─────────────────────────────────────────────────────────────
//
// Historical note: these tests were previously tagged `[!mayfail]` and
// `Toolbar add items` was excluded from the CI regex after an earlier
// version of ToolStrip had a name-collision crash. Both problems were
// fixed in commits 6ae96a5b and 4bf62949 but the tags were never cleaned
// up. The quality pass removes the tags and un-excludes the test from CI
// once the full suite has been confirmed green with `Toolbar add items`
// re-enabled. Behavioral coverage below also grows from "add/remove
// counts" into real hit-test + on_mouse_down exercises that would have
// caught the original crash.

TEST_CASE("Toolbar add items", "[gui][toolbar]") {
    ToolStrip toolbar;
    REQUIRE(toolbar.item_count() == 0);

    toolbar.add_button("play", "Play", []() {});
    toolbar.add_separator();
    toolbar.add_toggle("loop", "Loop", [](bool) {});
    toolbar.add_spacer();

    REQUIRE(toolbar.item_count() == 4);
}

TEST_CASE("Toolbar toggle state", "[gui][toolbar]") {
    ToolStrip toolbar;
    toolbar.add_toggle("mute", "Mute", [](bool) {});

    REQUIRE_FALSE(toolbar.is_toggled("mute"));
    toolbar.set_toggled("mute", true);
    REQUIRE(toolbar.is_toggled("mute"));
}

TEST_CASE("Toolbar set_enabled flag is queryable via add/click", "[gui][toolbar]") {
    // This test used to be a dead test — it set enabled=false and then
    // asserted nothing. The real contract is "a disabled button does
    // not fire its on_click when hit", and that's verified here via
    // on_mouse_down rather than with a manual call.
    ToolStrip toolbar;
    int click_count = 0;
    toolbar.add_button("save", "Save", [&]() { ++click_count; });
    toolbar.set_bounds({0, 0, 300, 36});

    // Baseline: enabled button clicks through
    toolbar.on_mouse_down({18.0f, 18.0f});  // inside first item (x ∈ [4, 32))
    REQUIRE(click_count == 1);

    // Disabled: click should NOT fire
    toolbar.set_enabled("save", false);
    toolbar.on_mouse_down({18.0f, 18.0f});
    REQUIRE(click_count == 1);  // unchanged

    // Re-enable: click fires again
    toolbar.set_enabled("save", true);
    toolbar.on_mouse_down({18.0f, 18.0f});
    REQUIRE(click_count == 2);
}

TEST_CASE("Toolbar remove item", "[gui][toolbar]") {
    ToolStrip toolbar;
    toolbar.add_button("a", "A", []() {});
    toolbar.add_button("b", "B", []() {});
    REQUIRE(toolbar.item_count() == 2);

    toolbar.remove_item("a");
    REQUIRE(toolbar.item_count() == 1);
}

TEST_CASE("Toolbar on_mouse_down fires on_click for horizontal button hit", "[gui][toolbar]") {
    ToolStrip toolbar;
    toolbar.set_bounds({0, 0, 400, 36});

    int play_hits = 0;
    int stop_hits = 0;
    toolbar.add_button("play", "Play", [&]() { ++play_hits; });
    toolbar.add_button("stop", "Stop", [&]() { ++stop_hits; });

    // First item starts at x=spacing_=4, width=item_size_=28 → x ∈ [4, 32)
    toolbar.on_mouse_down({16.0f, 16.0f});
    REQUIRE(play_hits == 1);
    REQUIRE(stop_hits == 0);

    // Second item starts at x=4+28+4=36 → x ∈ [36, 64)
    toolbar.on_mouse_down({48.0f, 16.0f});
    REQUIRE(play_hits == 1);
    REQUIRE(stop_hits == 1);

    // Gap between items (x=33) should hit nothing
    toolbar.on_mouse_down({33.0f, 16.0f});
    REQUIRE(play_hits == 1);
    REQUIRE(stop_hits == 1);
}

TEST_CASE("Toolbar on_mouse_down flips toggle and fires on_toggle", "[gui][toolbar]") {
    ToolStrip toolbar;
    toolbar.set_bounds({0, 0, 200, 36});

    bool last_state = false;
    int toggle_events = 0;
    toolbar.add_toggle("mute", "Mute", [&](bool on) {
        last_state = on;
        ++toggle_events;
    });

    REQUIRE_FALSE(toolbar.is_toggled("mute"));

    toolbar.on_mouse_down({16.0f, 16.0f});
    REQUIRE(toolbar.is_toggled("mute"));
    REQUIRE(last_state);
    REQUIRE(toggle_events == 1);

    toolbar.on_mouse_down({16.0f, 16.0f});
    REQUIRE_FALSE(toolbar.is_toggled("mute"));
    REQUIRE_FALSE(last_state);
    REQUIRE(toggle_events == 2);
}

TEST_CASE("Toolbar disabled toggle does not flip", "[gui][toolbar]") {
    ToolStrip toolbar;
    toolbar.set_bounds({0, 0, 200, 36});

    int toggle_events = 0;
    toolbar.add_toggle("mute", "Mute", [&](bool) { ++toggle_events; });
    toolbar.set_enabled("mute", false);

    toolbar.on_mouse_down({16.0f, 16.0f});

    REQUIRE_FALSE(toolbar.is_toggled("mute"));
    REQUIRE(toggle_events == 0);
}

TEST_CASE("Toolbar separator and spacer slots don't deliver clicks", "[gui][toolbar]") {
    // Hit-testing across a separator or spacer must still correctly
    // place subsequent items; a click between items must not spill
    // into either the preceding or following item.
    ToolStrip toolbar;
    toolbar.set_bounds({0, 0, 400, 36});

    int a_hits = 0;
    int b_hits = 0;
    toolbar.add_button("a", "A", [&]() { ++a_hits; });
    toolbar.add_separator();          // advances hit-test cursor by 2 * spacing_
    toolbar.add_button("b", "B", [&]() { ++b_hits; });

    // Item A is at x ∈ [4, 32). Click comfortably inside:
    toolbar.on_mouse_down({10.0f, 16.0f});
    REQUIRE(a_hits == 1);

    // After A: cursor at 32 + 4 = 36 (spacing after A). Separator
    // consumes 2 * spacing_ = 8, landing at 44. Item B: [44, 72).
    toolbar.on_mouse_down({50.0f, 16.0f});
    REQUIRE(b_hits == 1);

    // Clicking inside the separator zone (x=38) must not fire anything
    toolbar.on_mouse_down({38.0f, 16.0f});
    REQUIRE(a_hits == 1);
    REQUIRE(b_hits == 1);
}

TEST_CASE("Toolbar vertical orientation hit-tests on y axis", "[gui][toolbar]") {
    ToolStrip toolbar;
    toolbar.set_orientation(ToolStrip::Orientation::vertical);
    toolbar.set_bounds({0, 0, 40, 400});

    int clicks = 0;
    toolbar.add_button("play", "Play", [&]() { ++clicks; });

    // Vertical layout: first item at y=spacing_=4, item_size_=28 → y ∈ [4, 32)
    // The x coordinate is ignored in vertical hit-test
    toolbar.on_mouse_down({999.0f, 16.0f});
    REQUIRE(clicks == 1);

    // Below the item: no hit
    toolbar.on_mouse_down({999.0f, 100.0f});
    REQUIRE(clicks == 1);
}

TEST_CASE("Toolbar remove_item by id updates subsequent hit-test positions", "[gui][toolbar]") {
    // Removing an earlier item must shift later items up to the newly
    // vacated slot so hit-testing still lands on them.
    ToolStrip toolbar;
    toolbar.set_bounds({0, 0, 400, 36});

    int a_hits = 0;
    int b_hits = 0;
    toolbar.add_button("a", "A", [&]() { ++a_hits; });
    toolbar.add_button("b", "B", [&]() { ++b_hits; });

    // Before: B is at x ∈ [36, 64)
    toolbar.on_mouse_down({50.0f, 16.0f});
    REQUIRE(b_hits == 1);

    // Remove A — B should now be at x ∈ [4, 32)
    toolbar.remove_item("a");
    REQUIRE(toolbar.item_count() == 1);

    toolbar.on_mouse_down({16.0f, 16.0f});
    REQUIRE(a_hits == 0);
    REQUIRE(b_hits == 2);
}

TEST_CASE("Toolbar is_toggled returns false for unknown id", "[gui][toolbar]") {
    ToolStrip toolbar;
    REQUIRE_FALSE(toolbar.is_toggled("nonexistent"));
    // Setting a missing id is a no-op, not a crash
    toolbar.set_toggled("still_missing", true);
    REQUIRE_FALSE(toolbar.is_toggled("still_missing"));
    toolbar.set_enabled("still_missing", false);  // also a no-op
}

// ── ConcertinaPanel ─────────────────────────────────────────────────────

TEST_CASE("ConcertinaPanel add sections", "[gui][concertina]") {
    ConcertinaPanel panel;
    REQUIRE(panel.section_count() == 0);

    panel.add_section("Section A", nullptr, true);
    panel.add_section("Section B", nullptr, false);

    REQUIRE(panel.section_count() == 2);
    REQUIRE(panel.is_expanded(0));
    REQUIRE_FALSE(panel.is_expanded(1));
}

TEST_CASE("ConcertinaPanel toggle", "[gui][concertina]") {
    ConcertinaPanel panel;
    panel.add_section("Test", nullptr, false);

    REQUIRE_FALSE(panel.is_expanded(0));
    panel.toggle(0);
    REQUIRE(panel.is_expanded(0));
    panel.toggle(0);
    REQUIRE_FALSE(panel.is_expanded(0));
}

TEST_CASE("ConcertinaPanel exclusive mode", "[gui][concertina]") {
    ConcertinaPanel panel;
    panel.set_exclusive(true);
    panel.add_section("A", nullptr, true);
    panel.add_section("B", nullptr, false);

    REQUIRE(panel.is_expanded(0));
    REQUIRE_FALSE(panel.is_expanded(1));

    panel.expand(1);
    REQUIRE_FALSE(panel.is_expanded(0));  // Auto-collapsed
    REQUIRE(panel.is_expanded(1));
}

// ── TextButton ──────────────────────────────────────────────────────────

TEST_CASE("TextButton click callback", "[gui][button]") {
    TextButton btn("Click Me");
    REQUIRE(btn.label() == "Click Me");

    bool clicked = false;
    btn.on_click = [&]() { clicked = true; };

    btn.on_mouse_down({0, 0});
    REQUIRE(clicked);
}

TEST_CASE("TextButton disabled doesn't fire", "[gui][button]") {
    TextButton btn("Disabled");
    btn.set_enabled(false);

    bool clicked = false;
    btn.on_click = [&]() { clicked = true; };

    btn.on_mouse_down({0, 0});
    REQUIRE_FALSE(clicked);
}

// ── ArrowButton ─────────────────────────────────────────────────────────

TEST_CASE("ArrowButton direction and click", "[gui][button]") {
    ArrowButton btn(ArrowDirection::down);
    REQUIRE(btn.direction() == ArrowDirection::down);

    bool clicked = false;
    btn.on_click = [&]() { clicked = true; };

    btn.on_mouse_down({0, 0});
    REQUIRE(clicked);
}

// ── LassoComponent ──────────────────────────────────────────────────────

TEST_CASE("LassoComponent selection rect", "[gui][lasso]") {
    LassoComponent lasso;
    REQUIRE_FALSE(lasso.is_active());

    lasso.begin_selection(10.0f, 20.0f);
    REQUIRE(lasso.is_active());

    lasso.update_selection(50.0f, 60.0f);
    auto rect = lasso.selection_rect();
    REQUIRE(rect.x == 10.0f);
    REQUIRE(rect.y == 20.0f);
    REQUIRE(rect.width == 40.0f);
    REQUIRE(rect.height == 40.0f);

    lasso.end_selection();
    REQUIRE_FALSE(lasso.is_active());
}

TEST_CASE("LassoComponent handles reverse drag", "[gui][lasso]") {
    LassoComponent lasso;
    lasso.begin_selection(50.0f, 50.0f);
    lasso.update_selection(10.0f, 10.0f);

    auto rect = lasso.selection_rect();
    REQUIRE(rect.x == 10.0f);
    REQUIRE(rect.y == 10.0f);
    REQUIRE(rect.width == 40.0f);
    REQUIRE(rect.height == 40.0f);
}

TEST_CASE("LassoComponent callback fires", "[gui][lasso]") {
    LassoComponent lasso;
    SelectionRect last_rect;

    lasso.on_selection_changed = [&](const SelectionRect& r) { last_rect = r; };

    lasso.begin_selection(0, 0);
    lasso.update_selection(100, 100);

    REQUIRE(last_rect.width == 100.0f);
    REQUIRE(last_rect.height == 100.0f);
}

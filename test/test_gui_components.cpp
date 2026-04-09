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
//
// Behavioral coverage for the accordion-style stack. The original tests
// only exercised section count, toggle(), and exclusive mode by direct
// API call. The quality pass adds:
//   - on_mouse_down hit-testing for headers (the user-facing path)
//   - on_mouse_down ignoring the content area between headers
//   - on_mouse_down beyond the bottom of all sections is a no-op
//   - layout_sections positions content view bounds correctly
//   - non-exclusive mode lets multiple sections be open simultaneously
//   - direct collapse() works
//   - bounds-checked expand/collapse/toggle/is_expanded with bad indices
//   - add_section with a content view starts hidden when initially_expanded=false
//   - content view visibility tracks expand/collapse state changes

namespace {
// A minimal View subclass with a fixed intrinsic height so we can verify
// layout_sections positions it correctly without dragging in real widgets.
class FixedHeightView : public View {
public:
    explicit FixedHeightView(float h) : intrinsic_h_(h) {}
    float intrinsic_height() const override { return intrinsic_h_; }
    void paint(pulp::canvas::Canvas&) override {}
private:
    float intrinsic_h_;
};
}  // namespace

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

TEST_CASE("ConcertinaPanel non-exclusive lets multiple expand", "[gui][concertina]") {
    ConcertinaPanel panel;
    REQUIRE_FALSE(panel.is_exclusive());
    panel.add_section("A", nullptr, false);
    panel.add_section("B", nullptr, false);
    panel.add_section("C", nullptr, false);

    panel.expand(0);
    panel.expand(1);
    panel.expand(2);

    REQUIRE(panel.is_expanded(0));
    REQUIRE(panel.is_expanded(1));
    REQUIRE(panel.is_expanded(2));
}

TEST_CASE("ConcertinaPanel collapse() directly", "[gui][concertina]") {
    ConcertinaPanel panel;
    panel.add_section("A", nullptr, true);
    REQUIRE(panel.is_expanded(0));

    panel.collapse(0);
    REQUIRE_FALSE(panel.is_expanded(0));
}

TEST_CASE("ConcertinaPanel out-of-bounds index is a no-op", "[gui][concertina]") {
    ConcertinaPanel panel;
    panel.add_section("Only", nullptr, false);

    // is_expanded with bad index returns false instead of crashing or UB
    REQUIRE_FALSE(panel.is_expanded(-1));
    REQUIRE_FALSE(panel.is_expanded(99));
    REQUIRE_FALSE(panel.is_expanded(panel.section_count()));

    // expand/collapse/toggle with bad index leave state unchanged
    panel.expand(-5);
    panel.collapse(42);
    panel.toggle(99);
    REQUIRE(panel.section_count() == 1);
    REQUIRE_FALSE(panel.is_expanded(0));  // unchanged
}

TEST_CASE("ConcertinaPanel on_mouse_down on header toggles section", "[gui][concertina]") {
    ConcertinaPanel panel;
    panel.set_bounds({0, 0, 200, 600});
    panel.add_section("First", nullptr, false);
    panel.add_section("Second", nullptr, false);

    // header_height_ = 30 by default
    // Section 0 header: y ∈ [0, 30)
    // Section 1 header: y ∈ [30, 60) (because section 0 collapsed)
    REQUIRE_FALSE(panel.is_expanded(0));
    panel.on_mouse_down({100.0f, 10.0f});  // inside section 0 header
    REQUIRE(panel.is_expanded(0));

    // Re-clicking the same header collapses it
    panel.on_mouse_down({100.0f, 10.0f});
    REQUIRE_FALSE(panel.is_expanded(0));

    // Click section 1 header (still at y ∈ [30, 60) since 0 is collapsed)
    panel.on_mouse_down({100.0f, 40.0f});
    REQUIRE(panel.is_expanded(1));
}

TEST_CASE("ConcertinaPanel on_mouse_down skips expanded content area", "[gui][concertina]") {
    ConcertinaPanel panel;
    panel.set_bounds({0, 0, 200, 600});

    auto content = std::make_unique<FixedHeightView>(80.0f);
    panel.add_section("First", std::move(content), true);
    panel.add_section("Second", nullptr, false);

    // Section 0 header: y ∈ [0, 30)
    // Section 0 content: y ∈ [30, 110)  (content_h = 80)
    // Section 1 header: y ∈ [110, 140)
    REQUIRE(panel.is_expanded(0));
    REQUIRE_FALSE(panel.is_expanded(1));

    // Click in the content area of section 0 — must NOT toggle section 1
    panel.on_mouse_down({100.0f, 60.0f});
    REQUIRE(panel.is_expanded(0));  // unchanged
    REQUIRE_FALSE(panel.is_expanded(1));  // unchanged

    // Click section 1 header (y=120 lands inside [110, 140))
    panel.on_mouse_down({100.0f, 120.0f});
    REQUIRE(panel.is_expanded(0));  // still expanded (non-exclusive)
    REQUIRE(panel.is_expanded(1));
}

TEST_CASE("ConcertinaPanel on_mouse_down below all sections is a no-op", "[gui][concertina]") {
    ConcertinaPanel panel;
    panel.set_bounds({0, 0, 200, 600});
    panel.add_section("Only", nullptr, false);

    // Click way below the single header — no toggle
    panel.on_mouse_down({100.0f, 500.0f});
    REQUIRE_FALSE(panel.is_expanded(0));
}

TEST_CASE("ConcertinaPanel layout_sections positions content view bounds", "[gui][concertina]") {
    ConcertinaPanel panel;
    panel.set_bounds({0, 0, 240, 600});

    auto content = std::make_unique<FixedHeightView>(120.0f);
    auto* content_raw = content.get();
    panel.add_section("Section", std::move(content), true);

    panel.layout_sections();

    // Content view should be placed below the header at full panel width
    auto b = content_raw->bounds();
    REQUIRE(b.x == 0.0f);
    REQUIRE(b.y == panel.header_height());  // 30 by default
    REQUIRE(b.width == 240.0f);
    REQUIRE(b.height == 120.0f);
}

TEST_CASE("ConcertinaPanel content view visibility tracks expand/collapse", "[gui][concertina]") {
    ConcertinaPanel panel;
    auto content = std::make_unique<FixedHeightView>(50.0f);
    auto* content_raw = content.get();

    panel.add_section("Test", std::move(content), false);
    REQUIRE_FALSE(content_raw->visible());  // started collapsed

    panel.expand(0);
    REQUIRE(content_raw->visible());

    panel.collapse(0);
    REQUIRE_FALSE(content_raw->visible());

    panel.toggle(0);
    REQUIRE(content_raw->visible());
}

TEST_CASE("ConcertinaPanel exclusive mode hides other content views", "[gui][concertina]") {
    ConcertinaPanel panel;
    panel.set_exclusive(true);

    auto a_content = std::make_unique<FixedHeightView>(40.0f);
    auto b_content = std::make_unique<FixedHeightView>(40.0f);
    auto* a_raw = a_content.get();
    auto* b_raw = b_content.get();

    panel.add_section("A", std::move(a_content), true);
    panel.add_section("B", std::move(b_content), false);

    REQUIRE(a_raw->visible());
    REQUIRE_FALSE(b_raw->visible());

    panel.expand(1);

    REQUIRE_FALSE(a_raw->visible());  // exclusive mode collapsed A
    REQUIRE(b_raw->visible());
}

TEST_CASE("ConcertinaPanel custom header height affects hit testing", "[gui][concertina]") {
    ConcertinaPanel panel;
    panel.set_bounds({0, 0, 200, 600});
    panel.set_header_height(50.0f);
    panel.add_section("First", nullptr, false);
    panel.add_section("Second", nullptr, false);

    // Both sections collapsed, so:
    //   Section 0 header: y ∈ [0, 50)
    //   Section 1 header: y ∈ [50, 100)
    // The default 30px header would NOT match these clicks; this test
    // confirms set_header_height(50) actually drives hit-testing.
    panel.on_mouse_down({100.0f, 25.0f});  // inside section 0 header
    REQUIRE(panel.is_expanded(0));

    // Re-collapse 0 so section 1's header stays at y ∈ [50, 100)
    panel.collapse(0);
    panel.on_mouse_down({100.0f, 75.0f});  // inside section 1 header
    REQUIRE(panel.is_expanded(1));

    // A click at y=35 (where the OLD 30px hit-test would have placed
    // section 1's header) must hit section 0, NOT section 1, with the
    // new 50px height
    panel.collapse(1);
    panel.on_mouse_down({100.0f, 35.0f});
    REQUIRE(panel.is_expanded(0));
    REQUIRE_FALSE(panel.is_expanded(1));
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

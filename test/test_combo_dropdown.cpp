// Automated test for ComboBox dropdown interaction
#include <catch2/catch_test_macros.hpp>
#include <pulp/view/ui_components.hpp>
#include <pulp/view/input_events.hpp>
#include <pulp/view/theme.hpp>
#include <pulp/canvas/canvas.hpp>

using namespace pulp::view;
using pulp::canvas::RecordingCanvas;

TEST_CASE("ComboBox: set_items and selected_text", "[combo]") {
    ComboBox combo;
    combo.set_items({"Lowpass", "Highpass", "Bandpass"});
    combo.set_selected(0);
    REQUIRE(combo.selected_text() == "Lowpass");
    combo.set_selected(2);
    REQUIRE(combo.selected_text() == "Bandpass");
}

TEST_CASE("ComboBox: on_change fires on selection", "[combo]") {
    ComboBox combo;
    combo.set_items({"A", "B", "C"});
    combo.set_selected(0);

    int changed_to = -1;
    combo.on_change = [&](int idx) { changed_to = idx; };

    combo.set_selected(1);
    REQUIRE(changed_to == 1);
}

TEST_CASE("ComboBox: click opens, click item selects", "[combo]") {
    ComboBox combo;
    combo.set_items({"One", "Two", "Three"});
    combo.set_selected(0);
    combo.set_bounds({0, 0, 140, 120}); // tall enough for dropdown

    int changed_to = -1;
    combo.on_change = [&](int idx) { changed_to = idx; };

    // Click to open
    MouseEvent open_click;
    open_click.position = {70, 14}; // center of combo header
    open_click.is_down = true;
    combo.on_mouse_event(open_click);
    // Should be open now (internal state)

    // Click on second item (y = 30 + 24 = ~54)
    MouseEvent select_click;
    select_click.position = {70, 54};
    select_click.is_down = true;
    combo.on_mouse_event(select_click);

    REQUIRE(changed_to == 1);
    REQUIRE(combo.selected_text() == "Two");
}

TEST_CASE("ComboBox: paints with correct tokens", "[combo]") {
    RecordingCanvas rc;
    ComboBox combo;
    combo.set_theme(Theme::dark());
    combo.set_items({"Test"});
    combo.set_selected(0);
    combo.set_bounds({0, 0, 140, 28});
    combo.paint(rc);
    REQUIRE(rc.commands().size() > 0);
}

TEST_CASE("ComboBox: keyboard up/down changes selection", "[combo]") {
    ComboBox combo;
    combo.set_items({"A", "B", "C"});
    combo.set_selected(0);

    KeyEvent down;
    down.key = KeyCode::down;
    down.is_down = true;
    combo.on_key_event(down);
    REQUIRE(combo.selected_text() == "B");

    combo.on_key_event(down);
    REQUIRE(combo.selected_text() == "C");

    KeyEvent up;
    up.key = KeyCode::up;
    up.is_down = true;
    combo.on_key_event(up);
    REQUIRE(combo.selected_text() == "B");
}

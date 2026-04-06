// Keyboard event tests — validates key events and modifier handling

#include <catch2/catch_test_macros.hpp>
#include "test_helpers.hpp"

using namespace pulp::test;
using namespace pulp::view;

TEST_CASE("Keyboard: KeyEvent default state", "[events][keyboard]") {
    KeyEvent ke;
    REQUIRE(ke.key == KeyCode::unknown);
    REQUIRE(ke.modifiers == 0);
    REQUIRE(ke.is_down == true);
    REQUIRE(ke.is_repeat == false);
}

TEST_CASE("Keyboard: shift modifier flag", "[events][keyboard]") {
    KeyEvent ke;
    ke.modifiers = kModShift;
    REQUIRE(ke.isShiftDown());
    REQUIRE_FALSE(ke.isCtrlDown());
    REQUIRE_FALSE(ke.isAltDown());
    REQUIRE_FALSE(ke.isCmdDown());
}

TEST_CASE("Keyboard: ctrl modifier flag", "[events][keyboard]") {
    KeyEvent ke;
    ke.modifiers = kModCtrl;
    REQUIRE(ke.isCtrlDown());
    REQUIRE_FALSE(ke.isShiftDown());
}

TEST_CASE("Keyboard: alt modifier flag", "[events][keyboard]") {
    KeyEvent ke;
    ke.modifiers = kModAlt;
    REQUIRE(ke.isAltDown());
}

TEST_CASE("Keyboard: cmd modifier flag", "[events][keyboard]") {
    KeyEvent ke;
    ke.modifiers = kModCmd;
    REQUIRE(ke.isCmdDown());
}

TEST_CASE("Keyboard: combined modifiers", "[events][keyboard]") {
    KeyEvent ke;
    ke.modifiers = kModShift | kModCtrl | kModAlt;
    REQUIRE(ke.isShiftDown());
    REQUIRE(ke.isCtrlDown());
    REQUIRE(ke.isAltDown());
    REQUIRE_FALSE(ke.isCmdDown());
}

TEST_CASE("Keyboard: key code letter", "[events][keyboard]") {
    KeyEvent ke;
    ke.key = KeyCode::a;
    REQUIRE(ke.key == KeyCode::a);
}

TEST_CASE("Keyboard: on_key_event dispatches to view", "[events][keyboard]") {
    struct KeyTracker : View {
        KeyCode last_key = KeyCode::unknown;
        bool handled = false;
        bool on_key_event(const KeyEvent& e) override {
            last_key = e.key;
            handled = true;
            return true;
        }
    };

    KeyTracker v;
    v.set_bounds({0, 0, 100, 100});
    KeyEvent ke;
    ke.key = KeyCode::enter;
    ke.is_down = true;
    v.on_key_event(ke);

    REQUIRE(v.handled);
    REQUIRE(v.last_key == KeyCode::enter);
}

TEST_CASE("Keyboard: on_key_event returns false by default", "[events][keyboard]") {
    View v;
    KeyEvent ke;
    ke.key = KeyCode::a;
    bool result = v.on_key_event(ke);
    REQUIRE_FALSE(result);
}

TEST_CASE("Keyboard: on_key_press fires", "[events][keyboard]") {
    struct PressTracker : View {
        int last_code = 0;
        void on_key_press(int code) override { last_code = code; }
    };

    PressTracker v;
    v.on_key_press(static_cast<int>(KeyCode::space));
    REQUIRE(v.last_code == static_cast<int>(KeyCode::space));
}

TEST_CASE("Keyboard: MouseEvent modifier helpers", "[events][keyboard]") {
    MouseEvent me;
    me.modifiers = kModShift | kModCmd;
    REQUIRE(me.isShiftDown());
    REQUIRE(me.isCmdDown());
    REQUIRE_FALSE(me.isCtrlDown());
    REQUIRE_FALSE(me.isAltDown());
}

TEST_CASE("Keyboard: is_main_modifier platform-aware", "[events][keyboard]") {
    // On macOS, main modifier is Cmd; on others it's Ctrl
#ifdef __APPLE__
    REQUIRE(is_main_modifier(kModCmd));
    REQUIRE_FALSE(is_main_modifier(kModCtrl));
#else
    REQUIRE(is_main_modifier(kModCtrl));
    REQUIRE_FALSE(is_main_modifier(kModCmd));
#endif
}

TEST_CASE("Keyboard: key repeat flag", "[events][keyboard]") {
    KeyEvent ke;
    ke.is_repeat = true;
    REQUIRE(ke.is_repeat);
}

TEST_CASE("Keyboard: key up vs down", "[events][keyboard]") {
    KeyEvent down;
    down.key = KeyCode::a;
    down.is_down = true;

    KeyEvent up;
    up.key = KeyCode::a;
    up.is_down = false;

    REQUIRE(down.is_down);
    REQUIRE_FALSE(up.is_down);
}

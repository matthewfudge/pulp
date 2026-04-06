// Automated test for CanvasWidget (JS-driven custom drawing)
#include <catch2/catch_test_macros.hpp>
#include <pulp/view/canvas_widget.hpp>
#include <pulp/view/theme.hpp>
#include <pulp/canvas/canvas.hpp>

using namespace pulp::view;
using pulp::canvas::RecordingCanvas;

TEST_CASE("CanvasWidget: add and clear commands", "[canvas_widget]") {
    CanvasWidget cw;
    REQUIRE(cw.command_count() == 0);

    CanvasDrawCmd cmd;
    cmd.type = CanvasDrawCmd::Type::fill_rect;
    cmd.x = 0; cmd.y = 0; cmd.w = 100; cmd.h = 50;
    cmd.color = {255, 0, 0, 255};
    cw.add_command(cmd);
    REQUIRE(cw.command_count() == 1);

    cw.clear_commands();
    REQUIRE(cw.command_count() == 0);
}

TEST_CASE("CanvasWidget: paint replays commands", "[canvas_widget]") {
    RecordingCanvas rc;
    CanvasWidget cw;
    cw.set_bounds({0, 0, 200, 100});

    // Add a filled rect
    CanvasDrawCmd rect;
    rect.type = CanvasDrawCmd::Type::fill_rect;
    rect.x = 10; rect.y = 10; rect.w = 80; rect.h = 40;
    rect.color = {255, 0, 0, 255};
    cw.add_command(rect);

    // Add a circle
    CanvasDrawCmd circle;
    circle.type = CanvasDrawCmd::Type::fill_circle;
    circle.x = 50; circle.y = 50; circle.extra = 20; // radius
    circle.color = {0, 255, 0, 255};
    cw.add_command(circle);

    // Add a line
    CanvasDrawCmd line;
    line.type = CanvasDrawCmd::Type::stroke_line;
    line.x = 0; line.y = 0; line.w = 100; line.h = 100; // endpoint
    line.extra = 2.0f; // width
    line.color = {0, 0, 255, 255};
    cw.add_command(line);

    cw.paint(rc);
    // Should have produced fill_rect, fill_circle, stroke_line commands
    REQUIRE(rc.commands().size() >= 3);
}

TEST_CASE("CanvasWidget: fill_text command", "[canvas_widget]") {
    RecordingCanvas rc;
    CanvasWidget cw;
    cw.set_bounds({0, 0, 200, 50});

    CanvasDrawCmd text;
    text.type = CanvasDrawCmd::Type::fill_text;
    text.x = 10; text.y = 20;
    text.extra = 14.0f; // font size
    text.color = {255, 255, 255, 255};
    text.text = "Hello World";
    cw.add_command(text);

    cw.paint(rc);
    REQUIRE(rc.commands().size() > 0);
}

TEST_CASE("CanvasWidget: clear command fills background", "[canvas_widget]") {
    RecordingCanvas rc;
    CanvasWidget cw;
    cw.set_bounds({0, 0, 100, 100});

    CanvasDrawCmd clear;
    clear.type = CanvasDrawCmd::Type::clear;
    clear.color = {30, 30, 46, 255};
    cw.add_command(clear);

    cw.paint(rc);
    REQUIRE(rc.commands().size() > 0);
}

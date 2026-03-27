#pragma once

/// @file canvas_widget.hpp
/// A View that replays recorded draw commands in paint().
/// JS code records commands via canvasBegin/canvasRect/canvasEnd,
/// and the widget replays them each frame.

#include <pulp/view/view.hpp>
#include <pulp/canvas/canvas.hpp>
#include <string>
#include <vector>
#include <variant>

namespace pulp::view {

/// A draw command recorded from JS for replay in paint().
struct CanvasDrawCmd {
    enum class Type { fill_rect, fill_circle, stroke_line, fill_text, clear };
    Type type;
    float x, y, w, h;          // rect/circle/line coords
    canvas::Color color;
    float extra;                // line width, font size, circle radius
    std::string text;           // for fill_text
};

/// A View whose paint() replays a list of recorded draw commands.
/// JS fills the command list via bridge functions, then the widget
/// renders them each frame. Hot-reloadable — JS rebuilds commands on reload.
class CanvasWidget : public View {
public:
    CanvasWidget() = default;

    /// Clear all recorded commands.
    void clear_commands() { commands_.clear(); }

    /// Add a draw command.
    void add_command(CanvasDrawCmd cmd) { commands_.push_back(std::move(cmd)); }

    /// Get command count (for testing).
    size_t command_count() const { return commands_.size(); }

    void paint(canvas::Canvas& canvas) override;

private:
    std::vector<CanvasDrawCmd> commands_;
};

} // namespace pulp::view

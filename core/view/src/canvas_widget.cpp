#include <pulp/view/canvas_widget.hpp>

namespace pulp::view {

void CanvasWidget::paint(canvas::Canvas& canvas) {
    for (auto& cmd : commands_) {
        switch (cmd.type) {
        case CanvasDrawCmd::Type::clear:
            canvas.set_fill_color(cmd.color);
            canvas.fill_rect(0, 0, bounds().width, bounds().height);
            break;
        case CanvasDrawCmd::Type::fill_rect:
            canvas.set_fill_color(cmd.color);
            canvas.fill_rect(cmd.x, cmd.y, cmd.w, cmd.h);
            break;
        case CanvasDrawCmd::Type::fill_circle:
            canvas.set_fill_color(cmd.color);
            canvas.fill_circle(cmd.x, cmd.y, cmd.extra); // extra = radius
            break;
        case CanvasDrawCmd::Type::stroke_line:
            canvas.set_stroke_color(cmd.color);
            canvas.set_line_width(cmd.extra); // extra = line width
            canvas.stroke_line(cmd.x, cmd.y, cmd.w, cmd.h); // w,h = x1,y1
            break;
        case CanvasDrawCmd::Type::fill_text:
            canvas.set_fill_color(cmd.color);
            canvas.set_font("Inter", cmd.extra); // extra = font size
            canvas.set_text_align(canvas::TextAlign::left);
            canvas.fill_text(cmd.text, cmd.x, cmd.y);
            break;
        }
    }
}

} // namespace pulp::view

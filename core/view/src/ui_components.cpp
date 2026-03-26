#include <pulp/view/ui_components.hpp>
#include <algorithm>

namespace pulp::view {

const std::string ComboBox::empty_string_;

// ── ComboBox ─────────────────────────────────────────────────────────────

void ComboBox::set_selected(int index) {
    if (index >= 0 && index < static_cast<int>(items_.size()) && index != selected_) {
        selected_ = index;
        set_access_value(items_[static_cast<size_t>(index)]);
        if (on_change) on_change(index);
    }
}

const std::string& ComboBox::selected_text() const {
    if (selected_ >= 0 && selected_ < static_cast<int>(items_.size()))
        return items_[static_cast<size_t>(selected_)];
    return empty_string_;
}

void ComboBox::paint(canvas::Canvas& canvas) {
    auto b = local_bounds();
    auto bg = resolve_color("combo_bg", canvas::Color::hex(0x1a1a2e));
    auto border = resolve_color("border", canvas::Color::hex(0x3a3a5a));

    canvas.set_fill_color(bg);
    canvas.fill_rounded_rect(b.x, b.y, b.width, b.height, 4);
    canvas.set_stroke_color(border);
    canvas.set_line_width(1);
    canvas.stroke_rounded_rect(b.x, b.y, b.width, b.height, 4);

    // Selected text
    canvas.set_font("system", 13);
    canvas.set_fill_color(resolve_color("text", canvas::Color::hex(0xe0e0e0)));
    canvas.fill_text(selected_text(), b.x + 8, b.y + b.height / 2 + 4);

    // Dropdown arrow
    float ax = b.x + b.width - 16;
    float ay = b.y + b.height / 2;
    canvas.set_stroke_color(resolve_color("text_muted", canvas::Color::hex(0x808090)));
    canvas.set_line_width(1.5f);
    canvas.stroke_line(ax - 3, ay - 2, ax, ay + 2);
    canvas.stroke_line(ax, ay + 2, ax + 3, ay - 2);

    // Open dropdown (simplified — renders inline below)
    if (open_) {
        float y = b.y + b.height;
        canvas.set_fill_color(resolve_color("surface", canvas::Color::hex(0x16213e)));
        float dropdown_h = static_cast<float>(items_.size()) * 24.0f;
        canvas.fill_rounded_rect(b.x, y, b.width, dropdown_h, 4);
        canvas.set_stroke_color(border);
        canvas.stroke_rounded_rect(b.x, y, b.width, dropdown_h, 4);

        for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
            float item_y = y + static_cast<float>(i) * 24.0f;
            if (i == selected_) {
                canvas.set_fill_color(resolve_color("accent", canvas::Color::hex(0xe94560)));
                canvas.fill_rect(b.x + 1, item_y, b.width - 2, 24);
            }
            canvas.set_fill_color(resolve_color("text", canvas::Color::hex(0xe0e0e0)));
            canvas.fill_text(items_[static_cast<size_t>(i)], b.x + 8, item_y + 16);
        }
    }
}

void ComboBox::on_mouse_event(const MouseEvent& event) {
    if (!event.is_down) return;
    auto b = local_bounds();

    if (open_) {
        // Click in dropdown area
        float dropdown_top = b.y + b.height;
        if (event.position.y >= dropdown_top) {
            int index = static_cast<int>((event.position.y - dropdown_top) / 24.0f);
            if (index >= 0 && index < static_cast<int>(items_.size())) {
                set_selected(index);
            }
        }
        open_ = false;
    } else {
        open_ = true;
    }
}

bool ComboBox::on_key_event(const KeyEvent& event) {
    if (!event.is_down) return false;
    if (event.key == KeyCode::up && selected_ > 0) {
        set_selected(selected_ - 1);
        return true;
    }
    if (event.key == KeyCode::down && selected_ < static_cast<int>(items_.size()) - 1) {
        set_selected(selected_ + 1);
        return true;
    }
    if (event.key == KeyCode::escape && open_) {
        open_ = false;
        return true;
    }
    return false;
}

// ── Tooltip ──────────────────────────────────────────────────────────────

void Tooltip::show_at(Point position) {
    set_bounds({position.x, position.y - 24, 200, 22});
    set_visible(true);
}

void Tooltip::hide() { set_visible(false); }

void Tooltip::paint(canvas::Canvas& canvas) {
    auto b = local_bounds();
    canvas.set_fill_color(canvas::Color::hex(0x333344));
    canvas.fill_rounded_rect(b.x, b.y, b.width, b.height, 4);
    canvas.set_font("system", 11);
    canvas.set_fill_color(canvas::Color::hex(0xe0e0e0));
    canvas.fill_text(text_, b.x + 6, b.y + 15);
}

// ── ProgressBar ──────────────────────────────────────────────────────────

void ProgressBar::paint(canvas::Canvas& canvas) {
    auto b = local_bounds();

    // Track
    canvas.set_fill_color(resolve_color("progress_track", canvas::Color::hex(0x2a2a4a)));
    canvas.fill_rounded_rect(b.x, b.y, b.width, b.height, b.height / 2);

    // Fill
    if (progress_ >= 0) {
        float fill_w = b.width * std::clamp(progress_, 0.0f, 1.0f);
        canvas.set_fill_color(resolve_color("accent", canvas::Color::hex(0xe94560)));
        canvas.fill_rounded_rect(b.x, b.y, fill_w, b.height, b.height / 2);
    }

    // Label
    if (!label_.empty()) {
        canvas.set_font("system", 10);
        canvas.set_text_align(canvas::TextAlign::center);
        canvas.set_fill_color(resolve_color("text", canvas::Color::hex(0xe0e0e0)));
        canvas.fill_text(label_, b.x + b.width / 2, b.y + b.height / 2 + 3);
    }
}

// ── CallOutBox ───────────────────────────────────────────────────────────

std::unique_ptr<CallOutBox> CallOutBox::confirm(
    const std::string& message,
    std::function<void()> on_ok,
    std::function<void()> on_cancel)
{
    auto box = std::make_unique<CallOutBox>();
    box->set_message(message);
    box->on_confirm = std::move(on_ok);
    box->on_cancel = std::move(on_cancel);
    return box;
}

std::unique_ptr<CallOutBox> CallOutBox::notify(
    const std::string& message, float duration)
{
    auto box = std::make_unique<CallOutBox>();
    box->set_message(message);
    box->auto_dismiss_seconds = duration;
    return box;
}

void CallOutBox::paint(canvas::Canvas& canvas) {
    auto b = local_bounds();
    canvas.set_fill_color(resolve_color("callout_bg", canvas::Color::hex(0x16213e)));
    canvas.fill_rounded_rect(b.x, b.y, b.width, b.height, 8);
    canvas.set_stroke_color(resolve_color("border", canvas::Color::hex(0x3a3a5a)));
    canvas.set_line_width(1);
    canvas.stroke_rounded_rect(b.x, b.y, b.width, b.height, 8);

    canvas.set_font("system", 13);
    canvas.set_fill_color(resolve_color("text", canvas::Color::hex(0xe0e0e0)));
    canvas.fill_text(message_, b.x + 16, b.y + b.height / 2 + 4);
}

bool CallOutBox::on_key_event(const KeyEvent& event) {
    if (event.is_down && event.key == KeyCode::escape) {
        if (on_cancel) on_cancel();
        return true;
    }
    if (event.is_down && event.key == KeyCode::enter) {
        if (on_confirm) on_confirm();
        return true;
    }
    return false;
}

// ── TabPanel ─────────────────────────────────────────────────────────────

void TabPanel::add_tab(std::string title, std::unique_ptr<View> content) {
    content->set_visible(tabs_.empty()); // first tab visible
    add_child(std::move(content)); // transfer ownership to view tree
    tabs_.push_back({std::move(title), nullptr}); // content ptr managed by parent
}

void TabPanel::set_active_tab(int index) {
    if (index == active_ || index < 0 || index >= static_cast<int>(tabs_.size())) return;

    // Hide old, show new
    if (active_ < static_cast<int>(child_count()))
        child_at(static_cast<size_t>(active_))->set_visible(false);
    active_ = index;
    if (active_ < static_cast<int>(child_count()))
        child_at(static_cast<size_t>(active_))->set_visible(true);

    if (on_tab_change) on_tab_change(index);
}

void TabPanel::paint(canvas::Canvas& canvas) {
    auto b = local_bounds();

    // Tab bar background
    canvas.set_fill_color(resolve_color("tab_bar_bg", canvas::Color::hex(0x16213e)));
    canvas.fill_rect(b.x, b.y, b.width, tab_height_);

    // Draw tabs
    float tab_w = tabs_.empty() ? 0 : b.width / static_cast<float>(tabs_.size());
    for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
        float tx = b.x + static_cast<float>(i) * tab_w;
        if (i == active_) {
            canvas.set_fill_color(resolve_color("tab_active_bg", canvas::Color::hex(0x1a1a2e)));
            canvas.fill_rect(tx, b.y, tab_w, tab_height_);
            canvas.set_fill_color(resolve_color("accent", canvas::Color::hex(0xe94560)));
            canvas.fill_rect(tx, b.y + tab_height_ - 2, tab_w, 2);
        }
        canvas.set_font("system", 12);
        canvas.set_text_align(canvas::TextAlign::center);
        auto text_color = i == active_
            ? resolve_color("text", canvas::Color::hex(0xe0e0e0))
            : resolve_color("text_muted", canvas::Color::hex(0x808090));
        canvas.set_fill_color(text_color);
        canvas.fill_text(tabs_[static_cast<size_t>(i)].title,
                         tx + tab_w / 2, b.y + tab_height_ / 2 + 4);
    }
}

void TabPanel::on_mouse_event(const MouseEvent& event) {
    if (!event.is_down) return;
    if (event.position.y > tab_height_) return; // click below tab bar

    float tab_w = tabs_.empty() ? 0 : local_bounds().width / static_cast<float>(tabs_.size());
    int index = static_cast<int>(event.position.x / tab_w);
    set_active_tab(index);
}

// ── ScrollView ───────────────────────────────────────────────────────────

void ScrollView::set_scroll(float x, float y) {
    auto b = local_bounds();
    scroll_x_ = std::clamp(x, 0.0f, std::max(0.0f, content_size_.width - b.width));
    scroll_y_ = std::clamp(y, 0.0f, std::max(0.0f, content_size_.height - b.height));
}

void ScrollView::paint(canvas::Canvas& canvas) {
    auto b = local_bounds();
    canvas.save();
    canvas.clip_rect(b.x, b.y, b.width, b.height);
    canvas.translate(-scroll_x_, -scroll_y_);
    // Children paint themselves via paint_all
    canvas.restore();

    // Scroll bar (vertical)
    if (direction_ != Direction::horizontal && content_size_.height > b.height) {
        float ratio = b.height / content_size_.height;
        float bar_h = b.height * ratio;
        float bar_y = b.y + (scroll_y_ / content_size_.height) * b.height;
        canvas.set_fill_color(canvas::Color::rgba(255, 255, 255, 40));
        canvas.fill_rounded_rect(b.x + b.width - 6, bar_y, 4, bar_h, 2);
    }
}

void ScrollView::on_mouse_event(const MouseEvent& event) {
    // Drag to scroll
    if (event.is_down) return;
    // TODO: track drag delta for scroll
}

// ── ListBox ──────────────────────────────────────────────────────────────

void ListBox::set_selected(int index) {
    if (index != selected_) {
        selected_ = index;
        if (on_select) on_select(index);
    }
}

void ListBox::paint(canvas::Canvas& canvas) {
    auto b = local_bounds();

    canvas.save();
    canvas.clip_rect(b.x, b.y, b.width, b.height);

    int visible_start = static_cast<int>(scroll_offset_ / row_height_);
    int visible_count = static_cast<int>(b.height / row_height_) + 2;

    for (int i = visible_start; i < std::min(visible_start + visible_count, static_cast<int>(items_.size())); ++i) {
        float y = b.y + static_cast<float>(i) * row_height_ - scroll_offset_;

        if (i == selected_) {
            canvas.set_fill_color(resolve_color("list_selected_bg", canvas::Color::hex(0x0f3460)));
            canvas.fill_rect(b.x, y, b.width, row_height_);
        }

        canvas.set_font("system", 13);
        canvas.set_fill_color(resolve_color("text", canvas::Color::hex(0xe0e0e0)));
        canvas.fill_text(items_[static_cast<size_t>(i)], b.x + 8, y + row_height_ * 0.65f);
    }

    canvas.restore();

    // Scroll indicator
    float total_h = static_cast<float>(items_.size()) * row_height_;
    if (total_h > b.height) {
        float ratio = b.height / total_h;
        float bar_h = b.height * ratio;
        float bar_y = b.y + (scroll_offset_ / total_h) * b.height;
        canvas.set_fill_color(canvas::Color::rgba(255, 255, 255, 40));
        canvas.fill_rounded_rect(b.x + b.width - 6, bar_y, 4, bar_h, 2);
    }
}

void ListBox::on_mouse_event(const MouseEvent& event) {
    if (!event.is_down) return;
    int index = static_cast<int>((event.position.y + scroll_offset_) / row_height_);
    if (index >= 0 && index < static_cast<int>(items_.size())) {
        set_selected(index);
        if (event.click_count == 2 && on_activate) {
            on_activate(index);
        }
    }
}

bool ListBox::on_key_event(const KeyEvent& event) {
    if (!event.is_down) return false;
    if (event.key == KeyCode::up && selected_ > 0) {
        set_selected(selected_ - 1);
        return true;
    }
    if (event.key == KeyCode::down && selected_ < static_cast<int>(items_.size()) - 1) {
        set_selected(selected_ + 1);
        return true;
    }
    if (event.key == KeyCode::enter && selected_ >= 0 && on_activate) {
        on_activate(selected_);
        return true;
    }
    return false;
}

} // namespace pulp::view

#pragma once

#include <algorithm>
#include <cmath>

namespace pulp::view {

struct Point {
    float x = 0, y = 0;

    Point operator+(const Point& p) const { return {x + p.x, y + p.y}; }
    Point operator-(const Point& p) const { return {x - p.x, y - p.y}; }
    bool operator==(const Point& p) const { return x == p.x && y == p.y; }
};

struct Size {
    float width = 0, height = 0;

    bool operator==(const Size& s) const { return width == s.width && height == s.height; }
    bool is_empty() const { return width <= 0 || height <= 0; }
};

struct Rect {
    float x = 0, y = 0, width = 0, height = 0;

    float right() const { return x + width; }
    float bottom() const { return y + height; }
    Point origin() const { return {x, y}; }
    Size size() const { return {width, height}; }
    Point center() const { return {x + width * 0.5f, y + height * 0.5f}; }

    bool contains(Point p) const {
        return p.x >= x && p.x < right() && p.y >= y && p.y < bottom();
    }

    bool is_empty() const { return width <= 0 || height <= 0; }

    Rect inset(float amount) const {
        return {x + amount, y + amount,
                std::max(0.0f, width - 2 * amount),
                std::max(0.0f, height - 2 * amount)};
    }

    Rect inset(float h, float v) const {
        return {x + h, y + v,
                std::max(0.0f, width - 2 * h),
                std::max(0.0f, height - 2 * v)};
    }

    bool operator==(const Rect& r) const {
        return x == r.x && y == r.y && width == r.width && height == r.height;
    }
};

// Flex layout direction
enum class FlexDirection { row, column };

// Flex alignment
enum class FlexAlign { start, center, end, stretch };

/// Justify content modes (main axis space distribution)
enum class FlexJustify {
    start,          ///< Pack items to start (default)
    center,         ///< Center items
    end_,           ///< Pack items to end
    space_between,  ///< Equal space between items, no space at edges
    space_around,   ///< Equal space around each item
    space_evenly,   ///< Equal space between items AND at edges
};

// Flex layout properties for a view
struct FlexStyle {
    FlexDirection direction = FlexDirection::column;
    FlexAlign align_items = FlexAlign::stretch;
    FlexJustify justify_content = FlexJustify::start;
    float gap = 0;
    float padding = 0;
    float padding_top = -1;     ///< Per-side padding (-1 = use uniform `padding`)
    float padding_right = -1;
    float padding_bottom = -1;
    float padding_left = -1;
    float flex_grow = 0;        ///< 0 = fixed size, >0 = share remaining space
    float flex_shrink = 1;      ///< 1 = shrink proportionally if overflow
    float min_width = 0;
    float min_height = 0;
    float preferred_width = 0;
    float preferred_height = 0;
    float max_width = 0;        ///< 0 = no maximum
    float max_height = 0;       ///< 0 = no maximum
    bool flex_wrap = false;     ///< Wrap to next line when main axis overflows
};

} // namespace pulp::view

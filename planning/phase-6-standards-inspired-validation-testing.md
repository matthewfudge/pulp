# Phase 6 -- Standards-Inspired Validation & Testing

**Version:** 2026-03-27
**Status:** Planned
**Depends on:** Phase 5 (Web-Native Authoring Model), Phases 14-18 (CSS property parity)
**Goal:** Adapt browser engine testing practices to validate Pulp's CSS/HTML/JS compatibility layer with high confidence.

---

## Motivation

Browser engines maintain massive test suites (WPT, Blink layout tests, Gecko reftests) that verify every CSS property, layout algorithm, and DOM interaction. Pulp is not a browser, but it claims to support a significant subset of CSS/HTML/JS semantics. Without rigorous testing, "supported" is just a claim.

This phase establishes a testing framework modeled on browser engine practices, adapted for Pulp's native GPU runtime. Every CSS property, layout behavior, event interaction, and visual output gets tested. The test suite becomes the specification's enforcement mechanism.

---

## 1. Parser & Value Tests

### What They Verify

CSS value parsing in the web-compat layer: does `"12px 16px"` correctly expand to four padding values? Does `"rgb(255, 0, 0)"` produce the correct hex color? Does `"calc(100% - 20px)"` evaluate correctly?

### Test Categories

| Category | Example | Count (est.) |
|----------|---------|-------------|
| Unit parsing | `"12px"` -> 12, `"1.5em"` -> 21, `"50%"` -> computed | 30+ |
| Color parsing | `"#f00"`, `"rgb(255,0,0)"`, `"hsl(0,100%,50%)"`, `"red"` | 50+ |
| Shorthand expansion | `padding: "12px 16px"` -> top/right/bottom/left | 20+ |
| calc/min/max/clamp | `"calc(100% - 20px)"`, `"clamp(12px, 2vw, 18px)"` | 25+ |
| Transform parsing | `"scale(1.1) rotate(45deg)"` -> individual transforms | 15+ |
| Transition parsing | `"all 0.3s ease-out"` -> property/duration/easing | 10+ |
| var() resolution | `"var(--accent)"`, `"var(--missing, #000)"` | 10+ |
| Invalid values | `"banana"`, `""`, `undefined`, negative where disallowed | 20+ |

### Test Structure

```cpp
// test/web-compat/test_css_value_parser.cpp
TEST_CASE("CSS value parser: unit parsing") {
    SECTION("px values") {
        REQUIRE(parse_css_length("12px") == CSSLength{12.0f, Unit::Px});
        REQUIRE(parse_css_length("0") == CSSLength{0.0f, Unit::Px});
        REQUIRE(parse_css_length("100px") == CSSLength{100.0f, Unit::Px});
    }
    SECTION("em values") {
        REQUIRE(parse_css_length("1.5em") == CSSLength{1.5f, Unit::Em});
    }
    SECTION("percentage values") {
        REQUIRE(parse_css_length("50%") == CSSLength{50.0f, Unit::Percent});
    }
    SECTION("invalid values") {
        REQUIRE(parse_css_length("banana") == std::nullopt);
        REQUIRE(parse_css_length("") == std::nullopt);
    }
}

TEST_CASE("CSS value parser: color parsing") {
    SECTION("hex shorthand") {
        REQUIRE(parse_css_color("#f00") == Color{255, 0, 0, 255});
    }
    SECTION("hex full") {
        REQUIRE(parse_css_color("#ff0000") == Color{255, 0, 0, 255});
    }
    SECTION("hex with alpha") {
        REQUIRE(parse_css_color("#ff000080") == Color{255, 0, 0, 128});
    }
    SECTION("rgb functional") {
        REQUIRE(parse_css_color("rgb(255, 0, 0)") == Color{255, 0, 0, 255});
    }
    SECTION("rgba functional") {
        REQUIRE(parse_css_color("rgba(255, 0, 0, 0.5)") == Color{255, 0, 0, 128});
    }
    SECTION("hsl functional") {
        auto c = parse_css_color("hsl(0, 100%, 50%)");
        REQUIRE(c->r == 255);
        REQUIRE(c->g == 0);
        REQUIRE(c->b == 0);
    }
    SECTION("named colors") {
        REQUIRE(parse_css_color("red") == Color{255, 0, 0, 255});
        REQUIRE(parse_css_color("rebeccapurple") == Color{102, 51, 153, 255});
        REQUIRE(parse_css_color("transparent") == Color{0, 0, 0, 0});
    }
    SECTION("invalid") {
        REQUIRE(parse_css_color("not-a-color") == std::nullopt);
    }
}

TEST_CASE("CSS value parser: shorthand expansion") {
    SECTION("one value") {
        auto [t, r, b, l] = expand_shorthand("12px");
        REQUIRE(t == 12); REQUIRE(r == 12);
        REQUIRE(b == 12); REQUIRE(l == 12);
    }
    SECTION("two values") {
        auto [t, r, b, l] = expand_shorthand("12px 16px");
        REQUIRE(t == 12); REQUIRE(r == 16);
        REQUIRE(b == 12); REQUIRE(l == 16);
    }
    SECTION("three values") {
        auto [t, r, b, l] = expand_shorthand("12px 16px 8px");
        REQUIRE(t == 12); REQUIRE(r == 16);
        REQUIRE(b == 8);  REQUIRE(l == 16);
    }
    SECTION("four values") {
        auto [t, r, b, l] = expand_shorthand("12px 16px 8px 4px");
        REQUIRE(t == 12); REQUIRE(r == 16);
        REQUIRE(b == 8);  REQUIRE(l == 4);
    }
}

TEST_CASE("CSS value parser: calc expressions") {
    SECTION("simple subtraction") {
        auto expr = parse_calc("calc(100% - 20px)");
        REQUIRE(expr.evaluate(400.0f) == Approx(380.0f));
    }
    SECTION("nested min/max") {
        auto expr = parse_calc("min(200px, 50%)");
        REQUIRE(expr.evaluate(300.0f) == Approx(150.0f));
        REQUIRE(expr.evaluate(500.0f) == Approx(200.0f));
    }
    SECTION("clamp") {
        auto expr = parse_calc("clamp(100px, 50%, 300px)");
        REQUIRE(expr.evaluate(100.0f) == Approx(100.0f)); // 50% = 50, clamped to 100
        REQUIRE(expr.evaluate(400.0f) == Approx(200.0f)); // 50% = 200
        REQUIRE(expr.evaluate(800.0f) == Approx(300.0f)); // 50% = 400, clamped to 300
    }
}
```

### JS-Side Parser Tests

For parsers implemented in the JS prelude, test via QuickJS evaluation:

```cpp
TEST_CASE("JS CSS parser: color parsing via bridge") {
    ScriptEngine engine;
    engine.load_prelude(); // loads web-compat.js, css-parser.js, css-colors.js

    auto result = engine.eval("parseCSSColor('rgb(255, 128, 0)')");
    REQUIRE(result == "\"#ff8000\"");

    result = engine.eval("parseCSSColor('hsl(120, 100%, 50%)')");
    REQUIRE(result == "\"#00ff00\"");
}
```

---

## 2. Computed Style Tests

### What They Verify

After setting `element.style.width = "50%"` on a child whose parent is 400px wide, does `getComputedStyle(element).width` return `"200px"`? Do inherited properties (color, font-size) propagate correctly?

### Test Categories

| Category | Example | Count (est.) |
|----------|---------|-------------|
| Resolved lengths | `width: 50%` on 400px parent -> `200px` | 15+ |
| Inherited properties | Parent `color: red` -> child inherits | 10+ |
| Non-inherited defaults | Child `backgroundColor` is not inherited | 10+ |
| Token resolution | `var(--accent)` resolves to theme color | 10+ |
| Cascading (source order) | Two rules match; last one wins | 10+ |
| em/rem resolution | `fontSize: 1.5em` with parent 16px -> 24px | 10+ |
| calc resolution | `width: calc(100% - 40px)` -> resolved px | 10+ |

### Test Structure

```cpp
TEST_CASE("Computed style: percentage resolution") {
    auto [root, engine] = create_test_environment(400, 300);
    engine.eval(R"(
        const parent = document.createElement("div");
        parent.style.width = "400px";
        document.body.appendChild(parent);

        const child = document.createElement("div");
        child.style.width = "50%";
        parent.appendChild(child);
    )");
    // Force layout
    root->layout_children(400, 300);

    auto result = engine.eval("getComputedStyle(child).width");
    REQUIRE(result == "\"200px\"");
}

TEST_CASE("Computed style: inheritance") {
    auto [root, engine] = create_test_environment(400, 300);
    engine.eval(R"(
        const parent = document.createElement("div");
        parent.style.color = "#ff0000";
        document.body.appendChild(parent);

        const child = document.createElement("span");
        child.textContent = "Hello";
        parent.appendChild(child);
    )");
    root->layout_children(400, 300);

    // color inherits
    auto result = engine.eval("getComputedStyle(child).color");
    REQUIRE(result == "\"rgb(255, 0, 0)\"");

    // backgroundColor does NOT inherit
    result = engine.eval("getComputedStyle(child).backgroundColor");
    REQUIRE(result == "\"transparent\"");
}
```

---

## 3. Layout Tests

### What They Verify

Do flex containers position children correctly? Do gap, margin, padding, align-items, justify-content produce the expected box positions and sizes?

### Test Categories

| Category | Examples | Count (est.) |
|----------|---------|-------------|
| Flex row basic | 3 equal children fill width | 15+ |
| Flex column basic | 3 stacked children | 15+ |
| justify-content | start, center, end, space-between, space-around, space-evenly | 12+ |
| align-items | start, center, end, stretch | 8+ |
| align-self | Override parent align-items | 6+ |
| flex-grow | Proportional distribution | 10+ |
| flex-shrink | Overflow shrinking | 10+ |
| flex-basis | Initial size before grow/shrink | 8+ |
| flex-wrap | Multi-line wrapping | 10+ |
| gap (row/column) | Spacing between items | 8+ |
| margin | All sides, auto centering | 10+ |
| padding | All sides, nested | 8+ |
| order | Reordered items | 5+ |
| min/max constraints | min-width prevents shrink below threshold | 10+ |
| Nested flex | Flex inside flex, mixed directions | 10+ |
| Grid basic | Template columns/rows, placement | 20+ |
| Grid auto-flow | Auto-placed items | 10+ |
| Positioned layout | absolute, relative with TRBL offsets | 15+ |
| overflow | hidden clips, scroll enables scrolling | 8+ |
| box-sizing | border-box (default) includes padding | 5+ |

### Test Structure

```cpp
TEST_CASE("Layout: flex row with justify-content center") {
    View root;
    root.flex_style().direction = FlexDirection::Row;
    root.flex_style().justify_content = JustifyContent::Center;
    root.flex_style().preferred_width = 400;
    root.flex_style().preferred_height = 100;

    auto& a = root.add_child<View>();
    a.flex_style().preferred_width = 60;
    a.flex_style().preferred_height = 40;

    auto& b = root.add_child<View>();
    b.flex_style().preferred_width = 80;
    b.flex_style().preferred_height = 40;

    root.layout_children(400, 100);

    // Total child width = 140, remaining = 260, offset = 130
    REQUIRE(a.bounds().x == Approx(130));
    REQUIRE(b.bounds().x == Approx(190));
    REQUIRE(a.bounds().width == Approx(60));
    REQUIRE(b.bounds().width == Approx(80));
}

TEST_CASE("Layout: flex-grow proportional distribution") {
    View root;
    root.flex_style().direction = FlexDirection::Row;
    root.flex_style().preferred_width = 300;
    root.flex_style().preferred_height = 100;

    auto& a = root.add_child<View>();
    a.flex_style().flex_grow = 1;

    auto& b = root.add_child<View>();
    b.flex_style().flex_grow = 2;

    root.layout_children(300, 100);

    REQUIRE(a.bounds().width == Approx(100));
    REQUIRE(b.bounds().width == Approx(200));
}

TEST_CASE("Layout: margin auto centering") {
    View root;
    root.flex_style().direction = FlexDirection::Row;
    root.flex_style().preferred_width = 400;

    auto& child = root.add_child<View>();
    child.flex_style().preferred_width = 100;
    child.flex_style().margin_left = FlexStyle::MARGIN_AUTO;
    child.flex_style().margin_right = FlexStyle::MARGIN_AUTO;

    root.layout_children(400, 100);

    REQUIRE(child.bounds().x == Approx(150)); // (400 - 100) / 2
}

TEST_CASE("Layout: gap between items") {
    View root;
    root.flex_style().direction = FlexDirection::Row;
    root.flex_style().gap = 10;
    root.flex_style().preferred_width = 300;

    auto& a = root.add_child<View>();
    a.flex_style().preferred_width = 50;
    auto& b = root.add_child<View>();
    b.flex_style().preferred_width = 50;
    auto& c = root.add_child<View>();
    c.flex_style().preferred_width = 50;

    root.layout_children(300, 100);

    REQUIRE(a.bounds().x == Approx(0));
    REQUIRE(b.bounds().x == Approx(60));  // 50 + 10 gap
    REQUIRE(c.bounds().x == Approx(120)); // 50 + 10 + 50 + 10
}
```

### Layout Comparison Methodology

For complex layouts, use a **layout dump** format:

```
# Expected layout dump for test "dashboard-grid"
#id          x      y      w      h
root         0      0      800    600
header       0      0      800    48
sidebar      0      48     200    552
main         200    48     600    552
card-1       208    56     284    268
card-2       508    56     284    268
card-3       208    340    284    252
card-4       508    340    284    252
```

Test compares actual layout positions against the dump file (with 1px tolerance for rounding).

---

## 4. Event Tests

### What They Verify

Do pointer events fire on the correct element? Does event bubbling propagate up the tree? Do hover/focus/active states trigger correctly?

### Test Categories

| Category | Examples | Count (est.) |
|----------|---------|-------------|
| Click on element | Fires on correct target | 5+ |
| Click bubbling | Child click bubbles to parent | 5+ |
| stopPropagation | Stops bubbling | 3+ |
| Event delegation | Parent listener, child target | 5+ |
| mouseenter/mouseleave | Enter fires, leave fires, no bubble | 5+ |
| hover state | `:hover` style applied/removed | 5+ |
| focus/blur | Focus fires, blur fires | 5+ |
| :focus style | Style applied on focus | 3+ |
| :active state | Style applied on mousedown, removed on mouseup | 5+ |
| :disabled state | Events suppressed, style applied | 3+ |
| Keyboard events | keydown fires on focused element | 5+ |
| Input events | TextEditor change fires | 5+ |
| Scroll events | ScrollView scroll fires | 3+ |
| Pointer capture | gotpointercapture / lostpointercapture | 3+ |
| preventDefault | Prevents default behavior | 3+ |

### Test Structure

```cpp
TEST_CASE("Events: click bubbling") {
    auto [root, engine] = create_test_environment(400, 300);
    engine.eval(R"(
        window.__clicks = [];
        const parent = document.createElement("div");
        parent.id = "parent";
        parent.style.width = "200px";
        parent.style.height = "200px";
        parent.addEventListener("click", (e) => {
            window.__clicks.push("parent:" + e.target.id);
        });
        document.body.appendChild(parent);

        const child = document.createElement("div");
        child.id = "child";
        child.style.width = "100px";
        child.style.height = "100px";
        child.addEventListener("click", (e) => {
            window.__clicks.push("child:" + e.target.id);
        });
        parent.appendChild(child);
    )");
    root->layout_children(400, 300);

    // Simulate click on child
    simulate_click(*root, 50, 50);

    auto result = engine.eval("JSON.stringify(window.__clicks)");
    REQUIRE(result == "[\"child:child\",\"parent:child\"]");
}

TEST_CASE("Events: stopPropagation") {
    auto [root, engine] = create_test_environment(400, 300);
    engine.eval(R"(
        window.__clicks = [];
        const parent = document.createElement("div");
        parent.addEventListener("click", () => window.__clicks.push("parent"));
        document.body.appendChild(parent);

        const child = document.createElement("div");
        child.style.width = "100px";
        child.style.height = "100px";
        child.addEventListener("click", (e) => {
            window.__clicks.push("child");
            e.stopPropagation();
        });
        parent.appendChild(child);
    )");
    root->layout_children(400, 300);

    simulate_click(*root, 50, 50);

    auto result = engine.eval("JSON.stringify(window.__clicks)");
    REQUIRE(result == "[\"child\"]"); // parent NOT called
}

TEST_CASE("Events: hover state applies style") {
    auto [root, engine] = create_test_environment(400, 300);
    engine.eval(R"(
        const styles = new StyleSheet({
            ".btn": { backgroundColor: "#333" },
            ".btn:hover": { backgroundColor: "#555" },
        });
        styles.attach();

        const btn = document.createElement("div");
        btn.className = "btn";
        btn.style.width = "100px";
        btn.style.height = "40px";
        document.body.appendChild(btn);
    )");
    root->layout_children(400, 300);

    // Before hover
    auto bg = engine.eval("getComputedStyle(btn).backgroundColor");
    REQUIRE(bg.contains("333"));

    // Simulate mouse enter
    simulate_mouse_enter(*root, 50, 20);

    bg = engine.eval("getComputedStyle(btn).backgroundColor");
    REQUIRE(bg.contains("555"));

    // Simulate mouse leave
    simulate_mouse_leave(*root, 50, 20);

    bg = engine.eval("getComputedStyle(btn).backgroundColor");
    REQUIRE(bg.contains("333"));
}
```

---

## 5. Visual Reftests

### What They Are

A reftest renders a test case and compares the output image against a reference image. Browser engines use this extensively. Pulp adapts the concept using headless screenshot rendering.

### Methodology

1. **Test case**: A JS script that builds a specific UI
2. **Reference image**: A pre-approved PNG screenshot at a specific size
3. **Comparison**: Pixel-by-pixel with configurable tolerance

### Tolerance Strategy

| Level | Tolerance | Use Case |
|-------|-----------|----------|
| Exact | 0 pixels different | Layout-only tests (solid colors, no anti-aliasing) |
| Tight | < 0.1% pixels different, max delta 2 per channel | Text rendering, anti-aliased edges |
| Loose | < 1% pixels different, max delta 5 per channel | Complex compositions, gradient rendering |
| Structural | SSIM > 0.99 | Full UI screenshots with platform-dependent text rendering |

### Per-Platform Baselines

Text rendering varies by platform (macOS CoreText vs Skia/HarfBuzz standalone on Linux/Windows). Maintain separate reference images:

```
test/reftests/
  baselines/
    macos-arm64/
      flex-row-center.png
      border-radius-corners.png
      gradient-linear.png
    linux-x86_64/
      flex-row-center.png
      ...
    windows-x86_64/
      flex-row-center.png
      ...
  scripts/
    flex-row-center.js
    border-radius-corners.js
    gradient-linear.js
```

### Test Structure

```cpp
TEST_CASE("Reftest: flex row center alignment") {
    auto [root, engine] = create_test_environment(400, 200);
    engine.eval_file("test/reftests/scripts/flex-row-center.js");
    root->layout_children(400, 200);

    auto screenshot = render_to_image(*root, 400, 200);
    auto reference = load_reference("flex-row-center");

    REQUIRE(compare_images(screenshot, reference, Tolerance::Tight));
}
```

### Reftest Script Example

```js
// test/reftests/scripts/flex-row-center.js
const root = document.body;
root.style.display = "flex";
root.style.flexDirection = "row";
root.style.justifyContent = "center";
root.style.alignItems = "center";
root.style.backgroundColor = "#1a1a2e";
root.style.width = "400px";
root.style.height = "200px";

for (let i = 0; i < 3; i++) {
    const box = document.createElement("div");
    box.style.width = "60px";
    box.style.height = "60px";
    box.style.backgroundColor = ["#e74c3c", "#3498db", "#2ecc71"][i];
    box.style.borderRadius = "8px";
    box.style.margin = "0 8px";
    root.appendChild(box);
}
```

### Reftest Categories

| Category | Count (est.) | Notes |
|----------|-------------|-------|
| Flex layout positions | 20+ | Row, column, wrap, gap, alignment |
| Box model | 15+ | Margin, padding, border, border-radius |
| Colors and gradients | 10+ | Hex, rgb, hsl, linear-gradient, radial-gradient |
| Typography | 10+ | Font size, weight, style, alignment, line-height |
| Transforms | 8+ | Scale, rotate, translate, transform-origin |
| Shadows | 5+ | Box-shadow, inset shadow |
| Overflow | 5+ | Hidden, scroll, clip |
| Opacity and blending | 5+ | Opacity, blend modes |
| Filters | 5+ | Blur, brightness, contrast |
| Transitions/animations | 5+ | Mid-transition screenshot |

---

## 6. Screenshot Regression Tests

### Difference from Reftests

Reftests verify **correctness** against a hand-approved reference. Screenshot regression tests verify **stability** -- that the output hasn't changed since the last approved commit.

### Workflow

1. On each test run, render the UI and save a screenshot
2. Compare against the last committed baseline
3. If different beyond tolerance, fail the test
4. Developer reviews the diff, approves if intentional, fixes if regression
5. Approved screenshots are committed as new baselines

### Diff Visualization

Generate a diff image highlighting changed pixels:

```
test/screenshots/
  current/         # Generated on test run (gitignored)
  baselines/       # Committed reference images
  diffs/           # Generated diff images (gitignored)
```

Diff image: reference pixels where identical, bright red overlay where different.

### Test Runner Integration

```cpp
TEST_CASE("Screenshot regression: plugin-ui-default") {
    auto [root, engine] = create_test_environment(800, 600);
    engine.eval_file("examples/synth-plugin/ui.js");
    root->layout_children(800, 600);

    auto screenshot = render_to_image(*root, 800, 600);
    auto result = regression_check("plugin-ui-default", screenshot);

    if (result.status == RegressionStatus::NewBaseline) {
        save_baseline("plugin-ui-default", screenshot);
        WARN("New baseline saved for plugin-ui-default");
    } else {
        REQUIRE(result.status == RegressionStatus::Match);
    }
}
```

### Update Baselines

```bash
# Update all baselines after intentional visual changes
PULP_UPDATE_BASELINES=1 ctest --test-dir build -R "Screenshot"

# Update a specific baseline
PULP_UPDATE_BASELINES=1 ctest --test-dir build -R "Screenshot.*plugin-ui"
```

---

## 7. Integration Fixtures

### What They Are

Realistic UI patterns that exercise multiple subsystems together. These are not unit tests -- they verify that the full stack (JS -> bridge -> layout -> paint -> screenshot) produces correct results for real-world use cases.

### Fixture Categories

| Fixture | What It Tests | Elements |
|---------|--------------|----------|
| **Form layout** | Labeled inputs, select boxes, checkboxes in a column with alignment | ~20 |
| **Modal dialog** | Overlay positioning, backdrop blur, centered content, close button | ~10 |
| **Scrolling list** | ScrollView with 100+ items, scroll position, virtualization hints | ~100+ |
| **Dashboard grid** | Grid layout with cards, each containing meters/labels/progress bars | ~40 |
| **Tabbed panel** | Tab bar with active state, content switching, transition animation | ~15 |
| **Nested flex** | 3-level nested flex with mixed row/column, gap, margin, padding | ~25 |
| **Audio mixer** | Vertical faders, horizontal meters, knobs, labels, real-time updates | ~50 |
| **Responsive resize** | UI adapts to 3 different window sizes (small, medium, large) | ~30 |
| **Theme switching** | Same UI rendered in light, dark, and pro-audio themes | ~30 |
| **Keyboard navigation** | Tab through form elements, arrow keys in lists, Enter to submit | ~15 |

### Fixture Structure

```
test/fixtures/
  form-layout/
    setup.js          # Build the UI
    test.cpp          # Layout assertions + screenshot
    baseline.png      # Reference screenshot
  modal-dialog/
    setup.js
    test.cpp
    baseline.png
  scrolling-list/
    setup.js
    test.cpp
    baseline.png
  ...
```

### Example: Form Layout Fixture

```js
// test/fixtures/form-layout/setup.js
const styles = new StyleSheet({
    ".form": {
        display: "flex",
        flexDirection: "column",
        gap: "12px",
        padding: "24px",
        maxWidth: "400px",
    },
    ".field": {
        display: "flex",
        flexDirection: "column",
        gap: "4px",
    },
    ".field label": {
        fontSize: "12px",
        fontWeight: "600",
        textTransform: "uppercase",
        letterSpacing: "0.5px",
        color: "var(--text-secondary)",
    },
    ".field input": {
        padding: "8px 12px",
        borderRadius: "6px",
        border: "1px solid var(--border)",
        backgroundColor: "var(--surface-1)",
        fontSize: "14px",
    },
    ".actions": {
        display: "flex",
        flexDirection: "row",
        justifyContent: "flex-end",
        gap: "8px",
        marginTop: "8px",
    },
});
styles.attach();

const form = document.createElement("div");
form.className = "form";
document.body.appendChild(form);

function addField(labelText, inputType) {
    const field = document.createElement("div");
    field.className = "field";

    const label = document.createElement("label");
    label.textContent = labelText;
    field.appendChild(label);

    const input = document.createElement("input");
    input.type = inputType || "text";
    field.appendChild(input);

    form.appendChild(field);
    return input;
}

addField("Plugin Name", "text");
addField("Author", "text");
addField("Version", "text");

const actions = document.createElement("div");
actions.className = "actions";
form.appendChild(actions);

const cancel = document.createElement("button");
cancel.className = "btn";
cancel.textContent = "Cancel";
actions.appendChild(cancel);

const save = document.createElement("button");
save.className = "btn btn-primary";
save.textContent = "Save";
actions.appendChild(save);
```

---

## 8. Frontend Portability Suite

### Purpose

A collection of real HTML/CSS/JS patterns taken from common frontend development that should "just work" in Pulp's web-compat layer. These serve as smoke tests for frontend developer expectations.

### Test Patterns

| Pattern | Source Inspiration | What It Exercises |
|---------|-------------------|-------------------|
| Flexbox holy grail | Common CSS layout tutorial | Header/footer/sidebar/main with flex |
| Card grid | Typical dashboard pattern | Grid, border-radius, shadow, gap |
| Navigation bar | Standard nav component | Row flex, hover states, active indicator |
| Accordion | FAQ-style expandable sections | Details/summary, transitions, state management |
| Toast notifications | Notification system | Absolute positioning, z-index, transitions, setTimeout |
| Drag-and-drop list | Sortable list pattern | Pointer events, drag state, reorder |
| Color picker | HSL picker with preview | Canvas 2D, input range, computed colors |
| Audio EQ curve | Frequency response editor | Canvas 2D, mouse drag, real-time redraw |
| Resizable panels | Split pane with drag handle | Pointer capture, flex-basis manipulation |
| Infinite scroll | Lazy-loading list | ScrollView, scroll events, dynamic content |

### Test Structure

Each portability test is a self-contained JS file that:

1. Creates the UI using only `document.createElement` and `element.style.*`
2. Is tested for: (a) no JS errors, (b) correct layout, (c) screenshot match, (d) interaction works

```cpp
TEST_CASE("Portability: flexbox holy grail layout") {
    auto [root, engine] = create_test_environment(800, 600);
    engine.eval_file("test/portability/holy-grail.js");
    root->layout_children(800, 600);

    // No JS errors
    REQUIRE(engine.error_count() == 0);

    // Layout assertions
    auto header = engine.eval("document.getElementById('header').getBoundingClientRect()");
    REQUIRE(parse_rect(header).height == Approx(48));
    REQUIRE(parse_rect(header).width == Approx(800));

    auto sidebar = engine.eval("document.getElementById('sidebar').getBoundingClientRect()");
    REQUIRE(parse_rect(sidebar).width == Approx(200));

    // Screenshot
    auto screenshot = render_to_image(*root, 800, 600);
    REQUIRE(regression_check("portability-holy-grail", screenshot).status == RegressionStatus::Match);
}
```

---

## 9. CI Integration Plan

### Test Execution

```yaml
# ci/test-web-compat.yml
name: Web Compatibility Tests
on: [push, pull_request]

jobs:
  test:
    strategy:
      matrix:
        os: [macos-14, ubuntu-24.04, windows-2022]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - name: Configure
        run: cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DPULP_BUILD_TESTS=ON
      - name: Build
        run: cmake --build build -j4
      - name: Unit tests (parser, computed style, events)
        run: ctest --test-dir build -R "WebCompat" --output-on-failure
      - name: Layout tests
        run: ctest --test-dir build -R "Layout" --output-on-failure
      - name: Visual reftests
        run: ctest --test-dir build -R "Reftest" --output-on-failure
      - name: Screenshot regression
        run: ctest --test-dir build -R "Screenshot" --output-on-failure
      - name: Integration fixtures
        run: ctest --test-dir build -R "Fixture" --output-on-failure
      - name: Portability suite
        run: ctest --test-dir build -R "Portability" --output-on-failure
      - name: Upload diffs on failure
        if: failure()
        uses: actions/upload-artifact@v4
        with:
          name: visual-diffs-${{ matrix.os }}
          path: build/test/screenshots/diffs/
```

### Test Naming Convention

```
pulp-test-web-compat-parser     # CSS value parsing
pulp-test-web-compat-computed   # Computed style resolution
pulp-test-web-compat-layout     # Layout correctness
pulp-test-web-compat-events     # Event dispatch and bubbling
pulp-test-web-compat-reftest    # Visual reference tests
pulp-test-web-compat-screenshot # Screenshot regression
pulp-test-web-compat-fixture    # Integration fixtures
pulp-test-web-compat-portability # Frontend pattern smoke tests
```

### CTest Labels

```cmake
set_tests_properties(pulp-test-web-compat-parser PROPERTIES LABELS "unit;web-compat")
set_tests_properties(pulp-test-web-compat-reftest PROPERTIES LABELS "visual;web-compat")
set_tests_properties(pulp-test-web-compat-fixture PROPERTIES LABELS "integration;web-compat")
```

Run by category:

```bash
ctest --test-dir build -L "unit"           # All unit tests
ctest --test-dir build -L "visual"         # All visual tests
ctest --test-dir build -L "web-compat"     # All web-compat tests
ctest --test-dir build -L "integration"    # All integration tests
```

---

## 10. Test Architecture

### Directory Structure

```
test/
  web-compat/
    CMakeLists.txt
    test_css_value_parser.cpp       # Section 1: Parser tests
    test_css_color_parser.cpp       # Section 1: Color parsing
    test_css_shorthand.cpp          # Section 1: Shorthand expansion
    test_css_calc.cpp               # Section 1: calc/min/max/clamp
    test_computed_style.cpp         # Section 2: Computed style resolution
    test_layout_flex_row.cpp        # Section 3: Flex row layout
    test_layout_flex_column.cpp     # Section 3: Flex column layout
    test_layout_flex_wrap.cpp       # Section 3: Flex wrapping
    test_layout_grid.cpp            # Section 3: Grid layout
    test_layout_position.cpp        # Section 3: Positioned layout
    test_layout_nested.cpp          # Section 3: Nested layouts
    test_events_click.cpp           # Section 4: Click events
    test_events_hover.cpp           # Section 4: Hover events
    test_events_keyboard.cpp        # Section 4: Keyboard events
    test_events_focus.cpp           # Section 4: Focus events
    test_events_bubbling.cpp        # Section 4: Event propagation
    test_selector_matching.cpp      # StyleSheet selector matching
    test_element_api.cpp            # Element proxy API
  reftests/
    CMakeLists.txt
    test_reftests.cpp               # Section 5: Visual reftest runner
    scripts/                        # JS test scripts
      flex-row-center.js
      flex-wrap-gap.js
      border-radius-corners.js
      gradient-linear.js
      box-shadow-spread.js
      opacity-layers.js
      transform-rotate.js
      typography-weights.js
      ...
    baselines/
      macos-arm64/
        flex-row-center.png
        ...
      linux-x86_64/
        ...
      windows-x86_64/
        ...
  screenshots/
    CMakeLists.txt
    test_screenshot_regression.cpp  # Section 6: Regression runner
    baselines/                      # Committed reference screenshots
      plugin-ui-default.png
      ...
    current/                        # Generated (gitignored)
    diffs/                          # Generated (gitignored)
  fixtures/
    CMakeLists.txt
    test_fixtures.cpp               # Section 7: Integration fixture runner
    form-layout/
      setup.js
      baseline.png
    modal-dialog/
      setup.js
      baseline.png
    scrolling-list/
      setup.js
      baseline.png
    dashboard-grid/
      setup.js
      baseline.png
    audio-mixer/
      setup.js
      baseline.png
    ...
  portability/
    CMakeLists.txt
    test_portability.cpp            # Section 8: Portability test runner
    holy-grail.js
    card-grid.js
    nav-bar.js
    accordion.js
    toast-notifications.js
    color-picker.js
    audio-eq-curve.js
    resizable-panels.js
    ...
```

### Test Helpers

```cpp
// test/web-compat/test_helpers.hpp

// Create a headless test environment with root view + script engine
auto create_test_environment(int width, int height)
    -> std::pair<std::unique_ptr<View>, std::unique_ptr<ScriptEngine>>;

// Simulate mouse click at coordinates
void simulate_click(View& root, float x, float y);

// Simulate mouse enter at coordinates
void simulate_mouse_enter(View& root, float x, float y);

// Simulate mouse leave
void simulate_mouse_leave(View& root, float x, float y);

// Simulate key press
void simulate_key(View& root, const std::string& key, bool ctrl, bool shift, bool alt);

// Render view tree to in-memory image
Image render_to_image(View& root, int width, int height);

// Load reference image for current platform
Image load_reference(const std::string& name);

// Compare two images with tolerance
bool compare_images(const Image& a, const Image& b, Tolerance tol);

// Check screenshot against regression baseline
RegressionResult regression_check(const std::string& name, const Image& screenshot);

// Save new baseline
void save_baseline(const std::string& name, const Image& screenshot);

// Parse getBoundingClientRect JSON result
Rect parse_rect(const std::string& json);
```

### Image Comparison Implementation

```cpp
enum class Tolerance { Exact, Tight, Loose, Structural };

struct CompareResult {
    bool passed;
    int pixels_different;
    float percent_different;
    int max_channel_delta;
    float ssim; // structural similarity
};

CompareResult compare_images_detailed(
    const Image& actual,
    const Image& expected,
    Tolerance tolerance
) {
    // Exact: zero pixels different
    // Tight: < 0.1% different, max delta 2
    // Loose: < 1% different, max delta 5
    // Structural: SSIM > 0.99
}
```

---

## 11. Acceptance Criteria

1. **Parser tests**: 150+ tests covering all CSS value formats, all passing
2. **Computed style tests**: 50+ tests covering resolution, inheritance, and token lookup
3. **Layout tests**: 100+ tests covering flex, grid, positioning, and box model
4. **Event tests**: 50+ tests covering click, hover, focus, keyboard, bubbling, and delegation
5. **Visual reftests**: 80+ reference comparison tests with per-platform baselines
6. **Screenshot regression**: Baselines for all example UIs, all fixtures, delta reporting on failure
7. **Integration fixtures**: 10+ realistic UI patterns, each with layout + visual + interaction tests
8. **Portability suite**: 10+ real frontend patterns that run without modification
9. **CI integration**: All test categories run in GitHub Actions on macOS, Linux, and Windows
10. **Test runtime**: Full suite completes in < 120 seconds on CI (excluding screenshot generation on first run)
11. **Zero tolerance for regressions**: Any visual diff blocks merge until reviewed and approved
12. **Coverage tracking**: Matrix from `w3c-css-support-matrix.md` updated with test coverage status for each property

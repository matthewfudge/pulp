# Design Tokens

Design tokens are the building blocks of Pulp's theme system. They store colors, dimensions, typography, and motion values as structured data — not code. This guide covers how tokens work, how to create themes, and how to integrate with external design tools.

## How Tokens Work

A `Theme` is three flat maps:

```
Theme
├── colors      { "background": #0f0f1a, "accent": #58a6ff, ... }
├── dimensions  { "spacing_sm": 8, "corner_radius_md": 8, ... }
└── strings     { "font_family": "Inter", ... }
```

Every view in the tree can have its own theme. When a widget resolves a token, it walks up the parent chain until it finds a match:

```
child → parent → grandparent → root → built-in fallback
```

This means you can override tokens for a specific section without affecting the rest of the UI.

## Token Categories

### Colors

| Token | Purpose | Dark Default | Light Default |
|-------|---------|-------------|---------------|
| `background` | Window/root background | `#0f0f1a` | `#f5f5f5` |
| `surface` | Panel/card background | `#1a1a2e` | `#ffffff` |
| `surface_variant` | Alternate surface | `#1e1e36` | `#f0f0f0` |
| `on_surface` | Text on surface | `#e0e0e0` | `#1a1a1a` |
| `accent` | Primary interactive color | `#58a6ff` | `#0066cc` |
| `primary` | Brand/primary color | `#58a6ff` | `#0066cc` |
| `secondary` | Secondary accent | `#8b5cf6` | `#6633cc` |
| `tertiary` | Tertiary accent | `#ec4899` | `#cc3399` |
| `success` | Success/positive | `#22c55e` | `#16a34a` |
| `warning` | Warning/caution | `#f59e0b` | `#d97706` |
| `error` | Error/destructive | `#ef4444` | `#dc2626` |

### Dimensions

| Token | Purpose | Default |
|-------|---------|---------|
| `spacing_xs` | Extra-small spacing | `4` |
| `spacing_sm` | Small spacing | `8` |
| `spacing_md` | Medium spacing | `12` |
| `spacing_lg` | Large spacing | `16` |
| `spacing_xl` | Extra-large spacing | `24` |
| `corner_radius_sm` | Small border radius | `4` |
| `corner_radius_md` | Medium border radius | `8` |
| `corner_radius_lg` | Large border radius | `12` |

### Typography

| Token | Purpose | Default |
|-------|---------|---------|
| `font_family` | Primary font | `"Inter"` |
| `font_size_xs` | Extra-small text | `12` |
| `font_size_sm` | Small text | `14` |
| `font_size_md` | Medium text | `16` |
| `font_size_lg` | Large text | `18` |
| `font_weight_normal` | Normal weight | `400` |
| `font_weight_bold` | Bold weight | `700` |

### Motion

| Token | Purpose | Default |
|-------|---------|---------|
| `motion.duration.fast` | Quick transitions | `80` ms |
| `motion.duration.normal` | Standard transitions | `150` ms |
| `motion.duration.slow` | Slow/dramatic transitions | `300` ms |
| `motion.duration.meter_decay` | Meter falloff speed | `300` ms |
| `motion.duration.peak_hold` | Peak indicator hold time | `1500` ms |
| `motion.easing.interaction` | User interaction easing | `ease_out_cubic` |
| `motion.easing.enter` | Element enter easing | `ease_out_quad` |
| `motion.easing.exit` | Element exit easing | `ease_in_quad` |

## Creating a theme.json

A theme JSON file defines token overrides. Only include tokens you want to change — unspecified tokens fall through to the parent or built-in default.

```json
{
    "colors": {
        "background": "#0d1117",
        "surface": "#161b22",
        "surface_variant": "#1c2128",
        "on_surface": "#c9d1d9",
        "accent": "#58a6ff",
        "primary": "#58a6ff",
        "secondary": "#bc8cff",
        "success": "#3fb950",
        "warning": "#d29922",
        "error": "#f85149"
    },
    "dimensions": {
        "spacing_sm": 8,
        "spacing_md": 12,
        "corner_radius_sm": 6,
        "corner_radius_md": 10
    },
    "strings": {
        "font_family": "JetBrains Mono"
    }
}
```

## Using Tokens in JS

### Built-in Themes

```js
setTheme("dark");       // Default dark theme
setTheme("light");      // Light theme
setTheme("pro_audio");  // Pro Audio (DAW-style) theme
```

### Reading Theme Data

```js
const themeJson = getThemeJson();
const theme = JSON.parse(themeJson);
// theme.colors.accent → "#58a6ff"
```

### Applying Partial Overrides

```js
// Override just the accent color without replacing the entire theme
applyTokenDiff(JSON.stringify({
    colors: {
        accent: "#ff6b6b",
        primary: "#ff6b6b"
    }
}));
```

### Motion Tokens

```js
// Read motion timing from the theme
const duration = getMotionToken("motion.duration.normal"); // 150

// Use theme-consistent animation timing
animate("knob", "opacity", 1.0, duration, "ease_out_cubic");

// Override a motion token
setMotionToken("motion.duration.fast", 50);
```

## Using Tokens in C++

```cpp
// Set up a theme
Theme theme = Theme::dark();

// Override specific tokens
theme.colors["accent"] = Color::hex(0xff6b6b);
theme.dimensions["spacing_md"] = 16.0f;

// Apply to a view
root->set_theme(theme);

// Resolve a token (walks up parent chain)
Color bg = view.resolve_color("background", Color::hex(0x000000));
```

### Loading from JSON

```cpp
std::string json = read_file("ui/theme.json");
Theme custom = Theme::from_json(json);
root->set_theme(custom);
```

### Serializing to JSON

```cpp
std::string json = theme.to_json();
write_file("ui/theme.json", json);
```

## Inheritance and Cascading

Tokens cascade through the view tree. Override tokens at any level:

```
Root (dark theme)
��── Header Panel (override: accent → red)
│   ├── Title Label  → resolves accent as red
│   └── Subtitle     → resolves accent as red
├── Main Panel (no overrides)
│   ├── Knob         → resolves accent as blue (from root)
│   └── Fader        → resolves accent as blue (from root)
└── Footer (override: surface → darker)
    └── Label        → resolves surface as darker, accent as blue
```

In JS:

```js
// Override tokens for a specific panel
setBackground("header", getThemeColor("error")); // Use error red
// Children of "header" inherit the panel's visual context
```

In C++:

```cpp
Theme header_theme;
header_theme.colors["accent"] = Color::hex(0xff4444);
header_panel->set_theme(header_theme);
// All children of header_panel resolve "accent" as red
```

## Exporting to CSS Custom Properties

For web builds (WASM), tokens map to CSS custom properties:

```css
:root {
    --pulp-background: #0f0f1a;
    --pulp-surface: #1a1a2e;
    --pulp-accent: #58a6ff;
    --pulp-spacing-sm: 8px;
    --pulp-corner-radius-md: 8px;
    --pulp-font-family: 'Inter', sans-serif;
}
```

Generate CSS from a theme:

```bash
pulp docs export-tokens --format css > tokens.css
```

## Exporting to WGSL

For custom shader widgets, tokens are available as uniforms:

```wgsl
struct ThemeUniforms {
    accent: vec4<f32>,
    background: vec4<f32>,
    surface: vec4<f32>,
    on_surface: vec4<f32>,
}

@group(0) @binding(0) var<uniform> theme: ThemeUniforms;

@fragment
fn main(@location(0) uv: vec2<f32>) -> @location(0) vec4<f32> {
    return mix(theme.background, theme.accent, uv.x);
}
```

## Figma Integration

Export Pulp tokens to Figma-compatible format for design handoff:

```bash
pulp docs export-tokens --format figma > tokens.figma.json
```

The exported JSON uses Figma's token format:

```json
{
    "color": {
        "background": { "value": "#0f0f1a", "type": "color" },
        "accent": { "value": "#58a6ff", "type": "color" }
    },
    "spacing": {
        "sm": { "value": "8", "type": "spacing" },
        "md": { "value": "12", "type": "spacing" }
    }
}
```

Import into Figma using the [Tokens Studio](https://tokens.studio/) plugin. Changes in Figma can be exported back to `theme.json` to keep design and code in sync.

## Best Practices

1. **Use tokens, not hardcoded values.** Write `getThemeColor("accent")` instead of `"#58a6ff"`. This ensures your UI respects theme switching.

2. **Override at the narrowest scope.** Don't replace the entire theme when you only need a different accent color for one panel.

3. **Keep motion tokens consistent.** All animations in the same category (interactions, enters, exits) should use the same duration and easing.

4. **Name custom tokens with namespaces.** If you add plugin-specific tokens, prefix them: `mysynth.oscillator_color`, `mysynth.filter_glow`.

5. **Test with all three built-in themes.** A UI that only looks good in dark mode is incomplete.

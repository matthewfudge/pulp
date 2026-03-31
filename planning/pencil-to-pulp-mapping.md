# Pencil (OpenPencil) → Pulp Translation Guide

How [Pencil/OpenPencil](https://docs.pencil.dev) designs map to Pulp's rendering system.

---

## Overview

Pencil is an open-source (MIT) design editor that reads/writes Figma `.fig` files natively. Built with Skia CanvasKit + Yoga layout + Vue 3 + Tauri. It has a full MCP server with 90+ tools, and exports to Tailwind JSX.

**Key insight:** Pencil uses Yoga for layout internally — the same engine Pulp is adopting in Phase 10.0. Layout translation should be near-perfect.

## Access Patterns

| Method | How |
|--------|-----|
| **MCP (live)** | `batch_get(patterns)` → node tree, `get_variables()` → tokens, `get_style_guide()` → rules |
| **File export** | `.pen` / `.fig` files (binary, Kiwi + Zstd + ZIP) |
| **Code export** | Tailwind JSX via CLI: `open-pencil export --format tailwind` |
| **Token export** | `open-pencil analyze colors/typography/spacing` |
| **Image export** | PNG, JPG, WEBP, SVG, PDF via `export_nodes()` |

## Node Properties → Pulp

### Geometry & Position

| Pencil | CSS | Pulp Web-Compat |
|--------|-----|-----------------|
| `width`, `height` | `width`, `height` | `el.style.width = 'Wpx'` |
| `x`, `y` | `left`, `top` (if positioned) | `el.style.left = 'Xpx'` |
| `rotation` | `transform: rotate(deg)` | `el.style.transform = 'rotate(Ndeg)'` |

### Fills (per-node, array of fills)

| Pencil Fill Type | CSS | Pulp |
|-----------------|-----|------|
| `SOLID` (color, opacity) | `background-color: rgba(...)` | `el.style.backgroundColor = 'rgba(...)'` |
| `GRADIENT_LINEAR` (stops, transform) | `background: linear-gradient(...)` | `el.style.background = 'linear-gradient(...)'` |
| `GRADIENT_RADIAL` (stops, transform) | `background: radial-gradient(...)` | `setBackgroundGradient(id, 'radial-gradient(...)')` |
| `GRADIENT_ANGULAR` | conic-gradient | Not yet supported |
| `IMAGE` | `background-image: url(...)` | `setImageSource(id, path)` |

### Strokes

| Pencil | CSS | Pulp |
|--------|-----|------|
| `strokes[]` (color, opacity) | `border-color` | `el.style.borderColor = 'hex'` |
| `strokeThickness` | `border-width` | `el.style.borderWidth = 'Npx'` |
| `cap` (NONE/ROUND/SQUARE) | `stroke-linecap` | Canvas: `canvasSetLineCap(id, 'round')` |
| `join` (MITER/BEVEL/ROUND) | `stroke-linejoin` | Canvas: `canvasSetLineJoin(id, 'round')` |
| `dashPattern` | `stroke-dasharray` | Not yet in CSS bridge (canvas only) |

### Corner Radius

| Pencil | CSS | Pulp |
|--------|-----|------|
| `cornerRadius` (uniform) | `border-radius: Npx` | `el.style.borderRadius = 'Npx'` |
| `independentCorners: true` | per-corner radius | `el.style.borderTopLeftRadius = 'Npx'` etc. |
| `topLeftRadius`, `topRightRadius`, `bottomRightRadius`, `bottomLeftRadius` | individual corners | `setCornerRadius(id, 'TopLeft', N)` etc. |

### Effects

| Pencil Effect | CSS | Pulp |
|--------------|-----|------|
| `DROP_SHADOW` (offset, radius, color) | `box-shadow` | `el.style.boxShadow = 'Xpx Ypx Rpx color'` |
| `INNER_SHADOW` (offset, radius, color) | `box-shadow: inset ...` | `setBoxShadow(id, x, y, blur, spread, color)` |
| `LAYER_BLUR` (radius) | `filter: blur(Npx)` | `el.style.filter = 'blur(Npx)'` |
| `BACKGROUND_BLUR` (radius) | `backdrop-filter: blur(Npx)` | Not yet supported |
| `FOREGROUND_BLUR` | — | Not yet supported |

### Text

| Pencil | CSS | Pulp |
|--------|-----|------|
| `fontFamily` | `font-family` | `el.style.fontFamily = 'Inter'` |
| `fontSize` | `font-size` | `el.style.fontSize = 'Npx'` |
| `fontWeight` | `font-weight` | `el.style.fontWeight = 'N'` |
| `italic` | `font-style: italic` | `el.style.fontStyle = 'italic'` |
| `textDecoration` | `text-decoration` | `el.style.textDecoration = '...'` |
| `lineHeight` | `line-height` | `el.style.lineHeight = 'N'` |
| `letterSpacing` | `letter-spacing` | `el.style.letterSpacing = 'Npx'` |
| `styleRuns[]` (per-character) | — | Not supported (Pulp labels are uniform style) |

### Layout (Auto-Layout → Flexbox)

| Pencil | CSS | Pulp |
|--------|-----|------|
| `layoutMode: horizontal` | `flex-direction: row` | `el.style.flexDirection = 'row'` |
| `layoutMode: vertical` | `flex-direction: column` | `el.style.flexDirection = 'column'` |
| `layoutMode: wrap` | `flex-wrap: wrap` | `el.style.flexWrap = 'wrap'` |
| `itemSpacing` | `gap` | `el.style.gap = 'Npx'` |
| `counterAxisSpacing` | `column-gap` or `row-gap` | `el.style.columnGap = 'Npx'` |
| `paddingTop/Right/Bottom/Left` | `padding` | `el.style.paddingTop = 'Npx'` etc. |
| `primaryAxisAlign` | `justify-content` | `el.style.justifyContent = '...'` |
| `counterAxisAlign` | `align-items` | `el.style.alignItems = '...'` |
| `primaryAxisSizing: FIXED` | `width: Npx` | `el.style.width = 'Npx'` |
| `primaryAxisSizing: HUG` | `width: auto` (min-content) | Default behavior |
| `primaryAxisSizing: FILL` | `flex: 1` | `el.style.flexGrow = '1'` |
| `clipsContent` | `overflow: hidden` | `el.style.overflow = 'hidden'` |

### Shapes (for Canvas/CanvasWidget drawing)

| Pencil Shape | Pulp Canvas Command |
|-------------|---------------------|
| `Rectangle` | `canvasRect(id, x, y, w, h, color)` |
| `Ellipse` | `canvasFillCircle(id, cx, cy, r, color)` |
| `Line` | `canvasStrokeLine(id, x0, y0, x1, y1, color, width)` |
| `Polygon` (pointCount) | Canvas path: moveTo + lineTo × N |
| `Star` (pointCount, innerRadius) | Canvas path: alternating inner/outer vertices |
| `Path` (bezier curves) | `canvasBeginPath` + `canvasMoveTo/LineTo/CubicTo` |
| `Arc` (startAngle, endAngle, innerRadius) | `canvasArc(id, cx, cy, r, start, end, color, width)` |

### Variables → Pulp Design Tokens

| Pencil Variable Type | W3C Design Tokens | Pulp Theme |
|---------------------|-------------------|------------|
| `COLOR` | `{ "$value": "#hex", "$type": "color" }` | `theme.colors["token.name"]` |
| `FLOAT` | `{ "$value": 16, "$type": "dimension" }` | `theme.dimensions["token.name"]` |
| `STRING` | `{ "$value": "Inter", "$type": "fontFamily" }` | `theme.strings["token.name"]` |
| `BOOLEAN` | `{ "$value": true, "$type": "boolean" }` | JS variable |
| Variable collections | Token groups | Token namespace groups |
| Variable modes (Light/Dark) | Token modes | `Theme::dark()` / `Theme::light()` |

### Components → Pulp Widgets

| Pencil Node | HTML | Pulp |
|------------|------|------|
| Frame | `<div>` | `document.createElement('div')` |
| Frame (auto-layout) | `<div style="display:flex">` | `el.style.display = 'flex'` |
| Text | `<span>` | `document.createElement('span')` |
| Rectangle | `<div>` styled | `document.createElement('div')` with bg/border |
| Ellipse | `<div>` with border-radius: 50% | `el.style.borderRadius = '50%'` |
| Component | reusable template | JS function returning element tree |
| Instance | component usage | Call component function |
| Section | `<section>` | `document.createElement('section')` |
| Group | wrapper `<div>` | `document.createElement('div')` |

## Translation Pipeline (Future Phase 11)

```
Pencil MCP
  batch_get(["FRAME", "TEXT", "RECTANGLE", ...])  → node tree with all properties
  get_variables()                                   → design tokens
  get_style_guide(tags)                             → style rules
    ↓
  Claude translates using this mapping table
    ↓
  Pulp web-compat JS + theme token JSON + widget schema JSON
```

**Pencil's Tailwind JSX export** is also a viable intermediate:
```
open-pencil export --format tailwind
  → JSX with Tailwind classes
    → Use v0-to-pulp-mapping.md Tailwind table to translate
      → Pulp web-compat JS
```

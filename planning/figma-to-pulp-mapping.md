# Figma → Pulp Translation Guide

How Figma design concepts map to Pulp's rendering system. Use this when translating a Figma frame, Stitch/v0 React output, or any CSS/HTML design into Pulp code.

---

## Layout

| Figma | CSS | Pulp Web-Compat | Pulp Bridge |
|-------|-----|-----------------|-------------|
| Auto Layout (horizontal) | `display: flex; flex-direction: row` | `el.style.flexDirection = 'row'` | `setFlex(id, 'direction', 'row')` |
| Auto Layout (vertical) | `display: flex; flex-direction: column` | `el.style.flexDirection = 'column'` | `setFlex(id, 'direction', 'col')` |
| Auto Layout gap | `gap: 8px` | `el.style.gap = '8px'` | `setFlex(id, 'gap', 8)` |
| Auto Layout padding | `padding: 16px` | `el.style.padding = '16px'` | `setFlex(id, 'padding', 16)` |
| Hug contents | `width: auto` (intrinsic) | `el.style.width = 'auto'` | — (default) |
| Fill container | `flex: 1` | `el.style.flexGrow = '1'` | `setFlex(id, 'flex_grow', 1)` |
| Fixed size | `width: 200px` | `el.style.width = '200px'` | `setFlex(id, 'width', 200)` |
| Min/Max width | `min-width: 100px` | `el.style.minWidth = '100px'` | `setFlex(id, 'min_width', 100)` |
| Alignment (center) | `align-items: center` | `el.style.alignItems = 'center'` | `setFlex(id, 'align_items', 'center')` |
| Space between | `justify-content: space-between` | `el.style.justifyContent = 'space-between'` | `setFlex(id, 'justify_content', 'space-between')` |
| Absolute position | `position: absolute; top: 10px` | `el.style.position = 'absolute'; el.style.top = '10px'` | `setPosition(id, 'absolute'); setTop(id, 10)` |
| Grid layout | `display: grid; grid-template-columns: 1fr 2fr` | `el.style.gridTemplateColumns = '1fr 2fr'` | `setGrid(id, 'template_columns', '1fr 2fr')` |

## Visual

| Figma | CSS | Pulp Web-Compat | Pulp Bridge |
|-------|-----|-----------------|-------------|
| Fill (solid) | `background-color: #1a1a2e` | `el.style.backgroundColor = '#1a1a2e'` | `setBackground(id, '#1a1a2e')` |
| Fill (gradient) | `background: linear-gradient(...)` | `el.style.background = 'linear-gradient(...)'` | `setBackgroundGradient(id, '...')` |
| Stroke | `border: 1px solid #333` | `el.style.border = '1px solid #333'` | `setBorder(id, '#333', 1, 0)` |
| Stroke (per-side) | `border-bottom: 2px solid #333` | `el.style.borderBottom = '2px solid #333'` | `setBorderSide(id, 'bottom', 2, '#333')` |
| Corner radius | `border-radius: 8px` | `el.style.borderRadius = '8px'` | `setBorder(id, '', 0, 8)` |
| Corner radius (per-corner) | `border-top-left-radius: 12px` | `el.style.borderTopLeftRadius = '12px'` | `setCornerRadius(id, 'TopLeft', 12)` |
| Drop shadow | `box-shadow: 0 2px 8px rgba(0,0,0,0.3)` | `el.style.boxShadow = '0 2px 8px rgba(0,0,0,0.3)'` | `setBoxShadow(id, 0, 2, 8, 0, '#0000004d')` |
| Opacity | `opacity: 0.8` | `el.style.opacity = '0.8'` | `setOpacity(id, 0.8)` |
| Blur (layer) | `filter: blur(4px)` | `el.style.filter = 'blur(4px)'` | `setFilter(id, 'blur(4px)')` |
| Overflow hidden | `overflow: hidden` | `el.style.overflow = 'hidden'` | `setOverflow(id, 'hidden')` |

## Typography

| Figma | CSS | Pulp Web-Compat | Pulp Bridge |
|-------|-----|-----------------|-------------|
| Font size | `font-size: 14px` | `el.style.fontSize = '14px'` | `setFontSize(id, 14)` |
| Font weight | `font-weight: 700` | `el.style.fontWeight = '700'` | `setFontWeight(id, 700)` |
| Font style italic | `font-style: italic` | `el.style.fontStyle = 'italic'` | `setFontStyle(id, 'italic')` |
| Text color | `color: #e0e0e0` | `el.style.color = '#e0e0e0'` | `setTextColor(id, '#e0e0e0')` |
| Text align | `text-align: center` | `el.style.textAlign = 'center'` | `setTextAlign(id, 'center')` |
| Letter spacing | `letter-spacing: 0.5px` | `el.style.letterSpacing = '0.5px'` | `setLetterSpacing(id, 0.5)` |
| Line height | `line-height: 1.5` | `el.style.lineHeight = '1.5'` | `setLineHeight(id, 1.5)` |
| Text transform | `text-transform: uppercase` | `el.style.textTransform = 'uppercase'` | `setTextTransform(id, 'uppercase')` |

## Components → Widgets

| Figma Component | HTML | Pulp Widget | Create Function |
|----------------|------|-------------|-----------------|
| Frame / Auto Layout | `<div>` | View (row/col) | `document.createElement('div')` |
| Text | `<span>` / `<p>` | Label | `document.createElement('span')` |
| Button | `<button>` | ToggleButton | `document.createElement('button')` |
| Input field | `<input>` | TextEditor | `document.createElement('input')` |
| Slider / Range | `<input type="range">` | Fader | `document.createElement('input'); el.type = 'range'` |
| Checkbox | `<input type="checkbox">` | Checkbox | `document.createElement('input'); el.type = 'checkbox'` |
| Dropdown / Select | `<select>` | ComboBox | `document.createElement('select')` |
| Scrollable area | `<div style="overflow:scroll">` | ScrollView | `createScrollView(id)` |
| Progress bar | `<progress>` | ProgressBar | `document.createElement('progress')` |
| Image | `<img>` | ImageView | `document.createElement('img')` |
| Canvas (custom draw) | `<canvas>` | CanvasWidget | `document.createElement('canvas')` |
| **Audio knob** | — | Knob | `createKnob(id)` |
| **Level meter** | — | Meter | `createMeter(id)` |
| **XY Pad** | — | XYPad | `createXYPad(id)` |
| **Waveform** | — | WaveformView | `createWaveform(id)` |
| **Spectrum** | — | SpectrumView | `createSpectrum(id)` |

## Design Tokens

| Figma | W3C Design Tokens | Pulp Theme |
|-------|-------------------|------------|
| Color style | `{ "$value": "#1a1a2e", "$type": "color" }` | `theme.colors["bg.primary"] = "#1a1a2e"` |
| Spacing variable | `{ "$value": "16px", "$type": "dimension" }` | `theme.dimensions["spacing.md"] = 16` |
| Typography style | composite token | `setFontSize + setFontWeight + setFontStyle` |
| Effect style (shadow) | composite token | `setBoxShadow(...)` |
| Corner radius variable | `{ "$value": "8px", "$type": "dimension" }` | `theme.dimensions["radius.md"] = 8` |

## Interaction States

| Figma | CSS | Pulp |
|-------|-----|------|
| Hover variant | `:hover` pseudo-class | StyleSheet `.class:hover` or schema `states.hover` |
| Pressed variant | `:active` pseudo-class | StyleSheet `.class:active` or schema `states.active` |
| Focused variant | `:focus` pseudo-class | StyleSheet `.class:focus` or schema `states.focus` |
| Disabled variant | `:disabled` / opacity | `el.disabled = true` or schema `states.disabled` |

## React/Tailwind → Pulp Translation

When converting React/Tailwind output from v0 or Stitch:

| React/Tailwind | Pulp Equivalent |
|---------------|-----------------|
| `<div className="flex flex-row gap-4 p-4">` | `el.style.flexDirection = 'row'; el.style.gap = '16px'; el.style.padding = '16px'` |
| `<div className="grid grid-cols-3 gap-2">` | `el.style.gridTemplateColumns = '1fr 1fr 1fr'; el.style.gap = '8px'` |
| `<div className="bg-slate-900 rounded-lg">` | `el.style.backgroundColor = '#0f172a'; el.style.borderRadius = '8px'` |
| `<div className="absolute top-0 right-0">` | `el.style.position = 'absolute'; el.style.top = '0'; el.style.right = '0'` |
| `<p className="text-sm text-gray-400">` | `el.style.fontSize = '14px'; el.style.color = '#9ca3af'` |
| `onClick={handler}` | `el.addEventListener('click', handler)` |
| `useState(value)` | `getParam(name)` / `setParam(name, value)` |
| `useEffect(fn, [dep])` | `on(id, 'change', fn)` |

## What's NOT Translatable (Audio-Specific)

These Pulp features have no Figma/CSS/React equivalent:

| Pulp Feature | Why |
|-------------|-----|
| `getParam()` / `setParam()` | Plugin parameter binding |
| `animate(id, prop, target, dur, ease)` | Audio-rate animation |
| `setWidgetShader(id, skslCode)` | GPU shader styling |
| `setWidgetSchema(id, schemaJSON)` | Declarative widget rendering |
| `setWidgetLottie(id, lottieJSON)` | Lottie animation overlay |
| CanvasWidget draw commands | Custom 2D drawing |
| AudioBridge | Meter/waveform data from audio thread |

---

## Future: Automated Translation Pipeline

The goal is eventually:
```
Figma MCP → design data → Claude translates → Pulp web-compat JS
Stitch/v0 → React code → Claude translates → Pulp web-compat JS
Photo/screenshot → Claude describes → Pulp shader/schema
```

This mapping table makes translation trivial for Claude — it's a lookup, not invention.

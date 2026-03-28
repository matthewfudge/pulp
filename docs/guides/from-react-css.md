# From React/CSS to Pulp

A mapping guide for developers who know React and CSS. Pulp's JS bridge uses familiar concepts with audio-specific extensions.

## Mental Model

| React/CSS | Pulp |
|-----------|------|
| JSX component tree | `createKnob()`, `createCol()`, etc. — imperative widget creation |
| `useState` / `useReducer` | `getParam()` / `setParam()` — plugin parameters are the state |
| CSS flexbox | `setFlex(id, property, value)` — same model, subset of properties |
| CSS Grid | `setGrid(id, property, value)` — template columns/rows, gaps |
| `className` / CSS file | `setStyle(id, property, value)` or individual setters |
| Theme / CSS variables | Design tokens via `Theme` — colors, dimensions, strings |
| `useEffect` for animation | `animate(id, property, target, duration, easing)` |
| Event handlers (`onClick`) | `on(id, 'click', fn)` — register after creation |
| React DevTools | `enableInspectClick()` + component inspector |
| Hot Module Replacement | Built-in hot-reload — save JS, see changes instantly |

## Component Mapping

### React → Pulp Widgets

| React / HTML | Pulp | Create Function |
|-------------|------|-----------------|
| `<div>` | Panel / View | `createPanel(id, parentId)` |
| `<div style={{display:'flex', flexDirection:'row'}}>` | Row | `createRow(id, parentId)` |
| `<div style={{display:'flex', flexDirection:'column'}}>` | Col | `createCol(id, parentId)` |
| `<div style={{display:'grid'}}>` | Grid | `createGrid(id, parentId)` |
| `<div style={{overflow:'scroll'}}>` | ScrollView | `createScrollView(id, parentId)` |
| `<span>` / `<p>` / `<h1>` | Label | `createLabel(id, text, parentId)` |
| `<input type="range">` | Knob or Fader | `createKnob(id, parentId)` / `createFader(id, orientation, parentId)` |
| `<input type="checkbox">` | Checkbox | `createCheckbox(id, parentId)` |
| `<button>` (toggle) | ToggleButton | `createToggleButton(id, parentId)` |
| `<select>` | Combo | `createCombo(id, parentId)` |
| `<input type="text">` | TextEditor | `createTextEditor(id, parentId)` |
| `<progress>` | Progress | `createProgress(id, parentId)` |
| `<canvas>` | CanvasWidget | `createCanvas(id, parentId)` |
| Custom meter component | Meter | `createMeter(id, parentId)` |
| Custom waveform component | WaveformView | `createWaveform(id, parentId)` |
| Custom spectrum component | SpectrumView | `createSpectrum(id, parentId)` |
| Custom XY pad component | XYPad | `createXYPad(id, parentId)` |

### No JSX — Imperative Creation

React:
```jsx
function GainUI() {
    const [gain, setGain] = useState(0.5);
    return (
        <div className="column">
            <h1>MyGain</h1>
            <Knob value={gain} onChange={setGain} label="Gain" />
            <span>{formatDb(gain)}</span>
        </div>
    );
}
```

Pulp:
```js
const root = createCol("root");
setFlex("root", "gap", 12);

createLabel("title", "MyGain", "root");
setFontSize("title", 18);
setFontWeight("title", 700);

const knob = createKnob("gain", "root");
setFlex("gain", "width", 80);
setFlex("gain", "height", 80);
setValue("gain", getParam("Gain"));
setLabel("gain", "Gain");

const readout = createLabel("readout", "0.0 dB", "root");

on("gain", "change", (v) => {
    setParam("Gain", v);
    setText("readout", formatDb(v));
});
```

Key differences:
- No virtual DOM — widgets are created once, mutated in place
- No re-renders — event callbacks update specific widgets directly
- String IDs instead of refs — every widget gets a unique ID at creation
- Parent specified at creation time, not via nesting

## CSS Property Mapping

### Flexbox

| CSS | Pulp |
|-----|------|
| `display: flex` | Implicit — `createRow`/`createCol` are flex containers |
| `flex-direction: row` | `setFlex(id, "direction", "row")` or use `createRow` |
| `flex-direction: column` | `setFlex(id, "direction", "column")` or use `createCol` |
| `gap: 8px` | `setFlex(id, "gap", 8)` |
| `padding: 16px` | `setFlex(id, "padding_top", 16)` (per-side) |
| `margin: 8px` | `setFlex(id, "margin_top", 8)` (per-side) |
| `width: 200px` | `setFlex(id, "width", 200)` |
| `height: 40px` | `setFlex(id, "height", 40)` |
| `flex-grow: 1` | `setFlex(id, "flex_grow", 1)` |
| `flex-shrink: 0` | `setFlex(id, "flex_shrink", 0)` |
| `justify-content: center` | `setFlex(id, "justify_content", "center")` |
| `align-items: center` | `setFlex(id, "align_items", "center")` |

### Grid

| CSS | Pulp |
|-----|------|
| `display: grid` | `createGrid(id, parentId)` |
| `grid-template-columns: 1fr 2fr` | `setGrid(id, "template_columns", "1fr 2fr")` |
| `grid-template-rows: auto 1fr` | `setGrid(id, "template_rows", "auto 1fr")` |
| `column-gap: 8px` | `setGrid(id, "column_gap", 8)` |
| `row-gap: 8px` | `setGrid(id, "row_gap", 8)` |
| `grid-column: 1 / 3` | `setGrid(id, "column_start", 1)` + `setGrid(id, "column_end", 3)` |

### Typography

| CSS | Pulp |
|-----|------|
| `font-size: 14px` | `setFontSize(id, 14)` |
| `font-weight: 700` | `setFontWeight(id, 700)` |
| `font-style: italic` | `setFontStyle(id, "italic")` |
| `letter-spacing: 0.5px` | `setLetterSpacing(id, 0.5)` |
| `line-height: 1.5` | `setLineHeight(id, 1.5)` |
| `text-align: center` | `setTextAlign(id, "center")` |
| `color: #fff` | `setTextColor(id, "#ffffff")` |
| `text-transform: uppercase` | `setTextTransform(id, "uppercase")` |
| `text-decoration: underline` | `setTextDecoration(id, "underline")` |
| `text-overflow: ellipsis` | `setTextOverflow(id, "ellipsis")` |

### Visual Styling

| CSS | Pulp |
|-----|------|
| `background: #1a1a2e` | `setBackground(id, "#1a1a2e")` |
| `background: linear-gradient(...)` | `setBackgroundGradient(id, stops)` |
| `border: 1px solid #333` | `setBorder(id, "#333333", 1, 0)` |
| `border-radius: 8px` | `setBorder(id, color, width, 8)` |
| `opacity: 0.5` | `setOpacity(id, 0.5)` |
| `z-index: 10` | `setZIndex(id, 10)` |
| `box-shadow: 0 4px 8px rgba(...)` | `setBoxShadow(id, 0, 4, 8, 0, "rgba(0,0,0,0.3)")` |
| `filter: blur(4px)` | `setFilter(id, "blur(4px)")` |
| `overflow: hidden` | `setOverflow(id, "hidden")` |
| `cursor: pointer` | `setCursor(id, "pointer")` |
| `visibility: hidden` | `setVisible(id, false)` |

### Transforms

| CSS | Pulp |
|-----|------|
| `transform: translate(10px, 20px)` | `setTranslate(id, 10, 20)` |
| `transform: scale(1.5)` | `setScale(id, 1.5, 1.5)` |
| `transform: rotate(45deg)` | `setRotation(id, 45)` |
| `transform-origin: center` | `setTransformOrigin(id, 0.5, 0.5)` |

### Transitions & Animation

| CSS | Pulp |
|-----|------|
| `transition: all 200ms ease` | `setTransitionDuration(id, 200)` |
| `animation: pulse 1s infinite` | `setAnimation(id, "pulse", 1000)` |
| `@keyframes pulse { ... }` | `defineKeyframes("pulse", [...])` |
| Imperative animation | `animate(id, "opacity", 1.0, 300, "ease_out_cubic")` |

## State Management

### React: Component State + Context

```jsx
const [gain, setGain] = useState(0.5);
const [bypass, setBypass] = useState(false);

<Knob value={gain} onChange={(v) => setGain(v)} />
<Toggle checked={bypass} onChange={(v) => setBypass(v)} />
```

### Pulp: Parameter Store

```js
// Parameters are defined in C++, accessed from JS
setValue("gain-knob", getParam("Gain"));
setValue("bypass-toggle", getParam("Bypass"));

on("gain-knob", "change", (v) => setParam("Gain", v));
on("bypass-toggle", "toggle", (v) => setParam("Bypass", v ? 1.0 : 0.0));
```

Key differences:
- State lives in the `StateStore`, not in JS — it's shared with the audio thread
- Parameters are defined in C++ (`define_parameters`), not JS
- No prop drilling or context — `getParam`/`setParam` are global
- Undo/redo grouping is automatic via `Binding` gesture begin/end

## Event Handling

### React: Inline Handlers

```jsx
<button onClick={() => console.log("clicked")}>Click</button>
<div onMouseEnter={() => setHovered(true)} onMouseLeave={() => setHovered(false)}>
```

### Pulp: Register + Listen

```js
const btn = createToggleButton("btn", "root");
registerClick("btn");
on("btn", "click", () => { /* handle click */ });

registerHover("panel");
on("panel", "mouseenter", () => setOpacity("panel", 1.0));
on("panel", "mouseleave", () => setOpacity("panel", 0.7));
```

Key difference: you must call `registerClick`/`registerHover` before `on()` will fire. This is an opt-in performance optimization — only widgets that need events get the overhead.

## What Breaks

Things that **don't transfer** from React/CSS:

| Concept | Why |
|---------|-----|
| Virtual DOM / reconciliation | No VDOM. Widgets are created once and mutated. |
| Component composition / `children` | No nesting syntax. Use `parentId` parameter. |
| `useEffect` / lifecycle hooks | No lifecycle. Use `on()` for events. |
| CSS selectors / cascading | No selectors. Style each widget directly or use theme tokens. |
| `className` / CSS modules | No class system. Use `setStyle()` or individual setters. |
| Media queries / responsive | Fixed-size plugin windows. Use flex layout to fill available space. |
| Server-side rendering | Runs in plugin process only. |
| npm / bundlers | Single JS file, no build step. |

## What Transfers

Skills that **map directly**:

| Skill | How It Applies |
|-------|----------------|
| Flexbox layout | Same mental model — `setFlex()` uses the same properties |
| CSS Grid | Same mental model — `setGrid()` uses template columns/rows |
| Color values | Hex strings work (`"#1a1a2e"`, `"rgba(0,0,0,0.5)"`) |
| Design tokens | Theme system is similar to CSS custom properties |
| Event-driven UI | Same pattern — register handler, respond to events |
| Canvas 2D drawing | Same API shape — beginPath, moveTo, lineTo, fill, stroke |
| Animation/easing curves | Same easing names and keyframe concepts |

## What's Different

Audio plugin UIs have unique requirements:

| Requirement | How Pulp Handles It |
|-------------|---------------------|
| Real-time audio thread | Parameters use `std::atomic` — JS reads/writes are lock-free |
| DAW parameter automation | `setParam()` triggers host notification automatically |
| Undo/redo in DAW | Gesture begin/end via `Binding` — grouped automatically |
| Level meters at 60fps | `Meter` widget polls `AudioBridge` via `TripleBuffer` — no JS polling needed |
| Multiple plugin formats | Same UI JS works in VST3, AU, CLAP, and Standalone |
| GPU-accelerated rendering | Skia/Dawn backend — Canvas API maps to GPU draw calls |
| Hot-reload during development | Save JS file → UI updates instantly, no rebuild |

// Pulp Component Showcase
// Demonstrates every built-in widget type in a single file.
// Screenshot-testable: run with --screenshot to capture visual baseline.

// ─── Root Layout ─────────────────────────────────────────────

const root = createCol("root");
setFlex("root", "padding_top", 16);
setFlex("root", "padding_left", 16);
setFlex("root", "padding_right", 16);
setFlex("root", "padding_bottom", 16);
setFlex("root", "gap", 20);
setBackground("root", "#0f0f1a");

// ─── Header ──────────────────────────────────────────────────

createLabel("header", "Pulp Component Showcase", "root");
setFontSize("header", 22);
setFontWeight("header", 700);
setTextColor("header", "#e0e0e0");

// ─── Section: Knobs ──────────────────────────────────────────

function sectionHeader(id, text) {
    createLabel(id, text, "root");
    setFontSize(id, 14);
    setFontWeight(id, 600);
    setTextColor(id, "#8888bb");
    setTextTransform(id, "uppercase");
    setLetterSpacing(id, 1.5);
}

sectionHeader("s-knobs", "Knobs");

const knobRow = createRow("knob-row", "root");
setFlex("knob-row", "gap", 24);
setFlex("knob-row", "align_items", "flex_end");

["Gain", "Frequency", "Resonance", "Drive"].forEach((name, i) => {
    const col = createCol("k-col-" + i, "knob-row");
    setFlex("k-col-" + i, "align_items", "center");
    setFlex("k-col-" + i, "gap", 4);

    const id = "knob-" + i;
    createKnob(id, "k-col-" + i);
    setFlex(id, "width", 56);
    setFlex(id, "height", 56);
    setValue(id, 0.25 * (i + 1));

    createLabel("k-lbl-" + i, name, "k-col-" + i);
    setFontSize("k-lbl-" + i, 11);
    setTextColor("k-lbl-" + i, "#888888");
    setTextAlign("k-lbl-" + i, "center");
});

// ─── Section: Faders ─────────────────────────────────────────

sectionHeader("s-faders", "Faders");

const faderRow = createRow("fader-row", "root");
setFlex("fader-row", "gap", 16);
setFlex("fader-row", "height", 120);

["Volume", "Pan", "Send A", "Send B"].forEach((name, i) => {
    const col = createCol("f-col-" + i, "fader-row");
    setFlex("f-col-" + i, "align_items", "center");
    setFlex("f-col-" + i, "gap", 4);

    const id = "fader-" + i;
    createFader(id, "vertical", "f-col-" + i);
    setFlex(id, "width", 24);
    setFlex(id, "flex_grow", 1);
    setValue(id, 0.2 * (i + 1));

    createLabel("f-lbl-" + i, name, "f-col-" + i);
    setFontSize("f-lbl-" + i, 11);
    setTextColor("f-lbl-" + i, "#888888");
});

// Horizontal fader
const hFader = createFader("h-fader", "horizontal", "root");
setFlex("h-fader", "height", 24);
setFlex("h-fader", "width", 200);
setValue("h-fader", 0.5);

// ─── Section: Toggles & Checkboxes ──────────────────────────

sectionHeader("s-toggles", "Toggles & Checkboxes");

const toggleRow = createRow("toggle-row", "root");
setFlex("toggle-row", "gap", 16);
setFlex("toggle-row", "align_items", "center");

createToggle("toggle-1", "toggle-row");
setValue("toggle-1", 0);

createToggle("toggle-2", "toggle-row");
setValue("toggle-2", 1);

createCheckbox("checkbox-1", "toggle-row");
setValue("checkbox-1", 0);

createCheckbox("checkbox-2", "toggle-row");
setValue("checkbox-2", 1);

createToggleButton("tbtn-1", "toggle-row");
setLabel("tbtn-1", "Bypass");
setValue("tbtn-1", 0);

createToggleButton("tbtn-2", "toggle-row");
setLabel("tbtn-2", "Solo");
setValue("tbtn-2", 1);

// ─── Section: Labels & Typography ────────────────────────────

sectionHeader("s-labels", "Labels & Typography");

const labelCol = createCol("label-col", "root");
setFlex("label-col", "gap", 4);

createLabel("lbl-normal", "Normal text (14px)", "label-col");
setFontSize("lbl-normal", 14);
setTextColor("lbl-normal", "#cccccc");

createLabel("lbl-bold", "Bold text (16px, weight 700)", "label-col");
setFontSize("lbl-bold", 16);
setFontWeight("lbl-bold", 700);
setTextColor("lbl-bold", "#e0e0e0");

createLabel("lbl-italic", "Italic text", "label-col");
setFontSize("lbl-italic", 14);
setFontStyle("lbl-italic", "italic");
setTextColor("lbl-italic", "#aaaaaa");

createLabel("lbl-upper", "Uppercase with letter spacing", "label-col");
setFontSize("lbl-upper", 12);
setTextTransform("lbl-upper", "uppercase");
setLetterSpacing("lbl-upper", 2);
setTextColor("lbl-upper", "#888888");

// ─── Section: Meters ─────────────────────────────────────────

sectionHeader("s-meters", "Meters");

const meterRow = createRow("meter-row", "root");
setFlex("meter-row", "gap", 8);
setFlex("meter-row", "height", 100);

[0.3, 0.6, 0.85, 0.95, 0.5, 0.7].forEach((level, i) => {
    const id = "meter-" + i;
    createMeter(id, "vertical", "meter-row");
    setFlex(id, "width", 12);
    setFlex(id, "flex_grow", 1);
    setMeterLevel(id, level, level * 0.7);
});

// ─── Section: Spectrum & Waveform ────────────────────────────

sectionHeader("s-viz", "Spectrum & Waveform");

const vizRow = createRow("viz-row", "root");
setFlex("viz-row", "gap", 12);
setFlex("viz-row", "height", 80);

const spectrum = createSpectrum("spectrum", "viz-row");
setFlex("spectrum", "flex_grow", 1);
setFlex("spectrum", "height", 80);
// Generate a sample spectrum curve
const specData = [];
for (let i = 0; i < 64; i++) {
    specData.push(Math.max(0, 0.8 - i * 0.015 + Math.sin(i * 0.3) * 0.15));
}
setSpectrumData("spectrum", specData);

const waveform = createWaveform("waveform", "viz-row");
setFlex("waveform", "flex_grow", 1);
setFlex("waveform", "height", 80);
// Generate a sample waveform
const waveData = [];
for (let i = 0; i < 256; i++) {
    waveData.push(Math.sin(i * 0.05) * 0.7 + Math.sin(i * 0.13) * 0.2);
}
setWaveformData("waveform", waveData);

// ─── Section: XY Pad ─────────────────────────────────────────

sectionHeader("s-xy", "XY Pad");

const xyPad = createXYPad("xy-pad", "root");
setFlex("xy-pad", "width", 150);
setFlex("xy-pad", "height", 150);
setXY("xy-pad", 0.3, 0.7);

// ─── Section: Combo & Text Input ─────────────────────────────

sectionHeader("s-input", "Combo & Text Input");

const inputRow = createRow("input-row", "root");
setFlex("input-row", "gap", 12);
setFlex("input-row", "align_items", "center");

const combo = createCombo("combo", "input-row");
setItems("combo", ["Sine", "Square", "Saw", "Triangle", "Noise"]);

const textInput = createTextEditor("text-input", "input-row");
setFlex("text-input", "width", 200);
setFlex("text-input", "height", 28);
setPlaceholder("text-input", "Type something...");

// ─── Section: Progress ───────────────────────────────────────

sectionHeader("s-progress", "Progress");

const progressRow = createCol("progress-col", "root");
setFlex("progress-col", "gap", 8);

[0.25, 0.5, 0.75, 1.0].forEach((val, i) => {
    const id = "progress-" + i;
    createProgress(id, "progress-col");
    setFlex(id, "height", 8);
    setProgress(id, val);
});

// ─── Section: Scroll View ────────────────────────────────────

sectionHeader("s-scroll", "Scroll View");

const scroll = createScrollView("scroll", "root");
setFlex("scroll", "height", 100);
setFlex("scroll", "width", 200);
setBorder("scroll", "#333333", 1, 4);

const scrollContent = createCol("scroll-content", "scroll");
setFlex("scroll-content", "gap", 1);

for (let i = 0; i < 20; i++) {
    const id = "scroll-item-" + i;
    createLabel(id, "Item " + (i + 1), "scroll-content");
    setFlex(id, "padding_top", 6);
    setFlex(id, "padding_left", 10);
    setFlex(id, "padding_bottom", 6);
    setFontSize(id, 12);
    setTextColor(id, "#cccccc");
    setBackground(id, i % 2 === 0 ? "#1a1a2e" : "#1e1e32");
}
setScrollContentSize("scroll", 200, 20 * 28);

// ─── Section: Canvas ─────────────────────────────────────────

sectionHeader("s-canvas", "Canvas (Custom Drawing)");

const canvas = createCanvas("canvas", "root");
setFlex("canvas", "width", 300);
setFlex("canvas", "height", 120);

// Background
canvasRect("canvas", 0, 0, 300, 120, "#111122");

// Gradient bars
const colors = ["#ff4444", "#ff8800", "#ffcc00", "#44cc44", "#4488ff", "#8844ff"];
colors.forEach((color, i) => {
    canvasRect("canvas", 10 + i * 48, 10, 40, 60, color);
});

// Circle
canvasFillCircle("canvas", 150, 100, 12, "#ffffff");

// Text
canvasSetFont("canvas", "Inter", 11);
canvasFillText("canvas", "Custom draw commands", 10, 95, 11, "#888888");

// Path: triangle
canvasSetFillColor("canvas", "#44ccff");
canvasBeginPath("canvas");
canvasMoveTo("canvas", 220, 80);
canvasLineTo("canvas", 250, 110);
canvasLineTo("canvas", 190, 110);
canvasClosePath("canvas");
canvasFillPath("canvas");

// Lines
canvasSetStrokeColor("canvas", "#666666");
canvasSetLineWidth("canvas", 1);
canvasStrokeLine("canvas", 0, 75, 300, 75, "#333333", 1);

// ─── Section: Panels & Styling ───────────────────────────────

sectionHeader("s-panels", "Panels & Styling");

const panelRow = createRow("panel-row", "root");
setFlex("panel-row", "gap", 12);

// Plain panel
const p1 = createPanel("panel-1", "panel-row");
setFlex("panel-1", "width", 80);
setFlex("panel-1", "height", 60);
setFlex("panel-1", "padding_top", 8);
setFlex("panel-1", "padding_left", 8);
setBackground("panel-1", "#1a1a2e");
setBorder("panel-1", "#333333", 1, 4);
createLabel("p1-lbl", "Plain", "panel-1");
setFontSize("p1-lbl", 11);
setTextColor("p1-lbl", "#888888");

// Rounded panel with shadow
const p2 = createPanel("panel-2", "panel-row");
setFlex("panel-2", "width", 80);
setFlex("panel-2", "height", 60);
setFlex("panel-2", "padding_top", 8);
setFlex("panel-2", "padding_left", 8);
setBackground("panel-2", "#1e1e3a");
setBorder("panel-2", "#4444aa", 1, 12);
setBoxShadow("panel-2", 0, 4, 12, 0, "rgba(68,68,170,0.3)");
createLabel("p2-lbl", "Shadow", "panel-2");
setFontSize("p2-lbl", 11);
setTextColor("p2-lbl", "#aaaacc");

// Semi-transparent panel
const p3 = createPanel("panel-3", "panel-row");
setFlex("panel-3", "width", 80);
setFlex("panel-3", "height", 60);
setFlex("panel-3", "padding_top", 8);
setFlex("panel-3", "padding_left", 8);
setBackground("panel-3", "rgba(100,100,200,0.15)");
setBorder("panel-3", "rgba(100,100,200,0.3)", 1, 8);
createLabel("p3-lbl", "Glass", "panel-3");
setFontSize("p3-lbl", 11);
setTextColor("p3-lbl", "#bbbbdd");

// ─── Section: Icons ──────────────────────────────────────────

sectionHeader("s-icons", "Icons");

const iconRow = createRow("icon-row", "root");
setFlex("icon-row", "gap", 16);
setFlex("icon-row", "align_items", "center");

["image_upload", "send", "search", "close"].forEach((type, i) => {
    const col = createCol("icon-col-" + i, "icon-row");
    setFlex("icon-col-" + i, "align_items", "center");
    setFlex("icon-col-" + i, "gap", 4);

    createIcon("icon-" + i, type, "icon-col-" + i);

    createLabel("icon-lbl-" + i, type, "icon-col-" + i);
    setFontSize("icon-lbl-" + i, 10);
    setTextColor("icon-lbl-" + i, "#666666");
});

// ─── Footer ──────────────────────────────────���───────────────

createLabel("footer", "Pulp — All widget types demonstrated", "root");
setFontSize("footer", 11);
setTextColor("footer", "#444444");
setTextAlign("footer", "center");

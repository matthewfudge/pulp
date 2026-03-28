// Pulp Style Designer — JS-defined UI matching ai-style-designer layout
// Reference: ~/Code/ai-style-designer/Tools/theme-designer.html
// Hot-reloadable: edit and save to see changes instantly.

setTheme("dark");

// ═══════════════════════════════════════════════════════════════════
// Color/palette/app state
// ═══════════════════════════════════════════════════════════════════
var currentAccent = '#89B4FA';
var currentHarmony = 'monochromatic';
var msgCount = 0;

// ═══════════════════════════════════════════════════════════════════
// App colors (matching original --app-* CSS variables)
// ═══════════════════════════════════════════════════════════════════
var APP_BG      = '#18181f';
var APP_SURFACE = '#1e1e26';
var APP_PANEL   = '#242429';
var APP_BORDER  = '#2e2e36';
var APP_TEXT    = '#d4d4dc';
var APP_TEXT_DIM = '#808090';
var APP_ACCENT  = '#aa88ff';

// ═══════════════════════════════════════════════════════════════════
// Root: vertical column (toolbar → main → status bar)
// ═══════════════════════════════════════════════════════════════════
setFlex("", "direction", "col");
setFlex("", "gap", 0);
setBackground("", APP_BG);

// ═══════════════════════════════════════════════════════════════════
// TOOLBAR (44px, full width, space-between)
// ═══════════════════════════════════════════════════════════════════
createRow("toolbar");
setFlex("toolbar", "height", 44);
setFlex("toolbar", "flex_shrink", 0);
setFlex("toolbar", "padding_left", 12);
setFlex("toolbar", "padding_right", 12);
setFlex("toolbar", "align_items", "center");
setFlex("toolbar", "justify_content", "space-between");
setBackground("toolbar", APP_SURFACE);
setBorder("toolbar", APP_BORDER, 1, 0);

// Toolbar items (flat layout)
createLabel("theme-name-label", "Default Dark", "toolbar");
setFontSize("theme-name-label", 13);
setFlex("theme-name-label", "width", 90);

createCombo("preset-selector", "toolbar");
setItems("preset-selector", ["Default Dark", "Light", "Pro Audio", "Violet", "Amber", "Ocean", "Neon"]);
setFlex("preset-selector", "width", 120);
setFlex("preset-selector", "height", 26);

// Spacer
createCol("toolbar-spacer", "toolbar");
setFlex("toolbar-spacer", "flex_grow", 1);

// Toolbar action buttons with pill styling
var toolbarBtns = [
    { id: "undo-btn", label: "Undo", width: 44 },
    { id: "redo-btn", label: "Redo", width: 44 },
    { id: "import-btn", label: "Import", width: 52 },
    { id: "export-btn", label: "Export", width: 52, accent: true }
];
for (var tb = 0; tb < toolbarBtns.length; tb++) {
    var btn = toolbarBtns[tb];
    createCol(btn.id + "-pill", "toolbar");
    setFlex(btn.id + "-pill", "width", btn.width);
    setFlex(btn.id + "-pill", "height", 26);
    setFlex(btn.id + "-pill", "justify_content", "center");
    setFlex(btn.id + "-pill", "align_items", "center");
    setBorder(btn.id + "-pill", APP_BORDER, 1, 4);

    createLabel(btn.id, btn.label, btn.id + "-pill");
    setFontSize(btn.id, 11);
    if (btn.accent) setTextColor(btn.id, APP_ACCENT);
}

// ═══════════════════════════════════════════════════════════════════
// MAIN AREA (3 columns: left 310px | center flex | right 272px)
// ═══════════════════════════════════════════════════════════════════
createRow("main-area");
setFlex("main-area", "flex_grow", 1);

// ── LEFT PANEL (Token Browser — scrollable) ─────────────────────
createScrollView("left-panel", "main-area");
setFlex("left-panel", "width", 310);
setFlex("left-panel", "min_width", 260);
setFlex("left-panel", "flex_shrink", 0);
setBackground("left-panel", APP_SURFACE);
setBorder("left-panel", APP_BORDER, 1, 0);
setScrollContentSize("left-panel", 310, 1200);

// Color System section
createCol("color-section", "left-panel");
setFlex("color-section", "height", 340);
setFlex("color-section", "padding", 10);
setFlex("color-section", "gap", 6);

createLabel("cs-title", "COLOR SYSTEM", "color-section");
setFontSize("cs-title", 10);
setTextColor("cs-title", APP_TEXT_DIM);
setFlex("cs-title", "height", 14);

createCombo("harmony-selector", "color-section");
setItems("harmony-selector", ["Monochromatic", "Analogous", "Complementary", "Split Comp.", "None"]);
setFlex("harmony-selector", "height", 26);

createCombo("mode-selector", "color-section");
setItems("mode-selector", ["Dark", "Light"]);
setFlex("mode-selector", "height", 26);

createRow("hue-row", "color-section");
setFlex("hue-row", "gap", 8);
setFlex("hue-row", "align_items", "center");
setFlex("hue-row", "height", 24);

createLabel("hue-label", "Hue", "hue-row");
setFontSize("hue-label", 10);
setFlex("hue-label", "width", 30);

createFader("accent-hue", "horizontal", "hue-row");
setFlex("accent-hue", "flex_grow", 1);
setFlex("accent-hue", "height", 20);
setValue("accent-hue", 0.65);

// Token search field
createRow("token-search-row", "left-panel");
setFlex("token-search-row", "height", 32);
setFlex("token-search-row", "padding_left", 10);
setFlex("token-search-row", "padding_right", 10);
setFlex("token-search-row", "padding_top", 6);
setFlex("token-search-row", "align_items", "center");
setFlex("token-search-row", "gap", 0);

createIcon("search-icon", "search", "token-search-row");
setFlex("search-icon", "width", 24);
setFlex("search-icon", "height", 24);

createTextEditor("token-search", "token-search-row");
setPlaceholder("token-search", "Search tokens...");
setFlex("token-search", "flex_grow", 1);
setFlex("token-search", "height", 24);

// Token browser header
createRow("token-header", "left-panel");
setFlex("token-header", "height", 24);
setFlex("token-header", "flex_shrink", 0);
setFlex("token-header", "padding_left", 10);
setFlex("token-header", "align_items", "center");

createLabel("tokens-title", "TOKENS", "token-header");
setFontSize("tokens-title", 10);
setTextColor("tokens-title", APP_TEXT_DIM);

// Token list (inside left-panel scroll, no nested scroll needed)
createCol("token-list", "left-panel");
setFlex("token-list", "height", 500);

// Token groups
var tokenGroups = [
    { name: "Background", tokens: ["bg.primary", "bg.secondary", "bg.surface", "bg.elevated"] },
    { name: "Text", tokens: ["text.primary", "text.secondary", "text.disabled"] },
    { name: "Accent", tokens: ["accent.primary", "accent.secondary", "accent.success", "accent.warning", "accent.error"] },
    { name: "Controls", tokens: ["control.track", "control.fill", "control.thumb", "control.border"] }
];

for (var g = 0; g < tokenGroups.length; g++) {
    var group = tokenGroups[g];
    var gid = "tg-" + g;
    var groupHeight = 4 + 18 + 2 + group.tokens.length * 26; // padding_top + title + gap + tokens*(height+gap)
    createCol(gid, "token-list");
    setFlex(gid, "height", groupHeight);
    setFlex(gid, "padding_left", 10);
    setFlex(gid, "padding_right", 10);
    setFlex(gid, "padding_top", 4);
    setFlex(gid, "gap", 2);

    createLabel(gid + "-title", group.name, gid);
    setFontSize(gid + "-title", 10);
    setTextColor(gid + "-title", APP_TEXT_DIM);
    setFlex(gid + "-title", "height", 18);

    for (var t = 0; t < group.tokens.length; t++) {
        var tid = "tok-" + g + "-" + t;
        createRow(tid, gid);
        setFlex(tid, "height", 24);
        setFlex(tid, "gap", 6);
        setFlex(tid, "align_items", "center");

        // Color swatch (small colored box)
        var swatchId = tid + "-sw";
        createCol(swatchId, tid);
        setFlex(swatchId, "width", 16);
        setFlex(swatchId, "height", 16);
        setBorder(swatchId, APP_BORDER, 1, 3);

        // Token name
        createLabel(tid + "-name", group.tokens[t], tid);
        setFontSize(tid + "-name", 11);
        setFlex(tid + "-name", "flex_grow", 1);
    }
}

// Token search filtering
on("token-search", "change", function(query) {
    query = (query || "").toLowerCase();
    for (var g = 0; g < tokenGroups.length; g++) {
        var group = tokenGroups[g];
        var anyVisible = false;
        for (var t = 0; t < group.tokens.length; t++) {
            var tid = "tok-" + g + "-" + t;
            var visible = query.length === 0 || group.tokens[t].toLowerCase().indexOf(query) >= 0;
            setVisible(tid, visible);
            if (visible) anyVisible = true;
        }
        setVisible("tg-" + g, anyVisible || query.length === 0);
    }
    layout();
});

// ── Apply token colors to swatches ───────────────────────────────
function updateTokenSwatches() {
    var themeStr = getThemeJson();
    var theme = JSON.parse(themeStr);
    var colors = theme.colors || {};
    for (var g = 0; g < tokenGroups.length; g++) {
        var group = tokenGroups[g];
        for (var t = 0; t < group.tokens.length; t++) {
            var swatchId = "tok-" + g + "-" + t + "-sw";
            var tokenName = group.tokens[t];
            if (colors[tokenName]) {
                setBackground(swatchId, colors[tokenName]);
            }
        }
    }
}
updateTokenSwatches();

// ── Color System: OKLCH Shade Ramps ──────────────────────────────
var paletteNames = ["Accent", "Neutral", "Success", "Warning", "Error"];
var paletteKeys  = ["accent", "neutral", "success", "warning", "error"];

function buildShadeRamps() {
    var palette = PaletteSystem.create(currentAccent, currentHarmony);
    var steps = ShadeGenerator.STEPS;

    for (var p = 0; p < paletteNames.length; p++) {
        var rampId = "ramp-" + p;
        // Remove old ramp if it exists (on rebuild)
        removeWidget(rampId);

        createCol(rampId, "color-section");
        setFlex(rampId, "height", 36);
        setFlex(rampId, "gap", 2);

        createLabel(rampId + "-title", paletteNames[p], rampId);
        setFontSize(rampId + "-title", 9);
        setTextColor(rampId + "-title", APP_TEXT_DIM);
        setFlex(rampId + "-title", "height", 12);

        createRow(rampId + "-row", rampId);
        setFlex(rampId + "-row", "gap", 2);
        setFlex(rampId + "-row", "height", 20);

        var ramp = palette[paletteKeys[p]];
        for (var s = 0; s < steps.length; s++) {
            var shadeId = rampId + "-s" + s;
            createCol(shadeId, rampId + "-row");
            setFlex(shadeId, "flex_grow", 1);
            setFlex(shadeId, "height", 18);
            setBackground(shadeId, ramp[steps[s]].hex);
            setBorder(shadeId, APP_BORDER, 0, 2);
            // Click to show color info
            registerClick(shadeId);
            (function(hex, name, step) {
                on(shadeId, "click", function() {
                    setText("status-text", name + " " + step + ": " + hex);
                    showColorPicker(hex);
                });
            })(ramp[steps[s]].hex, paletteNames[p], steps[s]);
        }
    }
}
buildShadeRamps();

// ── Color Picker Popup ───────────────────────────────────────────
// Hidden panel that shows OKLCH values when a swatch is clicked.
// Appears over the left panel content.

var pickerVisible = false;
var pickerColor = { L: 0, C: 0, H: 0, hex: '#000000' };

createCol("color-picker", "");
setFlex("color-picker", "width", 280);
setFlex("color-picker", "height", 200);
setFlex("color-picker", "padding", 12);
setFlex("color-picker", "gap", 8);
setBackground("color-picker", APP_PANEL);
setBorder("color-picker", APP_BORDER, 1, 8);
setBoxShadow("color-picker", 0, 4, 12, 0, "#00000060");
setVisible("color-picker", false);

// Color preview swatch
createCol("picker-preview", "color-picker");
setFlex("picker-preview", "height", 40);
setBorder("picker-preview", APP_BORDER, 1, 6);

// OKLCH values row
createRow("picker-values", "color-picker");
setFlex("picker-values", "height", 16);
setFlex("picker-values", "gap", 12);
setFlex("picker-values", "align_items", "center");

createLabel("picker-l-label", "L: 0.00", "picker-values");
setFontSize("picker-l-label", 10);
setFlex("picker-l-label", "width", 60);

createLabel("picker-c-label", "C: 0.000", "picker-values");
setFontSize("picker-c-label", 10);
setFlex("picker-c-label", "width", 70);

createLabel("picker-h-label", "H: 0.0", "picker-values");
setFontSize("picker-h-label", 10);
setFlex("picker-h-label", "width", 60);

// Hex value
createRow("picker-hex-row", "color-picker");
setFlex("picker-hex-row", "height", 20);
setFlex("picker-hex-row", "align_items", "center");

createLabel("picker-hex", "#000000", "picker-hex-row");
setFontSize("picker-hex", 12);

// H slider
createRow("picker-h-row", "color-picker");
setFlex("picker-h-row", "height", 20);
setFlex("picker-h-row", "gap", 6);
setFlex("picker-h-row", "align_items", "center");

createLabel("picker-h-lbl", "H", "picker-h-row");
setFontSize("picker-h-lbl", 10);
setFlex("picker-h-lbl", "width", 14);

createFader("picker-h-fader", "horizontal", "picker-h-row");
setFlex("picker-h-fader", "flex_grow", 1);
setFlex("picker-h-fader", "height", 16);

// C slider
createRow("picker-c-row", "color-picker");
setFlex("picker-c-row", "height", 20);
setFlex("picker-c-row", "gap", 6);
setFlex("picker-c-row", "align_items", "center");

createLabel("picker-c-lbl", "C", "picker-c-row");
setFontSize("picker-c-lbl", 10);
setFlex("picker-c-lbl", "width", 14);

createFader("picker-c-fader", "horizontal", "picker-c-row");
setFlex("picker-c-fader", "flex_grow", 1);
setFlex("picker-c-fader", "height", 16);

// L slider
createRow("picker-l-row", "color-picker");
setFlex("picker-l-row", "height", 20);
setFlex("picker-l-row", "gap", 6);
setFlex("picker-l-row", "align_items", "center");

createLabel("picker-l-lbl", "L", "picker-l-row");
setFontSize("picker-l-lbl", 10);
setFlex("picker-l-lbl", "width", 14);

createFader("picker-l-fader", "horizontal", "picker-l-row");
setFlex("picker-l-fader", "flex_grow", 1);
setFlex("picker-l-fader", "height", 16);

function showColorPicker(hex) {
    var oklch = OklchEngine.hexToOklch(hex);
    pickerColor = { L: oklch.L, C: oklch.C, H: oklch.H, hex: hex };

    setBackground("picker-preview", hex);
    setText("picker-l-label", "L: " + oklch.L.toFixed(2));
    setText("picker-c-label", "C: " + oklch.C.toFixed(3));
    setText("picker-h-label", "H: " + oklch.H.toFixed(1));
    setText("picker-hex", hex);
    setValue("picker-h-fader", oklch.H / 360);
    setValue("picker-c-fader", Math.min(oklch.C / 0.4, 1));
    setValue("picker-l-fader", oklch.L);

    setVisible("color-picker", true);
    pickerVisible = true;
    layout();
}

function hideColorPicker() {
    setVisible("color-picker", false);
    pickerVisible = false;
    layout();
}

function updatePickerFromSliders() {
    var h = getValue("picker-h-fader") * 360;
    var c = getValue("picker-c-fader") * 0.4;
    var l = getValue("picker-l-fader");
    var mapped = OklchEngine.gamutMap(l, c, h);
    var hex = OklchEngine.oklchToHex(mapped.L, mapped.C, mapped.H);

    pickerColor = { L: mapped.L, C: mapped.C, H: mapped.H, hex: hex };
    setBackground("picker-preview", hex);
    setText("picker-l-label", "L: " + mapped.L.toFixed(2));
    setText("picker-c-label", "C: " + mapped.C.toFixed(3));
    setText("picker-h-label", "H: " + mapped.H.toFixed(1));
    setText("picker-hex", hex);
}

on("picker-h-fader", "change", function() { updatePickerFromSliders(); });
on("picker-c-fader", "change", function() { updatePickerFromSliders(); });
on("picker-l-fader", "change", function() { updatePickerFromSliders(); });

// ── Accent hue slider handler ────────────────────────────────────
on("accent-hue", "change", function(val) {
    var hue = val * 360;
    var oklch = OklchEngine.hexToOklch(currentAccent);
    currentAccent = OklchEngine.oklchToHex(oklch.L, oklch.C, hue);

    // Rebuild shade ramps with new hue
    buildShadeRamps();

    // Apply new palette as theme
    var palette = PaletteSystem.create(currentAccent, currentHarmony);
    var diff = PaletteSystem.toThemeDiff(palette);
    applyTokenDiff(diff);
    updateTokenSwatches();
    layout();
});

// Harmony selector handler
on("harmony-selector", "select", function(idx) {
    var modes = ["monochromatic", "analogous", "complementary", "splitComplementary", "none"];
    currentHarmony = modes[idx];
    buildShadeRamps();

    var palette = PaletteSystem.create(currentAccent, currentHarmony);
    var diff = PaletteSystem.toThemeDiff(palette);
    applyTokenDiff(diff);
    updateTokenSwatches();
    layout();
});

// Dark/Light mode handler
on("mode-selector", "select", function(idx) {
    var mode = idx === 0 ? "dark" : "light";
    setTheme(mode);
    buildShadeRamps();
    updateTokenSwatches();
    pushThemeSnapshot();
    layout();
});

// ── CENTER PANEL (Preview) ───────────────────────────────────────
createCol("center-panel", "main-area");
setFlex("center-panel", "flex_grow", 1);
setFlex("center-panel", "min_width", 350);
setFlex("center-panel", "padding", 20);
setFlex("center-panel", "gap", 12);
setBackground("center-panel", APP_BG);

// Preview content area (scrollable, flush to top)
createScrollView("preview-scroll", "center-panel");
setFlex("preview-scroll", "flex_grow", 1);
setBackground("preview-scroll", APP_PANEL);
setBorder("preview-scroll", APP_BORDER, 1, 4);
setScrollContentSize("preview-scroll", 500, 1600);

createCol("preview-area", "preview-scroll");
setFlex("preview-area", "height", 1600);
setFlex("preview-area", "flex_shrink", 0);
setFlex("preview-area", "padding", 12);
setFlex("preview-area", "padding_right", 24);  // extra space for scrollbar
setFlex("preview-area", "gap", 10);

// Foundations section: bg swatches + text hierarchy
createLabel("foundations-header", "Foundations", "preview-area");
setFontSize("foundations-header", 11);
setTextColor("foundations-header", APP_TEXT_DIM);
setFlex("foundations-header", "height", 16);

// Background swatches row
createRow("bg-swatches", "preview-area");
setFlex("bg-swatches", "gap", 6);
setFlex("bg-swatches", "height", 32);
setFlex("bg-swatches", "align_items", "center");

var bgTokens = ["bg.primary", "bg.secondary", "bg.surface", "bg.elevated"];
var bgLabels = ["Primary", "Secondary", "Surface", "Elevated"];
for (var bi = 0; bi < bgTokens.length; bi++) {
    var bsId = "bg-sw-" + bi;
    createCol(bsId, "bg-swatches");
    setFlex(bsId, "flex_grow", 1);
    setFlex(bsId, "height", 28);
    setBorder(bsId, APP_BORDER, 1, 4);
    setFlex(bsId, "padding", 4);
    setFlex(bsId, "justify_content", "center");
    // Color from theme
    var themeColors = JSON.parse(getThemeJson()).colors || {};
    if (themeColors[bgTokens[bi]]) setBackground(bsId, themeColors[bgTokens[bi]]);
    createLabel(bsId + "-l", bgLabels[bi], bsId);
    setFontSize(bsId + "-l", 8);
    setFlex(bsId + "-l", "height", 10);
}

// Text hierarchy row
createRow("text-hierarchy", "preview-area");
setFlex("text-hierarchy", "gap", 16);
setFlex("text-hierarchy", "height", 24);
setFlex("text-hierarchy", "align_items", "center");

createLabel("th-heading", "Heading", "text-hierarchy");
setFontSize("th-heading", 16);
setFlex("th-heading", "width", 80);

createLabel("th-body", "Body text", "text-hierarchy");
setFontSize("th-body", 12);
setFlex("th-body", "width", 80);

createLabel("th-caption", "Caption", "text-hierarchy");
setFontSize("th-caption", 9);
setTextColor("th-caption", APP_TEXT_DIM);
setFlex("th-caption", "width", 60);

// Controls section: knobs
createLabel("controls-header", "Controls", "preview-area");
setFontSize("controls-header", 11);
setTextColor("controls-header", APP_TEXT_DIM);
setFlex("controls-header", "height", 16);

createRow("knob-row", "preview-area");
setFlex("knob-row", "gap", 16);
setFlex("knob-row", "height", 64);
setFlex("knob-row", "align_items", "center");

createKnob("k1", "knob-row");
setLabel("k1", "Gain");
setFlex("k1", "width", 56);
setFlex("k1", "height", 64);
setValue("k1", 0.4);

createKnob("k2", "knob-row");
setLabel("k2", "Freq");
setFlex("k2", "width", 56);
setFlex("k2", "height", 64);
setValue("k2", 0.6);

createKnob("k3", "knob-row");
setLabel("k3", "Res");
setFlex("k3", "width", 56);
setFlex("k3", "height", 64);
setValue("k3", 0.8);

createKnob("k4", "knob-row");
setLabel("k4", "Mix");
setFlex("k4", "width", 56);
setFlex("k4", "height", 64);
setValue("k4", 1.0);

// Fader
createFader("slider1", "horizontal", "preview-area");
setFlex("slider1", "height", 24);
setValue("slider1", 0.65);

// Buttons row (Normal, Hover, Action, Disabled)
createRow("btn-row", "preview-area");
setFlex("btn-row", "gap", 8);
setFlex("btn-row", "height", 32);
setFlex("btn-row", "align_items", "center");

createCol("btn-normal", "btn-row");
setFlex("btn-normal", "flex_grow", 1);
setFlex("btn-normal", "height", 28);
setBackground("btn-normal", "#3a3a4c");
setBorder("btn-normal", APP_BORDER, 1, 6);
setFlex("btn-normal", "justify_content", "center");
setFlex("btn-normal", "align_items", "center");
createLabel("btn-normal-label", "Normal", "btn-normal");
setFontSize("btn-normal-label", 11);
setFlex("btn-normal-label", "height", 16);

createCol("btn-hover", "btn-row");
setFlex("btn-hover", "flex_grow", 1);
setFlex("btn-hover", "height", 28);
setBackground("btn-hover", "#4a4a5c");
setBorder("btn-hover", APP_BORDER, 1, 6);
setFlex("btn-hover", "justify_content", "center");
setFlex("btn-hover", "align_items", "center");
createLabel("btn-hover-label", "Hover", "btn-hover");
setFontSize("btn-hover-label", 11);
setFlex("btn-hover-label", "height", 16);

createCol("btn-action", "btn-row");
setFlex("btn-action", "flex_grow", 1);
setFlex("btn-action", "height", 28);
setBackground("btn-action", APP_ACCENT);
setBorder("btn-action", APP_ACCENT, 0, 6);
setFlex("btn-action", "justify_content", "center");
setFlex("btn-action", "align_items", "center");
createLabel("btn-action-label", "Action", "btn-action");
setFontSize("btn-action-label", 11);
setFlex("btn-action-label", "height", 16);

createCol("btn-disabled", "btn-row");
setFlex("btn-disabled", "flex_grow", 1);
setFlex("btn-disabled", "height", 28);
setBackground("btn-disabled", "#2a2a36");
setBorder("btn-disabled", APP_BORDER, 1, 6);
setOpacity("btn-disabled", 0.5);
setFlex("btn-disabled", "justify_content", "center");
setFlex("btn-disabled", "align_items", "center");
createLabel("btn-disabled-label", "Disabled", "btn-disabled");
setFontSize("btn-disabled-label", 11);
setFlex("btn-disabled-label", "height", 16);
setTextColor("btn-disabled-label", APP_TEXT_DIM);

// Toggles + checkbox row
createRow("toggle-row", "preview-area");
setFlex("toggle-row", "gap", 10);
setFlex("toggle-row", "height", 28);
setFlex("toggle-row", "align_items", "center");

createToggle("t1", "toggle-row");
setFlex("t1", "width", 36);
setFlex("t1", "height", 20);
setValue("t1", 1);

createLabel("toggle-on-label", "Enabled", "toggle-row");
setFontSize("toggle-on-label", 11);
setFlex("toggle-on-label", "width", 50);

createToggle("t2", "toggle-row");
setFlex("t2", "width", 36);
setFlex("t2", "height", 20);

createLabel("toggle-off-label", "Disabled", "toggle-row");
setFontSize("toggle-off-label", 11);
setFlex("toggle-off-label", "width", 52);

// Checkbox
createCheckbox("cb1", "toggle-row");
setFlex("cb1", "width", 22);
setFlex("cb1", "height", 22);
setValue("cb1", 1);

// Toggle button row
createRow("toggle-btn-row", "preview-area");
setFlex("toggle-btn-row", "height", 32);
setFlex("toggle-btn-row", "gap", 8);

createToggleButton("tb1", "toggle-btn-row");
setLabel("tb1", "Toggle");
setFlex("tb1", "flex_grow", 1);
setFlex("tb1", "height", 28);

// Text input + Placeholder + Combo preview
createRow("input-row", "preview-area");
setFlex("input-row", "gap", 8);
setFlex("input-row", "height", 30);
setFlex("input-row", "align_items", "center");

createTextEditor("sample-input", "input-row");
setPlaceholder("sample-input", "Some text");
setText("sample-input", "Some text");
setFlex("sample-input", "width", 120);
setFlex("sample-input", "height", 26);

createTextEditor("sample-placeholder", "input-row");
setPlaceholder("sample-placeholder", "Placeholder...");
setFlex("sample-placeholder", "width", 120);
setFlex("sample-placeholder", "height", 26);

createCombo("sample-combo", "input-row");
setItems("sample-combo", ["Select preset...", "Option A", "Option B"]);
setFlex("sample-combo", "width", 130);
setFlex("sample-combo", "height", 26);

// Data display: Waveform
createLabel("data-header", "Data Display", "preview-area");
setFontSize("data-header", 11);
setTextColor("data-header", APP_TEXT_DIM);
setFlex("data-header", "height", 16);

createWaveform("waveform", "preview-area");
setFlex("waveform", "height", 60);

var waveData = [];
for (var i = 0; i < 512; i++) {
    waveData.push(Math.sin(2 * Math.PI * 3 * i / 512) * 0.7 +
                  Math.sin(2 * Math.PI * 7 * i / 512) * 0.3);
}
setWaveformData("waveform", waveData);

// Meters
createRow("meter-row", "preview-area");
setFlex("meter-row", "gap", 4);
setFlex("meter-row", "height", 48);

createMeter("m1", "vertical", "meter-row");
setFlex("m1", "width", 12);
setMeterLevel("m1", 0.75, 0.88);

createMeter("m2", "vertical", "meter-row");
setFlex("m2", "width", 12);
setMeterLevel("m2", 0.55, 0.72);

createMeter("m3", "vertical", "meter-row");
setFlex("m3", "width", 12);
setMeterLevel("m3", 0.3, 0.45);

createMeter("m4", "vertical", "meter-row");
setFlex("m4", "width", 12);
setMeterLevel("m4", 0.85, 0.95);

// Layout section: 2x2 card grid
createLabel("layout-header", "Layout", "preview-area");
setFontSize("layout-header", 11);
setTextColor("layout-header", APP_TEXT_DIM);
setFlex("layout-header", "height", 16);

createRow("card-grid-top", "preview-area");
setFlex("card-grid-top", "gap", 8);
setFlex("card-grid-top", "height", 56);

createCol("card-1", "card-grid-top");
setFlex("card-1", "flex_grow", 1);
setBackground("card-1", APP_PANEL);
setBorder("card-1", APP_BORDER, 1, 8);
setFlex("card-1", "padding", 8);
createLabel("card-1-label", "Panel A", "card-1");
setFontSize("card-1-label", 10);
setFlex("card-1-label", "height", 14);

createCol("card-2", "card-grid-top");
setFlex("card-2", "flex_grow", 1);
setBackground("card-2", APP_PANEL);
setBorder("card-2", APP_BORDER, 1, 8);
setFlex("card-2", "padding", 8);
createLabel("card-2-label", "Panel B", "card-2");
setFontSize("card-2-label", 10);
setFlex("card-2-label", "height", 14);

createRow("card-grid-bot", "preview-area");
setFlex("card-grid-bot", "gap", 8);
setFlex("card-grid-bot", "height", 56);

createCol("card-3", "card-grid-bot");
setFlex("card-3", "flex_grow", 1);
setBackground("card-3", APP_ACCENT);
setBorder("card-3", APP_ACCENT, 0, 8);
setFlex("card-3", "padding", 8);
createLabel("card-3-label", "Accent", "card-3");
setFontSize("card-3-label", 10);
setFlex("card-3-label", "height", 14);

createCol("card-4", "card-grid-bot");
setFlex("card-4", "flex_grow", 1);
setBackground("card-4", "#8b3a3a");
setBorder("card-4", "#8b3a3a", 0, 8);
setFlex("card-4", "padding", 8);
createLabel("card-4-label", "Error", "card-4");
setFontSize("card-4-label", 10);
setFlex("card-4-label", "height", 14);

// ── Progress + Spinner ───────────────────────────────────────────
createRow("progress-row", "preview-area");
setFlex("progress-row", "gap", 12);
setFlex("progress-row", "height", 24);
setFlex("progress-row", "align_items", "center");

createProgress("prog1", "progress-row");
setFlex("prog1", "flex_grow", 1);
setFlex("prog1", "height", 6);
setProgress("prog1", 0.65);

createLabel("spinner-label", "\u25CB Loading...", "progress-row");
setFontSize("spinner-label", 10);
setTextColor("spinner-label", APP_TEXT_DIM);
setFlex("spinner-label", "width", 80);

// ── Tab Bar (General / Audio / MIDI / About) ─────────────────────
createLabel("tabs-header", "Tabs", "preview-area");
setFontSize("tabs-header", 11);
setTextColor("tabs-header", APP_TEXT_DIM);

createRow("tab-bar-preview", "preview-area");
setFlex("tab-bar-preview", "height", 30);
setFlex("tab-bar-preview", "gap", 0);
setFlex("tab-bar-preview", "align_items", "center");
setBorder("tab-bar-preview", APP_BORDER, 1, 0);

var tabNames = ["General", "Audio", "MIDI", "About"];
for (var ti = 0; ti < tabNames.length; ti++) {
    var tabId = "ptab-" + ti;
    createCol(tabId, "tab-bar-preview");
    setFlex(tabId, "flex_grow", 1);
    setFlex(tabId, "height", 30);
    setFlex(tabId, "justify_content", "center");
    setFlex(tabId, "align_items", "center");

    createLabel(tabId + "-l", tabNames[ti], tabId);
    setFontSize(tabId + "-l", 11);
    if (ti === 0) setTextColor(tabId + "-l", APP_ACCENT);
    else setTextColor(tabId + "-l", APP_TEXT_DIM);

    if (ti === 0) {
        createCol(tabId + "-line", tabId);
        setFlex(tabId + "-line", "height", 2);
        setFlex(tabId + "-line", "flex_grow", 1);
        setBackground(tabId + "-line", APP_ACCENT);
    }
}

// Panel content area with divider
createCol("panel-content", "preview-area");
setFlex("panel-content", "height", 56);
setFlex("panel-content", "padding", 8);
setFlex("panel-content", "gap", 4);
setBackground("panel-content", APP_PANEL);
setBorder("panel-content", APP_BORDER, 1, 6);

createLabel("panel-title", "Panel content area", "panel-content");
setFontSize("panel-title", 11);

createCol("panel-divider", "panel-content");
setFlex("panel-divider", "height", 1);
setBackground("panel-divider", APP_BORDER);

createLabel("panel-sub", "With divider and secondary text", "panel-content");
setFontSize("panel-sub", 10);
setTextColor("panel-sub", APP_TEXT_DIM);

// ── Overlays (Static Preview) ────────────────────────────────────
createLabel("overlays-header", "Overlays", "preview-area");
setFontSize("overlays-header", 11);
setTextColor("overlays-header", APP_TEXT_DIM);

createRow("overlay-row", "preview-area");
setFlex("overlay-row", "gap", 8);
setFlex("overlay-row", "height", 96);

// Confirm Action dialog card
createCol("dialog-card", "overlay-row");
setFlex("dialog-card", "flex_grow", 1);
setFlex("dialog-card", "padding", 8);
setFlex("dialog-card", "gap", 4);
setBackground("dialog-card", APP_PANEL);
setBorder("dialog-card", APP_BORDER, 1, 8);

createLabel("dialog-title", "Confirm Action", "dialog-card");
setFontSize("dialog-title", 11);

createLabel("dialog-msg", "Are you sure you want to delete this preset? This cannot be undone.", "dialog-card");
setFontSize("dialog-msg", 9);
setTextColor("dialog-msg", APP_TEXT_DIM);

createRow("dialog-btns", "dialog-card");
setFlex("dialog-btns", "gap", 6);
setFlex("dialog-btns", "height", 22);
setFlex("dialog-btns", "align_items", "center");
setFlex("dialog-btns", "justify_content", "flex-end");

createCol("dialog-cancel", "dialog-btns");
setFlex("dialog-cancel", "width", 50);
setFlex("dialog-cancel", "height", 20);
setBorder("dialog-cancel", APP_BORDER, 1, 4);
setFlex("dialog-cancel", "justify_content", "center");
setFlex("dialog-cancel", "align_items", "center");
createLabel("dialog-cancel-l", "Cancel", "dialog-cancel");
setFontSize("dialog-cancel-l", 9);

createCol("dialog-accept", "dialog-btns");
setFlex("dialog-accept", "width", 50);
setFlex("dialog-accept", "height", 20);
setBackground("dialog-accept", APP_ACCENT);
setBorder("dialog-accept", APP_ACCENT, 0, 4);
setFlex("dialog-accept", "justify_content", "center");
setFlex("dialog-accept", "align_items", "center");
createLabel("dialog-accept-l", "Accept", "dialog-accept");
setFontSize("dialog-accept-l", 9);

// Context menu card
createCol("ctx-menu", "overlay-row");
setFlex("ctx-menu", "width", 120);
setFlex("ctx-menu", "padding", 4);
setFlex("ctx-menu", "gap", 0);
setBackground("ctx-menu", APP_PANEL);
setBorder("ctx-menu", APP_BORDER, 1, 6);

var ctxItems = ["Copy", "Paste", "---", "Rename", "Delete"];
for (var ci = 0; ci < ctxItems.length; ci++) {
    if (ctxItems[ci] === "---") {
        createCol("ctx-sep-" + ci, "ctx-menu");
        setFlex("ctx-sep-" + ci, "height", 1);
        setBackground("ctx-sep-" + ci, APP_BORDER);
    } else {
        var cid = "ctx-" + ci;
        createRow(cid, "ctx-menu");
        setFlex(cid, "height", 22);
        setFlex(cid, "padding_left", 8);
        setFlex(cid, "align_items", "center");
        createLabel(cid + "-l", ctxItems[ci], cid);
        setFontSize(cid + "-l", 11);
        if (ctxItems[ci] === "Delete") setTextColor(cid + "-l", "#f38ba8");
    }
}

// ── States ───────────────────────────────────────────────────────
createLabel("states-header", "States", "preview-area");
setFontSize("states-header", 11);
setTextColor("states-header", APP_TEXT_DIM);

createRow("states-row", "preview-area");
setFlex("states-row", "gap", 6);
setFlex("states-row", "height", 26);
setFlex("states-row", "align_items", "center");

var stateNames = ["Normal", "Hover", "Active", "Focus", "Disabled"];
var stateBgs   = ["#3a3a4c", "#4a4a5c", APP_ACCENT, "#3a3a4c", "#2a2a36"];
for (var si = 0; si < stateNames.length; si++) {
    var sid = "state-" + si;
    createCol(sid, "states-row");
    setFlex(sid, "flex_grow", 1);
    setFlex(sid, "height", 24);
    setBackground(sid, stateBgs[si]);
    setBorder(sid, si === 3 ? APP_ACCENT : APP_BORDER, 1, 4);
    if (si === 4) setOpacity(sid, 0.5);
    setFlex(sid, "justify_content", "center");
    setFlex(sid, "align_items", "center");
    createLabel(sid + "-l", stateNames[si], sid);
    setFontSize(sid + "-l", 9);
}

// ── Effects ──────────────────────────────────────────────────────
createLabel("effects-header", "Effects", "preview-area");
setFontSize("effects-header", 11);
setTextColor("effects-header", APP_TEXT_DIM);

createRow("effects-row", "preview-area");
setFlex("effects-row", "gap", 8);
setFlex("effects-row", "height", 40);
setFlex("effects-row", "align_items", "center");

var effectNames = ["Shadow", "Glow", "Blur", "Gradient"];
for (var ei = 0; ei < effectNames.length; ei++) {
    var eid = "effect-" + ei;
    createCol(eid, "effects-row");
    setFlex(eid, "flex_grow", 1);
    setFlex(eid, "height", 36);
    setBackground(eid, APP_PANEL);
    setBorder(eid, APP_BORDER, 1, 6);
    setFlex(eid, "justify_content", "center");
    setFlex(eid, "align_items", "center");
    createLabel(eid + "-l", effectNames[ei], eid);
    setFontSize(eid + "-l", 10);
}

// ── RIGHT PANEL (Inspector + Chat) ──────────────────────────────
createCol("right-panel", "main-area");
setFlex("right-panel", "width", 272);
setFlex("right-panel", "flex_shrink", 0);
setBackground("right-panel", APP_SURFACE);
setBorder("right-panel", APP_BORDER, 1, 0);

// Tab bar
createRow("right-tabs", "right-panel");
setFlex("right-tabs", "height", 36);
setFlex("right-tabs", "flex_shrink", 0);
setFlex("right-tabs", "align_items", "center");
setFlex("right-tabs", "justify_content", "center");
setFlex("right-tabs", "gap", 0);
setFlex("right-tabs", "padding_left", 10);
setFlex("right-tabs", "padding_right", 10);
setBackground("right-tabs", APP_PANEL);
setBorder("right-tabs", APP_BORDER, 1, 0);

// Tab columns (label + underline indicator)
createCol("tab-inspector-col", "right-tabs");
setFlex("tab-inspector-col", "flex_grow", 1);
setFlex("tab-inspector-col", "height", 36);
setFlex("tab-inspector-col", "align_items", "center");
setFlex("tab-inspector-col", "justify_content", "center");
setFlex("tab-inspector-col", "gap", 0);

createLabel("tab-inspector", "INSPECTOR", "tab-inspector-col");
setFontSize("tab-inspector", 11);
setFlex("tab-inspector", "height", 28);
setTextAlign("tab-inspector", "center");
setTextColor("tab-inspector", APP_TEXT_DIM);

// Underline indicator — use width not flex_grow to avoid vertical expansion
createCol("tab-inspector-line", "tab-inspector-col");
setFlex("tab-inspector-line", "height", 2);
setFlex("tab-inspector-line", "width", 120);
setFlex("tab-inspector-line", "flex_shrink", 0);

createCol("tab-chat-col", "right-tabs");
setFlex("tab-chat-col", "flex_grow", 1);
setFlex("tab-chat-col", "height", 36);
setFlex("tab-chat-col", "align_items", "center");
setFlex("tab-chat-col", "justify_content", "center");
setFlex("tab-chat-col", "gap", 0);

createLabel("tab-chat", "CHAT", "tab-chat-col");
setFontSize("tab-chat", 11);
setFlex("tab-chat", "height", 28);
setTextAlign("tab-chat", "center");
setTextColor("tab-chat", APP_ACCENT);

createCol("tab-chat-line", "tab-chat-col");
setFlex("tab-chat-line", "height", 2);
setFlex("tab-chat-line", "width", 120);
setFlex("tab-chat-line", "flex_shrink", 0);
setBackground("tab-chat-line", APP_ACCENT);

// Inspector content area (hidden by default)
createCol("inspector-area", "right-panel");
setFlex("inspector-area", "flex_grow", 1);
setFlex("inspector-area", "padding", 10);
setFlex("inspector-area", "gap", 6);
setVisible("inspector-area", false);

createLabel("insp-title", "ELEMENT INSPECTOR", "inspector-area");
setFontSize("insp-title", 10);
setTextColor("insp-title", APP_TEXT_DIM);
setFlex("insp-title", "height", 14);

createLabel("insp-hint", "Cmd+click any element in the preview to inspect its properties.", "inspector-area");
setFontSize("insp-hint", 10);
setTextColor("insp-hint", APP_TEXT_DIM);
setFlex("insp-hint", "height", 28);

// Inspector property rows (placeholder)
createRow("insp-row-type", "inspector-area");
setFlex("insp-row-type", "height", 20);
setFlex("insp-row-type", "align_items", "center");
createLabel("insp-type-k", "Type:", "insp-row-type");
setFontSize("insp-type-k", 10);
setTextColor("insp-type-k", APP_TEXT_DIM);
setFlex("insp-type-k", "width", 60);
createLabel("insp-type-v", "—", "insp-row-type");
setFontSize("insp-type-v", 10);
setFlex("insp-type-v", "flex_grow", 1);

createRow("insp-row-id", "inspector-area");
setFlex("insp-row-id", "height", 20);
setFlex("insp-row-id", "align_items", "center");
createLabel("insp-id-k", "ID:", "insp-row-id");
setFontSize("insp-id-k", 10);
setTextColor("insp-id-k", APP_TEXT_DIM);
setFlex("insp-id-k", "width", 60);
createLabel("insp-id-v", "—", "insp-row-id");
setFontSize("insp-id-v", 10);
setFlex("insp-id-v", "flex_grow", 1);

createRow("insp-row-bounds", "inspector-area");
setFlex("insp-row-bounds", "height", 20);
setFlex("insp-row-bounds", "align_items", "center");
createLabel("insp-bounds-k", "Bounds:", "insp-row-bounds");
setFontSize("insp-bounds-k", 10);
setTextColor("insp-bounds-k", APP_TEXT_DIM);
setFlex("insp-bounds-k", "width", 60);
createLabel("insp-bounds-v", "—", "insp-row-bounds");
setFontSize("insp-bounds-v", 10);
setFlex("insp-bounds-v", "flex_grow", 1);

// Tab switching logic
var activeTab = "chat";
function switchTab(tab) {
    activeTab = tab;
    if (tab === "chat") {
        setVisible("chat-area", true);
        setVisible("inspector-area", false);
        setTextColor("tab-chat", APP_ACCENT);
        setBackground("tab-chat-line", APP_ACCENT);
        setTextColor("tab-inspector", APP_TEXT_DIM);
        setBackground("tab-inspector-line", APP_PANEL);  // hide by matching background
    } else {
        setVisible("chat-area", false);
        setVisible("inspector-area", true);
        setTextColor("tab-inspector", APP_ACCENT);
        setBackground("tab-inspector-line", APP_ACCENT);
        setTextColor("tab-chat", APP_TEXT_DIM);
        setBackground("tab-chat-line", APP_PANEL);  // hide by matching background
    }
    layout();
}

// Tab switching via registerClick + on() click events
registerClick("tab-inspector-col");
registerClick("tab-chat-col");
registerClick("tab-inspector");
registerClick("tab-chat");
on("tab-inspector-col", "click", function() { switchTab("inspector"); });
on("tab-chat-col", "click", function() { switchTab("chat"); });
on("tab-inspector", "click", function() { switchTab("inspector"); });
on("tab-chat", "click", function() { switchTab("chat"); });

// Chat content area
createCol("chat-area", "right-panel");
setFlex("chat-area", "flex_grow", 1);
setFlex("chat-area", "padding", 10);
setFlex("chat-area", "gap", 8);

// Model selector
createRow("model-row", "chat-area");
setFlex("model-row", "height", 24);
setFlex("model-row", "flex_shrink", 0);
setFlex("model-row", "align_items", "center");
setFlex("model-row", "justify_content", "space-between");

createLabel("context-label", "Editing: All Components", "model-row");
setFontSize("context-label", 9);
setTextColor("context-label", APP_TEXT_DIM);

createCombo("model-selector", "model-row");
setItems("model-selector", ["Sonnet 4.6", "Opus 4.6"]);
setFlex("model-selector", "width", 100);
setFlex("model-selector", "height", 22);

// Chat messages (scrollable)
createScrollView("chat-messages", "chat-area");
setFlex("chat-messages", "flex_grow", 1);
setScrollContentSize("chat-messages", 252, 400);

createLabel("welcome-msg", "Describe a visual style and the preview will update live.", "chat-messages");
setFontSize("welcome-msg", 11);
setFlex("welcome-msg", "height", 30);

createLabel("hint-msg", 'Try: "warm vintage" or "neon cyberpunk"', "chat-messages");
setFontSize("hint-msg", 10);
setTextColor("hint-msg", APP_TEXT_DIM);
setFlex("hint-msg", "height", 16);

// Chat input area
createRow("chat-input-row", "chat-area");
setFlex("chat-input-row", "height", 32);
setFlex("chat-input-row", "flex_shrink", 0);
setFlex("chat-input-row", "gap", 6);

// Upload button (camera icon placeholder)
createCol("upload-btn", "chat-input-row");
setFlex("upload-btn", "width", 28);
setFlex("upload-btn", "height", 28);
setBackground("upload-btn", APP_PANEL);
setBorder("upload-btn", APP_BORDER, 1, 6);
setFlex("upload-btn", "justify_content", "center");
setFlex("upload-btn", "align_items", "center");
createIcon("upload-icon", "image_upload", "upload-btn");
setFlex("upload-icon", "width", 20);
setFlex("upload-icon", "height", 20);

createTextEditor("chat-input", "chat-input-row");
setPlaceholder("chat-input", "Describe a style...");
setFlex("chat-input", "flex_grow", 1);
setFlex("chat-input", "height", 28);

// Send button (arrow icon placeholder)
createCol("send-btn", "chat-input-row");
setFlex("send-btn", "width", 28);
setFlex("send-btn", "height", 28);
setBackground("send-btn", APP_ACCENT);
setBorder("send-btn", APP_ACCENT, 0, 6);
setFlex("send-btn", "justify_content", "center");
setFlex("send-btn", "align_items", "center");
createIcon("send-icon", "send", "send-btn");
setFlex("send-icon", "width", 20);
setFlex("send-icon", "height", 20);

// ═══════════════════════════════════════════════════════════════════
// STATUS BAR (28px, full width)
// ═══════════════════════════════════════════════════════════════════
createRow("status-bar");
setFlex("status-bar", "height", 28);
setFlex("status-bar", "flex_shrink", 0);
setFlex("status-bar", "padding_left", 12);
setFlex("status-bar", "padding_right", 12);
setFlex("status-bar", "align_items", "center");
setFlex("status-bar", "justify_content", "space-between");
setBackground("status-bar", APP_SURFACE);
setBorder("status-bar", APP_BORDER, 1, 0);

createLabel("status-text", "0 tokens modified", "status-bar");
setFontSize("status-text", 10);
setTextColor("status-text", APP_TEXT_DIM);

createLabel("status-schema", "pulp-theme/v1", "status-bar");
setFontSize("status-schema", 10);
setTextColor("status-schema", APP_TEXT_DIM);

// ═══════════════════════════════════════════════════════════════════
// Inspector: Cmd+click detection
// ═══════════════════════════════════════════════════════════════════
enableInspectClick();
on("__inspect__", "click", function(widgetId) {
    // Populate inspector with widget info
    setText("insp-type-v", widgetId ? "View" : "—");
    setText("insp-id-v", widgetId || "—");
    setText("insp-bounds-v", "—");  // bounds not accessible from JS yet
    switchTab("inspector");
});

// ═══════════════════════════════════════════════════════════════════
// Global keyboard shortcuts (Cmd+Z undo, Cmd+Shift+Z redo)
// ═══════════════════════════════════════════════════════════════════
on("__global__", "keydown", function(evt) {
    if (!evt) return;
    var cmd = (evt.mods & 0x18) !== 0;  // kModMeta | kModCmd
    var shift = (evt.mods & 0x01) !== 0;
    // 'z' key = 122 ASCII or platform key code
    if (cmd && evt.key === 122) {
        if (shift) {
            // Redo
            if (historyIndex < themeHistory.length - 1) {
                historyIndex++;
                applyTokenDiff(themeHistory[historyIndex]);
                updateTokenSwatches();
                setText("status-text", "Redo (" + historyIndex + "/" + (themeHistory.length - 1) + ")");
                layout();
            }
        } else {
            // Undo
            if (historyIndex > 0) {
                historyIndex--;
                applyTokenDiff(themeHistory[historyIndex]);
                updateTokenSwatches();
                setText("status-text", "Undo (" + historyIndex + "/" + (themeHistory.length - 1) + ")");
                layout();
            }
        }
    }
});

// ═══════════════════════════════════════════════════════════════════
// Chat logic
// ═══════════════════════════════════════════════════════════════════

function addChatMessage(role, text) {
    var id = "msg-" + (msgCount++);
    // Capture theme snapshot for this message
    var snapshot = getThemeJson();

    // Estimate height: role(12) + gap(4) + text(~14/line) + restore btn(16) + padding(16)
    var lineCount = Math.ceil(text.length / 30);
    var hasRestore = (role === "assistant");
    var msgHeight = 12 + 4 + lineCount * 14 + (hasRestore ? 20 : 0) + 16;

    createCol(id, "chat-messages");
    setFlex(id, "height", msgHeight);
    setFlex(id, "padding", 8);
    setFlex(id, "gap", 4);
    setBorder(id, APP_BORDER, 1, 6);
    if (role === "user") {
        setBackground(id, "#2a2a3c");
    } else {
        setBackground(id, APP_PANEL);
    }

    // Role label row with optional restore button
    createRow(id + "-header", id);
    setFlex(id + "-header", "height", 12);
    setFlex(id + "-header", "align_items", "center");
    setFlex(id + "-header", "justify_content", "space-between");

    createLabel(id + "-role", role === "user" ? "You" : "Designer", id + "-header");
    setFontSize(id + "-role", 9);
    setTextColor(id + "-role", APP_TEXT_DIM);
    setFlex(id + "-role", "width", 60);

    if (hasRestore) {
        var restoreId = id + "-restore";
        createLabel(restoreId, "Restore", id + "-header");
        setFontSize(restoreId, 8);
        setTextColor(restoreId, APP_ACCENT);
        setFlex(restoreId, "width", 44);
        registerClick(restoreId);
        // Capture snapshot in closure
        (function(snap, rid) {
            on(rid, "click", function() {
                applyTokenDiff(snap);
                updateTokenSwatches();
                buildShadeRamps();
                setText("status-text", "Restored snapshot");
                layout();
            });
        })(snapshot, restoreId);
    }

    createLabel(id + "-text", text, id);
    setFontSize(id + "-text", 11);
    setFlex(id + "-text", "height", lineCount * 14);

    // Update scroll content size to fit all messages
    var totalHeight = (msgCount + 1) * (msgHeight + 8) + 60;
    setScrollContentSize("chat-messages", 252, totalHeight);
    layout();
}

on("chat-input", "return", function(text) {
    if (!text || text.length === 0) return;
    addChatMessage("user", text);
    setText("chat-input", "");
    setText("status-text", "Generating...");
    layout();

    var themeJson = getThemeJson();
    var model = "claude-sonnet-4-6";
    var prompt = "You are a design token expert for audio plugin UIs.\n";
    prompt += "Modify design tokens to achieve the requested style.\n\n";
    prompt += "## Current Theme\n" + themeJson + "\n\n";
    prompt += "## RULES\n1. Output ONLY JSON. No markdown.\n";
    prompt += "2. Include ONLY tokens that CHANGE (5-12 colors typically).\n";
    prompt += "3. Do NOT change dimensions unless asked.\n\n";
    prompt += '## Request\n"' + text + '"\n\n## Output\n';

    var tmpFile = "/tmp/pulp-design-prompt.txt";
    exec("cat > " + tmpFile + " << 'PULPEOF'\n" + prompt + "\nPULPEOF");
    var response = exec("cat " + tmpFile + " | claude --print --model " + model + " 2>/dev/null");

    if (!response || response.length === 0) {
        addChatMessage("assistant", "No response from Claude");
        setText("status-text", "Error");
        layout();
        return;
    }

    var jsonStart = response.indexOf("{");
    var jsonEnd = response.lastIndexOf("}");
    if (jsonStart < 0 || jsonEnd < 0) {
        addChatMessage("assistant", "No JSON in response");
        setText("status-text", "Error");
        layout();
        return;
    }

    var jsonDiff = response.substring(jsonStart, jsonEnd + 1);
    applyTokenDiff(jsonDiff);
    pushThemeSnapshot();

    var count = (jsonDiff.match(/#[0-9a-fA-F]{6}/g) || []).length;
    addChatMessage("assistant", "Applied " + count + " token changes");
    setText("status-text", count + " tokens modified");
    layout();
});

// Send button triggers same as return key
registerClick("send-btn");
on("send-btn", "click", function() {
    var text = getText("chat-input");
    if (text && text.length > 0) {
        // Trigger the return handler by dispatching manually
        __dispatch__("chat-input", "return", text);
    }
});

// Preset handler
on("preset-selector", "select", function(idx) {
    var presets = [
        { theme: "dark", accent: "#89B4FA" },
        { theme: "light", accent: "#2563EB" },
        { theme: "pro_audio", accent: "#89B4FA" },
        { theme: "dark", accent: "#AA88FF" },
        { theme: "dark", accent: "#D4A017" },
        { theme: "dark", accent: "#0EA5E9" },
        { theme: "dark", accent: "#FF00FF" }
    ];
    var p = presets[idx];
    currentAccent = p.accent;
    setTheme(p.theme);
    setText("theme-name-label", ["Default Dark","Light","Pro Audio","Violet","Amber","Ocean","Neon"][idx]);
    buildShadeRamps();
    updateTokenSwatches();
    pushThemeSnapshot();
    layout();
});

// ═══════════════════════════════════════════════════════════════════
// Export/Import buttons
// ═══════════════════════════════════════════════════════════════════
registerClick("export-btn-pill");
on("export-btn-pill", "click", function() {
    var json = getThemeJson();
    var path = "/tmp/pulp-theme-export.json";
    exec("cat > " + path + " << 'PULPEOF'\n" + json + "\nPULPEOF");
    setText("status-text", "Exported to " + path);
    layout();
});

registerClick("import-btn-pill");
on("import-btn-pill", "click", function() {
    var path = "/tmp/pulp-theme-export.json";
    var json = exec("cat " + path + " 2>/dev/null");
    if (json && json.length > 10) {
        applyTokenDiff(json);
        updateTokenSwatches();
        setText("status-text", "Imported from " + path);
    } else {
        setText("status-text", "No theme file found");
    }
    layout();
});

// Undo/Redo (snapshot-based)
var themeHistory = [];
var historyIndex = -1;

function pushThemeSnapshot() {
    var snap = getThemeJson();
    if (historyIndex < themeHistory.length - 1) {
        themeHistory = themeHistory.slice(0, historyIndex + 1);
    }
    themeHistory.push(snap);
    historyIndex = themeHistory.length - 1;
}
pushThemeSnapshot();

registerClick("undo-btn-pill");
on("undo-btn-pill", "click", function() {
    if (historyIndex > 0) {
        historyIndex--;
        applyTokenDiff(themeHistory[historyIndex]);
        updateTokenSwatches();
        setText("status-text", "Undo (" + historyIndex + "/" + (themeHistory.length - 1) + ")");
        layout();
    }
});

registerClick("redo-btn-pill");
on("redo-btn-pill", "click", function() {
    if (historyIndex < themeHistory.length - 1) {
        historyIndex++;
        applyTokenDiff(themeHistory[historyIndex]);
        updateTokenSwatches();
        setText("status-text", "Redo (" + historyIndex + "/" + (themeHistory.length - 1) + ")");
        layout();
    }
});

layout();

"Implement Phase 9: themeable widget library + resource/asset system for the v3 track.

References:
- planning/dsl-agentic-validation-webgpu-audit-v3.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase1-contract.md
- core/view/include/pulp/view/theme.hpp
- core/view/src/theme.cpp
- core/view/include/pulp/view/widgets.hpp
- core/view/src/widgets.cpp
- core/view/include/pulp/view/view.hpp
- examples/design-tool/design-tool.js
- Solar theme system at ~/Code/Solar (token structure reference)

GOAL:
Evolve the existing token-based theme system into a full theme management platform with OS appearance tracking, import/export, accessibility-aware contrast, and a rich set of bundled themes. Expand the widget library to fill gaps identified by comparison with JUCE, iPlug2, and Visage. Build a centralized resource/asset system for images, fonts, shaders, and embedded resources.

DEPENDENCIES:
- Requires Phase 1 contract/truth pass

CAN RUN IN PARALLEL:
- yes, after Phase 1
- can run in parallel with Phases 2, 3, 6, 8, 10, 11

EXISTING FOUNDATION:
- Token-based Theme struct with color/dimension/string maps (theme.hpp)
- JSON serialization (from_json/to_json)
- 3 built-in presets: dark(), light(), pro_audio()
- 13 token groups in style designer: Background, Text, Accent, Controls, Knob, Slider, Meter, Waveform, Cards, Overlay, Tabs, Effects, Gradient
- Style designer with per-token undo/redo, live hot-reload, preset selector
- Widget set: Knob, Fader, Toggle, Checkbox, ToggleButton, XYPad, Meter, WaveformView, SpectrumView, Panel, ScrollView, TabPanel, ListBox, TextEditor, Label, ComboBox, Tooltip, ProgressBar, CallOutBox, Icon, ImageView, CanvasWidget

NON-NEGOTIABLES:
- Build on the existing Theme/token infrastructure. Do not replace it.
- Theme tokens must follow the established dot-notation convention (bg.primary, text.secondary, etc.).
- All new widgets must be token-aware — they read colors/dimensions from the active theme.
- OS appearance change must propagate automatically (light↔dark) unless the user has locked a specific theme.
- Contrast logic must be real: when a background is light, text must be dark, and vice versa. Use WCAG AA contrast ratio (4.5:1 for normal text, 3:1 for large text) as the floor.
- Theme import/export must use JSON, compatible with the existing from_json/to_json format.
- The style designer must remain the primary theme authoring tool — enhance it, don't bypass it.
- Align with the Solar theme system's approach where applicable: semantic token names, light/dark pairs per theme, clean preset definition structure.

DELIVERABLES:

### Theme Management Platform
1. OS appearance tracking:
   - detect system light/dark mode on macOS (NSAppearance) and Windows (registry/theme API)
   - auto-switch theme when OS appearance changes
   - option to lock a specific theme regardless of OS setting
   - callback/event when appearance changes
2. Theme import/export:
   - save current theme to JSON file
   - load theme from JSON file
   - validate loaded themes (required tokens present, colors parse correctly)
   - merge imported theme with defaults for missing tokens
3. Theme preset library:
   - port all 38 tweakcn-sourced presets from ~/Code/Solar/Solar/Models/ThemePresets.swift
   - each preset already has light and dark variants (paired) with 15 semantic colors + 5 chart colors
   - presets: amberMinimal, amethystHaze, boldTech, bubblegum, caffeine, candyland, catppuccin,
     claude, claymorphism, cleanSlate, cosmicNight, cyberpunk, doom64, elegantLuxury, graphite,
     kodamaGrove, midnightBloom, mochaMousse, modernMinimal, mono, nature, neoBrutalism,
     northernLights, notebook, oceanBreeze, pastelDreams, perpetuity, quantumRose, retroArcade,
     solarDusk, starryNight, supabase, sunsetHorizon, t3Chat, tangerine, twitter, vintagePaper,
     violetBloom
   - map Solar's SemanticColors (background, foreground, card, primary, secondary, muted, accent,
     destructive, border, input, ring + chart colors) to Pulp's token groups via a derivation layer
   - IMPORTANT: tweakcn presets define ~15 semantic UI tokens but Pulp has ~50 tokens including
     audio-specific groups (Knob, Slider, Meter, Waveform, Controls) with no tweakcn equivalent.
     The derivation layer auto-generates audio tokens from the semantic base:
       - accent.primary → knob.arc, slider.fill, progress.fill, spinner, tab.active
       - background → knob.arc.bg, slider.track, control.track, waveform.grid background
       - foreground → knob.thumb, slider.thumb, control.thumb
       - muted/mutedForeground → waveform.grid, control.border, text.secondary, text.disabled, tab.inactive
       - destructive → meter.red, accent.error
       - accent with warm hue shift → meter.yellow, accent.warning
       - accent with green hue shift → meter.green, accent.success
       - card → card.empty, card.ready
       - card with muted tint → card.loading
       - destructive with low alpha → card.error
       - border → modal.border, control.border
       - secondary → bg.secondary, overlay.bg
       - primary → gradient.start; secondary → gradient.end
       - input → bg.surface
     This derivation must be documented and overridable — a preset can supply explicit audio tokens
     to override any derived value. The style designer must show both derived and explicit tokens.
   - keep existing dark/light/pro_audio presets as Pulp-native defaults alongside the ported set
     (these define all tokens explicitly, not via derivation)
   - preset definition structure: name, description, semantic colors (light+dark), optional explicit audio token overrides
   - source: https://github.com/jnsahaj/tweakcn/blob/82c7d1e079ca6d53f656895bfcdf3467d6cf9844/utils/theme-presets.ts#L1752
4. Accessibility-aware contrast:
   - auto-contrast function: given a background color, compute accessible foreground color
   - WCAG AA compliance check for any token pair
   - theme validation: warn when token combinations fail contrast requirements
   - auto-adjust mode: when switching themes, fix contrast violations automatically
5. Token system expansion:
   - add dimension tokens for standard spacing scale (xs, sm, md, lg, xl)
   - add dimension tokens for border radii, font sizes, line heights
   - add string tokens for font families
   - component-level token overrides (e.g., a specific panel can override bg.primary locally)

### Widget Library Expansion
6. New widgets:
   - EqCurveView: parametric EQ curve visualization with draggable band handles
   - MidiKeyboard: piano keyboard widget for note input/display
   - ColorPicker: HSL/HSB/hex color selection with swatches
   - FileDropZone: drag-and-drop file target with visual feedback
   - TreeView: hierarchical expandable tree (for presets, file browsers)
   - PropertyList: key-value property editor (for inspector panels)
   - Breadcrumb: navigation breadcrumb trail
   - SplitView: resizable split pane (horizontal/vertical)
7. Widget enhancements:
   - Button variants: primary, secondary, danger, ghost, icon-only
   - ComboBox: searchable/filterable dropdown
   - TabPanel: closeable tabs, reorderable tabs, overflow menu
   - Tooltip: rich tooltips with formatted text and images

### Resource/Asset System
8. Centralized AssetManager:
   - load images from file path, memory buffer, or embedded resource
   - load fonts from file path or embedded resource
   - shader string management with compilation error reporting
   - Lottie animation loading from file or embedded
   - resource caching with LRU eviction
   - async loading with completion callbacks
9. Resource bundling:
   - CMake helper to embed assets in binary (images, fonts, shaders, Lottie JSON)
   - runtime lookup by asset name
   - platform-appropriate asset resolution (1x/2x for retina)
   - FORWARD COMPATIBILITY FOR PHASE 13 (Three.js bridge):
     Three.js loads textures, models, and shaders via Image/fetch. Phase 13 will shim
     these to call AssetManager. Design the loading API so it can be called from:
     - C++ directly (normal Pulp usage)
     - JS via native bindings (Phase 13's Image/fetch shims)
     Support loading from: file path, embedded resource, memory buffer, data: URI.
     Return raw pixel data (RGBA bytes) that can be uploaded to Dawn via writeTexture().
10. Font system:
    - custom font file loading (TTF/OTF)
    - font fallback chain
    - font metrics API for layout

### Style Designer Enhancements
11. Update style designer to support:
    - theme import/export from the UI
    - light/dark variant editing side-by-side
    - contrast validation indicators on token pairs
    - new widget previews for added widgets
    - dimension and string token editing (not just colors)

### Tests
12. Tests:
    - theme round-trip: create → export JSON → import → verify identical
    - OS appearance tracking: simulated appearance change triggers theme switch
    - contrast validation: known-good and known-bad pairs produce correct results
    - new widgets: headless screenshot tests for each new widget
    - new widgets: interaction tests (click, drag, keyboard) for interactive widgets
    - asset loading: load from file, memory, and embedded resource
    - asset caching: verify LRU eviction under memory pressure
    - font loading: custom font renders correctly in headless screenshot

ACCEPTANCE:
- themes can be imported/exported as JSON files
- OS appearance changes auto-switch themes
- contrast logic prevents illegible text/background combinations
- 38+ bundled theme presets (ported from tweakcn via Solar) with light/dark variants plus 3 Pulp-native defaults
- new widgets are real, token-aware, and test-backed
- AssetManager provides centralized resource loading with caching
- style designer supports the expanded capabilities
- docs describe the theme management and widget library at implemented scope

Use repo prompt when searching the codebase

ONLY WHEN ALL CONDITIONS ARE MET:
Output exactly: <promise>DONE</promise>

IF STUCK:
- After 20 iterations, document in LEARNINGS.md:
- What is blocked
- Why
- What was attempted
- What assumption may be wrong" --completion-promise "DONE" --max-iterations 120

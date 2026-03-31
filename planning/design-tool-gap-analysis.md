# Design Tool Gap Analysis

**Date:** 2026-03-28
**Comparing:** `ai-style-designer/Tools/theme-designer.html` (reference) vs `pulp/examples/design-tool/` (current)

---

## What Works in the Pulp Desktop App

- 3-panel layout (left/center/right)
- Color system with OKLCH, harmony modes, shade ramps
- Hue slider with live ramp regeneration
- Token browser with search/filter
- Preview: foundations, knobs, fader, buttons, toggles, text inputs, waveform, spectrum, meters, cards, tabs, overlays, states, effects, GPU showcase
- Chat with Claude (Sonnet/Opus model selector)
- Chat snapshots with restore
- Undo/redo (Cmd+Z / Cmd+Shift+Z)
- Import/export (JSON to /tmp/)
- Dark/light mode switching
- 7 built-in presets
- Inspector (Cmd+click shows Type/ID)
- Status bar with token count

---

## Gaps: Features in HTML Reference Missing from Pulp Desktop

### P0 — Broken / Critically Missing

| # | Gap | HTML Has | Pulp Has | Impact |
|---|-----|----------|----------|--------|
| 1 | **Token editing** — click a token swatch to edit its color | Full inline palette picker + gamut picker | View-only swatches, no click-to-edit | Can't manually tweak individual tokens |
| 2 | **Token hex input** — type a hex value directly for any token | Hex text input next to each swatch | No hex input | Can't type exact colors |
| 3 | **Token modification tracking** — status bar shows "N tokens modified" | Counts and displays modified tokens | Shows "0 tokens modified" always | No feedback on what changed |
| 4 | **Shade swatch click → color picker** | Click any shade → inline picker with H/C/L sliders | Click shade → basic picker with 3 sliders | Picker exists but limited (no gamut triangle, no palette context) |
| 5 | **Per-token undo/redo** | Per-token history with reset-to-original | Global undo only | Can't revert individual token changes |
| 6 | **Scrolling in preview panel** | Full scrollable preview | Scroll works but content may be clipped | May need content size tuning |

### P1 — Feature Gaps (High Value)

| # | Gap | HTML Has | Pulp Has | Impact |
|---|-----|----------|----------|--------|
| 7 | **Export format tabs** | 7 formats: JSON, CSS Vars, C++ Header, Palette, OKLCH, Color System, Style System | JSON only | Limits integration with other tools |
| 8 | **Export preview** | Code preview with syntax display before download | Direct file write, no preview | Can't review before saving |
| 9 | **Gamut triangle picker** | Interactive 2D gamut triangle (chroma × lightness) with draggable dot | H/C/L sliders only | Less intuitive color selection |
| 10 | **State scrubber pills** | Toolbar pills: Default, Hover, Focus, Disabled, Error — live-switch preview | States shown as static preview section | Can't see full UI in each state |
| 11 | **Palette template selector** | Dropdown: Tailwind slate/gray/zinc/neutral/stone/red/orange/amber/etc. | None — accent-driven only | Can't start from Tailwind palettes |
| 12 | **Generate Opposite Mode** | One-click generates light from dark (or vice versa) | Manual mode switch only | Tedious to create both modes |
| 13 | **Context-scoped chat** | Cmd+click component → chat edits only that component's tokens | Chat edits all tokens | Less precise AI editing |
| 14 | **Image upload in chat** | Upload reference image for AI inspiration | Upload icon exists but no implementation | Can't show AI a reference design |
| 15 | **Chat change summary** | Shows diff of what changed after AI response | Shows message only | Hard to see what AI modified |

### P2 — Polish Gaps (Nice to Have)

| # | Gap | HTML Has | Pulp Has | Impact |
|---|-----|----------|----------|--------|
| 16 | **Full token registry** | 50+ semantic tokens (Modal, Tooltip, Context Menu, Drag, Focus Ring, etc.) | 16 tokens in 4 groups | Limited design control |
| 17 | **Toast notifications** | Temporary messages for actions (copied, applied, restored) | No toast system | No action feedback |
| 18 | **Help/info modals** | (?) buttons with context-sensitive help | None | Steeper learning curve |
| 19 | **Contrast ratio display** | WCAG compliance checking (AAA/AA/fail) | None | No accessibility feedback |
| 20 | **Spinner/loading animation** | Animated spinner in preview | Static "loading" label | Less realistic preview |
| 21 | **Dropdown/combo preview** | Dropdown with multi-option hover states | Basic combo widget | Less complete preview |
| 22 | **Modal/tooltip overlays** | Static overlay previews (modal, context menu, tooltip) | Basic overlays section | Could be more detailed |
| 23 | **Palette save/load** | Save/load individual palette configurations | Only full theme import/export | Can't reuse palettes across themes |
| 24 | **Chat export** | Export chat history | No chat export | Can't save design conversation |
| 25 | **Style system properties** | Geometry (corner radius, border width, shadow), typography sizes, widget styles | Colors/dimensions only | Can't control shapes/sizes from designer |

---

## Proposed Phases

### Phase D1: Token Editing (gaps 1-5)
Make the design tool actually useful for manual tweaking.
- Click token swatch → open color picker with palette context
- Hex input field next to each token swatch
- Track modified tokens, show count in status bar
- Per-token undo with reset-to-original button
- Enhanced color picker with gamut triangle

### Phase D2: Export & State (gaps 7-10)
Professional export workflow and state visualization.
- Multi-format export dialog (JSON, CSS, C++, OKLCH)
- Code preview before download/copy
- State scrubber pills in toolbar (Default/Hover/Focus/Disabled/Error)
- Live state switching across all preview components

### Phase D3: AI Enhancement (gaps 13-15)
Make the AI design assistant more precise and useful.
- Context-scoped editing (Cmd+click component → scope chat to it)
- Image upload for reference/inspiration
- Change summary display after AI response
- Diff visualization (before/after token values)

### Phase D4: Palette & Template (gaps 11-12, 23)
Starting points and palette management.
- Tailwind color template selector
- Generate Opposite Mode (auto light↔dark)
- Save/load individual palettes
- Palette preset library

### Phase D5: Full Token Registry (gap 16, 25)
Expand from 16 tokens to 50+ semantic roles.
- Add all missing tokens (modal, tooltip, focus ring, drag, etc.)
- Add geometry/typography properties
- Token sections matching HTML reference (Visage, Custom, Values, JUCE)

### Phase D6: Polish (gaps 17-22, 24)
Production quality touches.
- Toast notification system
- Help/info modals
- WCAG contrast ratio display
- Animated spinner preview
- Enhanced dropdown/combo preview
- Chat history export

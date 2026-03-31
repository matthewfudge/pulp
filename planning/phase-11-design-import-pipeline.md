# Phase 11 — Design Import Pipeline (Draft)

**Version:** 2026-03-29
**Status:** Draft — refine after Phase 10 ships
**Depends on:** Phase 10 (shader design system)
**Goal:** Design in Figma/Stitch/v0/Pencil → import into Pulp project → open in AI design tool for refinement.

---

## Vision

A developer designs a plugin UI in their preferred tool (Figma, Stitch, v0, Pencil), then runs one command to import that design into their Pulp project. The import translates layout, colors, typography, and components into Pulp code. They can then open the AI design tool to refine the style with shaders, animations, and audio-specific widgets.

---

## User Flow

```
1. Design in external tool (Figma / Stitch / v0 / Pencil)
2. Export or connect via MCP
3. Run: pulp import-design --from figma --file design.json
   OR: pulp import-design --from stitch --project my-project
   OR: pulp import-design --from v0 --url https://v0.dev/t/abc123
   OR: pulp import-design --from pencil --file design.pen
4. CLI generates: my-plugin-ui.js (Pulp web-compat code)
5. Open in design tool to add audio widgets, refine shaders, adjust tokens
```

---

## Architecture

### Layer 1: Design Source Adapters

Each external tool gets an adapter that reads its output format and produces a normalized intermediate representation (IR):

| Source | Input | Adapter |
|--------|-------|---------|
| **Figma** | MCP JSON (via com.figma.mcp) or .fig export | Parse frames, auto-layout, fills, strokes, effects, text, components |
| **Stitch** | MCP screen data or HTML export | Parse component tree, inline styles, design system tokens |
| **v0** | React/TSX + Tailwind via API | Parse JSX tree, extract Tailwind classes, map shadcn/ui components |
| **Pencil** | MCP node tree or .pen export | Parse nodes, properties, variables, style guide |

### Layer 2: Normalized Intermediate Representation

```json
{
  "type": "frame",
  "name": "PluginUI",
  "layout": { "direction": "column", "gap": 16, "padding": 16 },
  "style": { "backgroundColor": "#1a1a2e", "borderRadius": 8 },
  "children": [
    {
      "type": "text",
      "content": "My Plugin",
      "style": { "fontSize": 24, "fontWeight": 700, "color": "#e0e0e0" }
    },
    {
      "type": "frame",
      "layout": { "direction": "row", "gap": 8 },
      "children": [
        { "type": "slider", "label": "Gain", "min": 0, "max": 1 },
        { "type": "slider", "label": "Mix", "min": 0, "max": 1 }
      ]
    }
  ],
  "tokens": {
    "colors": { "bg.primary": "#1a1a2e", "accent.primary": "#e94560" },
    "dimensions": { "spacing.md": 16, "radius.md": 8 }
  }
}
```

### Layer 3: Pulp Code Generator

Takes the IR and produces:
- **Pulp web-compat JS** — `document.createElement` / `element.style` code
- **Theme token JSON** — imported via `applyTokenDiff`
- **Widget schema JSON** — for any audio-specific widgets (knobs, faders, meters)

### Layer 4: CLI Command

```bash
# From Figma (via MCP or exported JSON)
pulp import-design --from figma --frame "Plugin UI"

# From Stitch (via MCP)
pulp import-design --from stitch --screen "Main Screen"

# From v0 (via URL or API)
pulp import-design --from v0 --url "https://v0.dev/t/abc123"

# From Pencil (via MCP or .pen file)
pulp import-design --from pencil --file design.pen

# Options
  --output my-plugin-ui.js    # output file (default: ui.js)
  --tokens theme-tokens.json  # extract design tokens
  --dry-run                   # show what would be generated without writing
```

### Layer 5: Claude Translation (the actual work)

The CLI doesn't hardcode translation rules — it uses Claude (or any configured AI) with the mapping tables:
1. Read the design source data
2. Build a prompt with the appropriate mapping doc (figma-to-pulp-mapping.md, etc.)
3. Claude generates Pulp code using the mapping
4. Write the output file

This means the translation quality improves with the AI model, and adding new source tools is just writing a new mapping doc.

---

## MCP Integration

For tools with MCP servers, the import can be live:

```
Figma MCP (com.figma.mcp)
  → read_file, get_selection, get_styles
    → Claude translates with figma-to-pulp-mapping.md
      → Pulp JS code

Stitch MCP (mcp__stitch__*)
  → get_screen, get_project, list_design_systems
    → Claude translates with stitch-to-pulp-mapping.md
      → Pulp JS code

Pencil MCP (mcp__pencil__*)
  → batch_get, get_variables, get_style_guide
    → Claude translates with pencil-to-pulp-mapping.md
      → Pulp JS code
```

The Claude Code plugin could even do this inline:
```
User: "import the design from my open Figma file"
Claude: [reads Figma MCP] → [translates] → [writes ui.js]
```

---

## What's NOT in Phase 11

- Real-time sync (Figma changes → Pulp updates live) — Phase 12+
- Visual diff (compare Figma design vs Pulp render) — Phase 12+
- Bidirectional: Pulp changes → update Figma — Phase 13+
- Component library sharing across tools — Phase 13+

---

## Mapping Docs (created in Phase 10)

These already exist and make the translation trivial for Claude:
- `planning/figma-to-pulp-mapping.md` — comprehensive
- `planning/stitch-to-pulp-mapping.md` — comprehensive
- `planning/v0-to-pulp-mapping.md` — comprehensive (Tailwind + shadcn/ui)
- `planning/pencil-to-pulp-mapping.md` — comprehensive

---

## Estimated Scope

| Component | Effort | Notes |
|-----------|--------|-------|
| Normalized IR format | ~1 day | JSON schema definition |
| Figma adapter | ~2 days | MCP integration + frame parsing |
| Stitch adapter | ~1 day | MCP integration + HTML parsing |
| v0 adapter | ~1 day | API call + TSX/Tailwind parsing |
| Pencil adapter | ~1 day | MCP integration + node parsing |
| Code generator | ~2 days | IR → Pulp web-compat JS |
| CLI command | ~1 day | `pulp import-design` |
| Claude prompt templates | ~1 day | Per-tool prompts using mapping docs |
| **Total** | **~10 days** | |

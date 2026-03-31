"Phase 11: Design Import Pipeline — Figma, Stitch, v0, Pencil → Pulp.

Reference: planning/phase-11-design-import-pipeline.md (full spec)
Reference: planning/figma-to-pulp-mapping.md
Reference: planning/stitch-to-pulp-mapping.md
Reference: planning/v0-to-pulp-mapping.md
Reference: planning/pencil-to-pulp-mapping.md
CLAUDE.md (build, test, architecture)

GOAL:
Build a design import pipeline so developers can design in Figma, Stitch, v0, or Pencil,
then import into their Pulp project with one command. The import translates layout, colors,
typography, and components into Pulp web-compat JS code + theme tokens.

Two entry points:
1. CLI command: pulp import-design --from figma/stitch/v0/pencil
2. Claude plugin: natural language 'import the design from my Figma file'

NON-NEGOTIABLES:
- Model-agnostic: AI CLI is configurable (Claude/Gemini/Codex)
- Translation uses mapping docs, not hardcoded rules
- Output is valid Pulp web-compat JS that runs without modification
- Theme tokens exported in W3C Design Tokens format
- All existing tests must pass

CODEX DELEGATION (OPTIONAL):
- Use codex exec --full-auto <task> for parallel work (tests, reviews, second opinions)
- Don't delegate tasks that depend on what you're currently building

PHASE 11.1 — CLI Command (pulp import-design):
Goal: pulp import-design --from <source> generates a Pulp JS file from a design.

1. Add 'import-design' subcommand to tools/cli/src/main.cpp:
   - --from: figma | stitch | v0 | pencil (required)
   - --file: input file path (for .fig, .pen, exported JSON)
   - --url: URL (for v0 share links, Figma file URLs)
   - --frame: frame/screen name to import (optional, defaults to first)
   - --output: output JS file (default: ui.js)
   - --tokens: output token file (default: tokens.json in W3C format)
   - --dry-run: show what would be generated

2. Source adapters (one per design tool):

   Figma adapter (--from figma):
   - Input: Figma MCP JSON (via com.figma.mcp read_file) or exported .fig/.json
   - Parse: frames, auto-layout, fills, strokes, effects, text, components
   - Use: planning/figma-to-pulp-mapping.md as translation reference

   Stitch adapter (--from stitch):
   - Input: Stitch MCP (get_screen) or HTML export
   - Parse: component tree, inline styles, design system tokens
   - Design system tokens → W3C format → Pulp theme
   - Use: planning/stitch-to-pulp-mapping.md

   v0 adapter (--from v0):
   - Input: v0 API response (POST /v1/chats) or pasted TSX+Tailwind
   - Parse: JSX tree → createElement calls, Tailwind → inline styles
   - Map: shadcn/ui components → Pulp widgets (Button→ToggleButton, Slider→Fader, etc.)
   - Use: planning/v0-to-pulp-mapping.md

   Pencil adapter (--from pencil):
   - Input: Pencil MCP (batch_get) or .pen file
   - Parse: node tree, fills, strokes, effects, layout, variables
   - Variables → W3C Design Tokens → Pulp theme
   - Pencil uses Yoga internally = near-perfect layout translation
   - Use: planning/pencil-to-pulp-mapping.md

3. Each adapter produces a normalized intermediate representation (IR):
   ```json
   {
     "type": "frame", "name": "PluginUI",
     "layout": { "direction": "column", "gap": 16, "padding": 16 },
     "style": { "backgroundColor": "#1a1a2e", "borderRadius": 8 },
     "children": [...],
     "tokens": { "colors": {...}, "dimensions": {...} }
   }
   ```

4. Code generator: IR → Pulp web-compat JS
   - Each IR node → document.createElement + style assignments
   - Layout → flexDirection, gap, padding, etc.
   - Components → appropriate Pulp widget (div, span, button, input, etc.)
   - Audio widgets detected by naming convention or annotation:
     'knob', 'fader', 'meter', 'waveform' → Pulp audio widgets
   - Tokens → W3C Design Tokens JSON file + applyTokenDiff call

5. Test: pulp import-design --from v0 --file sample.tsx --output test-ui.js
   Verify: generated JS loads and renders correctly in Pulp
   Commit: "Phase 11.1: pulp import-design CLI command"

PHASE 11.2 — Claude Plugin Integration:
Goal: Natural language design import via Claude Code plugin.

1. Add 'import-design' skill to claude/ plugin:
   - Trigger: "import design from figma/stitch/v0/pencil"
   - Trigger: "create plugin UI from this design"
   - Trigger: "translate this Figma file to Pulp"

2. MCP-aware flow:
   - If Figma MCP available: read current file/selection directly
   - If Stitch MCP available: list projects, get screen
   - If Pencil MCP available: get editor state, batch_get nodes
   - If no MCP: ask user for file/URL

3. Skill implementation:
   ```
   User: "import my Figma design for the plugin UI"
   Claude: [checks Figma MCP] → [reads current selection]
           → [applies figma-to-pulp-mapping.md]
           → [generates Pulp web-compat JS]
           → [writes ui.js + tokens.json]
   "Created ui.js with 12 elements and tokens.json with 24 design tokens."
   ```

4. Interactive refinement:
   - "The knob should be smaller" → Claude edits the generated JS
   - "Add a meter next to the gain knob" → Claude adds audio widget
   - "Use the dark theme colors" → Claude adjusts token values

5. Test: Open Figma file → "import this design" → valid Pulp JS generated
   Commit: "Phase 11.2: Claude plugin import-design skill"

PHASE 11.3 — Token Round-Trip:
Goal: Design tokens flow bidirectionally between external tools and Pulp.

1. W3C Design Tokens import:
   - Parse standard format: { "$value": "#hex", "$type": "color", "$description": "..." }
   - Map to Pulp Theme: colors, dimensions, strings
   - Bridge: importDesignTokens(json) → applyTokenDiff

2. W3C Design Tokens export:
   - Serialize Pulp Theme to W3C format
   - Bridge: exportDesignTokens() → W3C JSON string
   - CLI: pulp export-tokens --format w3c --output tokens.json

3. Figma Variables sync (via MCP):
   - Read Figma variables → W3C tokens → Pulp theme
   - Export Pulp theme → W3C tokens → update Figma variables

4. Stitch Design System sync (via MCP):
   - Read Stitch design system → map colors/fonts/roundness → Pulp tokens
   - Export Pulp tokens → create/update Stitch design system

5. Test: Round-trip: export Pulp tokens → import back → identical theme
   Commit: "Phase 11.3: W3C Design Token round-trip"

PHASE 11.4 — Polish + Documentation:
Goal: Smooth developer experience, comprehensive docs.

1. Documentation:
   - docs/guides/importing-designs.md — how to import from each tool
   - docs/reference/design-import.md — CLI reference, IR format, adapter API
   - Update docs/guides/from-react-css.md with v0/Stitch workflow

2. Error handling:
   - Unsupported Figma features → warning + fallback
   - Missing fonts → substitute with system font
   - Complex gradients → simplify to linear
   - Nested components → flatten to direct elements

3. Templates:
   - pulp create --template from-figma → project with import workflow
   - pulp create --template from-v0 → project with v0 API integration

4. Test: End-to-end: design in Stitch → import → run in Pulp → matches design
   Commit: "Phase 11.4: Import pipeline polish + documentation"

TESTING:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure --exclude-regex AudioWorkgroup

WHAT YOU GET WHEN DONE:
- pulp import-design --from figma → Pulp JS file from any Figma design
- pulp import-design --from stitch → Pulp JS file from Google Stitch screen
- pulp import-design --from v0 --url https://v0.dev/t/abc → Pulp JS from v0 generation
- pulp import-design --from pencil → Pulp JS file from Pencil/OpenPencil design
- 'Import my Figma design' in Claude → automatic translation
- W3C Design Tokens round-trip between Pulp and external design tools
- Mapping docs for Claude to translate any design on the fly
- Audio widgets auto-detected from naming conventions
- Theme tokens extracted and importable" --completion-promise "PHASE 11 COMPLETE" --max-iterations 150

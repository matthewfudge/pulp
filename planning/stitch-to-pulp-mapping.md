# Google Stitch → Pulp Translation Guide

How [Google Stitch](https://stitch.withgoogle.com/docs) design output maps to Pulp's rendering system.

---

## Overview

Stitch is Google's AI-powered design tool that generates UI screens from text prompts. It has an MCP server (`mcp__stitch__*`) and an SDK. Stitch outputs structured screen data with HTML rendering, design system tokens, and component hierarchies.

## Access Pattern

Stitch designs are accessed via:
- **MCP tools:** `generate_screen_from_text`, `edit_screens`, `get_screen`, `list_screens`, `get_project`
- **SDK:** `@anthropic/stitch-sdk` for programmatic screen generation
- **Export:** Screens can be exported as HTML with inline styles

## Stitch Output → Pulp

Stitch generates screens as component trees. Each component has layout, styling, and content properties.

### Layout

| Stitch | CSS | Pulp Web-Compat | Pulp Bridge |
|--------|-----|-----------------|-------------|
| Flex row | `display: flex; flex-direction: row` | `el.style.flexDirection = 'row'` | `setFlex(id, 'direction', 'row')` |
| Flex column | `display: flex; flex-direction: column` | `el.style.flexDirection = 'column'` | `setFlex(id, 'direction', 'col')` |
| Grid | `display: grid` | `el.style.gridTemplateColumns = '...'` | `setGrid(id, 'template_columns', '...')` |
| Gap | `gap: 16px` | `el.style.gap = '16px'` | `setFlex(id, 'gap', 16)` |
| Padding | `padding: 16px` | `el.style.padding = '16px'` | `setFlex(id, 'padding', 16)` |
| Alignment | `align-items: center` | `el.style.alignItems = 'center'` | `setFlex(id, 'align_items', 'center')` |
| Justify | `justify-content: space-between` | `el.style.justifyContent = 'space-between'` | `setFlex(id, 'justify_content', 'space-between')` |
| Width/Height | `width: 200px` | `el.style.width = '200px'` | `setFlex(id, 'width', 200)` |

### Visual

| Stitch | CSS | Pulp Web-Compat | Pulp Bridge |
|--------|-----|-----------------|-------------|
| Background color | `background-color` | `el.style.backgroundColor` | `setBackground(id, hex)` |
| Border | `border` | `el.style.border` | `setBorder(id, hex, width, radius)` |
| Border radius | `border-radius` | `el.style.borderRadius` | `setBorder(id, '', 0, radius)` |
| Shadow | `box-shadow` | `el.style.boxShadow` | `setBoxShadow(id, ...)` |
| Opacity | `opacity` | `el.style.opacity` | `setOpacity(id, val)` |

### Components → Widgets

| Stitch Component | HTML | Pulp Widget |
|-----------------|------|-------------|
| Container | `<div>` | View (row/col) |
| Text | `<p>`, `<h1>`-`<h6>`, `<span>` | Label |
| Button | `<button>` | ToggleButton |
| Input | `<input>` | TextEditor |
| Select | `<select>` | ComboBox |
| Image | `<img>` | ImageView |
| Card | `<div>` with styling | Panel |
| List | `<ul>` / `<ol>` | View with children |

### Design System → Pulp Tokens

Stitch supports design systems (`mcp__stitch__create_design_system`, `apply_design_system`). These map to Pulp's theme token system:

| Stitch Design System | Pulp Theme |
|---------------------|------------|
| Primary color | `theme.colors["accent.primary"]` |
| Background color | `theme.colors["bg.primary"]` |
| Text color | `theme.colors["text.primary"]` |
| Border radius token | `theme.dimensions["radius.md"]` |
| Spacing token | `theme.dimensions["spacing.md"]` |
| Font family | `theme.strings["font.family"]` |

## Translation Pipeline (Future Phase 11)

```
Stitch MCP (get_screen → HTML/component tree)
  → Parse component hierarchy and inline styles
    → Claude translates using this mapping table
      → Pulp web-compat JS or widget schema JSON

Stitch SDK (generate_screen_from_text → structured data)
  → Extract layout + styles + component types
    → Map to Pulp createElement + style assignments
```

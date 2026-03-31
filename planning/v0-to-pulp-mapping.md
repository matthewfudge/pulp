# v0.dev → Pulp Translation Guide

How [v0.dev](https://v0.dev/docs/api/platform/overview) output maps to Pulp's rendering system.

---

## Overview

v0 is Vercel's AI code generator. It takes natural language prompts (or screenshots) and generates React/Next.js components using Tailwind CSS and shadcn/ui. It has a REST API for programmatic generation.

## What v0 Outputs

- **React TSX** with shadcn/ui components
- **Tailwind CSS** utility classes for styling
- **TypeScript** with proper types
- Can generate from: text prompts, screenshots, URLs, or existing code

## Tailwind → Pulp

| Tailwind Class | CSS | Pulp Web-Compat |
|---------------|-----|-----------------|
| `flex flex-row` | `display:flex; flex-direction:row` | `el.style.flexDirection = 'row'` |
| `flex flex-col` | `display:flex; flex-direction:column` | `el.style.flexDirection = 'column'` |
| `gap-4` | `gap: 16px` | `el.style.gap = '16px'` |
| `p-4` | `padding: 16px` | `el.style.padding = '16px'` |
| `px-4 py-2` | `padding: 8px 16px` | `el.style.padding = '8px 16px'` |
| `m-auto` | `margin: auto` | `el.style.margin = 'auto'` |
| `w-full` | `width: 100%` | `el.style.width = '100%'` |
| `w-64` | `width: 256px` | `el.style.width = '256px'` |
| `min-w-0` | `min-width: 0` | `el.style.minWidth = '0'` |
| `max-w-md` | `max-width: 448px` | `el.style.maxWidth = '448px'` |
| `h-screen` | `height: 100vh` | `el.style.height = '100vh'` |
| `items-center` | `align-items: center` | `el.style.alignItems = 'center'` |
| `justify-between` | `justify-content: space-between` | `el.style.justifyContent = 'space-between'` |
| `grid grid-cols-3` | `display:grid; grid-template-columns: repeat(3,1fr)` | `el.style.gridTemplateColumns = '1fr 1fr 1fr'` |
| `bg-slate-900` | `background-color: #0f172a` | `el.style.backgroundColor = '#0f172a'` |
| `bg-gradient-to-r from-blue-500 to-purple-500` | `background: linear-gradient(to right, ...)` | `el.style.background = 'linear-gradient(to right, #3b82f6, #a855f7)'` |
| `text-sm` | `font-size: 14px` | `el.style.fontSize = '14px'` |
| `text-gray-400` | `color: #9ca3af` | `el.style.color = '#9ca3af'` |
| `font-bold` | `font-weight: 700` | `el.style.fontWeight = '700'` |
| `text-center` | `text-align: center` | `el.style.textAlign = 'center'` |
| `rounded-lg` | `border-radius: 8px` | `el.style.borderRadius = '8px'` |
| `border border-gray-700` | `border: 1px solid #374151` | `el.style.border = '1px solid #374151'` |
| `shadow-lg` | `box-shadow: 0 10px 15px -3px rgba(0,0,0,0.1)` | `el.style.boxShadow = '0 10px 15px -3px rgba(0,0,0,0.1)'` |
| `opacity-50` | `opacity: 0.5` | `el.style.opacity = '0.5'` |
| `overflow-hidden` | `overflow: hidden` | `el.style.overflow = 'hidden'` |
| `absolute top-0 right-0` | `position:absolute; top:0; right:0` | `el.style.position = 'absolute'; el.style.top = '0'` |
| `z-10` | `z-index: 10` | `el.style.zIndex = '10'` |
| `cursor-pointer` | `cursor: pointer` | `el.style.cursor = 'pointer'` |
| `transition-all duration-300` | `transition: all 0.3s` | `el.style.transition = 'all 0.3s'` |
| `hover:bg-slate-800` | `:hover { background-color }` | `StyleSheet({ '.class:hover': { backgroundColor: '#1e293b' } })` |

## shadcn/ui Components → Pulp Widgets

| shadcn/ui | HTML | Pulp Widget |
|-----------|------|-------------|
| `<Button>` | `<button>` | ToggleButton |
| `<Input>` | `<input>` | TextEditor |
| `<Slider>` | `<input type="range">` | Fader |
| `<Select>` | `<select>` | ComboBox |
| `<Checkbox>` | `<input type="checkbox">` | Checkbox |
| `<Switch>` | toggle | Toggle |
| `<Progress>` | `<progress>` | ProgressBar |
| `<Card>` | `<div>` styled | Panel |
| `<ScrollArea>` | `<div overflow:auto>` | ScrollView |
| `<Tabs>` | tab group | TabPanel |
| `<Dialog>` | modal | CallOutBox |
| `<Tooltip>` | tooltip | Tooltip |
| `<Label>` | `<label>` | Label |

## React Patterns → Pulp

| React | Pulp |
|-------|------|
| `<div className="...">` | `const el = document.createElement('div'); el.className = '...'` |
| `{children}` | `parent.appendChild(child)` |
| `onClick={handler}` | `el.addEventListener('click', handler)` |
| `onChange={handler}` | `el.addEventListener('change', handler)` |
| `useState(val)` | `getParam(name)` / `setParam(name, val)` |
| `useEffect(fn, [])` | Run at script load |
| `useRef()` | Store widget ID variable |
| `className={cn(...)}` | `el.className = [class1, class2].join(' ')` |
| `{condition && <El/>}` | `if (condition) parent.appendChild(el)` |
| `array.map(item => <El/>)` | `for (var i...) { parent.appendChild(createItem(item)) }` |

## Translation Pipeline (Future Phase 11)

```
v0 API (POST /v0/generate → React/Tailwind code)
  → Parse TSX + Tailwind classes
    → Claude translates using this mapping table
      → Pulp web-compat JS

v0 screenshot → v0 generates code → translate to Pulp
```

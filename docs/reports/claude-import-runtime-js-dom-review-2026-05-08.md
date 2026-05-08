# Claude Design Import Runtime — JS DOM Library Review

**Date:** 2026-05-08
**Author:** RepoPrompt review session
**Scope:** Evaluate `htmlparser2`, `jsdom`, `happy-dom`, and `react-testing-library` for patterns that would materially improve Pulp's Claude Design import/runtime materialization pipeline (`parse_claude_html_with_runtime`, `import-runtime.js`, `web-compat-*.js`, `WidgetBridge`).
**Bug under investigation:** Spectr's real Claude Design export reports "runtime success" while only materializing an HTML shell. Recent fixes (preserve inert `<script type="application/json">`, raise the success-floor) keep the static parser as a safety net but don't unblock the React commit.

---

## Context / Scope

Pulp's import pipeline today:

| Layer | File | Approximate role |
|---|---|---|
| Static parser (fallback) | `core/view/src/design_import.cpp` `parse_claude_html` | Loader-shell IR (~30 nodes) |
| Bundle envelope extraction | `core/view/src/design_import.cpp` `parse_claude_bundle` | Splits `<template>` HTML, gzip-base64 JS payloads, inline `text/babel` |
| HTML → DOM construction | `core/view/js/import-runtime.js` `buildDom` | Hand-rolled tag-state machine, ~150 LOC |
| Web-compat DOM | `core/view/js/web-compat-*.js` | Document, Element, dom-ops, observers, scheduler, style-decl |
| Bridge to native widgets | `core/view/src/widget_bridge.cpp` | `__domAppend`, `__domInsertBefore`, `__domRemove`, etc. |
| Settle | `parse_claude_html_with_runtime` after bundle eval | `DOMContentLoaded` dispatch + 12 × `pump_message_loop` + `service_frame_callbacks` |
| Walk | `walkDomJson()` | Plain-JSON tree → `json_to_ir_node` |

This review answers: **which patterns from these four projects would close the gap between "we ran the bundle without throwing" and "React 18 actually committed something walkable"?**

The four probes are summarized below (full probe transcripts are referenced inline). Pulp **cannot** vendor any of these projects wholesale — they are all Node-coupled and 10–60× the size of the embedded JS budget. The actionable extractions are patterns, not packages.

---

## Findings

### 1. htmlparser2 — `/Users/danielraffel/Code/htmlparser2`

**License:** MIT (clean to embed pieces).
**Size:** ~2,060 LOC across `Tokenizer.ts` (1,150) + `Parser.ts` (823) + `index.ts` (86). Runtime deps: `entities`, `domhandler`, `domutils`, `domelementtype` (all MIT, all separate packages).
**Architecture:** Push-based callback machine. Tokenizer is a 28-state explicit char-code state machine using `Uint8Array` sequence-cursors for multi-char matching (`</script`, `<![CDATA[`, …). No streams, no Buffers — `write(string)` + `end()` is the entire surface. Parser layer maintains the element stack, void/raw-text rules, foreign-content (SVG/MathML) adjustments.

**Relevance to Pulp's gap:** Pulp's parser is *probably not* the cause of the empty-shell behavior — Claude exports are well-formed HTML and the existing `import-runtime.js` parser handles them. **However** it has two known correctness gaps that could plausibly matter on a real Spectr export:

- `parseAttrs` (`import-runtime.js` L57–67) is regex-based and silently mangles attribute values containing `=`, unbalanced quotes, or stray `<`. Spectr's editor.js encodes inline data in `data-*` attributes — a single misparsed attribute can drop a `data-pulp-role` marker.
- `decodeEntities` (`import-runtime.js` L41–50) handles only seven hardcoded entities. Anything else (`&copy;`, `&#x2014;`, named non-ASCII) becomes a literal ampersand string, which can break React keys derived from text content.

**Concrete extractions:**
- **Attribute state machine** — port `Tokenizer.ts` L609–725 (~9 states, ~60 LOC) verbatim into `import-runtime.js` to replace `parseAttrs`.
- **Raw-text end-tag scanner** — `stateInSpecialTag` (L487–529, ~40 LOC) replaces `findRawTextTagClose` with a case-insensitive sequence-cursor that handles `<</script>` correctly.
- **Entities trie** — design pattern only; we ship the 6–8 actually-needed named entities and a `&#NNN;`/`&#xNNN;` numeric path. Don't ship the 8 KB trie.
- **`openImpliesClose` table** (`Parser.ts` L21–70) — copy/paste-ready if Pulp ever needs to ingest non-Claude HTML.

### 2. jsdom — `/Users/danielraffel/Code/jsdom`

**License:** MIT.
**Size:** ~60k+ LOC, parse5-based HTML, `vm.createContext` execution, WebIDL-generated wrappers, `symbol-tree` for tree links. Hard-coupled to Node's `vm` and `worker_threads` modules.
**Architecture:** Living-DOM modules in `lib/jsdom/living/{nodes,events,mutation-observer,…}` with each WebIDL type split into `*-impl.js` (logic) + generated wrapper (IDL coercion). Scripts run via `vm.runInContext` with `runScripts: "dangerously"` opt-in.

**Relevance to Pulp's gap:** Three patterns that could be the actual cause of the empty shell on real Spectr exports.

**Concrete extractions:**

- **Listener snapshot before dispatch** (`living/events/EventTarget-impl.js` L325):
  ```js
  const handlers = listeners[type].slice();
  // and re-check membership inside the loop:
  if (!listeners[type].includes(listener)) continue;
  ```
  React 18 synthetically adds and removes listeners *during* dispatch (capture cleanup, hydration cleanup). If `web-compat-document.js` iterates the live listener array, listeners can be skipped or double-fired. **This is a strong candidate for the empty-shell bug** — if the React root's commit listener is removed during a synthetic dispatch, the commit never lands.
- **Microtask-coalesced MutationObserver drain** (`living/helpers/mutation-observers.js` L118–135): single `Promise.resolve().then(notify)` guarded by a `mutationObserverMicrotaskQueueFlag`. Pulp's MO is currently a no-op; if the bundle includes a polyfill that depends on MO callbacks (some React 18 transitions / `useSyncExternalStore` polyfills do), they hang. ~15 LOC to fix.
- **`symbol-tree` sibling links** (design only): O(1) `nextSibling`/`previousSibling`. Pulp's `_children.indexOf(node)` is O(n) and is hot during React keyed reconciliation.

**Critical scheduler note:** React 18's scheduler tries `MessageChannel.postMessage` first, falls back to `setTimeout(0)`, falls back to `queueMicrotask`. jsdom *does not* expose `MessageChannel` and React handles its absence. **Verify Pulp's `web-compat-scheduler.js` exposes all three of `setTimeout`, `clearTimeout`, `queueMicrotask` on `globalThis`** — if any are missing, React's scheduler degrades to "synchronous flush only on commit boundary," which combined with our 12-iteration drain may explain non-deterministic empty shells.

### 3. happy-dom — `/Users/danielraffel/Code/happy-dom`

**License:** MIT.
**Size:** ~87k TS LOC but only three runtime deps (`entities`, `whatwg-mimetype`, `ws`). Self-contained — no parse5, no nwsapi, no css-tree, no symbol-tree.
**Architecture:** `Browser → BrowserContext → BrowserPage → BrowserFrame → Window` with a `WindowContextClassExtender` class-factory binding every DOM class to a window at construction time. No globals. `[PropertySymbol.window]` lives on each node.

**Relevance to Pulp's gap:** This is the most directly applicable project of the four — it solves the exact problem Pulp has (run modern frontend code in a non-browser host without dragging the kitchen sink). Three patterns are immediately useful, all of them small and portable.

**Concrete extractions:**

- **`AsyncTaskManager` quiescence sentinel** (`src/async-task-manager/AsyncTaskManager.ts`, ~340 LOC, but the *idea* is ~30 LOC):
  - Every async task (timer, microtask, fetch, MO callback, rAF) bumps a counter on start, decrements on end.
  - `whenComplete()` schedules a `setTimeout(1)` sentinel after the counter hits zero — if the counter is still zero when the sentinel fires, all work has drained.
  - **Replaces Pulp's hard-coded "12 iterations" drain with a deterministic signal.** This alone is likely the highest-leverage change for the Spectr bug: the 12-iteration loop is a guess, and if React 18's commit chain queues work in a 13th turn (likely on a real-sized bundle) we walk the DOM mid-commit.
- **`MutationObserverListener` debounce** (`src/mutation-observer/MutationObserverListener.ts`, 112 LOC):
  - `WeakRef` to callback so observers can be GC'd while still attached.
  - `#microtaskQueued` flag prevents duplicate microtask scheduling per listener per turn.
  - Direct `node[PropertySymbol.observeMutations](listener)` — listeners live on the target, no central registry. ~110 LOC, drops in cleanly next to `web-compat-observers.js`.
- **Per-node WeakRef selector cache** (`src/nodes/node/Node.ts` L80–104, `src/query-selector/SelectorParser.ts` L1–60):
  - Each Node carries a `[PropertySymbol.cache]` Map keyed by selector string → `WeakRef<NodeList>`.
  - Cache invalidation propagates up the tree on any mutation via a `[PropertySymbol.affectsCache]` walk.
  - **React 18 calls `querySelector` heavily during hydration / portal mounting.** If Pulp's selector engine is O(tree-size) per call, large React commits are quadratic and likely hit a native-bridge throughput limit before React finishes. Even a tag-name cache would help.
- **HTML parser** (`src/html-parser/HTMLParser.ts`, ~919 LOC) — *not* extractable as-is (coupled to NodeFactory), but the two-regex tokenizer pattern (one `MARKUP_REGEXP`, one `ATTRIBUTE_REGEXP`, a 5-state document-structure enum) is ~200 LOC of pure logic and is significantly more correct than Pulp's current parser for messy markup. Lower priority than the three above.

### 4. react-testing-library — `/Users/danielraffel/Code/react-testing-library`

**License:** MIT.
**Size:** Tiny — 598 LOC across 5 source files: `pure.js` (363), `act-compat.js` (91), `fire-event.js` (69), `index.js` (41), `config.js` (34).
**Architecture:** Thin wrapper over `@testing-library/dom` plus React's built-in `act()` / `React.act()` (with a fallback to `react-dom/test-utils.act`). No DOM logic of its own.

**Relevance to Pulp's gap:** RTL doesn't carry portable utilities Pulp can lift — its "drain pending React work" logic *is* `React.act()`, which lives inside React itself. **But it tells us something important about the bug:**

- `act-compat.js` L28–34 sets `globalThis.IS_REACT_ACT_ENVIRONMENT = true` before rendering. React 18's concurrent mode uses this flag to switch from "schedule commit on a future MessageChannel turn" to "flush commit synchronously inside the act callback." **If Pulp set `globalThis.IS_REACT_ACT_ENVIRONMENT = true` before evaluating Babel-transformed app code that calls `ReactDOM.createRoot().render(...)`, React commits synchronously and the 12-iteration drain becomes redundant.** This is the single most actionable insight from RTL.
- `pure.js` L37–53 (`asyncWrapper`): the way RTL drains microtasks is to `await new Promise(resolve => setTimeout(resolve, 0))`. That's the same pattern as happy-dom's `AsyncTaskManager` sentinel — a `setTimeout(0)` after promise-then is the lowest-priority signal that "everything else has run." Pulp's harness should drain through that pattern, not by counting iterations.
- `pure.js` L264 wraps every render in `act(() => { root.render(...) })`. The takeaway: **the eval order Pulp uses today** (load library payloads → load Babel → transform inline scripts → eval them → dispatch DOMContentLoaded → drain) **probably needs to flip — set `IS_REACT_ACT_ENVIRONMENT` first, then eval app code, *then* drain.**

No code to lift — but a behavior change Pulp can land in `parse_claude_html_with_runtime` in ~10 lines.

---

## Recommendations

### Immediate (this week, in `parse_claude_html_with_runtime` + `import-runtime.js`)

These are small, low-risk, and target the most likely root causes of the empty-shell bug.

1. **Set `IS_REACT_ACT_ENVIRONMENT` before evaluating bundle payloads.** Add a `globalThis.IS_REACT_ACT_ENVIRONMENT = true` eval right after `buildDom`, before the JS payload loop in `design_import.cpp` ~L809. React 18 commits become synchronous; the empty-shell case becomes deterministic.
   *Source:* RTL `act-compat.js` L28–34.
2. **Audit listener dispatch in `web-compat-document.js` for snapshot-before-iterate.** If the dispatch loop iterates `_listeners[type]` directly, copy `.slice()` it and re-check `includes()` per iteration. ~5 LOC change.
   *Source:* jsdom `EventTarget-impl.js` L325.
3. **Verify scheduler globals.** `setTimeout`, `clearTimeout`, `queueMicrotask` must all be live on `globalThis` before `Babel.transform` runs. Add a startup assertion in `WidgetBridge::register_api()` and a corresponding test.
   *Source:* jsdom + happy-dom both rely on these; React degrades silently when missing.
4. **Replace `parseAttrs` regex with the htmlparser2 attribute state machine.** ~60 LOC, fixes silent attribute drops in Spectr-style markup.
   *Source:* htmlparser2 `Tokenizer.ts` L609–725.
5. **Add three failing tests up front** (tests-ship-with-fixes — see CLAUDE.md):
   - Spectr fixture renders ≥ 100 nodes through the runtime walker (current floor is 30; 100 is a reasonable React-app post-commit floor).
   - A fixture that adds + removes a listener inside a dispatched handler — assert no skipped/double dispatch.
   - A fixture with `data-foo='a=b'` and `&#x2014;` text — assert attributes and text round-trip.

### Medium-term (next 2–4 weeks)

6. **Replace the 12-iteration drain with an `AsyncTaskManager`-style quiescence sentinel.** Track in-flight timers, microtasks, and rAF callbacks in `web-compat-scheduler.js`; expose `__pulpImportRuntime__.whenIdle()` that resolves when the counter is zero across two consecutive `setTimeout(1)` ticks. Replace the `for (i = 0; i < 12; ++i)` loop in `design_import.cpp` ~L897 with a single `await __pulpImportRuntime__.whenIdle()` (driven from C++ via `engine.evaluate` + `pump_message_loop` until the sentinel resolves).
   *Source:* happy-dom `AsyncTaskManager.ts`, `DocumentReadyStateManager.ts`.
7. **Ship a real `MutationObserver`** behind an opt-in flag. `MutationObserverListener` pattern from happy-dom (~110 LOC). Some React 18 / `useSyncExternalStore` polyfills require MO callbacks; today's no-op silently breaks them.
   *Source:* happy-dom `MutationObserverListener.ts`.
8. **Per-node selector cache with mutation-driven invalidation.** Even a coarse implementation (cache by tag-name and class only, invalidate on subtree mutation) would prevent O(N²) hydration on large bundles.
   *Source:* happy-dom `Node.ts` L80–104.

### Not worth adopting

- **Vendoring jsdom or happy-dom as a whole.** Both are Node-coupled and far over Pulp's embedded JS budget. Use them as reference reads only.
- **htmlparser2 as a package.** Its tokenizer is excellent but Pulp's input surface is too narrow to justify the dependency; port the two specific pieces above.
- **RTL itself.** No portable utilities; its value is the single `IS_REACT_ACT_ENVIRONMENT` insight, which is a one-liner in C++.
- **WebIDL wrapper/impl split (jsdom).** Adds ~40% complexity for type coercion Pulp doesn't need.
- **Shadow DOM, custom elements, vm.createContext, XHR/fetch/WebSocket, Range/Selection, computed style cascade.** Out of scope for the import-materialization path.

### Licensing summary

| Project | License | Verdict |
|---|---|---|
| htmlparser2 | MIT | Safe to lift specific files / patterns. Attribution to NOTICE.md if any code is copied. |
| jsdom | MIT | Same. Same caveat. |
| happy-dom | MIT | Same. Same caveat. |
| react-testing-library | MIT | No code lifted — patterns only — no NOTICE entry needed. |

All four are MIT, compatible with Pulp's MIT release.

---

## Suggested next steps for the import runtime

1. Land Recommendations 1–5 in a single PR on a `feature/claude-import-act-env` branch. Tests-ship-with-fixes.
2. Capture a deterministic Spectr export under `test/fixtures/imports/claude/spectr-real-export/` with the failing fixture committed alongside Recommendation 5's tests so CI tracks regressions.
3. After 1–5 prove out, open a `feature/claude-import-quiescence` branch for Recommendations 6–7 and benchmark the drain — replacing the 12-iteration loop should be measured, not assumed.
4. Add a `docs/guides/import-runtime.md` page describing the React-act env contract, the scheduler global requirements, and the quiescence sentinel — this contract is currently implicit in C++ comments and is at risk of drifting between the JS and C++ sides.

---

## Files to read line-for-line (priority order)

| Project | File | LOC | Why |
|---|---|---|---|
| RTL | `src/act-compat.js` | 91 | The `IS_REACT_ACT_ENVIRONMENT` contract |
| jsdom | `lib/jsdom/living/events/EventTarget-impl.js` | ~440 | Listener snapshot pattern (L325) |
| happy-dom | `src/async-task-manager/AsyncTaskManager.ts` | 340 | Quiescence sentinel |
| happy-dom | `src/mutation-observer/MutationObserverListener.ts` | 112 | MO debounce pattern |
| htmlparser2 | `src/Tokenizer.ts` L609–725 | ~120 | Attribute state machine |
| happy-dom | `src/nodes/node/Node.ts` L80–104 | ~25 | Per-node WeakRef selector cache |
| jsdom | `lib/jsdom/living/helpers/mutation-observers.js` L118–135 | ~20 | Microtask-coalesced MO drain |
| htmlparser2 | `src/Tokenizer.ts` L487–529 | ~40 | Raw-text end-tag scanner |

Total: ~1,200 LOC of focused reading covers every actionable recommendation in this report.

# Design Debugging

Use `pulp design-debug` to exercise the design-tool chat pipeline non-interactively
and publish reproducible artifacts for review.

This tool is useful when you want to:
- compare prompts side by side
- replay a saved model response without calling AI again
- keep provider/model/reasoning-effort metadata with the screenshots
- inspect which tokens, dimensions, and widget looks changed
- upload artifact bundles to GitHub or share them remotely

## What It Produces

Each run writes an artifact bundle under `planning/screenshots/design-debug/` by default:

- `*-prompt.txt`
- `*-response.txt`
- `*-before.png`
- `*-after.png`
- `*-diff.png`
- `*-report.json`
- `*-target-before.png` / `*-target-after.png` / `*-target-diff.png` for targeted widget runs
- `latest-report.json` with the newest full report
- `latest-run.json` with a compact summary of the newest run
- `runs.jsonl` with one JSON summary per run for remote progress tracking

The JSON report includes:
- provider, model, reasoning effort
- requested capture backend and actual render backend
- whether widget SkSL was rendered in the artifact images
- target widget id and bounds
- target-region crop bounds and ROI diff stats for targeted runs (`target_diff_pixels`, `target_diff_pct`)
- the exact AI command used
- screenshot diff stats
- `debug_state` from the real design tool script:
  - request text
  - changed colors
  - changed dimensions
  - widget look ids
  - summary / error state

## Examples

Target one widget:

```bash
pulp design-debug \
  --prompt "make the gain knob look like macOS 7" \
  --target k1
```

Run a Codex/OpenAI model with explicit reasoning effort:

```bash
pulp design-debug \
  --prompt "make the gain knob look like a precision analyzer control with restrained chrome" \
  --target k1 \
  --provider codex \
  --model gpt-5.4 \
  --reasoning-effort low
```

Restyle the whole preview with Claude:

```bash
pulp design-debug \
  --prompt "design a cyberpunk interface for a modern synth plugin" \
  --target all \
  --provider claude \
  --model claude-sonnet-4-6
```

Replay a saved response:

```bash
pulp design-debug \
  --prompt "warm analog EQ" \
  --target all \
  --response-file saved-response.json
```

## Provider and Model Metadata

The interactive `pulp design` app and the headless `pulp design-debug` harness
share the same prompt builder and AI command-template expansion.

Today the design tool exposes:
- provider: `Claude` or `Codex`
- provider-specific model selector
- reasoning-effort selector for Codex/OpenAI models

That metadata is preserved in the debug report so you can compare:
- Claude vs Codex
- `gpt-5.4` vs another Codex/OpenAI model
- `low` vs `xhigh` reasoning effort

For targeted widget runs, the harness also emits target-only crops and ROI stats
so you can answer the important question quickly:

- did the selected widget materially change?
- was the change localized to the intended control?
- which prompt/model combination produced the strongest targeted delta?

The report records `target_region_source` so you can tell whether the crop came
from the requested widget bounds or a changed-pixels fallback.

## Capture Backends

The debug tool supports two render paths:

- `--capture-backend skia` (default)
- `--capture-backend coregraphics`

The default Skia path makes the harness much more useful for widget restyling,
because widget SkSL actually renders in the screenshots. The report makes this explicit:

```json
{
  "render_backend": "skia-headless",
  "requested_capture_backend": "skia",
  "sksl_gpu_supported": false,
  "widget_sksl_render_supported": true
}
```

The CoreGraphics path is still available, but it does not faithfully render
custom widget SkSL and is mainly useful as a baseline/comparison path.

## Current Limitation

Even with Skia capture, the harness still renders offscreen rather than through
the final live GPU presentation path used by the interactive app.

This means the harness is very good for:
- validating prompt construction
- validating provider/model/reasoning metadata
- validating JSON parsing and apply flow
- validating widget SkSL shape/material changes in before/after screenshots
- comparing token/dimension/widget-look diffs
- publishing deterministic before/after/report bundles

It is still not the final source of truth for live GPU presentation parity.
For final shader fidelity, judge the result in the interactive GPU-backed
`pulp design` app.

## Testing

Relevant local checks:

```bash
node --check examples/design-tool/design-tool.js
ctest --test-dir build -R "Design tool:|WidgetBridge"
```

Those tests cover the prompt builder, provider/model selector behavior, debug-state
capture, and the widget-restyling bridge APIs. They do not replace visual QA in the
interactive GPU path for final SkSL polish.

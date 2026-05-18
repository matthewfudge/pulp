# Format validator baseline fixtures

Companion-track U-3 (planning/2026-05-17-refactor-roadmap-final.md).

This directory holds the committed golden output of `auval` (AU),
`pluginval` (VST3), and `clap-validator` (CLAP) running against a
representative Pulp plugin. The
[`format-baseline-diff`](../../.github/workflows/format-baseline-diff.yml)
CI gate diffs the current capture against these fixtures whenever a PR
touches `core/format/` or `core/host/plugin_slot_*`.

The goal: catch the class of format-adapter bug that compiles clean but
breaks DAW compatibility (silent regressions in parameter automation,
state round-trip, MIDI routing, bus arrangement negotiation, etc.).

## When to re-capture

After any intentional change to validator-visible behavior:

```bash
tools/scripts/format_baseline_capture.sh --build --plugin PulpDrums
```

This regenerates the files in this directory. Commit them in the same
PR that introduces the behavior change.

## When NOT to re-capture

If the diff is **not** intentional, don't update the baseline — fix the
underlying regression instead. The CI gate output tells you the
specific diff lines; trace them back to the format-adapter or
plugin-slot change in the same PR.

## Validator availability

| Validator | Required for | How to install |
|---|---|---|
| `auval` | AU lane | ships with macOS — no install |
| `pluginval` | VST3 lane | `brew install --cask pluginval` or build from source |
| `clap-validator` | CLAP lane | `cargo install clap-validator` |

The capture script runs whichever validators are available and skips the
rest. The CI workflow installs all three in the self-hosted macOS runner.

## Representative plugin

The baseline uses **PulpEffect** as the representative plugin. Rationale:

- Ships in all three formats: `FORMATS VST3 AU CLAP` per
  `examples/pulp-effect/CMakeLists.txt`. The CLAP-only plugins
  (PulpDrums, PulpSynth) would force `auval` and `pluginval` to
  skip — those lanes would never produce a baseline.
- Covers parameter automation, bus arrangement, and stateful behavior
  — the surfaces most likely to regress silently in format adapters.

`pulp_add_plugin()` emits one CMake target per format
(`PulpEffect_AU`, `PulpEffect_VST3`, `PulpEffect_CLAP`); the
workflow builds all three.

Plugin bundles are emitted under top-level format dirs per
`tools/cmake/PulpUtils.cmake` (`LIBRARY_OUTPUT_DIRECTORY` =
`${CMAKE_BINARY_DIR}/AU`, `/VST3`, `/CLAP`).

If a future change requires a different representative plugin, update
both `format_baseline_capture.sh` and `format_baseline_diff.py`
defaults plus the build/install steps in
`.github/workflows/format-baseline-diff.yml`.

## Normalization

`format_baseline_capture.sh` runs validator output through a `sed`
pipeline that strips inherently non-deterministic content (timestamps,
absolute paths, dylib load addresses, elapsed times, etc.). The
committed baseline is the normalized output — so a diff means
something genuinely structural changed, not just a different
wall-clock time.

If a validator adds a new ephemeral pattern in its output, extend the
`normalize()` function in the capture script. Don't add a one-off
"ignore-this-line" exception.

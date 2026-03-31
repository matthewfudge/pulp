# v3 Phase Status Tracker

**Last updated:** 2026-03-31
**Tracking:** DSL / Agentic Validation / WebGPU / UI Framework v3

---

## Phase Status

| Phase | Name | Branch | Status | PR | Verified |
|-------|------|--------|--------|-----|----------|
| 1 | Contract/truth pass | phase/dsl-agentic-webgpu-v3-phase1 | **complete** | — | — |
| 2 | Validation harness + RTSan/vstvalidator | phase/dsl-agentic-webgpu-v3-phase2 | not started | | |
| 3 | FAUST offline codegen | phase/dsl-agentic-webgpu-v3-phase3 | not started | | |
| 4 | Cmajor adapter | phase/dsl-agentic-webgpu-v3-phase4 | blocked (needs 3) | | |
| 5 | JSFX adapter | phase/dsl-agentic-webgpu-v3-phase5 | blocked (needs 4) | | |
| 6 | Multi-window framework | phase/dsl-agentic-webgpu-v3-phase6 | not started | | |
| 7 | WebView integration (iPlug2-style) | phase/dsl-agentic-webgpu-v3-phase7 | blocked (needs 6) | | |
| 8 | Audio visualization (STFT/spectrogram) | phase/dsl-agentic-webgpu-v3-phase8 | not started | | |
| 9 | Widgets + assets + themes | phase/dsl-agentic-webgpu-v3-phase9 | not started | | |
| 10 | JS engine abstraction (V8/JSC) | phase/dsl-agentic-webgpu-v3-phase10 | not started | | |
| 11 | WebGPU compute exploration | phase/dsl-agentic-webgpu-v3-phase11 | not started | | |
| 12 | Offline video exploration | phase/dsl-agentic-webgpu-v3-phase12 | blocked (needs 2, 11) | | |
| 13 | Three.js / WebGPU JS bridge | phase/dsl-agentic-webgpu-v3-phase13 | blocked (needs 10, 11) | | |
| 14 | Verification pass | — | blocked (needs all) | | |

**Status values:** not started, in progress, complete, blocked (needs X), failed, parked

**Verified column:** filled during Phase 14 verification pass (pass/fail + date)

---

## Parallel Batch 1 (after Phase 1 merges to main)

Can run simultaneously in separate worktrees:
- [ ] Phase 2 — Validation harness
- [ ] Phase 3 — FAUST
- [ ] Phase 6 — Multi-window
- [ ] Phase 8 — Audio visualization
- [ ] Phase 9 — Widgets + assets + themes
- [ ] Phase 10 — JS engine abstraction
- [ ] Phase 11 — WebGPU compute

## Sequential Chain: DSLs

- [ ] Phase 3 (FAUST) → merge to main
- [ ] Phase 4 (Cmajor) → merge to main
- [ ] Phase 5 (JSFX) → merge to main

## Sequential Chain: Multi-window → WebView

- [ ] Phase 6 (multi-window) → merge to main
- [ ] Phase 7 (WebView) → merge to main

## Sequential Chain: Late phases

- [ ] Phase 10 + 11 both merged → Phase 13 (Three.js bridge)
- [ ] Phase 2 + 11 both merged → Phase 12 (offline video)
- [ ] All phases merged → Phase 14 (verification pass)

---

## Per-Phase Completion Checklist

When marking a phase **complete**, confirm ALL of the following:

- [ ] All deliverables listed in the phase prompt are implemented
- [ ] All tests pass (ctest --test-dir build --output-on-failure)
- [ ] Known test exclusions documented (AudioWorkgroup, GpuSurface, etc.)
- [ ] CLI commands added/updated (tools/cli/pulp_cli.cpp)
- [ ] Claude Code skills/commands updated (claude/)
- [ ] Docs updated (docs/reference/cli.md, capabilities.md, status manifests)
- [ ] No overclaims in docs — only claims what code backs
- [ ] PR created, local CI passed (python3 tools/local-ci/local_ci.py ship)
- [ ] Merged to main
- [ ] This status doc updated

---

## Phase Completion Log

Record when each phase lands with notes on what was delivered vs what was deferred.

### Phase 1 — Contract/truth pass
- **Merged:** 2026-03-31
- **Delivered:** machine-readable validation contract (docs/contracts/phase1-reality-snapshot.yaml), truth pass on docs/planning, status labels, dependency map
- **Deferred:** nothing
- **Notes:** all subsequent phases reference this as the authoritative baseline

### Phase 2 — Validation harness + RTSan/vstvalidator
- **Merged:** —
- **Delivered:** —
- **Deferred:** —
- **Notes:** —

### Phase 3 — FAUST offline codegen
- **Merged:** —
- **Delivered:** —
- **Deferred:** —
- **Notes:** —

### Phase 4 — Cmajor adapter
- **Merged:** —
- **Delivered:** —
- **Deferred:** —
- **Notes:** —

### Phase 5 — JSFX adapter
- **Merged:** —
- **Delivered:** —
- **Deferred:** —
- **Notes:** —

### Phase 6 — Multi-window framework
- **Merged:** —
- **Delivered:** —
- **Deferred:** —
- **Notes:** —

### Phase 7 — WebView integration
- **Merged:** —
- **Delivered:** —
- **Deferred:** —
- **Notes:** —

### Phase 8 — Audio visualization
- **Merged:** —
- **Delivered:** —
- **Deferred:** —
- **Notes:** —

### Phase 9 — Widgets + assets + themes
- **Merged:** —
- **Delivered:** —
- **Deferred:** —
- **Notes:** —

### Phase 10 — JS engine abstraction
- **Merged:** —
- **Delivered:** —
- **Deferred:** —
- **Notes:** —

### Phase 11 — WebGPU compute exploration
- **Merged:** —
- **Delivered:** —
- **Deferred:** —
- **Notes:** —

### Phase 12 — Offline video exploration
- **Merged:** —
- **Delivered:** —
- **Deferred:** —
- **Notes:** —

### Phase 13 — Three.js / WebGPU JS bridge
- **Merged:** —
- **Delivered:** —
- **Deferred:** —
- **Notes:** —

### Phase 14 — Verification pass
- **Merged:** —
- **Delivered:** —
- **Deferred:** —
- **Notes:** —

# Coverage Compliance Status

Last reviewed: 2026-04-24

This is the durable tracker for the repo-wide coverage compliance
program under `#641`. The Phase 1 representation stack has now landed
on `main`; this document records the corrected Codecov baseline that
Phase 2 planning should use.

## Goal

Get Codecov as close as practical to the intended first-party local
source surface, then use that corrected baseline to plan and close the
real measured test gaps.

Target tiers remain unchanged in `ci/coverage-targets.yaml`:

- `80%`: `audio`, `format`, `host`, `midi`, `signal`, `platform`
- `70%`: `render`, `view`, `cli`
- `50%`: `events`, `runtime`, `state`, `canvas`, `osc`, `ship`, `tools`

## Three-phase program

### Phase 1 - Representation / Codecov truth

Status: code and workflow slices are merged; this rebaseline is the
closeout artifact.

Finish line state:

- `windows` is non-null on `main` (`0.0%` at the current baseline).
- `dsl` is a real component on `main` (`63.38%`).
- `apple/Sources/**` materializes on Codecov for the macOS Swift package
  lane and the native `PulpBridge.cpp` lane.
- `android/app/src/main/kotlin/**` materializes on Codecov.
- `bindings/python/bindings.cpp` materializes on Codecov.
- the clearly known remaining "still not on Codecov" buckets are listed
  below as explicit follow-up or out-of-scope surfaces.
- this status document is rebaselined from the corrected `main` surface.

### Phase 2 - Gap planning

Status: next.

Finish line:

- counted components are ranked from the corrected `main` baseline
- major low-coverage files and components are grouped into tranche issues
- remaining out-of-scope surfaces are either accepted for now or have
  explicit expansion issues
- the repo has an actionable issue / PR roadmap for closing measured
  gaps

### Phase 3 - Gap closure

Status: parked until Phase 2 produces the tranche plan.

Finish line:

- planned coverage tranches are implemented and merged
- target components move materially toward or to their configured
  thresholds
- deferred or exceptional surfaces stay documented explicitly instead of
  silently omitted

## Corrected main baseline

Source:

- Codecov public totals and components API for `main` commit
  `abe2b07a820d9e705864a3ecd3f0350772f694d1`
- GitHub Actions Coverage run `24886403280`, completed successfully
  across Android/Kotlin, Linux/Clang, Windows/Clang, and macOS/Clang

Current observed `main` snapshot:

- overall tracked coverage: `39.28%` over `68,001` lines in `551` files
- `audio`: `20.95%`
- `canvas`: `64.06%`
- `dsl`: `63.38%`
- `events`: `24.14%`
- `format`: `43.86%`
- `host`: `33.38%`
- `midi`: `45.2%`
- `osc`: `60.25%`
- `platform`: `35.41%`
- `render`: `55.87%`
- `runtime`: `46.89%`
- `signal`: `61.85%`
- `state`: `50.94%`
- `view`: `42.41%`
- `android`: `13.83%`
- `apple`: `29.93%`
- `linux`: `0.0%`
- `windows`: `0.0%`
- `cli`: `21.38%`
- `ship`: `31.45%`
- `tools`: `34.07%`

Representation proof points from the same Codecov snapshot:

- Apple files now visible:
  - `apple/Sources/PulpSwift/PulpBridge.cpp`: `70.78%`
  - `apple/Sources/PulpSwift/PulpBridge.swift`: `98.7%`
  - `apple/Sources/PulpSwift/PulpParameter.swift`: `100.0%`
  - `apple/Sources/PulpSwift/PulpViews.swift`: `79.79%`
- Python perimeter files now visible, including top-level `tools/*.py`,
  `tools/packages/**`, `tools/deps/**`, `tools/local-ci/**`,
  `tools/scripts/**`, and `core/view/js/embed_js.py`.
- `bindings/python/bindings.cpp` is visible as a counted file
  (`0.0%`, `0/121` lines). It is represented but still a test gap.
- `tools/packages/freshness_check.py` and
  `tools/packages/validate_registry.py` are visible as counted files.
- Python test modules remain intentionally omitted from the reported
  source set.

## Clearly known still not on Codecov

These are explicit perimeter decisions after the corrected baseline.
They should not be treated as silent omissions while Phase 2 ranks the
represented surface.

- Authored JavaScript source roots remain out of the represented surface
  until a dedicated JS lane is added - tracked by `#659`.
- `bindings/nodejs/bindings.cpp` remains explicitly out of scope for now
  because the repo does not have a supported Node binding CI/test lane -
  tracked by `#657`.
- Shell and PowerShell scripts remain explicitly out of scope for now.
  They are tested indirectly where practical, but do not surface as
  first-class Codecov lines - tracked by `#657`.
- iOS-only Swift and standalone Swift outside the macOS SwiftPM lane stay
  outside the represented surface unless a later simulator/runtime lane
  pulls them in. Known examples:
  - `apple/Sources/PulpSwift/PulpAudioSession.swift`
  - `templates/ios-auv3/HostApp/ContentView.swift`
  - `templates/ios-auv3/HostApp/PulpHostApp.swift`
  - `tools/local-ci/macos_window_probe.swift`
  tracked by `#77`; the Apple perimeter classification work in `#656`
  is closed.
- `apple/Sources/PulpSwift/PulpBridge.h` is a declaration-only C header
  and is not materialized as an executable-line Codecov row. The
  implementation in `PulpBridge.cpp` is now represented and tested.
- Optional native surfaces that are not compiled by the current coverage
  configure, such as AAX/ARA runtime sources, Android native device
  shims, and Web/WASM-specific sources, remain outside the measured
  graph unless Phase 2 decides to add focused follow-up issues.

## Phase 1 issue map

### Control plane and verification

- `#641` authoritative umbrella and phase tracker
- `#639` Codecov control plane and dashboard truth follow-up
- `#647` merged representation tranche for `dsl`, `windows`, and the
  durable status report
- `#715` merged Python coverage normalization / zero-file visibility fix

### Completed perimeter-expansion work

- `#656` Swift / Apple perimeter classification, with
  `PulpBridge.cpp` represented through `#678`
- `#658` Python perimeter expansion, implemented through `#677` and
  normalized by `#715`
- `#679` / `#680` `bindings/python/bindings.cpp` representation
- `#633` Android/Kotlin JaCoCo lane
- `#615` Apple Swift package lane

### Remaining explicit perimeter follow-ups

- `#659` add a JavaScript source lane for authored repo assets
- `#657` classify optional bindings and shell / PowerShell surfaces in
  docs; after this rebaseline lands, the remaining implementation
  decision is whether to keep Node and scripts out of scope or file
  dedicated future lanes
- `#77` decide and, if in scope, add mobile runtime coverage from
  Android instrumentation and iOS simulator / runtime app paths

### Supporting infrastructure

- `#671` Windows Shipyard bundle-lock infra failure: closed
- `#692` Windows Namespace capacity fallback for PR lanes: closed
- `#655` Android SDK / NDK provisioning on SSH validation hosts remains
  useful supporting infra but is not a blocker for hosted Codecov truth
  on `main`
- `#568` historical multi-language expansion umbrella. Active execution
  now rolls up through `#641`.

## Phase 2 starting point

Phase 2 should start from the component snapshot above, not from older
pre-`#715` numbers. The first planning pass should:

1. Rank below-target counted components against `ci/coverage-targets.yaml`.
2. Separate "represented but zero/low coverage" files from intentionally
   unrepresented surfaces.
3. Refresh or close the parked tranche issues based on the corrected
   numbers.
4. Post the final Phase 1 closeout and Phase 2 entry note on `#641`.

### Phase 2 / Phase 3 issues to re-evaluate

- `#640` audio / platform tranche
- `#642` events tranche
- `#643` CLI / tools tranche
- `#644` ship tranche
- `#645` midi / signal tranche
- `#646` render tranche
- `#493` host / format / view and related hardening gaps

### Parked draft PRs to re-evaluate

- `#648` draft events tranche
- `#649` draft CLI / tools tranche
- `#666` draft state / ship hardening split from `#647`

## Control-plane invariants

- Every top-level `core/*` directory must have matching subsystem
  entries in `codecov.yml` `flags:` and `component_management:`.
- Platform axes must match the live repo path conventions, including
  `core/**/win/**` for Windows.
- Swift upload artifacts must be repo-relative LCOV with `SF:` paths
  rooted under `apple/Sources/**`.
- Android Kotlin coverage must run on the always-on Coverage workflow,
  not only on path-gated Android workflow commits.
- Upload-only flags stay `os-linux`, `os-macos`, and `os-windows`.
- `codecov.yml` `ignore:` must stay aligned with
  `scripts/run_coverage.sh` `COVERAGE_IGNORE_REGEX`.
- The authoritative program tracker is `#641`, not the older language
  umbrella `#568`.

## Validation commands for this slice

- `python3 tools/scripts/test_codecov_config.py`
- `python3 tools/scripts/test_run_swift_coverage.py`
- `python3 tools/scripts/test_coverage_diff_comment.py`
- `tools/check-docs.sh`

## Follow-ups not solved by this doc

- If another top-level `core/*` subsystem is added, update `codecov.yml`
  in the same change; the structural test should fail immediately if the
  mapping is missed.
- `docs/reference/modules.md` still lacks a `#dsl` section even though
  `docs/status/modules.yaml` points at one. That is real docs drift, but
  it is outside this coverage-compliance slice.

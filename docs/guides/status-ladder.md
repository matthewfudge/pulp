# Status Ladder

Pulp uses a small, controlled vocabulary for capability maturity. This page
documents what each label means, the rule that promotes a capability up the
ladder, and where the labels live.

The ladder exists to stop "it compiles" being mistaken for "it ships." Two
honest labels (`stable`, `usable`) are always better than one ambiguous one.

## Source of truth

`docs/status/support-matrix.yaml` is the **single source of truth** for every
capability maturity label. Prose docs (`docs/reference/capabilities.md`,
guides, README) cite or generate from it; they never contradict it.

The drift checker in `tools/check-docs-consistency.py` enforces this for the
sections it knows about. The vocabulary checker (this page's tool) enforces
that the labels themselves are well-formed.

## Vocabulary

These are the only allowed values for a `status:` field anywhere in the
matrix:

| Label | Meaning | Suitable for production? |
|---|---|---|
| `stable` | Cross-platform validated, format validators green where applicable, CI test linked, no known critical limitations. Backwards-compatible API. | **Yes.** |
| `usable` | Production-quality on at least one platform but not yet validated everywhere `stable` would require. CI test linked. May have documented limitations. | Yes for declared scope; consult `limitations:`. |
| `partial` | Implemented in part. Some features work; others are stubs or known to fail under specific conditions. CI test optional. | No without reading the entry. |
| `experimental` | API may change; not recommended for end-user shipped products. | No. |
| `planned` | Spec exists, no code. | No. |
| `unsupported` | Not on the roadmap. Documented for negative-space clarity. | No. |

There are no other values. PRs that introduce one are rejected by
`tools/check-docs-consistency.py` and by `tools/check_status_ladder.py`.

## Promotion rule (the ladder)

A capability moves up the ladder by **proof**, not declaration. The bar:

| To promote *from* → *to* | Proof required |
|---|---|
| `planned` → `experimental` | Code exists. Builds in at least one platform. |
| `experimental` → `partial` | Behaviour documented; partial test coverage; known limitations enumerated in the matrix entry's `limitations:` field. |
| `partial` → `usable` | (1) Cross-platform validation **or** explicit `platform:` scope on the entry. (2) A CI test referenced via the `ci_test:` field (see below). (3) Format validators (auval / clap-validator / pluginval) green for any format adapter capability. |
| `usable` → `stable` | (1) All `usable` requirements met on every supported platform. (2) Real-host validation: at least two third-party hosts loaded the capability without crash or warning. (3) No critical entries in `limitations:`. |

The `tools/check_status_ladder.py` script enforces the `partial → usable`
gate today: any entry labeled `usable` without a `ci_test:` field (or an
explicit `platform:` scope plus `ci_test:`) is reported.

The `usable → stable` gate is currently policy-only. Promotion is a human
decision pending a host-coverage matrix that the production-readiness work
will deliver.

## Schema fields that participate

Add these to a matrix entry to satisfy the ladder:

```yaml
some_capability:
  status: usable
  platform: macos                 # required when capability is platform-scoped
  ci_test: pulp-test-some-cap     # required to be usable+ — points at a CI target
  limitations:                    # optional list; rendered as "Known limitations"
    - "Sample rate fixed at 48 kHz on Windows."
  notes: Free-form prose for readers.
```

`ci_test:` may also be a list when more than one target covers the
capability:

```yaml
ci_test:
  - pulp-test-foo
  - pulp-test-foo-roundtrip
```

The checker matches `ci_test:` values against the set of `add_executable`
targets in `test/CMakeLists.txt`. A typo or a removed test triggers a
report.

## Decision examples

- A new format adapter compiles on macOS only. Status: `experimental`.
- That adapter passes `clap-validator` and has `pulp-test-clap` linked. Status:
  `partial` (no Windows / Linux validation yet).
- The same adapter now passes `clap-validator` on macOS, Linux, and Windows
  with CI coverage on all three. Status: `usable`.
- The same adapter has been confirmed to load and process audio in Reaper,
  Bitwig, and FL Studio without warnings. Status: `stable`.

## How drift is caught

| Surface | Tool | Trigger |
|---|---|---|
| Status vocabulary | `tools/check-docs-consistency.py` | Any `status:` value outside the table above. |
| Section-level drift between matrix and `capabilities.md` | `tools/check-docs-consistency.py` | Mismatch in the accessibility section (more sections coming). |
| Ladder rule violation | `tools/check_status_ladder.py` | A `usable` entry without `ci_test:` or with a `ci_test:` value that no test target matches. |

All three are wired into `tools/check-docs.sh`, run by `pulp docs check`,
and gated in CI by `.github/workflows/version-skill-check.yml`.

## Phase-in policy

`check_status_ladder.py` ships in **report mode**: it prints the violations
but exits 0. After one release of cleanup, it will be promoted to **block
mode** (exit 1 on violations). Tracked under production-readiness workstream
08, sub-deliverable 8.4.

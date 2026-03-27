# GitHub Issues vs Linear For Pulp Orchestration

Date: 2026-03-27

Related:

- `planning/symphony-idea-pipeline-proposal-2026-03-27.md`

## Decision Summary

If Pulp wants to experiment with Symphony-style orchestration soon, the practical choice is:

- use GitHub Issues as the shared backlog and collaboration surface
- keep a local filesystem-based intake/refinement layer in `planning/`
- do not force Linear across the whole project up front
- if official Symphony is the chosen orchestrator, either:
  - pilot a small Linear execution board just for agent-ready work, or
  - defer official Symphony and evaluate a GitHub-native alternative first

If Pulp wants the cleanest near-term path to:

- GitHub Issues
- local board files
- Codex and/or Claude compatibility

then the stronger near-term candidate is not official Symphony itself, but a Symphony-style orchestrator that already supports GitHub Issues and non-Codex runners.

## Why This Decision Exists

There are really three separate choices hiding inside one question:

1. What is the work-tracking system?
2. What is the execution orchestrator?
3. What is the coding-agent runtime?

Those should not be collapsed into one irreversible decision.

## Current State Of The Options

## Official OpenAI Symphony

Good at:

- issue-driven orchestration
- isolated workspaces
- repo-owned workflow policy
- codified retries/reconciliation

Current constraints:

- reference implementation is Linear-first
- workflow examples are Linear-shaped
- runner examples are Codex-shaped

Interpretation:

- good long-term direction
- higher integration friction if Pulp wants GitHub-first tracking and Claude support immediately

## GitHub Issues As Tracker

Good at:

- shared visibility
- PR linkage
- comment/review flow
- lower team overhead if GitHub is already home base

Constraints:

- weaker built-in workflow semantics than Linear
- more state conventions need to be defined by labels, projects, or fields
- dependencies/blocking must be represented more explicitly by Pulp conventions

Interpretation:

- a good collaboration surface
- especially good if Pulp wants to stay repo-native

## Linear As Tracker

Good at:

- stronger workflow state machine out of the box
- cleaner agent-ready queue semantics
- easier mapping to “candidate / active / terminal” issue states

Constraints:

- another system to adopt
- may be overkill if the project already prefers GitHub-centered collaboration

Interpretation:

- the easiest way to use official Symphony now
- not obviously the best long-term source of truth for Pulp

## Codex vs Claude

Official Symphony today aligns much more naturally with Codex.

If Pulp wants freedom to choose:

- Codex for some work
- Claude for some work
- perhaps other runners later

then the orchestrator should be selected with runner pluggability as a first-class requirement.

## Recommended Direction

## Source Of Truth

Use this split:

- local `planning/` files for rough intake, refinement, and lightweight kanban persistence
- GitHub Issues for shared backlog and PR-linked collaboration

This keeps the project understandable even without any orchestrator running.

## Execution Queue

Only issues that are actually ready for autonomous execution should enter an orchestrator-managed queue.

Criteria:

- scope is clear
- acceptance criteria are explicit
- dependencies are known
- issue is not primarily architectural exploration

## Orchestrator Choice

### Near-Term Recommendation

Do not force official Symphony immediately unless there is a strong reason to adopt Linear now.

Instead:

- keep Pulp GitHub-first
- design the local planning/work-item pipeline first
- evaluate a Symphony-style orchestrator that already supports GitHub Issues and multiple runners

### If Official Symphony Is Still Preferred

Then use this compromise:

- GitHub Issues remains the shared backlog
- a very small Linear project becomes the execution-only queue
- only promoted, agent-ready work is mirrored into Linear

This avoids making Linear the whole project-management system while still making official Symphony usable.

## Runner Recommendation

Pulp should not commit itself to Codex-only orchestration unless that becomes a deliberate strategic decision.

Recommended standard:

- the orchestration layer should support Codex
- it should remain possible to use Claude for at least some execution modes
- tracker choice should not dictate runner choice

That means Pulp should prefer a runner-pluggable architecture over a single-agent commitment.

## Actionable Build Plan

## Phase 1: Local Idea Pipeline

Build first:

- `planning/inbox/`
- `planning/backlog/`
- `planning/wip/`
- `planning/blocked/`
- `planning/done/`
- one normalized work-item template
- one prioritization/refinement workflow

Goal:

- prove that rough notes can become durable, actionable work items

## Phase 2: GitHub Issue Promotion

Build next:

- issue template for promoted work items
- labels for priority, agent-readiness, blocked, type
- optional GitHub Project board for backlog / ready / in-progress / done

Goal:

- make the work visible and collaborative without abandoning local planning files

## Phase 3: Orchestrator Pilot

Choose one:

### Path A: Official Symphony Pilot

- create a tiny Linear execution board
- mirror only `agent-ready` work into it
- use official Symphony + Codex

Success criterion:

- can run a few contained issues end to end without changing how the whole project is managed

### Path B: GitHub-Native Pilot

- keep GitHub Issues as the tracked queue
- use a Symphony-style orchestrator that already supports GitHub Issues
- validate both Codex and Claude-family execution paths

Success criterion:

- can run a few contained issues without introducing Linear

## Phase 4: Decide The Long-Term Standard

After the pilot, decide:

- official Symphony + Linear execution lane
- GitHub-native orchestrator
- or build/adapt a GitHub tracker adapter if official Symphony is still strategically preferred

This decision should be made from operational evidence, not preference alone.

## Recommendation Matrix

### If the priority is “use official Symphony as soon as possible”

Choose:

- Linear execution lane
- Codex

### If the priority is “stay GitHub-native and keep Claude viable”

Choose:

- GitHub Issues
- local planning board
- a runner-pluggable Symphony-style orchestrator

### If the priority is “minimize process overhead right now”

Choose:

- local planning files
- GitHub Issues
- no orchestrator yet

## What Pulp Should Avoid

- forcing all raw ideas directly into an orchestrator
- making tracker adoption heavier than the engineering value gained
- tying the whole system to one model/vendor before the workflow is proven
- introducing Linear as the source of truth before knowing it is truly worth the overhead

## My Actual Recommendation

For Pulp today:

1. Build the local idea pipeline first.
2. Promote refined items into GitHub Issues.
3. Pilot orchestration on a narrow subset of agent-ready work.
4. Prefer a GitHub-native, runner-pluggable approach unless official Symphony + Linear turns out to be clearly better operationally.

That keeps the project flexible:

- GitHub-first
- local-first
- Claude-compatible
- Codex-compatible
- and still able to adopt official Symphony later if it earns the complexity

## Questions Worth Asking Before Building

These are the most useful open questions for an orchestrator maintainer or heavy user:

- Is GitHub Issues support good enough in practice, or does Linear materially reduce orchestration friction?
- Do mixed-runner teams actually work well, or does one runner inevitably become the standard?
- Is the operational overhead of a second tracker justified for a codebase like Pulp?
- Is it better to adapt official Symphony, or to adopt a more GitHub-native implementation and keep the workflow compatible at the conceptual level?

## Bottom Line

Pulp should not start by choosing between “Linear forever” and “no Symphony.”

The better framing is:

- keep planning local
- keep shared backlog in GitHub
- test orchestration on a small execution queue
- only adopt Linear if the operational benefit clearly outweighs the extra system

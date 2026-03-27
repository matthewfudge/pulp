# Symphony + Pulp Idea Pipeline Proposal

Date: 2026-03-27

## Short Answer

Yes, Symphony is directionally a good fit for Pulp.

It offers a way to move from:

- work items in a tracker
- to isolated agent workspaces
- to autonomous implementation runs
- to PRs, validation, and handoff

That matches a lot of what Pulp is already trying to do with loop prompts, work items, planning docs, and agent-driven execution.

The important caveat is that the current Symphony specification and reference implementation are Linear-first, not GitHub-Issues-first.

So the answer is:

- conceptually: yes
- today, with minimal extra engineering: best with Linear
- with a reasonable adapter/sync layer: GitHub Issues can work
- for your specific "idea-pipeline" concept: the best shape is probably a hybrid local-files + GitHub Issues + later Symphony execution flow

## What Symphony Would Offer Pulp

Based on the public repository, Symphony is designed to:

- continuously read work from an issue tracker
- create isolated per-issue workspaces
- run autonomous coding-agent sessions inside those workspaces
- preserve workflow policy in a repository-owned `WORKFLOW.md`
- reconcile active work, retries, and terminal states over time

That is valuable for Pulp because it would give you:

### 1. A Better Execution Layer For Existing Work Items

Pulp already has a lot of planning documents and structured prompts. Symphony could turn selected work items into long-running autonomous execution runs instead of one-off manual agent sessions.

### 2. Isolated Work Per Item

This is one of Symphony’s strongest ideas.

For each issue or task:

- separate workspace
- separate branch
- separate logs
- separate retry/reconciliation state

That is a strong match for Pulp, where many tasks are sizable and where test/docs/status updates often need to stay bundled with the implementation.

### 3. Versioned Workflow Policy

Symphony’s `WORKFLOW.md` approach fits Pulp very well.

You already think in terms of:

- repo-owned prompts
- planning docs
- explicit acceptance criteria
- sequential work phases

That maps naturally onto a repo-owned workflow contract.

### 4. Less Human Supervision Of Coding Agents

If the work item is clear enough, Symphony’s value is that you manage the work queue and review outputs instead of supervising each tool call live.

That is especially attractive for:

- docs/status consistency work
- contained feature work
- validation/test expansion
- example/plugin maintenance
- repetitive repo hardening tasks

## What Symphony Does Not Solve By Itself

Symphony is not your backlog brain.

It is primarily a scheduler/orchestrator for agent execution once work is already clear enough to run.

So for your idea:

> rough ideas/brain dumps -> refine into backlog item -> prioritize -> lightweight kanban -> persistent tracking

Symphony is strongest at the later half:

- queue selected work
- run it in isolation
- track active execution
- reconcile completion/handoff

It is weaker as the raw idea-capture and refinement surface unless you build that layer on top.

## Recommended Shape For Your Idea Pipeline

I would not make Symphony the first thing that touches raw notes or speech dumps.

I would structure the system in four layers.

## Layer 1: Local Idea Intake

This is the place for messy thinking.

Suggested structure:

- `planning/inbox/`
- `planning/backlog/`
- `planning/wip/`
- `planning/done/`
- `planning/blocked/`
- `planning/board.yaml` or `planning/index.md`

Each inbox item can start very loose:

- typed thought
- speech-to-text dump
- transcript excerpt
- rough sketch of a feature/problem

Then a local skill or agent pass refines it into a normalized work-item document:

- title
- why it matters
- desired outcome
- acceptance criteria
- dependencies
- confidence
- effort estimate
- recommended next action

This gives you the lightweight kanban-like filesystem you described.

## Layer 2: Promotion To Shared Tracking

Once a local item is real enough, promote it to shared tracking.

This is where GitHub Issues makes sense.

Why GitHub Issues is useful here:

- easy for collaborators to see and comment on
- easy to link to PRs
- easier than Linear if your collaborators already live in GitHub
- keeps the public/project work surface closer to the code

Suggested rule:

- local files are the draft and refinement layer
- GitHub Issues are the shared coordination layer

## Layer 3: Execution Queue

Only some issues should become autonomous Symphony work.

That queue should be smaller and cleaner:

- clear scope
- clear acceptance criteria
- low enough ambiguity for an agent to execute
- minimal architectural uncertainty

This is where Symphony is valuable.

In effect:

- not every idea becomes a Symphony run
- only promoted, execution-ready work becomes a Symphony run

## Layer 4: Review / Landing / Archive

After execution:

- PR created
- tests/validation attached
- human review
- merge or rework
- local/GitHub tracker updated
- work item archived into `done`

That gives you a full pipeline from messy idea to durable shipped outcome.

## GitHub Issues Or Linear?

## Current Reality

Today’s Symphony reference implementation is Linear-first.

The current spec explicitly says:

- the tracker in this specification version is Linear
- the reference implementation polls Linear
- the example setup expects `LINEAR_API_KEY`
- there is a TODO for pluggable tracker adapters beyond Linear

So:

- Symphony does not conceptually require Linear forever
- but the current spec/reference implementation absolutely leans on Linear today

## What That Means Practically

### Option A: Use Linear For Execution, GitHub For Collaboration

This is the least engineering risk if you want to try Symphony soon.

Flow:

1. Capture/refine work locally.
2. Promote real work items to GitHub Issues.
3. Mirror "agent-ready" issues into a Linear project used only as the Symphony execution queue.
4. Let Symphony monitor Linear and create runs.
5. Link resulting PRs back to GitHub Issues.

Pros:

- easiest path to using Symphony now
- least custom integration work

Cons:

- two trackers
- some sync/mirror complexity

### Option B: Build A GitHub Issues Adapter

This is the cleanest long-term fit if you want GitHub to be the main shared tracker.

What would be needed:

- implement `tracker.kind: github`
- map GitHub Issues or GitHub Projects states into Symphony’s normalized issue model
- implement candidate fetch, terminal fetch, and issue-state refresh
- decide how labels, priority, blocked state, and project lanes map

This is very doable, but it is real integration work.

Pros:

- one shared tracker
- cleaner repo-native workflow

Cons:

- you own the adapter
- you must decide state semantics that Linear currently gives the reference implementation

### Option C: Local Files First, No Shared Tracker Yet

This is best for solo experimentation.

Flow:

- local `planning/inbox/backlog/wip/done`
- lightweight agent skill for refinement/prioritization
- maybe no Symphony yet
- optionally promote only the highest-value items into GitHub or Linear later

Pros:

- lowest overhead
- best for experimenting with your “idea pipeline” concept

Cons:

- weaker shared visibility
- weaker direct Symphony compatibility

## My Recommendation

For Pulp, I would recommend:

### Near Term

- local files for idea intake and refinement
- GitHub Issues for shared backlog
- no requirement to adopt Linear across the whole project

### If You Want Symphony Soon

Use one of these:

- a small Linear execution board just for Symphony
- or a thin GitHub -> Linear mirror for agent-ready work only

### Long Term

Build a proper GitHub adapter for Symphony and drop the Linear dependency if you do not want it strategically.

That keeps the path practical now without locking you into the wrong tracker forever.

## How This Could Look In Pulp

## Proposed Filesystem Model

```text
planning/
  inbox/
  backlog/
  wip/
  blocked/
  done/
  templates/
    work-item.md
    issue-promotion.md
  board.yaml
  WORKFLOW.md
```

## Proposed Work Item Shape

Each normalized work item should contain:

- summary
- problem
- desired outcome
- why now
- acceptance criteria
- dependencies
- risk
- recommended execution mode:
  - `human`
  - `agent-assisted`
  - `symphony-ready`

That last field is important. It keeps Symphony from trying to execute vague or architectural work too early.

## Proposed Agent Flow

### Step 1: Intake Skill

Input:

- rough note
- transcript
- brainstorm

Output:

- refined markdown work item in `planning/inbox/` or `planning/backlog/`

### Step 2: Prioritization Skill

Adds:

- priority
- effort
- dependency tags
- recommended next state

### Step 3: Promotion

If shared visibility is needed:

- open/update GitHub Issue
- add labels
- assign milestone/project

### Step 4: Execution Routing

If the work item is execution-ready:

- create Symphony-tracked issue
- or push into the execution lane the adapter watches

### Step 5: Autonomous Run

Symphony:

- creates/reuses per-issue workspace
- runs the agent in that workspace
- preserves logs/state
- reconciles retries/state changes

### Step 6: Review + Merge

- PR
- tests
- review
- merge
- mark done in shared tracker and local files

## Branch / Workspace Strategy

If you use Symphony with Pulp, I would strongly prefer:

- one workspace per issue
- one branch per issue
- PR as the integration boundary

I would not recommend trying to keep multiple long-lived execution branches manually in sync.

The cleaner model is:

- issue -> workspace -> branch -> PR

Then merge or discard.

For Pulp specifically, I would likely customize workspace setup to prefer `git worktree` or a lightweight clone strategy rather than ad hoc manual branch syncing.

## Best Use Cases For Symphony In Pulp

Good fits:

- work items with crisp acceptance criteria
- doc/status consistency passes
- adding tests around known behaviors
- contained feature completion
- example/plugin updates
- repetitive repo hardening

Poor fits:

- foundational architecture decisions
- ambiguous R&D
- work that still needs product definition
- work where the “correct” output is still mostly conversational

That reinforces why the idea-pipeline layer should exist before the Symphony layer.

## Recommended Adoption Plan

## Phase 1: Local-First Idea Pipeline

Build the simple filesystem-based intake/refinement/prioritization flow first.

Goal:

- prove the workflow
- keep it pleasant
- create persistent tracking

## Phase 2: GitHub Issue Promotion

Add promotion from refined local work item -> GitHub Issue.

Goal:

- make collaboration easier
- keep the backlog visible

## Phase 3: Symphony Pilot

Pilot Symphony on a small execution-only queue.

Best choices:

- use a dedicated Linear board if you want the fastest path
- or prototype a GitHub adapter if you want the cleaner long-term path

## Phase 4: Tight Integration

Once proven:

- automatic issue promotion
- automatic status sync
- automatic PR linking
- optional branch/workspace cleanup

## Bottom Line

Symphony makes sense for Pulp, but not as the whole pipeline.

The right design is:

- local idea capture
- local refinement
- lightweight kanban persistence
- optional GitHub Issues for shared coordination
- Symphony only for execution-ready work

And on the tracker question:

- today: Symphony is effectively Linear-first
- strategically: it can be adapted to GitHub Issues
- recommended for Pulp: use a hybrid path now, and only build a GitHub-native Symphony adapter if the orchestration model proves valuable enough to keep

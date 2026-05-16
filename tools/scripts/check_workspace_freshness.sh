#!/usr/bin/env bash
# check_workspace_freshness.sh — guard against running validation on a
# stale checkout / SDK install.
#
# Why this exists
# ---------------
# Real session 2026-05-15: a Spectr roundtrip was run from a feature branch
# 175 commits behind origin/main. The score (0.5408) reflected the stale
# branch's framework code, not current main. We spent 15 minutes drawing
# parser conclusions before noticing the checkout was old.
#
# This script is the preflight that would have caught it. Source it from
# any validation harness that's supposed to reflect "what's on main."
#
# Usage
# -----
#   tools/scripts/check_workspace_freshness.sh                # exits non-zero if stale
#   tools/scripts/check_workspace_freshness.sh --warn         # warn-only
#   tools/scripts/check_workspace_freshness.sh --max-behind N # tolerate up to N commits behind
#   tools/scripts/check_workspace_freshness.sh --json         # machine-readable status
#   PULP_FRESHNESS_BYPASS=1 tools/scripts/check_workspace_freshness.sh   # bypass entirely
#
# Exit codes
#   0 — fresh enough (or bypassed / warn-only)
#   1 — stale (in enforcing mode)
#   2 — invocation / git error

set -euo pipefail

MODE="enforce"
MAX_BEHIND=0
JSON=0
REMOTE="origin"
BRANCH="main"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --warn) MODE="warn"; shift ;;
    --max-behind) MAX_BEHIND="${2:-0}"; shift 2 ;;
    --json) JSON=1; shift ;;
    --remote) REMOTE="${2:-origin}"; shift 2 ;;
    --branch) BRANCH="${2:-main}"; shift 2 ;;
    -h|--help)
      sed -n '2,/^set -e/p' "$0" | head -30
      exit 0
      ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

if [[ "${PULP_FRESHNESS_BYPASS:-0}" == "1" ]]; then
  if [[ "$JSON" == "1" ]]; then
    printf '{"bypassed": true, "behind": 0, "ahead": 0}\n'
  else
    echo "[freshness] PULP_FRESHNESS_BYPASS=1 set; skipping check"
  fi
  exit 0
fi

# Must be in a git repo
if ! git rev-parse --git-dir >/dev/null 2>&1; then
  echo "[freshness] error: not in a git repository" >&2
  exit 2
fi

# Fetch latest origin/main (best-effort; fall back to existing ref if offline)
git fetch --quiet "$REMOTE" "$BRANCH" 2>/dev/null || true

if ! git rev-parse --verify --quiet "$REMOTE/$BRANCH" >/dev/null; then
  echo "[freshness] error: $REMOTE/$BRANCH not found (fetch failed?)" >&2
  exit 2
fi

# Calculate ahead/behind from the merge base
HEAD_SHA=$(git rev-parse HEAD)
REMOTE_SHA=$(git rev-parse "$REMOTE/$BRANCH")
read -r AHEAD BEHIND < <(git rev-list --left-right --count "HEAD...$REMOTE/$BRANCH" 2>/dev/null || echo "0 0")

# Detect dirty working tree
DIRTY=0
if ! git diff --quiet 2>/dev/null || ! git diff --quiet --cached 2>/dev/null; then
  DIRTY=1
fi

if [[ "$JSON" == "1" ]]; then
  printf '{"head": "%s", "remote": "%s", "ahead": %d, "behind": %d, "dirty": %d, "max_behind": %d, "mode": "%s"}\n' \
    "$HEAD_SHA" "$REMOTE_SHA" "$AHEAD" "$BEHIND" "$DIRTY" "$MAX_BEHIND" "$MODE"
fi

# Decide
STALE=0
if (( BEHIND > MAX_BEHIND )); then
  STALE=1
fi

if (( STALE == 0 )); then
  if [[ "$JSON" != "1" ]]; then
    printf '[freshness] OK — HEAD %s is %d ahead, %d behind %s/%s (max_behind=%d)\n' \
      "${HEAD_SHA:0:9}" "$AHEAD" "$BEHIND" "$REMOTE" "$BRANCH" "$MAX_BEHIND"
  fi
  exit 0
fi

# Stale — emit a clear human-readable message
if [[ "$JSON" != "1" ]]; then
  cat >&2 <<MSG
[freshness] WARNING — checkout is $BEHIND commits behind $REMOTE/$BRANCH

  HEAD:    ${HEAD_SHA:0:9}
  remote:  ${REMOTE_SHA:0:9}  ($REMOTE/$BRANCH)
  delta:   $AHEAD ahead, $BEHIND behind
  dirty:   $([[ $DIRTY == 1 ]] && echo yes || echo no)

Validation results from this checkout will reflect old framework code, not
current $REMOTE/$BRANCH. To reset:

  # Option A — fast-forward this branch (loses uncommitted changes if dirty)
  git fetch $REMOTE && git reset --hard $REMOTE/$BRANCH

  # Option B — work in a fresh worktree off $REMOTE/$BRANCH
  git worktree add /tmp/pulp-fresh $REMOTE/$BRANCH
  cd /tmp/pulp-fresh
  # … run validation here

  # Option C — explicitly accept the staleness for this run
  PULP_FRESHNESS_BYPASS=1 <command>
  # or use --warn / --max-behind N when invoking the harness
MSG
fi

if [[ "$MODE" == "warn" ]]; then
  exit 0
fi
exit 1

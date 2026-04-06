#!/usr/bin/env bash
# Sync planning/ directory to a separate private repository.
#
# Usage:
#   ./tools/scripts/sync-planning.sh              # sync and commit
#   ./tools/scripts/sync-planning.sh --init <url>  # first-time setup
#
# The private repo is cloned into .planning-repo/ (gitignored).
# Files from planning/ are copied there and committed/pushed.

set -euo pipefail

PULP_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PLANNING_DIR="$PULP_ROOT/planning"
REPO_DIR="$PULP_ROOT/.planning-repo"

if [[ ! -d "$PLANNING_DIR" ]]; then
    echo "No planning/ directory found. Nothing to sync."
    exit 0
fi

if [[ "${1:-}" == "--init" ]]; then
    if [[ -z "${2:-}" ]]; then
        echo "Usage: $0 --init <git-remote-url>"
        exit 1
    fi
    git clone "$2" "$REPO_DIR"
    echo "Initialized planning repo at $REPO_DIR"
    exit 0
fi

if [[ ! -d "$REPO_DIR/.git" ]]; then
    echo "Planning repo not initialized. Run: $0 --init <git-remote-url>"
    exit 1
fi

# Sync files (delete removed, add new)
rsync -a --delete \
    --exclude='.DS_Store' \
    --exclude='*.tmp' \
    "$PLANNING_DIR/" "$REPO_DIR/"

cd "$REPO_DIR"
git add -A

if git diff --cached --quiet; then
    echo "No planning changes to sync."
    exit 0
fi

DATE=$(date +%Y-%m-%d)
git commit -m "Sync planning docs ($DATE)"
git push
echo "Planning docs synced and pushed."

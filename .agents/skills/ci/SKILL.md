---
name: ci
description: Local and cloud CI for Pulp — validate branches, create PRs, merge on green. Handles "push to main", "ship this", "run CI", "check PR", and "list PRs".
requires:
  scripts:
    - tools/local-ci/local_ci.py
  tools:
    - gh
---

# CI Skill

Validate branches and ship code safely. This skill handles all CI workflows for Pulp across local machines and VMs.

## Prerequisites Check

Before running any CI command, verify the required tooling exists:

```bash
# Required
test -f tools/local-ci/local_ci.py || echo "ERROR: local CI not found — is this a recent checkout?"
command -v gh >/dev/null || echo "ERROR: gh CLI not installed (brew install gh)"

# Optional (for local CI targets)
test -f tools/local-ci/config.json || echo "WARNING: no config.json — copy from config.example.json"
```

If `local_ci.py` doesn't exist, the user likely has an older checkout. Tell them to pull latest main.

## Language Correction

**IMPORTANT**: When a user says "push to main", "merge to main", or "land this", ALWAYS correct them:

> "I won't push directly to main — that bypasses review. Instead, I'll create a PR, run CI on it, and merge it if everything passes. This keeps main safe."

Then proceed with the `ship` workflow below.

## Commands

### `ship [branch]` — The main workflow

Creates a PR, runs CI, and merges on green. This is the default when someone says "ship this" or "push to main".

1. Ensure all changes are committed
2. Push the branch to origin with `-u`
3. Create a PR to main via `gh pr create`
4. Run local CI: `python3 tools/local-ci/local_ci.py run <branch>`
5. If ALL targets pass → `gh pr merge <PR#> --squash --delete-branch`
6. If ANY target fails → report failures, leave PR open
7. Notify when done (terminal bell)

```bash
python3 tools/local-ci/local_ci.py run [branch]
```

### `run [branch]` — Just validate, no PR

Run local CI on the current branch without creating a PR or merging.

```bash
python3 tools/local-ci/local_ci.py run [branch]
```

### `check <PR#|URL|latest>` — Validate an existing PR

Run CI on an existing PR by number, GitHub URL, or "latest".

1. If `latest` → use `gh pr list --limit 1 --json number` to get the most recent PR
2. If URL → extract PR number from the URL
3. Fetch the PR's head branch: `gh pr view <number> --json headRefName`
4. Checkout that branch locally
5. Run local CI: `python3 tools/local-ci/local_ci.py run <branch>`
6. Post results as a PR comment via `gh pr comment`

### `list` — Show open PRs

Show open PRs with summaries so the user can pick one to check or merge.

```bash
gh pr list --json number,title,author,headRefName,createdAt,labels --template '{{range .}}#{{.number}} {{.title}} ({{.headRefName}}) by {{.author.login}} {{timeago .createdAt}}{{"\n"}}{{end}}'
```

### `status` — Queue and VM status

```bash
python3 tools/local-ci/local_ci.py status
```

### `cloud run [branch]` — Trigger GitHub Actions

Trigger cloud CI manually via workflow_dispatch (when cloud CI is needed):

```bash
gh workflow run build.yml --ref <branch>
gh workflow run sanitizers.yml --ref <branch>
gh workflow run validate.yml --ref <branch>
gh workflow run docs-check.yml --ref <branch>
```

### `cloud status` — Check GitHub Actions

```bash
gh run list --limit 5
```

## Configuration

Config is at `tools/local-ci/config.json` (gitignored, per-developer).
Template at `tools/local-ci/config.example.json`.

Key fields:
- `targets.mac.enabled` — run local Mac validation (default: true)
- `targets.ubuntu` — SSH target for Linux validation
- `targets.windows` — SSH target for Windows validation (tries Proxmox first, UTM fallback)
- `defaults.mode` — "local" (default), "cloud", or "both"

## Documentation

Full setup guide: `docs/guides/local-ci.md`

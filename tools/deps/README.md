# Dependency Tooling

This directory is the machine-readable dependency inventory and update-validation lane for Pulp.

## Files

- `manifest.json` — source of truth for tracked third-party dependencies, pins, licenses, and documentation requirements
- `audit.py` — audits `DEPENDENCIES.md` / `NOTICE.md` coverage and optionally checks upstream refs/tags
- `validate_hosts.py` — runs outer-loop validation locally and on optional SSH targets

## Usage

```bash
# Audit docs/notice coverage
python3 tools/deps/audit.py --strict

# Audit coverage plus upstream drift
python3 tools/deps/audit.py --check-upstream --format markdown

# Run local outer-loop validation plus any configured SSH hosts
python3 tools/deps/validate_hosts.py
```

## Local SSH Host Config

Create `tools/deps/hosts.local.json` for your machine-specific validators. This file is gitignored.

Example:

```json
{
  "unix_targets": [
    { "host": "ubuntu", "path": "/home/daniel/Code/pulp" }
  ],
  "windows_targets": [
    { "host": "win2", "path": "C:\\\\Users\\\\danielraffel\\\\Code\\\\pulp" }
  ]
}
```

Remote validation is intended to be git-based:

- push the branch to `origin`
- point each target at a real `git clone` / worktree of the repo
- let the validator `git fetch` / `git checkout` / `git pull` before running the clean build
- ensure required build tools are installed on that machine

If a remote checkout does not have an `origin` remote configured, the validator currently falls back to validating the current checkout (or a matching local branch if present) so personal machines still produce a useful result. That fallback is a convenience, not the preferred setup. If the configured path is not a real git checkout, validation fails clearly.

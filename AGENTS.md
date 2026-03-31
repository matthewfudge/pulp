# AGENTS.md

Shared instructions for all AI coding agents working on Pulp (Codex, Claude Code, Cursor, etc.).

Detailed guidance lives in `CLAUDE.md` — treat it as the single source of truth for build commands, architecture, clean-room rules, commit standards, and testing requirements.

## Key Rules

- **Never push directly to main.** Create a PR and run CI first. Use the `ci` skill.
- **Clean-room discipline.** Never reference JUCE source code. See CLAUDE.md.
- **License policy.** Only MIT, BSD, Apache 2.0, ISC, zlib, BSL-1.0, public domain. No copyleft.
- **Tests required.** If it's not tested, it doesn't work.

## Skills

Shared skills live in `.agents/skills/`. They are versioned with the code — a skill must never reference features that don't exist at the same commit.

| Skill | Purpose |
|-------|---------|
| `ci` | Create PRs, run local/cloud CI, merge on green |
| `import-design` | Import from Figma, Stitch, v0, Pencil |

When you create a new repeatable workflow, consider adding it as a skill. See the "Self-Enhancement" section in `CLAUDE.md` for guidelines.

## CI Workflow

```bash
# Validate current branch locally
python3 tools/local-ci/local_ci.py run

# Ship: PR + CI + merge on green
# Use the ci skill — say "ship this" or "create a PR and push to main"
```

Setup: `docs/guides/local-ci.md`

## Quick Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure
```

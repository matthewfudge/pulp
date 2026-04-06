---
name: ci
description: Run CI validation, create PRs, and merge on green
---

Use the `ci` skill for all CI workflows. This is the primary gate for landing code on main.

## Ship (branch → PR → CI → merge)

1. Ensure all changes are committed
2. Push the branch to origin
3. Create a PR: `gh pr create`
4. Run local CI: `python3 tools/local-ci/local_ci.py run <branch>`
5. If ALL targets pass (mac, ubuntu, windows) → merge: `gh pr merge <PR#> --squash --delete-branch`
6. If ANY target fails → report failures, fix, rerun

## Just validate (no PR)

```bash
python3 tools/local-ci/local_ci.py run [branch]
python3 tools/local-ci/local_ci.py run [branch] --smoke
```

## Check status

```bash
python3 tools/local-ci/local_ci.py status
```

## Cloud CI

```bash
python3 tools/local-ci/local_ci.py cloud run build <branch>
```

See `.agents/skills/ci/SKILL.md` for the full workflow reference.

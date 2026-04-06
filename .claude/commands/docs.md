---
name: docs
description: Search and browse local documentation
---

Search and browse the Pulp documentation locally.

```bash
./build/tools/cli/pulp docs search <query>     # search docs
./build/tools/cli/pulp docs check              # validate docs consistency
./build/tools/cli/pulp docs open <slug>        # open a doc page
./build/tools/cli/pulp docs show support <x>   # show support status
./build/tools/cli/pulp docs show command <x>   # show CLI command details
./build/tools/cli/pulp docs show cmake <x>     # show CMake function docs
```

Use `pulp docs check` after modifying code in `core/`, `examples/`, or `tools/cli/` to verify docs stay consistent with the codebase.

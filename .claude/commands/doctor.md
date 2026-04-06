---
name: doctor
description: Check environment and diagnose build issues
---

Check the development environment for missing dependencies and configuration issues.

```bash
./build/tools/cli/pulp doctor
```

Options:
- `pulp doctor --fix` — auto-fix issues where possible
- `pulp doctor --ci` — CI mode, exit codes only
- `pulp doctor --dry-run` — show what --fix would do

Checks include: CMake version, C++ compiler, git-lfs, Skia binaries, VST3 SDK, AudioUnit SDK, platform-specific dependencies (ALSA on Linux, Xcode CLI tools on macOS).

Run this first when builds fail unexpectedly or on a new machine.

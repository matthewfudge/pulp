# Pulp Install & Onboarding Proposal

**Date:** 2026-03-27
**Status:** Draft
**Branch:** feature/first-time-setup

---

## 1. Issue #1 Analysis — Concrete Pain Points

From Jeff Huston's first-time experience (Issue #1):

| # | Pain Point | Root Cause | Severity |
|---|-----------|------------|----------|
| 1 | `cmake -B build` fails immediately — `examples/design-tool` doesn't exist | Unreleased directory referenced in CMakeLists.txt | **Blocker** |
| 2 | Skia headers are git-lfs pointers, not actual files — C++ compiler sees `version https://git-lfs.github.com/spec/v1` | No git-lfs setup in onboarding flow | **Blocker** |
| 3 | `external/vst3sdk` and `external/AudioUnitSDK` are broken symlinks pointing to developer's home dir | Symlinks committed instead of proper clone/fetch | **Blocker** |
| 4 | README says `cmake -B build` — no mention of prerequisites | Missing dependency documentation | **High** |
| 5 | `setup.sh` exists in `tools/scripts/` but isn't discoverable | Buried in subdirectory, not mentioned in README | **High** |
| 6 | Docs are circular — unclear where build artifacts end up | Documentation structure issue | **Medium** |
| 7 | Standalone build didn't produce a visible artifact | Unclear output paths | **Medium** |
| 8 | No clear "pulp CLI" install path — user expected it to drive the experience | CLI requires successful build first (chicken-and-egg) | **High** |

**Key insight:** 3 of the top 4 blockers are dependency/setup issues that a proper bootstrap script would eliminate entirely. The 4th is a repo bug (design-tool reference).

### Implicit Expectations (reading between the lines)

- Expected `git clone` + one command to get a working build
- Expected the CLI to be the primary interface (not raw cmake)
- Expected symlinks/lfs to "just work" or be handled automatically
- Compared experience to Homebrew's one-liner install model
- Wanted clear output: "here's what was built, here's how to use it"

---

## 2. Competitive Analysis — Design Decisions for Pulp

### rustup: What makes it effective

- **One binary manages everything** — install, update, switch versions, add targets
- **No system pollution** — installs to `~/.rustup/` and `~/.cargo/bin/`
- **Cross-platform from day one** — same mental model on macOS/Windows/Linux
- **Offline-capable** — ships toolchain as archives, not build-from-source

**Pulp takeaway:** The installer (`pulpup`) should be a standalone binary/script that manages the Pulp CLI installation without requiring the user to build anything first. Pre-built CLI binaries per platform.

### uv (Astral): What makes it effective

- **Replaces the entire toolchain** — no need to install Python, pip, venv separately
- **Blazing fast** — Rust-based, resolves and installs in seconds
- **Deterministic** — lockfiles, exact version pinning
- **Zero config for common cases** — `uv init && uv run` just works

**Pulp takeaway:** `pulp init` should produce a project that builds and runs with zero additional configuration. Dependencies should be fetched automatically. The happy path should require zero manual intervention.

### create-next-app: What makes it effective

- **Immediate success** — `npx create-next-app my-app && cd my-app && npm run dev` → working app in browser
- **Interactive template selection** — but with sensible defaults
- **Verification built in** — the dev server starting IS the verification

**Pulp takeaway:** After `pulp init my-plugin`, the user should be able to `pulp build && pulp run` and hear audio or see a plugin loaded. The "first success" must be tangible — not just "tests pass" but "I can hear/see something."

### Ghost CLI: What makes it effective

- **Lifecycle management** — install, setup, start, stop, update, doctor
- **Environment awareness** — detects Node version, DB, config issues
- **`ghost doctor`** — diagnoses and fixes common problems

**Pulp takeaway:** `pulp doctor` should detect missing dependencies, broken symlinks, stale builds, and provide exact fix commands. This is the escape hatch when automated setup fails.

---

## 3. Install Architecture — Three Layers

### Layer A: Installer (`pulpup`)

**Purpose:** Install and manage the Pulp CLI binary. This is the only thing the user needs to bootstrap.

**Entrypoints:**
```bash
# macOS / Linux
curl -fsSL https://generouscorp.com/pulp/install.sh | sh

# Windows (PowerShell)
irm https://generouscorp.com/pulp/install.ps1 | iex
```

**What it does:**
1. Detect OS + architecture (x86_64, arm64)
2. Download pre-built `pulp` CLI binary from GitHub releases
3. Install to `~/.pulp/bin/pulp`
4. Add `~/.pulp/bin` to PATH (append to shell profile)
5. Verify installation: `pulp --version`

**What it does NOT do:**
- Install compilers, CMake, or system dependencies (that's `pulp doctor`'s job)
- Clone any repos
- Modify system directories

**Version management:**
```bash
pulpup update          # update CLI to latest
pulpup install 0.2.0   # install specific version
pulpup list            # show installed versions
pulpup default 0.1.0   # switch active version
```

**Implementation:** Shell script (install.sh) + PowerShell script (install.ps1). The scripts are trivial — download a tarball, extract, add to PATH. The `pulpup` binary itself is the Pulp CLI with a `self-update` subcommand (no separate binary needed).

**Alternative considered:** Distributing via Homebrew/cargo/npm. Rejected because:
- Homebrew is macOS-only and requires Homebrew itself
- cargo requires Rust toolchain (Pulp is C++, confusing)
- npm requires Node.js (wrong signal for a C++ framework)
- A standalone installer avoids ecosystem lock-in

### Layer B: Project Bootstrap (`pulp new` / `pulp init`)

**Purpose:** Create a new Pulp plugin or application project that builds and runs immediately.

```bash
# Interactive (recommended for first time)
pulp new my-plugin

# Non-interactive (CI, scripting) — defaults to all platform-supported formats
pulp new my-plugin --template=effect --no-interactive

# Override formats explicitly
pulp new my-plugin --template=effect --format=vst3,clap --no-interactive
```

**Interactive flow:**
```
$ pulp new my-plugin

  What are you building?
  › Audio effect plugin       (processes audio: EQ, compressor, reverb)
    Instrument plugin         (generates audio: synth, sampler, drum machine)
    Standalone application    (audio app, no DAW needed)

  Which formats? [all supported formats pre-selected for your platform]
  ✓ VST3        (Windows, macOS, Linux)
  ✓ CLAP        (Windows, macOS, Linux)
  ✓ AU          (macOS only — auto-selected on macOS, hidden on others)
  ✓ Standalone  (all platforms)
  ○ LV2         (Linux only — auto-selected on Linux, hidden on others)
  ○ WAM         (Web — opt-in, requires Emscripten)

  Plugin name: My Plugin
  Manufacturer: My Company
  Bundle ID [com.mycompany.my-plugin]:

  Creating my-plugin/...
  ✓ Project scaffolded
  ✓ Dependencies configured
  ✓ Building... (first build takes ~30s)
  ✓ Tests pass (3/3)

  Your plugin is ready:
    VST3:  my-plugin/build/VST3/MyPlugin.vst3
    CLAP:  my-plugin/build/CLAP/MyPlugin.clap

  Next steps:
    cd my-plugin
    pulp build          # rebuild after changes
    pulp test           # run tests
    pulp run            # load in standalone host
    pulp install        # install to system plugin folders
```

**What `pulp new` does internally:**
1. Check environment (`pulp doctor --quick`) — fail fast if compiler/cmake missing
2. Create project directory from template
3. Generate CMakeLists.txt, source files, test file
4. Run `cmake -B build` (fetches FetchContent deps automatically)
5. Run `cmake --build build`
6. Run `ctest`
7. Report success with artifact paths

**Templates shipped:**
- `effect` — stereo gain effect (default)
- `instrument` — simple sine synth
- `app` — standalone audio application
- `bare` — minimal CMakeLists.txt, no example code

### Layer C: Lifecycle CLI (`pulp`)

The existing `pulp` CLI, extended with onboarding-critical commands:

| Command | Purpose | Exists Today? |
|---------|---------|--------------|
| `pulp new <name>` | Create new project | Partial (`pulp create`) |
| `pulp init` | Initialize Pulp in existing directory | No |
| `pulp build` | Configure + build | Yes |
| `pulp test` | Run tests | Yes |
| `pulp run` | Launch standalone or load in test host | No |
| `pulp doctor` | Diagnose environment issues | No |
| `pulp install` | Install plugins to system folders | Partial |
| `pulp upgrade` | Self-update CLI | No |
| `pulp status` | Show project state | Yes |
| `pulp validate` | Validate plugin formats | Yes |
| `pulp ship` | Package for distribution | Yes |
| `pulp clean` | Remove build artifacts | Yes |

---

## 4. Dependency Strategy

### Platform-Gated Features

The entire toolchain is platform-aware. Features are shown/hidden/defaulted based on the detected OS:

| Feature | macOS | Windows | Linux |
|---------|-------|---------|-------|
| **Formats: VST3** | Default ON | Default ON | Default ON |
| **Formats: CLAP** | Default ON | Default ON | Default ON |
| **Formats: AU v2/v3** | Default ON | Hidden | Hidden |
| **Formats: LV2** | Hidden | Hidden | Default ON |
| **Formats: Standalone** | Default ON | Default ON | Default ON |
| **Formats: WAM/WebCLAP** | Opt-in | Opt-in | Opt-in |
| **Audio I/O: CoreAudio** | Auto | N/A | N/A |
| **Audio I/O: WASAPI** | N/A | Auto | N/A |
| **Audio I/O: ALSA** | N/A | N/A | Auto |
| **Audio I/O: JACK** | N/A | N/A | Opt-in |
| **Deps: Xcode CLT** | Required | N/A | N/A |
| **Deps: VS Build Tools** | N/A | Required | N/A |
| **Deps: ALSA dev headers** | N/A | N/A | Required |
| **GPU: Metal surface** | Auto | N/A | N/A |
| **GPU: Dawn/Vulkan** | N/A | Auto | Auto |
| **Signing: codesign** | Available | N/A | N/A |
| **Signing: signtool** | N/A | Available | N/A |
| **Packaging: DMG/PKG** | Available | N/A | N/A |
| **Packaging: NSIS** | N/A | Available | N/A |
| **Packaging: deb/tar.gz** | N/A | N/A | Available |

"Hidden" means not shown in interactive prompts (user can still opt in via `--format=au` on Linux if cross-compiling). "Opt-in" means shown but not selected by default. "Auto" means detected and configured without user input.

`pulp doctor` output is also platform-gated — it only checks for dependencies relevant to the current OS.

### Philosophy

1. **Detect first, install second, guide third** — never silently install system packages
2. **Prompt before any system modification** — even with `--yes`, log what was done
3. **Provide exact manual commands** — if automation fails, the user can copy-paste
4. **Dry run mode** — `pulp doctor --dry-run` shows what would be installed
5. **CI mode** — `pulp doctor --ci` exits with status codes, no prompts

### macOS

| Dependency | Detection | Auto-install? | Manual fallback |
|-----------|-----------|--------------|-----------------|
| Xcode CLT | `xcode-select -p` | Yes (`xcode-select --install`) | "Run: xcode-select --install" |
| CMake 3.24+ | `cmake --version` | Yes (via Homebrew if available) | "Install from cmake.org or `brew install cmake`" |
| git-lfs | `git lfs version` | Yes (via Homebrew if available) | "`brew install git-lfs && git lfs install`" |
| Homebrew | `brew --version` | No (suggest only) | "Visit brew.sh" |
| C++20 compiler | `clang++ --version` | Comes with Xcode CLT | "Install Xcode Command Line Tools" |

**git-lfs is critical** — Issue #1's Skia build failure was entirely due to missing git-lfs. The setup flow must handle this before any build attempt.

### Windows

| Dependency | Detection | Auto-install? | Manual fallback |
|-----------|-----------|--------------|-----------------|
| Visual Studio Build Tools | `vswhere.exe` or registry | Prompt + guide | "Download from visualstudio.microsoft.com" |
| CMake 3.24+ | `cmake --version` | Yes (via VS installer or winget) | "Install from cmake.org or `winget install cmake`" |
| git-lfs | `git lfs version` | Yes (via winget) | "`winget install git-lfs`" |
| git | `git --version` | Yes (via winget) | "`winget install git`" |

**Windows-specific:** The VS Build Tools installer is interactive and large. We detect it, link to the download, and specify which workloads to select ("Desktop development with C++").

### Linux

| Dependency | Detection | Auto-install? | Manual fallback |
|-----------|-----------|--------------|-----------------|
| C++20 compiler | `g++ --version` / `clang++ --version` | Prompt with distro command | "apt install g++-13" / "dnf install gcc-c++" |
| CMake 3.24+ | `cmake --version` | Prompt with distro command | Exact apt/dnf/pacman command |
| git-lfs | `git lfs version` | Prompt with distro command | Exact apt/dnf/pacman command |
| ALSA dev | `pkg-config --exists alsa` | Prompt with distro command | "apt install libasound2-dev" |
| JACK dev | `pkg-config --exists jack` | Optional, prompt | "apt install libjack-jackd2-dev" |

**Distro detection:** Read `/etc/os-release` to determine package manager (apt, dnf, pacman, zypper). Provide exact commands for the detected distro.

### Dependency Flow (all platforms)

```
pulp doctor
  │
  ├─ Check: C++20 compiler
  │   ├─ Found: clang 17.0.0 ✓
  │   └─ Missing: "Install Xcode CLT: xcode-select --install"
  │
  ├─ Check: CMake ≥ 3.24
  │   ├─ Found: cmake 3.28.1 ✓
  │   ├─ Old version: "Update cmake: brew upgrade cmake"
  │   └─ Missing: "Install cmake: brew install cmake"
  │
  ├─ Check: git-lfs
  │   ├─ Installed + initialized ✓
  │   ├─ Installed but not initialized: "Run: git lfs install"
  │   └─ Missing: "Install: brew install git-lfs && git lfs install"
  │
  ├─ Check: git-lfs files pulled (if in a Pulp repo)
  │   ├─ Files present ✓
  │   └─ LFS pointers only: "Run: git lfs pull"
  │
  ├─ Check: External SDKs (if in a Pulp repo)
  │   ├─ VST3 SDK present ✓
  │   └─ Missing: "Will be cloned during build"
  │
  └─ Summary:
      ✓ 4/5 checks passed
      ✗ 1 issue found — run `pulp doctor --fix` to resolve
```

`pulp doctor --fix` runs the auto-install commands (with confirmation prompt).

---

## 5. First-Time Experience — End to End

### Scenario: New developer, fresh macOS machine

```
# Step 1: Install Pulp CLI (~5 seconds)
$ curl -fsSL https://generouscorp.com/pulp/install.sh | sh

  Installing Pulp CLI v0.1.0 for macOS (arm64)...
  ✓ Downloaded pulp-0.1.0-darwin-arm64.tar.gz
  ✓ Installed to ~/.pulp/bin/pulp
  ✓ Added ~/.pulp/bin to PATH

  Run `pulp new my-plugin` to create your first plugin.

# Step 2: Create a project (~30-90 seconds including first build)
$ pulp new my-first-plugin

  Checking environment...
  ✓ clang 17.0.0
  ✓ cmake 3.28.1
  ✗ git-lfs not found

  git-lfs is required for GPU rendering assets.
  Install now? [Y/n] y
  → brew install git-lfs && git lfs install
  ✓ git-lfs 3.5.1

  What are you building?
  › Audio effect plugin

  Which formats? [all platform-supported formats selected by default]
  ›  ✓ VST3  ✓ CLAP  ✓ AU  ○ LV2  ✓ Standalone
  (AU auto-selected on macOS; LV2 auto-selected on Linux)

  Plugin name [My First Plugin]:
  Manufacturer [My Company]:

  Creating my-first-plugin/...
  ✓ Project created
  ⠼ Building... (fetching dependencies on first build)
  ✓ Build complete (47s)
  ✓ Tests pass (3/3)

  Done! Your plugin is ready:
    VST3:  my-first-plugin/build/VST3/MyFirstPlugin.vst3
    CLAP:  my-first-plugin/build/CLAP/MyFirstPlugin.clap

  Next steps:
    cd my-first-plugin
    pulp run              # hear it in the standalone host
    pulp install          # install to ~/Library/Audio/Plug-Ins/

# Step 3: Hear something (~5 seconds)
$ cd my-first-plugin && pulp run

  Launching MyFirstPlugin standalone...
  ✓ Audio output: default device (MacBook Pro Speakers)
  ✓ Plugin loaded — adjust the Gain knob to hear the effect

  Press Ctrl+C to stop.
```

### Success Criteria

| Metric | Target |
|--------|--------|
| Time from `curl` to working CLI | < 10 seconds |
| Time from `pulp new` to successful build | < 2 minutes (first time, including dep fetch) |
| Time from `pulp new` to hearing audio | < 3 minutes |
| Commands required | 3 (`curl`, `pulp new`, `pulp run`) |
| Manual dependency resolution | 0 on macOS with Homebrew; 1-2 prompts otherwise |

### Failure Recovery

Every failure must produce:
1. **What failed** — one sentence
2. **Why** — technical detail (collapsible/verbose)
3. **How to fix** — exact command to run
4. **Escape hatch** — link to troubleshooting docs

Example:
```
✗ Build failed: CMake could not find a C++20 compiler

  Your compiler (Apple Clang 14.0) does not support all required C++20 features.
  Pulp requires Clang 15+ or GCC 13+.

  Fix: Update Xcode Command Line Tools:
    softwareupdate --install -a

  Or install a newer Clang via Homebrew:
    brew install llvm

  Docs: https://generouscorp.com/pulp/troubleshooting#compiler
```

---

## 6. UX Principles

1. **Zero-confusion entrypoint** — One URL (generouscorp.com/pulp), one command per platform. No choosing between npm/cargo/brew.

2. **Deterministic behavior** — Same inputs → same outputs. No "works on my machine." Lockfiles for dependencies, pinned SDK versions.

3. **Clear progress** — Show what's happening at each step. Use spinners for long operations. Show elapsed time. Never hang silently.

4. **Actionable errors** — Every error includes a fix command. Never just "Error: failed." Always "Error: X failed because Y. Fix: run Z."

5. **Idempotent commands** — Running `pulp new` twice doesn't break. Running `pulp doctor --fix` twice is safe. Running `pulp build` when already built is a no-op.

6. **CI/headless mode** — Every command supports `--no-interactive` and `--ci`. No TTY required. JSON output available (`--json`).

7. **Respect the developer's system** — Never install to system directories without asking. Never modify shell profiles without confirmation. Provide `--no-modify-path` for manual setup.

---

## 7. Phased Rollout Plan

### Phase F1: Immediate Fixes (unblock Issue #1)
**Scope:** Fix the repo so `git clone && ./setup.sh` works for a new developer today.

- [ ] Remove broken symlinks from `external/` (vst3sdk, AudioUnitSDK)
- [ ] Fix `examples/CMakeLists.txt` — guard `design-tool` with `if(EXISTS ...)`
- [ ] Move `tools/scripts/setup.sh` → `setup.sh` (repo root, discoverable)
- [ ] Add git-lfs setup to `setup.sh` (`git lfs install && git lfs pull`)
- [ ] Add AudioUnitSDK clone to `setup.sh` (like VST3 SDK)
- [ ] Update README.md with prerequisites and setup instructions
- [ ] Update README.md "Building" section to reference `setup.sh`

**Success criteria:** A new developer can `git clone`, run `./setup.sh`, and get a successful build + test run. Both `setup.sh` (one-shot bootstrap) and `pulp doctor --fix` (ongoing diagnosis) coexist — setup.sh is the "just make it work" path, doctor is the "what's wrong and how do I fix it" path.
**Risks:** None — these are bug fixes.
**Dependencies:** None.

### Phase F2: `pulp doctor`
**Scope:** Add environment diagnosis to the pulp CLI.

- [ ] Implement `pulp doctor` command in `tools/cli/pulp_cli.cpp`
- [ ] Detect: C++20 compiler, CMake version, git-lfs, platform SDKs
- [ ] Detect: git-lfs file state (pointers vs actual files)
- [ ] Detect: external SDK presence (VST3, AudioUnit, Skia)
- [ ] `--fix` flag: auto-resolve with user confirmation
- [ ] `--ci` flag: exit codes only, no prompts
- [ ] `--dry-run` flag: show what would be done

**Success criteria:** `pulp doctor` correctly identifies all Issue #1 problems and provides fix commands.
**Risks:** Platform-specific detection may have edge cases. Start with macOS, iterate.
**Dependencies:** Phase F1 (repo must be in fixable state).

### Phase F3: Project Scaffolding (`pulp new`)
**Scope:** Generate working plugin projects from templates.

- [ ] Extend `pulp create` into `pulp new` with interactive prompts
- [ ] Ship 3 templates: effect, instrument, app
- [ ] Templates include: CMakeLists.txt, source, test, README
- [ ] `pulp new` runs `pulp doctor --quick` before scaffolding
- [ ] `pulp new` runs build + test after scaffolding
- [ ] Non-interactive mode for CI

**Success criteria:** `pulp new my-plugin` produces a project that builds and tests in one command.
**Risks:** Template maintenance — templates must stay in sync with framework API.
**Dependencies:** Phase F2 (doctor check before scaffold).

### Phase F4: Installer (`pulpup`)
**Scope:** One-liner install of the Pulp CLI binary.

- [ ] Pre-built CLI binaries in GitHub Releases (macOS arm64/x86_64, Linux x86_64, Windows x86_64)
- [ ] `install.sh` for macOS/Linux
- [ ] `install.ps1` for Windows
- [ ] Host scripts at generouscorp.com/pulp/
- [ ] Self-update: `pulp upgrade`
- [ ] Version management: install, switch, list

**Success criteria:** `curl ... | sh` installs a working `pulp` binary with no prerequisites beyond curl and a shell.
**Risks:** Pre-built binaries require CI release pipeline. Binary size management (strip symbols, static link).
**Dependencies:** Phase F3 (CLI must have `new` and `doctor` before distributing).

### Phase F5: Cross-Platform Hardening
**Scope:** Validate install + onboarding on all platforms.

- [ ] Test on: macOS (arm64, x86_64), Windows 10/11, Ubuntu 22/24, Fedora 39+, Arch
- [ ] Windows: VS Build Tools detection, winget integration
- [ ] Linux: distro-specific package commands (apt, dnf, pacman)
- [ ] CI: test install flow in GitHub Actions matrix
- [ ] Edge cases: no Homebrew, no admin rights, corporate proxies

**Success criteria:** Full install → build → run flow works on all target platforms.
**Risks:** Windows VS Build Tools detection is notoriously unreliable. Linux distro fragmentation.
**Dependencies:** Phase F4.

### Phase F6: Polish
**Scope:** UX refinement, documentation, messaging.

- [ ] Progress indicators (spinners, elapsed time, ETA)
- [ ] Color output (with `--no-color` flag)
- [ ] Comprehensive error messages with fix commands
- [ ] Troubleshooting guide on docs site
- [ ] Update all docs to reference new install flow
- [ ] Record terminal demo (asciinema/GIF) for README
- [ ] `pulp run` command — launch standalone host with built plugin

**Success criteria:** First-time user rates experience as "easy" in feedback. Zero documentation dead-ends.
**Risks:** Polish is unbounded — timebox to specific improvements.
**Dependencies:** Phase F5.

---

## 8. Immediate vs. Pre-built CLI: Sequencing Decision

There's a chicken-and-egg problem: the `pulp` CLI is currently a C++ binary built by CMake. You need a working build environment to get the CLI, but the CLI is what should help you set up the build environment.

**Short-term (Phases F1-F3):** `setup.sh` is the entrypoint. It bootstraps deps, builds the project (including the CLI), and the CLI becomes available after first build. This works today with minimal changes.

**Medium-term (Phase F4):** Pre-built CLI binaries distributed via GitHub Releases. The CLI becomes a standalone download that doesn't require building the framework first. `pulpup` installs these pre-built binaries.

This sequencing means:
- **Phase F1 immediately unblocks Issue #1** with zero infrastructure work
- **Phases F2-F3 improve the experience** within the existing build-first model
- **Phase F4 achieves the vision** of a one-liner install with pre-built binaries

---

## 9. Documentation & README Updates

As part of each phase, documentation must be updated in lockstep:

### Phase F1 (README rewrite)
- Prerequisites section: CMake, C++20 compiler, git-lfs
- Quick start: `git clone ... && ./setup.sh`
- Build artifacts: where to find VST3/CLAP/AU/Standalone output
- Link to full Getting Started guide

### Phase F2-F3 (Getting Started guide update)
- Replace cmake-centric instructions with `pulp` CLI commands
- Add `pulp doctor` as first step for troubleshooting
- Add `pulp new` walkthrough with screenshots/terminal output

### Phase F4+ (New install page)
- generouscorp.com/pulp/ landing page with install one-liner
- Platform-specific tabs (macOS / Windows / Linux)
- "30 seconds to your first plugin" positioning

---

## 10. Issue #1 Response Draft

```markdown
Thanks for this detailed writeup — exactly the kind of feedback that makes the difference.

### What we're fixing

**Immediate (this week):**
- Broken symlinks in `external/` → replaced with proper git clone in setup script
- Missing `examples/design-tool/` reference → guarded in CMakeLists.txt
- git-lfs not in setup flow → `setup.sh` now runs `git lfs install && git lfs pull`
- `setup.sh` moved to repo root for discoverability
- README rewritten with prerequisites and working build instructions

**Near-term:**
- `pulp doctor` command — detects missing deps, suggests exact fix commands
- `pulp new` command — scaffolds a working plugin project end-to-end
- Better build artifact reporting (where did my .vst3 end up?)

**Planned:**
- One-liner install: `curl -fsSL https://generouscorp.com/pulp/install.sh | sh`
- Pre-built CLI binary (no build-the-framework-first chicken-and-egg)
- Interactive project creation with template selection

### Design principles we're adopting
- Detect → prompt → install (never silently modify the system)
- Every error includes a fix command
- `pulp doctor` as the universal "something's wrong" escape hatch
- First success in < 3 minutes (install CLI → create project → hear audio)

Full proposal: planning/pulp-install-onboarding.md on the `feature/first-time-setup` branch.

**Decisions made based on initial feedback:**
1. **Default formats: all platform-supported** — on macOS you get VST3 + CLAP + AU + Standalone by default. On Linux, VST3 + CLAP + LV2 + Standalone. No reason to make people opt in to formats their platform supports.
2. **Both `setup.sh` and `pulp doctor --fix`** — `setup.sh` at repo root for "just make it work" first-time bootstrap; `pulp doctor --fix` for ongoing diagnosis and repair. Different tools for different moments.
3. **Platform-gated throughout** — formats, dependencies, doctor checks, and interactive prompts are all filtered by detected OS. Nothing shown that doesn't apply to your platform.

Any other friction points from your session?
```

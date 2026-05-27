# Real-plugin integration tests — developer-supplied lane

Pulp's host stack ships an opt-in real-plugin integration suite
(`test/integration/real_plugin_runner.cpp`, gated behind the CMake
option `PULP_REAL_PLUGIN_TESTS=ON`). The suite drives Surge XT, Vital,
OB-Xd, and Dexed end-to-end through `PluginScanner` → `PluginSlot::load`
→ `prepare` → `process` → state round-trip → `release`.

The suite has two lanes for getting plugin binaries onto disk.

## Lane 1 — Pinned download (CI-friendly, used by default)

For plugins whose binaries can be fetched by an unauthenticated HTTP
client, the sha256 of the canonical release archive is pinned in
`test/integration/real_plugins.toml`. The downloader fetches the archive,
verifies the sha256, refuses to unpack on mismatch, and lands the bundle
at `~/.cache/pulp/real-plugins/<id>/<bundle>`.

```bash
python3 tools/scripts/fetch_real_plugins.py
```

Today this lane covers **Surge XT 1.3.4** (all three OSes) and
**Dexed 0.9.9** (Linux ZIP + macOS DMG verified — DMG ships a `.pkg`
installer the runner can't unpack unattended, so macOS Dexed still needs
the developer-supplied lane below).

## Lane 2 — Developer-supplied (auth/EULA-gated plugins)

**Vital 1.5.5** and **OB-Xd 3.4** both require the developer to either
sign in (Vital's `account.vital.audio` returns HTTP 500 to anonymous
clients) or click through an HTML EULA landing page (`discodsp.com`
returns a 1.7 KB EULA page in place of the archive). Neither can be
fetched by a plain downloader, and neither can be redistributed with
Pulp's repo.

The runner accepts these plugins from a developer-managed cache
directory instead. Drop the bundle in, set one environment variable, and
the runner picks it up — no sha256 verification, but the bundle path,
shape, and format are all validated before the plugin is loaded.

### Quickstart

1. **Install the plugin from the vendor** (the normal way — accept the
   EULA, run the installer, etc.). This is a one-time per developer.

2. **Point `PULP_REAL_PLUGIN_CACHE` at a directory under your control**:

   ```bash
   export PULP_REAL_PLUGIN_CACHE="$HOME/Pulp/real-plugins"
   mkdir -p "$PULP_REAL_PLUGIN_CACHE"
   ```

3. **Drop the bundles in** at the relative paths the manifest expects.
   The id and `bundle_relpath` come from `test/integration/real_plugins.toml`:

   ```text
   $PULP_REAL_PLUGIN_CACHE/
     vital/Vital.vst3        # copy or symlink from /Library/Audio/Plug-Ins/VST3/Vital.vst3
     obxd/OB-Xd.vst3         # copy or symlink from /Library/Audio/Plug-Ins/VST3/OB-Xd.vst3
   ```

   On macOS, symlinks against the system install work fine:

   ```bash
   ln -s "/Library/Audio/Plug-Ins/VST3/Vital.vst3" \
         "$PULP_REAL_PLUGIN_CACHE/vital/Vital.vst3"
   ln -s "/Library/Audio/Plug-Ins/VST3/OB-Xd.vst3" \
         "$PULP_REAL_PLUGIN_CACHE/obxd/OB-Xd.vst3"
   ```

4. **Validate the cache** (no network, no plugin loading — just checks
   that every TBD entry's bundle is in place with a sane shape):

   ```bash
   python3 tools/scripts/fetch_real_plugins.py --validate-cache
   ```

   Expected output for a correctly populated cache:

   ```text
   [surge-xt] already present at /…/surge-xt/Surge XT.clap
   [vital] developer-supplied OK (directory bundle) → /…/vital/Vital.vst3
   [obxd] developer-supplied OK (directory bundle) → /…/obxd/OB-Xd.vst3
   [dexed] developer-supplied OK (directory bundle) → /…/dexed/Dexed.vst3
   ```

5. **Configure + build the runner**:

   ```bash
   cmake -S . -B build -DPULP_REAL_PLUGIN_TESTS=ON
   cmake --build build --target pulp-test-real-plugins
   ```

6. **Run the suite** (Catch2 — pass `--reporter console` for live
   per-test output):

   ```bash
   PULP_REAL_PLUGIN_CACHE="$HOME/Pulp/real-plugins" \
       ./build/test/pulp-test-real-plugins
   ```

   For each developer-supplied entry the runner prints a `WARN` line
   that names the lane:

   ```text
   WARN: running Vital from developer-supplied bundle (no sha256 verification)
   ```

   Pinned-download entries do not emit that line.

### Safety guards

The developer-supplied lane is deliberately conservative — it never
silently accepts a stale or fake bundle:

* **The hash gate is bypassed only when `PULP_REAL_PLUGIN_CACHE` is set
  in the environment.** A bundle left over at `~/.cache/pulp/real-plugins/`
  from an earlier pinned run does NOT activate the developer-supplied
  lane.
* **The bundle must exist** at the manifest's `bundle_relpath`. A typo
  in the directory name surfaces as a clear skip reason.
* **Empty placeholders are rejected** by `--validate-cache`: a zero-byte
  file or empty directory fails the shape check before the runner ever
  tries to load it.
* **The system plugin folder is never written.** The cache is always
  the developer-managed directory under `PULP_REAL_PLUGIN_CACHE`.

### Adding a new auth-gated plugin

When a new plugin needs the developer-supplied lane:

1. Add an entry to `test/integration/real_plugins.toml` with `sha256 = "TBD"`
   on the platforms where the binary can't be pinned. Keep `bundle_relpath`
   pointing at the canonical bundle name (`Plugin.vst3` / `Plugin.component`
   / `Plugin.clap`).
2. Add a `TEST_CASE("real-plugin: <Name>", …)` block in
   `real_plugin_runner.cpp` that calls `run_plugin_case("<id>")` — the
   resolver handles the lane selection.
3. Update this doc's quickstart with the symlink command for the new id.

### How the resolver decides

The full logic is in
`test/integration/real_plugin_fixture.hpp::resolve_fixture()`. The
decision tree:

```text
sha256 is pinned (non-"TBD") + bundle on disk      → PinnedDownload   (run)
sha256 is pinned (non-"TBD") + bundle missing      → Missing          (skip — "run fetch_real_plugins.py")
sha256 is "TBD" + PULP_REAL_PLUGIN_CACHE unset      → Missing          (skip — "see this doc")
sha256 is "TBD" + override set + bundle on disk    → DeveloperSupplied (run with WARN)
sha256 is "TBD" + override set + bundle missing    → Missing          (skip — "expected at …")
```

The resolver is unit-tested by `pulp-test-real-plugin-runner-cache`,
which exercises all five branches against a synthetic cache directory.

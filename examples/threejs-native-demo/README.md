# Pulp Native Three.js Demo

This example runs the real MIT `three.webgpu.js` renderer inside Pulp's native
JS engine on top of the Dawn-backed WebGPU bridge. It does not use a browser or
WebView.

Current demo modes:

- `--demo spectrum` (default): hybrid native shell with a live audio-reactive
  peak bar and FFT-driven spectrum field fed by Pulp's `VisualizationBridge`
- `--demo particles`: audio-reactive particle cloud driven by beat + RMS + FFT
  data from the same native bridge
- `--demo ribbon`: streaming 3D waveform ribbon driven by beat + RMS + FFT
  data from the same native bridge
- `--demo reverb`: room reverb scene with a native room shell, reflection
  lines, and a diffuse late-reverb cloud driven by room parameters + waveform
  data from the same native bridge
- `--demo cube`: the earlier animated green cube smoke scene
- both modes use the real `THREE.WebGPURenderer` on the Dawn-backed native
  canvas path
- pointer drag, wheel zoom, and trackpad pinch are routed through native input
  events
- real `OrbitControls` addon import and initialization works on the native
  canvas path
- native GPU window presentation goes through Pulp's `WindowHost`
- redraw is driven by Pulp's host `FrameClock` + `requestAnimationFrame`
- buffered GPU bridge uses structured native payloads instead of JSON text
  round-trips on the hot render path
- clean macOS quit path avoids the earlier V8 shutdown crash
- capture mode now primes several real frame callbacks before screenshotting so
  GPU-backed demos are captured after their first truthful present, not at an
  empty warm-up frame

## Build

```bash
cmake -S . -B build-phase13-v8 -DCMAKE_BUILD_TYPE=Debug
cmake --build build-phase13-v8 --target pulp-threejs-native-demo -j8
```

## Run

```bash
./build-phase13-v8/examples/threejs-native-demo/pulp-threejs-native-demo
# same as: --demo spectrum
# try particles: --demo particles
# try ribbon: --demo ribbon
# try reverb: --demo reverb
```

Explicit cube smoke mode:

```bash
./build-phase13-v8/examples/threejs-native-demo/pulp-threejs-native-demo --demo cube
```

Optional window size:

```bash
./build-phase13-v8/examples/threejs-native-demo/pulp-threejs-native-demo --size 768x768
```

Optional one-shot capture:

```bash
./build-phase13-v8/examples/threejs-native-demo/pulp-threejs-native-demo --demo spectrum --capture /tmp/threejs-demo.png
```

## Why This Example Exists

This is the first visible native Three.js proof for the original Phase 13
bridge plan:

- real `THREE.WebGPURenderer`
- native `GPUCanvasContext`
- shared Dawn device between the bridge and the visible GPU host
- hybrid 2D+3D composition on the native widget/layout path
- four real audio-reactive Phase 13 examples via `VisualizationBridge`
  (spectrum + particle visualizer + waveform ribbon + room reverb)
- live redraw through the host frame loop (not just a one-shot frame)
- native pointer/input events reaching the Three.js canvas path
- trackpad pinch and wheel zoom support on the native host path
- no browser-hosted fallback

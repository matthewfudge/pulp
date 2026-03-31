"Implement Phase 8: audio visualization system for the v3 track.

References:
- planning/dsl-agentic-validation-webgpu-audit-v3.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase1-contract.md
- core/view/include/pulp/view/audio_bridge.hpp
- core/view/include/pulp/view/widgets.hpp
- core/view/src/widgets.cpp
- core/signal/include/pulp/signal/fft.hpp
- core/runtime/ (TripleBuffer, SpscQueue)

GOAL:
Build a production-quality audio visualization system: STFT processing abstractions, multi-channel metering, spectrogram rendering, and the lock-free infrastructure to get audio data to the UI thread in the fastest, most memory-efficient way possible.

DEPENDENCIES:
- Requires Phase 1 contract/truth pass

CAN RUN IN PARALLEL:
- yes, after Phase 1
- can run in parallel with Phases 2, 3, 6, 9, 10, 11

NON-NEGOTIABLES:
- All audio→UI data transfer must be lock-free. Use TripleBuffer for latest-value publication and SpscQueue for ordered streams. No mutexes on the audio thread.
- STFT must be configurable: window function, FFT size, hop size, overlap.
- Metering must support multi-channel (stereo, surround, ambisonic channel counts).
- Visualization widgets must work with the existing theme token system.
- Performance: visualization processing must not cause audio dropouts. Budget is <1ms per UI frame for visualization data preparation on the UI thread.

DELIVERABLES:
1. STFT abstraction:
   - configurable window function (Hann, Hamming, Blackman, Kaiser, flat-top)
   - configurable FFT size (256–8192)
   - configurable hop size / overlap
   - magnitude and phase output
   - audio-thread-safe: accumulate samples, emit completed frames via TripleBuffer or SpscQueue
2. Spectrogram pipeline:
   - STFT → magnitude → dB scaling → color mapping
   - scrolling spectrogram display (time vs frequency vs magnitude)
   - configurable color ramps (inferno, viridis, grayscale, custom)
   - frequency axis scaling (linear, logarithmic, mel)
3. Multi-channel metering:
   - support for arbitrary channel counts (mono through ambisonic)
   - per-channel peak, RMS, LUFS-integrated, LUFS-momentary
   - configurable ballistics (attack/release time constants)
   - clip indicators with hold time
   - correlation meter (stereo)
4. Enhanced visualization widgets:
   - SpectrogramView: scrolling time-frequency display
   - MultiMeter: multi-channel level meter with configurable layout (horizontal/vertical, stacked/side-by-side)
   - CorrelationMeter: stereo correlation display
   - Upgrade existing SpectrumView to use the new STFT abstraction
   - Upgrade existing WaveformView to support multi-channel display
5. Lock-free data flow:
   - document the audio→UI publication paths clearly
   - provide a VisualizationBridge that manages STFT + metering publication
   - single configuration point: set FFT size, channel count, metering mode
6. FORWARD COMPATIBILITY FOR PHASE 13 (Three.js bridge):
   Phase 13's audio plugin examples (3D spectrum analyzer, reactive particle visualizer)
   consume FFT/metering data from AudioBridge via TripleBuffer. Ensure the visualization
   data publication API is accessible from JS — the same AudioBridge data that drives
   SpectrumView/WaveformView must be readable by Three.js scene update code in JS.
   This is already natural if AudioBridge data flows through the JS bridge, but verify
   the data path is low-latency enough for 60fps 3D rendering.
7. Tests:
   - STFT output matches known-good reference for test signals (sine, impulse)
   - metering values match expected levels for calibration signals
   - lock-free publication: stress test with simulated audio thread at 44.1k/48k/96k
   - visualization widgets render correctly (headless screenshot tests)
   - performance benchmark: measure UI-thread time for visualization data processing

ACCEPTANCE:
- STFT abstraction is real, configurable, and test-backed
- spectrogram rendering works with multiple color ramps and axis scales
- multi-channel metering supports at least 2/4/6/8 channels
- all audio→UI paths are lock-free
- visualization does not cause audio dropouts under stress
- docs describe the visualization system at the implemented scope


Use repo prompt when searching the codebase

ONLY WHEN ALL CONDITIONS ARE MET:
Output exactly: <promise>DONE</promise>

IF STUCK:
- After 20 iterations, document in LEARNINGS.md:
- What is blocked
- Why
- What was attempted
- What assumption may be wrong" --completion-promise "DONE" --max-iterations 120
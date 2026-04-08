# Module Reference

Pulp is organized into independent subsystems under `core/`. Each is a separate CMake library (`pulp::runtime`, `pulp::audio`, etc.) that you link as needed.

```cmake
# Link what you need
target_link_libraries(my_plugin PRIVATE pulp::format pulp::signal pulp::view)
```

---

## runtime

> Core utilities — the foundation everything else builds on.

**Link:** `pulp::runtime` · **Include prefix:** `<pulp/runtime/...>`

### SIMD — Portable vectorized math

Hardware-accelerated math via Google Highway. Dispatches to the best instruction set at runtime (SSE2, AVX2, NEON). Use for inner-loop DSP where every cycle counts.

```cpp
#include <pulp/runtime/simd.hpp>
using namespace pulp::runtime;

float a[256], b[256], result[256];
simd_add(a, b, result, 256);        // result[i] = a[i] + b[i]
simd_scale(a, 0.5f, result, 256);   // result[i] = a[i] * 0.5
float peak = simd_reduce_max(a, 256);
```

### XML — Parse and generate XML

Wraps pugixml (MIT). Parse from string or file, query with XPath, generate documents.

```cpp
#include <pulp/runtime/xml.hpp>

XmlDocument doc;
doc.parse(R"(<plugin name="MyPlugin"><param>Gain</param></plugin>)");
auto name = doc.root_attribute("name");   // "MyPlugin"
auto params = doc.xpath_strings("//param"); // ["Gain"]
doc.save_file("settings.xml");
```

### HTTP — Network requests

GET, POST, and file download via cpp-httplib (MIT). Use for license checks, cloud presets, update notifications.

```cpp
#include <pulp/runtime/http.hpp>

auto response = http_get("https://api.example.com/presets");
if (response.ok())
    process_presets(response.body);

http_download("https://example.com/ir.wav", "/tmp/impulse.wav");
```

### Crypto — Hashing and encryption

SHA-256, MD5, AES-256-CBC via Mbed TLS (Apache 2.0). Use for license validation, integrity checks, secure preset storage.

```cpp
#include <pulp/runtime/crypto.hpp>

auto hash = sha256_hex("my data");              // 64-char hex string
auto id = machine_id();                          // Deterministic hardware fingerprint

auto encrypted = aes_encrypt(data, size, key32, iv16);
auto decrypted = aes_decrypt(encrypted->data(), encrypted->size(), key32, iv16);
```

### Licensing — Plugin copy protection

RSA-signed license keys with online activation. The public key lives in your plugin; the private key stays on your server.

```cpp
#include <pulp/runtime/license.hpp>

LicenseValidator validator;
validator.set_public_key(my_rsa_public_key_pem);

auto status = validator.validate(license_key_string);
if (status == LicenseStatus::Valid) { /* unlocked */ }
if (status == LicenseStatus::Expired) { /* show renewal dialog */ }

auto info = validator.validate_and_parse(key);
// info->product_id, info->user_email, info->edition
```

### i18n — String translation

Load translations from `.strings` (Apple), `.po` (gettext), or `.json` files. Positional argument substitution with `{0}`, `{1}`.

```cpp
#include <pulp/runtime/i18n.hpp>

LocalisedStrings::instance().load_json_file("lang/de.json");
auto text = tr("hello_user", {{"Max"}});  // "Hallo, Max!"
auto locale = LocalisedStrings::system_locale();  // "de"
```

### Expression — Math evaluator

Parse and evaluate math expressions at runtime. Supports variables, 15+ functions, constants (pi, e). Use for user-defined formulas, automation curves.

```cpp
#include <pulp/runtime/expression.hpp>

auto result = evaluate("sin(pi / 2)");           // 1.0
auto freq = evaluate("base * 2^(note/12)", {{"base", 440.0}, {"note", 7.0}});

ExpressionEvaluator eval;
eval.set("x", 0.5);
eval.evaluate("x * 100 + 10");  // 60.0
```

### Other runtime utilities

| Feature | Header | Description |
|---------|--------|-------------|
| ZIP/GZIP | `zip.hpp` | Compress/decompress data and archives (miniz) |
| Sockets | `socket.hpp` | TCP/UDP client and server for networked audio |
| Named Pipes | `named_pipe.hpp` | Cross-platform IPC (mkfifo / CreateNamedPipe) |
| Base64 | `base64.hpp` | `base64_encode(data)` / `base64_decode(text)` |
| Memory Map | `memory_mapped_file.hpp` | Zero-copy large file access via mmap |
| Child Process | `child_process.hpp` | `run_process("/usr/bin/auval", {"-a"})` with stdout capture |
| Dynamic Library | `dynamic_library.hpp` | `lib.open("plugin.dylib"); lib.find_symbol("entry")` |
| IPC Lock | `inter_process_lock.hpp` | Cross-process mutex via file locks |
| Temp File | `temporary_file.hpp` | Auto-deleting temp file — `TemporaryFile tmp(".wav")` |
| Timer | `high_resolution_timer.hpp` | Sub-millisecond periodic callback on a dedicated thread |
| System Info | `system.hpp` | CPU model, core count, RAM, OS, SIMD features (runtime detected) |
| BigInteger | `big_integer.hpp` | Arbitrary-precision math for RSA — `a.mod_pow(exp, modulus)` |
| Analytics | `analytics.hpp` | Thread-safe `Analytics::instance().log("preset_load", {{"name", "Init"}})` |
| Range | `range.hpp` | `Range<float>(0, 1).contains(0.5)`, intersection, union |
| Primes | `primes.hpp` | `is_prime(97)`, `generate_prime(32)`, `sieve_primes(1000)` |
| Scope Guard | `scope_guard.hpp` | `PULP_ON_SCOPE_EXIT(file.close())` |
| Identity | `identity.hpp` | `Uuid::generate()`, typed SessionId/ObjectId/RunId |
| Text Diff | `text_diff.hpp` | Line-by-line diff with formatted +/- output |
| IP Address | `ip_address.hpp` | `local_ipv4_address()`, `hostname()`, `is_valid_ipv4(addr)` |

---

## events

> Event loop, timers, IPC, and process management.

**Link:** `pulp::events` · **Include prefix:** `<pulp/events/...>`

### IPC — Inter-process communication

Length-prefixed messages over named pipes or TCP sockets. Use for crash-isolated plugin scanning, multi-process architectures, standalone↔plugin communication.

```cpp
#include <pulp/events/interprocess_connection.hpp>

// Server side
InterprocessConnection server;
server.on_text_message = [](std::string_view msg) { handle(msg); };
server.create_server("my_pipe", IpcTransport::NamedPipe);

// Client side
InterprocessConnection client;
client.connect("my_pipe", IpcTransport::NamedPipe);
client.send_message("scan_plugin:/path/to/plugin.vst3");
```

### Child Process Pool — Crash-isolated workers

Launch child processes with IPC channels. If a child crashes (e.g., while loading a broken plugin), the host survives.

```cpp
#include <pulp/events/child_process_manager.hpp>

ChildProcessManager pool;
auto* worker = pool.launch("/usr/bin/my-scanner", {"--plugin", path});
worker->send_message("scan");
worker->on_message = [](std::string_view result) { update_list(result); };
```

### Other event utilities

| Feature | Header | Description |
|---------|--------|-------------|
| Event Loop | `event_loop.hpp` | `EventLoop loop; loop.post([]{...}); loop.run()` |
| Timer | `timer.hpp` | `Timer t; t.start(100ms, []{...})` — periodic or one-shot |
| Async Updater | `async_updater.hpp` | Coalesce rapid cross-thread triggers into one callback |
| Action Broadcaster | `async_updater.hpp` | `broadcaster.send_action("file_open")` to all listeners |
| Volume Detector | `volume_detector.hpp` | Poll for USB drive mount/unmount events |
| Service Discovery | `volume_detector.hpp` | mDNS/Bonjour browsing for networked audio devices |

---

## audio

> Device I/O, file formats, channel layouts, and offline processing.

**Link:** `pulp::audio` · **Include prefix:** `<pulp/audio/...>`

### Reading and writing audio files

The `FormatRegistry` handles codec dispatch. Pass a file path — it picks the right decoder from the extension.

```cpp
#include <pulp/audio/format_registry.hpp>

// Read any supported format
auto data = FormatRegistry::instance().read("drums.flac");
// data->channels[0] = left channel floats, data->sample_rate = 44100

// Write to WAV
FormatRegistry::instance().write("output.wav", *data);

// Read just the metadata (no audio decode)
auto info = FormatRegistry::instance().read_info("song.mp3");
// info->duration_seconds, info->num_channels, info->sample_rate
```

### Streaming write — large files without loading into memory

```cpp
#include <pulp/audio/streaming_writer.hpp>

StreamingWriter writer;
writer.open("recording.wav", 48000, 2, 24);  // 48kHz stereo 24-bit

while (recording) {
    writer.write_frames(buffer, num_frames);  // Write in chunks
}
writer.close();  // Finalizes WAV header
```

### Audio device I/O

```cpp
#include <pulp/audio/device.hpp>

auto system = create_audio_system();  // Platform-appropriate backend
auto devices = system->enumerate_devices();

DeviceConfig config;
config.sample_rate = 48000;
config.buffer_size = 256;

auto device = system->create_device(devices[0].id);
device->open(config);
device->start([](const auto& input, auto& output, const auto& ctx) {
    // Real-time audio callback — no allocation, no locks
    process(input, output, ctx.buffer_size);
});
```

**Device backends:** CoreAudio (macOS), WASAPI (Windows), ALSA + JACK (Linux), Web Audio (browser)

### Audio file format support

| Format | Read | Write | Backend |
|--------|:----:|:-----:|---------|
| WAV | ✓ | ✓ | CHOC + StreamingWriter |
| FLAC | ✓ | ✓* | dr_flac / libflac (`pulp add libflac`) |
| MP3 | ✓ | ✓* | dr_mp3 / LAME (`pulp add lame --accept-license LGPL-2.0`) |
| OGG Vorbis | ✓ | — | stb_vorbis |
| AIFF / AIFF-C | ✓ | ✓ | Native (8/16/24/32-bit big-endian) |
| AAC | ✓ | ✓* | ExtAudioFile (macOS) / FDK AAC (`pulp add fdk-aac --accept-license FDK-AAC`) |
| ALAC | ✓ | ✓* | ExtAudioFile (macOS) / Apple ALAC (`pulp add alac`) |

*\*Write via optional `pulp add` packages. Permissive (libflac, ALAC) install freely. Copyleft (LAME, fdk-aac) require `--accept-license`.*

### Other audio features

| Feature | Header | Description |
|---------|--------|-------------|
| Channel Sets | `channel_set.hpp` | `ChannelSet::surround_5_1()`, mono through 7.1.4 Atmos |
| Offline Processor | `offline_processor.hpp` | `offline_process(input, callback, 512)` — batch render |
| Buffering Reader | `buffering_reader.hpp` | Ring buffer with background read thread for streaming |
| System Volume | `system_volume.hpp` | `get_system_volume()` / `set_system_volume(0.8f)` |
| Memory-Mapped Reader | `mmap_reader.hpp` | Zero-copy access for large sample libraries |
| Subsection Reader | `subsection_reader.hpp` | Read frame range without copying — `reader.sample(ch, frame)` |
| Load Measurer | `load_measurer.hpp` | Track CPU usage of your audio callback |

---

## midi

> MIDI I/O, file handling, and MIDI 2.0 support.

**Link:** `pulp::midi` · **Include prefix:** `<pulp/midi/...>`

### MIDI message sequence — editing and offline processing

```cpp
#include <pulp/midi/midi_message_sequence.hpp>

MidiMessageSequence seq;
seq.add_note_on(0.0, 0, 60, 100);   // C4 at time 0
seq.add_note_off(0.5, 0, 60);       // Release after 0.5s
seq.add_cc(0.25, 0, 1, 64);         // Modulation at 0.25s

auto events = seq.events_in_range(0.0, 1.0);  // All events in first second
auto off = seq.find_note_off(0);               // Find matching note-off
```

### MIDI CI — Device discovery and profiles

```cpp
#include <pulp/midi/midi_ci.hpp>

CiDiscovery ci;
ci.on_device_discovered = [](const CiDeviceInfo& device) {
    log("Found: MUID=" + std::to_string(device.muid.value));
};

auto inquiry = ci.create_discovery_inquiry();
send_sysex(inquiry);  // Send over MIDI port
```

### Other MIDI features

| Feature | Header | Description |
|---------|--------|-------------|
| Messages | via CHOC | `ShortMessage::noteOn(0, 60, 100)` |
| Buffer | `midi_buffer.hpp` | Timestamped event buffer for `process()` callbacks |
| Files | `midi_file.hpp` | Read/write Standard MIDI Files |
| UMP | `ump.hpp` | MIDI 2.0 Universal MIDI Packets, MPE zones |
| Device I/O | platform/ | CoreMIDI (macOS), WinMIDI (Windows), ALSA (Linux), Web MIDI |

---

## signal

> 30+ real-time-safe DSP processors. All operate on single samples or buffers. All are safe for the audio thread.

**Link:** `pulp::signal` · **Include prefix:** `<pulp/signal/...>`

### Using a processor

Every processor follows the same pattern: configure, set sample rate, process.

```cpp
#include <pulp/signal/compressor.hpp>

Compressor comp;
comp.set_params({.threshold_db = -20, .ratio = 4, .attack_ms = 5, .release_ms = 100});
comp.set_sample_rate(48000);

for (int i = 0; i < num_samples; ++i)
    buffer[i] = comp.process(buffer[i]);
```

### Applying a mono processor to stereo

```cpp
#include <pulp/signal/processor_duplicator.hpp>

ProcessorDuplicator<Compressor> stereo_comp;
stereo_comp.prepare(2, 48000);
stereo_comp.for_each([](Compressor& c) { c.set_params({...}); });
stereo_comp.process(channels, 2, num_samples);
```

### Dry/wet mixing with latency compensation

```cpp
#include <pulp/signal/dry_wet_mixer.hpp>

DryWetMixer mixer;
mixer.set_mix(0.7f);                    // 70% wet
mixer.set_curve(MixCurve::EqualPower);  // Constant-power crossfade
mixer.set_wet_latency(512);             // Compensate 512 samples of plugin latency
mixer.prepare(2, 1024);

mixer.push_dry(input_channels, 2, num_frames);
// ... run your effect on the wet path ...
mixer.mix_wet(output_channels, 2, num_frames);
```

### Convolution — load an impulse response

```cpp
#include <pulp/signal/convolver.hpp>

PartitionedConvolver conv;
conv.init(impulse_response.data(), ir_length, block_size);

// In your process callback:
conv.process(input, output, block_size);
```

### Available processors

**Filters:** Biquad (IIR), FIR, State Variable (TPT), Ladder (4-pole Moog-style), Linkwitz-Riley, Filter Design (Butterworth/Chebyshev)

**Effects:** Partitioned Convolution, Reverb, Chorus, Phaser, Delay Line (linear/cubic/sinc), Waveshaper, Oversampling (2x/4x/8x)

**Dynamics:** Compressor (soft knee), Limiter (brickwall), Noise Gate, DryWetMixer (latency-compensated)

**Generators:** Oscillator (wavetable), ADSR envelope, FFT (vDSP on Apple), STFT, Windowing (Hann/Hamming/Blackman/Kaiser)

**Math:** Smoothed Value, Lookup Table, Fast Math (sin/cos/tanh approx), Special Functions (sinc/bessel/dB/MIDI↔freq), Matrix (2×2–4×4), Bias, SIMD Buffer

---

## state

> Parameters, state trees, presets, and settings.

**Link:** `pulp::state` · **Include prefix:** `<pulp/state/...>`

### Plugin parameters — the core state system

```cpp
#include <pulp/state/store.hpp>

StateStore store;
auto gain_id = store.add_param({.name = "Gain", .min = -60, .max = 12, .default_val = 0});
auto mix_id = store.add_param({.name = "Mix", .min = 0, .max = 1, .default_val = 1});

// Audio thread reads atomically (no locks)
float gain = store.get(gain_id);

// UI thread writes with gesture grouping for undo
store.begin_gesture(gain_id);
store.set(gain_id, -6.0f);
store.end_gesture(gain_id);
```

### StateTree — reactive hierarchical state

Like a JSON document that notifies you when anything changes. Use for complex plugin state beyond flat parameters.

```cpp
#include <pulp/state/state_tree.hpp>

auto root = StateTree::create("synth");
root->set("name", std::string("My Patch"));
root->set("polyphony", int64_t(8));

auto osc = StateTree::create("oscillator");
osc->set("waveform", std::string("saw"));
osc->set("detune", 7.0);
root->add_child(osc);

root->add_listener([](StateTree& node, std::string_view prop, auto&, auto& new_val) {
    log(std::string(prop) + " changed");
});

std::string json = root->to_json();           // Serialize
auto restored = StateTree::from_json(json);   // Deserialize
```

### Persistent settings

```cpp
#include <pulp/state/properties_file.hpp>

PropertiesFile settings;
settings.load("~/.config/MyPlugin/settings.json");
settings.set_string("theme", "dark");
settings.set_int("buffer_size", 512);
settings.save();

// Or use platform-standard paths automatically:
ApplicationProperties app("MyPlugin");
app.load();
app.user_settings().set_bool("first_run", false);
app.save();
```

### Other state features

| Feature | Header | Description |
|---------|--------|-------------|
| Binding | `binding.hpp` | Connect UI widget ↔ parameter with undo gesture grouping |
| Cached Property | `cached_property.hpp` | `CachedProperty<double> freq(tree, "freq", 440.0)` — auto-updates |
| Preset Manager | `preset_manager.hpp` | Factory/user presets, next/prev navigation, import/export |
| Undo Manager | `undo_manager.hpp` | `undo_mgr.perform(action)` / `undo_mgr.undo()` |
| StateTree Sync | `state_tree_sync.hpp` | Binary delta sync over IPC for multi-process state |

---

## format

> Plugin format adapters — write your plugin once, deploy to 9 formats.

**Link:** `pulp::format` · **Include prefix:** `<pulp/format/...>`

### Writing a plugin

```cpp
#include <pulp/format/processor.hpp>

class MyPlugin : public Processor {
public:
    Descriptor descriptor() override {
        return {.name = "MyGain", .vendor = "MyCompany", .uid = "com.myco.gain"};
    }
    
    void define_parameters(StateStore& store) override {
        gain_id_ = store.add_param({.name = "Gain", .min = -60, .max = 12});
    }
    
    void process(BufferView<float>& audio, MidiBuffer& midi,
                 const ProcessContext& ctx) override {
        float gain = db_to_linear(store().get(gain_id_));
        for (int ch = 0; ch < audio.num_channels(); ++ch)
            for (int i = 0; i < audio.num_frames(); ++i)
                audio[ch][i] *= gain;
    }
};
```

This single class automatically works as VST3, AU, CLAP, LV2, AAX, Standalone, WAM, and WCLAP.

### Supported formats

| Format | Status | Notes |
|--------|--------|-------|
| VST3 | ✓ Stable | Full parameter sync, state, editor resize |
| AU v2 | ✓ Stable | macOS only, via AudioUnitSDK |
| AU v3 | ✓ Stable | macOS + iOS |
| CLAP | ✓ Stable | First-class — modulation, WebView, note expressions |
| LV2 | ✓ Usable | Linux plugin format |
| AAX | ✓ Usable | Requires developer-supplied Avid SDK |
| Standalone | ✓ Stable | Desktop app with audio settings, test signal |
| WAM | ✓ Experimental | Web Audio Module (browser) |
| WCLAP | ✓ Experimental | Web CLAP (browser) |

### Other format features

| Feature | Header | Description |
|---------|--------|-------------|
| Host Detection | `host_type.hpp` | `detect_host_type()` → Logic, Reaper, Ableton, etc. |
| Settings Panel | `settings_panel.hpp` | Audio/MIDI device selector with test signal and meters |
| ARA | `ara.hpp` | Audio Random Access document controller (stub) |

---

## canvas

> 2D drawing with GPU acceleration and smart text layout.

**Link:** `pulp::canvas` · **Include prefix:** `<pulp/canvas/...>`

### Drawing

```cpp
void MyWidget::paint(Canvas& canvas) {
    // Background
    canvas.set_fill_color(Color::rgba(30, 30, 40));
    canvas.fill_rounded_rect(0, 0, width, height, 8);
    
    // Gradient fill
    canvas.set_fill_gradient(LinearGradient{0, 0, 0, height, 
        Color::rgba(80, 120, 255), Color::rgba(40, 60, 180)});
    canvas.fill_rect(10, 10, width - 20, 4);
    
    // Text
    canvas.set_fill_color(Color::rgba(220, 220, 230));
    canvas.set_font("Inter", 14);
    canvas.fill_text("Hello Pulp", 10, 30);
    
    // Image
    canvas.draw_image_from_file("icon.png", x, y, 32, 32);
}
```

**Backends:** Skia Graphite (GPU — Metal/Vulkan/D3D12) or CoreGraphics (macOS/iOS native).

### TextShaper — measure once, reflow forever

Inspired by [Cheng Lou's PreText](https://github.com/chenglou/pretext). The expensive text measurement runs once; resizing uses just arithmetic.

```cpp
#include <pulp/canvas/text_shaper.hpp>

TextShaper shaper;
auto prepared = shaper.prepare("Long text that wraps...", "Inter", 14);

// On every resize — pure arithmetic, no font calls:
auto layout = shaper.layout(prepared, container_width);
// layout.line_count, layout.total_height, layout.lines[i].text

float height = shaper.measure_height(prepared, 300.0f);  // Fast
```

**CMake option:** `PULP_TEXT_SHAPING` — ON with GPU (real HarfBuzz metrics via Skia), OFF without (character-width estimation). Same API either way.

### Other canvas features

| Feature | Header | Description |
|---------|--------|-------------|
| Attributed String | `attributed_string.hpp` | Rich text spans — mixed font, color, weight per range |
| Rectangle List | `rectangle_list.hpp` | Clip regions with add/subtract/intersect for dirty tracking |
| Image Convolution | `image_convolution.hpp` | `ImageConvolutionKernel::gaussian_blur_5().apply(pixels, w, h)` |
| SVG | `svg.hpp` | Load and render SVG vector graphics via nanosvg |
| Effects | `effects.hpp` | Drop shadow, bloom, blur, color adjustment |

---

## view

> Full widget toolkit with CSS-inspired layout and JS scripting.

**Link:** `pulp::view` · **Include prefix:** `<pulp/view/...>`

### Creating a UI

```cpp
#include <pulp/view/widgets.hpp>

auto root = std::make_unique<Panel>();
root->set_background_token("bg.surface");

auto knob = std::make_unique<Knob>();
knob->set_label("Gain");
knob->set_value(0.5f);
knob->on_change = [&](float v) { store.set(gain_id, v); };
root->add_child(std::move(knob));

auto meter = std::make_unique<Meter>();
meter->set_orientation(Meter::Orientation::vertical);
root->add_child(std::move(meter));
```

### Available widgets (30+)

**Controls:** Knob, Fader, Toggle, Checkbox, TextButton, ComboBox, TextEditor

**Containers:** Panel, TabPanel, SplitView, ScrollView, ConcertinaPanel (accordion), Toolbar

**Data display:** Label, ListBox, TreeView, TableListBox (sortable), FileBrowser, FileTree

**Audio visualization:** Meter, MultiMeter, CorrelationMeter, WaveformView, SpectrumView, SpectrogramView, EqCurveView, XYPad

**Specialized:** MidiKeyboard, PresetBrowser, ColorPicker, CodeEditor, LassoComponent (marquee selection), ImageView, CanvasWidget, SplashScreen, LiveConstantEditor

### Layout — Yoga flexbox + CSS Grid

```cpp
// Flexbox
root->style().set_flex_direction(FlexDirection::Row);
root->style().set_gap(8);

knob->style().set_flex_grow(1);
meter->style().set_width(40);
meter->style().set_height_percent(100);
```

### Theming — design tokens

```cpp
Theme theme;
theme.set("bg.surface", Color::rgba(25, 25, 35));
theme.set("control.accent", Color::rgba(80, 130, 255));
theme.set("text.primary", Color::rgba(220, 220, 230));

// Widgets resolve tokens automatically:
canvas.set_fill_color(resolve_color("bg.surface"));
```

### JS-scripted UI

Write your entire plugin UI in JavaScript with hot-reload during development:

```javascript
// plugin-ui.js
const knob = new Knob({ label: "Gain", param: "gain" });
const meter = new Meter({ orientation: "vertical" });
document.body.append(knob, meter);
```

**Engines:** QuickJS (default, lightweight), JavaScriptCore (Apple, fast JIT), V8 (full ES2024)

### Accessibility

VoiceOver (macOS + iOS), UIA (Windows), AT-SPI (Linux). Widgets declare their role and Pulp maps to platform accessibility APIs.

```cpp
knob->set_access_role(AccessRole::slider);
knob->set_access_label("Gain");
knob->set_access_value("-6 dB");
```

---

## osc

> Open Sound Control messaging for networked audio control.

**Link:** `pulp::osc` · **Include prefix:** `<pulp/osc/...>`

```cpp
#include <pulp/osc/osc.hpp>

// Send
Sender sender;
sender.connect("192.168.1.100", 9000);
sender.send("/synth/freq", 440.0f);
sender.send("/synth/gate", 1);

// Receive
Receiver receiver;
receiver.bind(9000);
receiver.on_message = [](const Message& msg) {
    if (msg.address == "/synth/freq")
        set_frequency(msg.args[0].as_float());
};
```

Supports bundles with timetags and address pattern matching (`*`, `?`, `[...]`, `{a,b}`).

---

## render

> GPU surface management — you rarely use this directly. Canvas and View handle it.

| Feature | What It Does |
|---------|-------------|
| Dawn/WebGPU | Cross-platform GPU abstraction (Metal, Vulkan, D3D12, OpenGL) |
| Skia Graphite | 2D rendering on top of Dawn — what Canvas uses internally |
| GPU Compute | Experimental batch processing for >64K element workloads |

---

## ship

> Packaging and distribution — from code signing to installer to update feed.

**Link:** `pulp::ship` · **Include prefix:** `<pulp/ship/...>`

```bash
# Sign
pulp ship sign --identity "Developer ID Application: My Company"

# Package
pulp ship package --version 1.2.0   # Creates DMG (macOS), NSIS (Windows), .deb (Linux)

# Notarize (macOS)
pulp ship notarize

# Generate update feed
pulp ship appcast --version 1.2.0 --notes "Bug fixes and new presets"
```

| Feature | What It Does |
|---------|-------------|
| Code Signing | macOS `codesign` + Windows `signtool` |
| Notarization | macOS notarization with `notarytool` |
| DMG / PKG | macOS installer creation |
| Windows Installer | NSIS-based with optional Authenticode |
| Linux Packaging | `.deb` and `.tar.gz` |
| Appcast | Sparkle (macOS) / WinSparkle (Windows) update feed |

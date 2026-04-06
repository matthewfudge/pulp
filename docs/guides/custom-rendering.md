# Custom Rendering

Pulp provides three layers of custom rendering, from simple JS draw commands to full GPU shader access. Choose the layer that matches your needs.

## When to Use Each Layer

| Layer | API | Language | GPU-Accelerated | Best For |
|-------|-----|----------|-----------------|----------|
| **B** — CanvasWidget | JS bridge canvas functions | JavaScript | Yes (via Skia) | Custom meters, visualizations, simple graphics |
| **C** — Canvas API | `Canvas` abstract interface | C++ | Yes (via Skia) | Custom `View` subclasses, complex procedural drawing |
| **C+** — Dawn/WebGPU | WebGPU render pipeline | C++ / WGSL | Direct GPU | Shader-driven visuals, particle systems, spectrograms |

Start with Layer B. Move to Layer C when you need C++ performance or complex state. Move to C+ when you need custom shaders.

---

## Layer B: CanvasWidget from JS

Create a `CanvasWidget` and issue draw commands from JavaScript. The widget queues commands that are executed on the render thread by the Skia backend.

### Setup

```js
const canvas = createCanvas("viz", "root");
setFlex("viz", "width", 300);
setFlex("viz", "height", 200);
```

### Basic Shapes

```js
// Clear previous frame
canvasClear("viz");

// Filled rectangle
canvasRect("viz", 10, 10, 100, 50, "#2a2a4a");

// Stroked rectangle
canvasStrokeRect("viz", 10, 10, 100, 50, "#6666aa", 2);

// Filled circle
canvasFillCircle("viz", 80, 120, 30, "#44ccff");

// Line
canvasStrokeLine("viz", 0, 100, 300, 100, "#333333", 1);

// Text
canvasSetFont("viz", "Inter", 14);
canvasFillText("viz", "Hello", 10, 180, 14, "#ffffff");
```

### Paths

```js
// Bezier curve
canvasBeginPath("viz");
canvasMoveTo("viz", 10, 150);
canvasCubicTo("viz", 60, 50, 150, 200, 290, 80);
canvasStrokePath("viz");

// Filled polygon
canvasSetFillColor("viz", "#ff6b6b");
canvasBeginPath("viz");
canvasMoveTo("viz", 150, 10);
canvasLineTo("viz", 180, 60);
canvasLineTo("viz", 120, 60);
canvasClosePath("viz");
canvasFillPath("viz");
```

### State Management

```js
canvasSave("viz");          // Push state (color, transform, clip)
canvasTranslate("viz", 50, 50);
canvasRotate("viz", 0.3);   // Radians
canvasRect("viz", -20, -20, 40, 40, "#44ff44");
canvasRestore("viz");       // Pop state — transform reset
```

### Example: Custom Level Meter

```js
function drawMeter(peak, rms) {
    canvasClear("viz");
    const w = 300, h = 200;

    // Background
    canvasRect("viz", 0, 0, w, h, "#0a0a12");

    // RMS bar
    const rmsH = rms * h;
    canvasRect("viz", 20, h - rmsH, 60, rmsH, "#225533");

    // Peak bar
    const peakH = peak * h;
    const color = peak > 0.9 ? "#ff4444" : peak > 0.7 ? "#ffaa00" : "#44ff44";
    canvasRect("viz", 20, h - peakH, 60, peakH, color);

    // Peak hold line
    canvasStrokeLine("viz", 15, h - peakH, 85, h - peakH, "#ffffff", 2);

    // dB grid lines
    [-6, -12, -24, -48].forEach((db) => {
        const y = h - Math.pow(10, db / 20) * h;
        canvasStrokeLine("viz", 0, y, w, y, "#222222", 1);
        canvasFillText("viz", db + " dB", 100, y + 4, 10, "#555555");
    });
}
```

---

## Layer C: Canvas API in C++

Create a custom `View` subclass and override `paint()` to draw directly to a `Canvas`. This is the same API that all built-in widgets use internally.

### Custom View

```cpp
#include <pulp/view/view.hpp>
#include <pulp/canvas/canvas.hpp>

class SpectrumDisplay : public pulp::view::View {
public:
    void set_data(const std::vector<float>& magnitudes) {
        magnitudes_ = magnitudes;
        set_needs_repaint();
    }

    void paint(pulp::canvas::Canvas& canvas) override {
        auto [w, h] = bounds().size();

        // Background
        canvas.set_fill_color(resolve_color("surface", Color::hex(0x1a1a2e)));
        canvas.fill_rect(0, 0, w, h);

        if (magnitudes_.empty()) return;

        // Draw spectrum as filled path
        canvas.set_fill_color(Color::rgba(88, 166, 255, 80));
        canvas.begin_path();
        canvas.move_to(0, h);

        float bar_width = w / static_cast<float>(magnitudes_.size());
        for (size_t i = 0; i < magnitudes_.size(); ++i) {
            float x = i * bar_width;
            float y = h - magnitudes_[i] * h;
            canvas.line_to(x, y);
        }

        canvas.line_to(w, h);
        canvas.close_path();
        canvas.fill_current_path();

        // Stroke the top edge
        canvas.set_stroke_color(Color::rgba(88, 166, 255, 200));
        canvas.set_line_width(1.5f);
        canvas.begin_path();
        for (size_t i = 0; i < magnitudes_.size(); ++i) {
            float x = i * bar_width;
            float y = h - magnitudes_[i] * h;
            if (i == 0) canvas.move_to(x, y);
            else canvas.line_to(x, y);
        }
        canvas.stroke_current_path();
    }

private:
    std::vector<float> magnitudes_;
};
```

### Canvas API Highlights

```cpp
// Gradients
canvas.set_fill_gradient_linear(0, 0, 0, h,
    {Color::hex(0x58a6ff), Color::hex(0x0f0f1a)},  // colors
    {0.0f, 1.0f},                                     // positions
    2);                                                // count

// Rounded rectangles
canvas.fill_rounded_rect(x, y, w, h, 8.0f);

// Arcs
canvas.stroke_arc(cx, cy, radius, start_angle, end_angle);

// Blend modes
canvas.set_blend_mode(BlendMode::screen);

// Opacity
canvas.set_opacity(0.5f);

// Text metrics
auto metrics = canvas.measure_text_full("Hello");
// metrics.width, metrics.ascent, metrics.descent, metrics.line_height

// SDF shapes (GPU-accelerated signed distance field rendering)
canvas.draw_sdf_shape(SDFShape::rounded_rect, x, y, w, h, {
    .fill_color = Color::hex(0x1a1a2e),
    .stroke_color = Color::hex(0x333333),
    .stroke_width = 1.0f,
    .corner_radius = 8.0f,
});

// Backdrop blur (frosted glass effect)
canvas.draw_blurred_backdrop(x, y, w, h,
    12.0f,                    // blur radius
    8.0f,                     // corner radius
    Color::rgba(0, 0, 0, 80)); // tint color

// GPU-accelerated waveform
canvas.draw_waveform(samples, count, x, y, w, h, {
    .line_color = Color::hex(0x58a6ff),
    .fill_color = Color::rgba(88, 166, 255, 40),
    .line_thickness = 1.5f,
    .show_fill = true,
    .fill_center = 0.5f,
});
```

### Testing with RecordingCanvas

Verify draw commands without a GPU:

```cpp
#include <pulp/canvas/recording_canvas.hpp>

TEST_CASE("SpectrumDisplay draws correctly") {
    SpectrumDisplay display;
    display.set_bounds({0, 0, 300, 200});
    display.set_data({0.5f, 0.8f, 0.3f, 0.6f});

    pulp::canvas::RecordingCanvas canvas;
    display.paint(canvas);

    // Verify draw commands were issued
    REQUIRE(canvas.command_count() > 0);
    REQUIRE(canvas.count(RecordingCanvas::Type::fill_rect) >= 1);
    REQUIRE(canvas.count(RecordingCanvas::Type::fill_current_path) >= 1);
}
```

---

## Layer C+: Dawn/WebGPU Shaders

For fully custom GPU rendering — particle systems, spectrograms, oscilloscopes, or any effect that benefits from running per-pixel on the GPU.

### Architecture

```
View::paint()
  └── Dawn render pass
        ├── Vertex buffer (quad or custom geometry)
        ├── Uniform buffer (theme colors, time, audio data)
        └── WGSL fragment shader (your custom effect)
```

### Shader Widget Pattern

```cpp
#include <pulp/render/render_context.hpp>
#include <pulp/view/view.hpp>

class ShaderWidget : public pulp::view::View {
public:
    void initialize(pulp::render::RenderContext& ctx) {
        // Compile shader
        auto result = ctx.compile_shader(vertex_wgsl, fragment_wgsl);
        pipeline_ = result.pipeline;

        // Create uniform buffer for per-frame data
        uniform_buffer_ = ctx.create_buffer(sizeof(Uniforms),
            wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst);
    }

    void paint(pulp::canvas::Canvas& canvas) override {
        auto* ctx = render_context();
        if (!ctx || !pipeline_) return;

        // Update uniforms
        Uniforms u {
            .time = static_cast<float>(elapsed_seconds()),
            .resolution = {bounds().width, bounds().height},
            .accent = theme_color_vec4("accent"),
        };
        ctx->write_buffer(uniform_buffer_, &u, sizeof(u));

        // Draw fullscreen quad with our shader
        ctx->draw_quad(pipeline_, uniform_buffer_);
    }

private:
    struct Uniforms {
        float time;
        float resolution[2];
        float accent[4];
    };

    wgpu::RenderPipeline pipeline_;
    wgpu::Buffer uniform_buffer_;
};
```

### WGSL Fragment Shader

```wgsl
struct Uniforms {
    time: f32,
    resolution: vec2<f32>,
    accent: vec4<f32>,
}

@group(0) @binding(0) var<uniform> u: Uniforms;

@fragment
fn main(@location(0) uv: vec2<f32>) -> @location(0) vec4<f32> {
    // Animated gradient using theme accent color
    let t = sin(u.time * 2.0 + uv.x * 6.28) * 0.5 + 0.5;
    let bg = vec4<f32>(0.06, 0.06, 0.1, 1.0);
    return mix(bg, u.accent, t * uv.y);
}
```

### Validating Shaders

Validate shader code without running it:

```js
// From JS
const result = compileShader(skslCode);
if (!result.success) {
    console.error("Shader error: " + result.error);
}
```

```cpp
// From C++
auto result = render_context.compile_shader(vertex_src, fragment_src);
if (!result.success) {
    log::error("Shader: {}", result.error);
}
```

### Passing Audio Data to Shaders

For audio-reactive visuals, push data from the audio thread to a GPU buffer:

```cpp
// Audio thread → TripleBuffer → UI thread → GPU buffer
AudioBridge bridge;

// In process():
bridge.push_spectrum(fft_magnitudes, bin_count);

// In paint():
if (bridge.poll_spectrum()) {
    auto& data = bridge.spectrum_data();
    ctx->write_buffer(spectrum_buffer_, data.bins.data(),
        data.bin_count * sizeof(float));
}
```

---

## Performance Guidelines

| Technique | When to Use | Overhead |
|-----------|------------|----------|
| `canvasRect` / `canvasFillCircle` | Simple shapes, few per frame | Very low |
| Canvas paths | Complex shapes, curves | Low |
| `draw_sdf_shape` | Anti-aliased shapes at any size | Very low (GPU) |
| `draw_waveform` | Audio waveforms | Very low (GPU batch) |
| `draw_blurred_backdrop` | Frosted glass effects | Medium (GPU blur pass) |
| Custom WGSL shader | Per-pixel effects, particles | Depends on shader complexity |

### Tips

1. **Minimize `canvasClear` + redraw.** Only redraw what changed. The Canvas queues commands — unchanged regions are cheap.

---

## GPU Capabilities: What Is Real Today

Pulp uses the GPU in three distinct ways. Do not conflate them:

| Capability | Status | What It Does |
|------------|--------|-------------|
| **Dawn/Skia Graphite rendering** | Shipped | All UI drawing goes through the GPU via Skia Graphite on a Dawn wgpu::Device. This is the rendering pipeline, not compute. |
| **SkSL runtime effects** | Shipped | Fragment shaders for visual effects (SDF shapes, blur, gradients). These run per-pixel during rendering. Not compute shaders. |
| **WebGPU compute for audio** | Experimental | WGSL compute shaders for batch spectral processing. Viable for large offline workloads (>64K elements). Not viable for real-time per-buffer audio. See `docs/reports/webgpu-compute-feasibility.md`. |

The first two are production rendering features. The third is a separate compute pipeline that shares the same Dawn device but operates independently of rendering. It does NOT run in the audio callback.

2. **Use SDF shapes over path-based shapes** when possible. SDF rendering is resolution-independent and faster for rounded rectangles, circles, and arcs.

3. **Use `draw_waveform` over manual line drawing** for audio displays. It batches all samples into a single GPU draw call.

4. **Avoid per-frame shader compilation.** Compile once in `initialize()`, reuse the pipeline.

5. **Keep shader uniforms small.** A few floats and a texture reference — don't upload entire audio buffers every frame. Use storage buffers for large data.

"Phase 10: AI-Driven GPU Shader Design System for Pulp.

Reference: planning/phase-10-ai-shader-design-system.md (full spec with research findings)
Reference: planning/figma-to-pulp-mapping.md (Figma/React/Tailwind translation tables)
CLAUDE.md (build, test, architecture)
Tracking: planning/STATUS.md

GOAL:
Build a complete AI-driven visual style system where users describe widget styles via chat
and see them rendered in real-time on plugin knobs, faders, and toggles. Four rendering
strategies: SkSL GPU shaders, Canvas 2D recipes, declarative widget schema, Lottie animations.

NON-NEGOTIABLES:
- RuntimeEffectCache at renderer/bridge lifetime, NOT on SkiaCanvas (recreated every frame)
- Shader support on CanvasWidget FIRST, then widgets — never on abstract Canvas
- Shader replaces body/track/fill layer ONLY — keep C++ labels, value text, hover overlays
- Use layout(color) for SkSL color uniforms (Skia color management)
- Wire ContextOptions::fShaderErrorHandler for Graphite backend failures
- All existing tests must pass
- Every new feature gets automated tests
- Use XcodeBuildMCP for build/test verification

CODEX DELEGATION (OPTIONAL):
- Use /codex <task> or codex exec --full-auto <task> for parallel work.
- Good candidates for Codex delegation:
  - Writing tests for code you just wrote
  - Implementing a component while you work on another
  - Code review of completed work items
  - Second opinions on issues or work items
- Do NOT delegate to Codex when:
  - The task depends on something you're currently building
  - Multiple agents would edit the same file
  - The task requires your current conversation context
- When delegating, run Codex in background and continue your work.
- Check Codex output before marking work item complete.

PHASE 10.0 — Yoga Layout Integration (do this first):
Goal: Replace hand-rolled flexbox with Meta's Yoga engine (MIT, C++20).
Fixes: margin:auto centering, flex-wrap, absolute positioning, min-content/max-content, box-sizing.

1. Add Yoga as CMake FetchContent dependency:
   - FetchContent_Declare(yoga GIT_REPOSITORY https://github.com/facebook/yoga.git GIT_TAG v3.2.1)
   - Link pulp-view against yogacore

2. Create YogaLayout adapter in core/view/src/yoga_layout.cpp:
   - Convert View tree to YGNode tree before layout
   - Map FlexStyle fields to YGNodeStyleSet* calls
   - Map GridStyle to Yoga grid properties
   - Call YGNodeCalculateLayout() on root
   - Walk tree to apply YGNodeLayoutGet{Left,Top,Width,Height} back to View::set_bounds()

3. Replace View::layout_children() to call Yoga adapter
   - Yoga's measure callback = View::intrinsic_width()/intrinsic_height()
   - Keep FlexStyle/GridStyle as the public API, Yoga is internal

4. Verify: margin:auto centers, flex-wrap works, position:absolute positions correctly
5. Run all tests — layout tests should still pass or improve
6. Commit: "Integrate Yoga layout engine — correct flexbox, margin:auto, wrap, absolute"

PHASE 10.1 — Shader Engine (C++ core):
Goal: SkSL shaders compile and render on any widget's body layer.

1. Create RuntimeEffectCache class (core/canvas/src/runtime_effect_cache.hpp/.cpp):
   - std::unordered_map<size_t, sk_sp<SkRuntimeEffect>> keyed by std::hash<string>(sksl)
   - get_or_compile(sksl) → returns cached effect or compiles new, nullptr on failure
   - error() → last compilation error string
   - Thread-safe via std::mutex (one compilation at a time)

2. Wire compileShader() bridge in widget_bridge.cpp:
   - Actually call RuntimeEffectCache::get_or_compile()
   - Return {success: bool, error: string} with real SkSL error messages
   - If PULP_HAS_SKIA is not defined, return {success: false, error: "No Skia"}

3. Add shader overlay to CanvasWidget:
   - CanvasWidget::set_shader_source(string sksl) stores SkSL code
   - In CanvasWidget::paint(): after replaying draw commands, if shader set:
     a. Get RuntimeEffectCache from somewhere accessible (static, or passed via draw context)
     b. Compile/cache the effect
     c. Build SkRuntimeShaderBuilder with standard uniforms
     d. Draw full-bounds rect with shader paint
   - Register setCanvasShader(canvasId, skslCode) bridge

4. Standard SkSL uniform contract:
   ```glsl
   uniform float2 resolution;           // widget size in pixels
   uniform float value;                  // 0-1 parameter value
   uniform float time;                   // seconds since start (requires FrameClock)
   layout(color) uniform float4 accentColor;
   layout(color) uniform float4 bgColor;
   layout(color) uniform float4 trackColor;
   layout(color) uniform float4 fillColor;
   layout(color) uniform float4 thumbColor;
   half4 main(float2 coord) { ... }
   ```

5. Add custom_sksl_ to Knob/Fader/Toggle (widgets.hpp):
   - std::string custom_sksl_
   - void set_custom_shader(string sksl) / void clear_custom_shader()

6. Body-layer shader in Knob::paint():
   - If custom_sksl_ set: draw shader for the arc/track/fill area (steps 2-5 of paint)
   - Then CONTINUE to draw: labels (step 6), value text (step 7), hover glow (step 1)
   - Same pattern for Fader::paint() and Toggle::paint()

7. Register bridge functions:
   - setWidgetShader(id, skslCode) → set custom_sksl_ on widget
   - clearWidgetShader(id) → clear custom_sksl_
   - setCanvasShader(canvasId, skslCode) → set shader on CanvasWidget

8. FrameClock subscription: if shader uses `time`, auto-request continuous repaint

9. Reference SkSL shaders to include as built-in examples:
   Arc knob (default):
   ```glsl
   uniform float2 resolution;
   uniform float value;
   layout(color) uniform float4 trackColor;
   layout(color) uniform float4 fillColor;

   half4 main(float2 coord) {
       float2 uv = coord / resolution;
       float2 center = float2(0.5);
       float dist = length(uv - center);
       float angle = atan(uv.y - 0.5, uv.x - 0.5);
       float startAngle = radians(135.0);
       float totalSweep = radians(270.0);
       float normalized = (angle - startAngle) / totalSweep;
       if (dist > 0.35 && dist < 0.45) {
           if (normalized >= 0.0 && normalized <= 1.0) {
               return normalized <= value ? half4(fillColor) : half4(trackColor);
           }
       }
       return half4(0);
   }
   ```

10. Test: Build, screenshot, verify custom SkSL renders on knob body
    Commit: "Phase 10.1: Shader engine — RuntimeEffectCache, CanvasWidget shader, widget body shaders"

PHASE 10.2 — Declarative Widget Schema (Rive-inspired):
Goal: JSON schema defining widget appearance as data, not code. Claude generates structured definitions.

1. Define schema format (JSON):
   ```json
   {
     "type": "knob",
     "elements": [
       {
         "type": "arc", "id": "track",
         "cx": "50%", "cy": "50%", "radius": "45%", "width": 3,
         "startAngle": -135, "sweepAngle": 270,
         "color": "control.track"
       },
       {
         "type": "arc", "id": "value",
         "cx": "50%", "cy": "50%", "radius": "45%", "width": 3,
         "startAngle": -135,
         "sweepAngle": { "bind": "value", "range": [0, 270] },
         "color": "control.fill"
       },
       {
         "type": "line", "id": "thumb",
         "from": { "r": "30%", "angle": "valueAngle" },
         "to": { "r": "45%", "angle": "valueAngle" },
         "width": 2, "color": "control.thumb"
       }
     ],
     "states": {
       "hover": {
         "track.glow": { "radius": 4, "color": "accent.primary", "alpha": 0.3 }
       },
       "active": {
         "thumb.width": 3
       },
       "disabled": {
         "*.alpha": 0.5
       }
     },
     "transitions": {
       "hover": { "duration": 150, "easing": "ease-out" },
       "active": { "duration": 50, "easing": "linear" }
     }
   }
   ```

2. Schema interpreter in core/view/src/widgets.cpp:
   - SchemaRenderer class: parse JSON → issue canvas draw calls
   - Resolve "50%" against widget bounds
   - Resolve { "bind": "value", "range": [min, max] } against widget value
   - Resolve "control.track" via resolve_color()
   - Handle state: check widget hover/active/disabled → merge state overrides
   - Animate transitions via ValueAnimation on state change

3. Register setWidgetSchema(id, schemaJSON) bridge function

4. Built-in schemas as JSON resources:
   - "default-arc" (matches current Knob::paint)
   - "filled-pie" (filled wedge showing value)
   - "notched-ticks" (tick marks around perimeter)
   - "glossy-3d" (radial gradient sphere with arc)

5. Test: Schema-rendered knob matches hardcoded paint() visually
   Commit: "Phase 10.2: Declarative widget schema — styles as data, state machines, transitions"

PHASE 10.3 — Lottie Animations via Skottie:
Goal: AI-generatable animated widget effects via Lottie JSON rendered by Skia's Skottie.

1. Enable Skottie module in Skia build (check if already available in external/skia-build)
   - If not: add skottie to the Skia build flags

2. Create LottieOverlay class (core/view/src/lottie_overlay.hpp/.cpp):
   - Loads Lottie JSON string via skottie::Animation::Make()
   - Renders at a given frame/time into a canvas region
   - seek(float t) → sets playback position (0-1 or frame number)

3. Add lottie_overlay_ to widgets:
   - Knob/Fader/Toggle get optional LottieOverlay
   - In paint(): render Lottie animation at widget bounds after body, before labels

4. Register bridge functions:
   - setWidgetLottie(id, lottieJSON) → parse and attach animation
   - seekWidgetLottie(id, time) → scrub to position
   - playWidgetLottie(id, fromFrame, toFrame) → play segment

5. Claude generates Lottie JSON for:
   - Hover glow pulse (opacity keyframes on a circle shape layer)
   - Value indicator rotation (rotation keyframed 0-270 degrees)
   - Active press feedback (scale keyframe 1.0 → 0.95 → 1.0)

6. Reference Lottie JSON (minimal knob rotation):
   ```json
   {"v":"5.5.2","fr":60,"ip":0,"op":60,"w":100,"h":100,
    "layers":[{"ty":4,"nm":"indicator","ks":{
      "r":{"a":1,"k":[
        {"t":0,"s":[0],"e":[270]},
        {"t":60,"s":[270]}
      ]}
    },"shapes":[
      {"ty":"rc","p":{"a":0,"k":[50,10]},"s":{"a":0,"k":[4,30]}}
    ]}]}
   ```

7. Test: Lottie animation renders on widget, seek maps to parameter value
   Commit: "Phase 10.3: Lottie animations via Skottie — AI-generatable widget effects"

PHASE 10.4 — Web Animations API + Inspector Multi-Select:
Goal: element.animate() API + multi-select inspector for scoped style application.

1. Implement element.animate(keyframes, options) in web-compat.js:
   ```js
   Element.prototype.animate = function(keyframes, options) {
       var duration = typeof options === 'number' ? options : (options.duration || 300);
       var easing = (options.easing || 'ease');
       var fill = (options.fill || 'none');
       var animation = new Animation(this, keyframes, duration, easing, fill);
       animation.play();
       return animation;
   };
   ```
   - Animation object with play/pause/cancel/finish/reverse
   - finished promise
   - Drives property interpolation via requestAnimationFrame
   - Fires 'finish' event on completion

2. Inspector multi-select in design-tool.js:
   - Cmd+click: toggle single selection (existing, harden)
   - Cmd+Shift+click: add/remove from multi-selection
   - Escape: clear all
   - Click empty: clear all
   - selectedWidgets[] array with IDs + types
   - Visual highlight on selected widgets (outline ring)

3. Selection context in chat prompt:
   - 0 selected → "Apply to: all widgets of the affected type"
   - 1 selected → "Apply to: knob-1 (Knob)"
   - N selected → "Apply to: knob-1, fader-2, knob-3 (2 Knobs, 1 Fader)"

4. Apply mode dispatcher:
   - Color changes → applyTokenDiff (fast, existing)
   - Geometric changes → setWidgetSchema for each selected widget
   - GPU effects → setWidgetShader for each selected widget
   - Animations → setWidgetLottie for each selected widget

5. Test: Select 3 knobs → "make these metallic" → only those 3 get shader
   Commit: "Phase 10.4: Web Animations API + inspector multi-select + scoped style application"

PHASE 10.5 — Chat Prompt Engineering + Presets:
Goal: Claude generates valid SkSL/schema/Lottie from natural language descriptions.

1. Three-tier system prompt:
   Tier 1 (color/token): "make it warmer" → JSON diff of color tokens
   Tier 2 (geometry/schema): "notched ticks with pointer" → widget schema JSON
   Tier 3 (GPU effects): "brushed metal with LED ring" → SkSL shader code
   Tier 4 (animation): "pulse glow on hover" → Lottie JSON

2. System prompt includes:
   - SkSL uniform contract (resolution, value, time, layout(color) colors)
   - Widget schema format specification with examples
   - Lottie JSON subset specification (shapes, keyframes, transforms)
   - Token namespace listing
   - Current selection context
   - 2-3 reference SkSL/schema/Lottie examples

3. Response parsing:
   - Detect ```sksl```, ```schema```, ```lottie```, ```json``` code blocks
   - Try SkSL compile → if success, apply via setWidgetShader
   - Try schema validate → if success, apply via setWidgetSchema
   - Try Lottie parse → if success, apply via setWidgetLottie
   - Fall back to token diff (color-only)
   - Show compile/parse errors in chat, offer retry

4. Switch to execAsync for non-blocking chat

5. Preset system:
   - saveStylePreset(name, { shader?, schema?, lottie?, tokens? })
   - loadStylePreset(name)
   - Built-in presets: default-arc, filled-pie, notched-ticks, glossy-3d, led-ring, moog-analog
   - Apply preset across widget types

6. Reference image upload:
   - Drop/upload photo → save to temp
   - Include path in chat prompt: "User uploaded reference image. Generate matching style."
   - Claude describes the visual style and generates appropriate shader/schema

7. Undo/redo for shader/schema changes (snapshot stack)

8. W3C Design Tokens import/export:
   - Import: parse W3C format { "$value": "#ff0000", "$type": "color", "$description": "..." }
   - Export: serialize Theme to W3C format for Figma/Stitch/v0 interop
   - Bridge: importDesignTokens(json), exportDesignTokens() → W3C JSON string
   - This enables: Figma token plugins → Pulp, Pulp → Figma

9. Model-agnostic AI CLI:
   - Extract CLI command into configurable variable (not hardcoded to Claude):
     var AI_CLI = "claude --print --model claude-sonnet-4-6";
   - Support override: setAICli("gemini --print") or setAICli("codex exec")
   - Prompt format and response parsing stay model-agnostic (structured JSON/code blocks)
   - Register setAICli(command) bridge so it's configurable at runtime

10. Test: "analog Moog knob with LED ring" → valid output applied to preview
    Test: importDesignTokens with W3C format → theme updates correctly
    Commit: "Phase 10.5: Chat prompt engineering + presets + design tokens + model-agnostic AI"

TESTING AT EACH STEP:
1. cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
2. cmake --build build -j$(sysctl -n hw.ncpu)
3. ctest --test-dir build --output-on-failure --exclude-regex AudioWorkgroup
4. Screenshot verify: ./build/examples/ui-preview/pulp-ui-preview --screenshot

WHAT YOU'LL GET WHEN DONE:
- User types 'brushed metal Moog knob with LED ring' → metallic 3D knob renders in preview
- User Cmd+clicks 3 knobs → 'warmer colors' → only those 3 change
- User Cmd+Shift+clicks multiple widgets → 'darker, more contrast' → all selected change
- User drops a photo of a Buchla panel → AI generates matching shader/schema
- Save as 'Moog LED' preset → apply to any knob/fader later
- 4 rendering strategies: SkSL GPU shaders, Canvas 2D recipes, declarative widget schema, Lottie animations
- Correct CSS Flexbox via Yoga (margin:auto, flex-wrap, absolute positioning, min-content all work)
- element.animate() Web Animations API for JS-driven keyframe animations
- W3C Design Tokens import/export (Figma/Stitch/v0 interop ready)
- Model-agnostic AI: works with Claude CLI, swappable to Gemini/Codex CLI later
- Figma-to-Pulp mapping documented for future automated translation
- All existing 1247 tests still pass" --completion-promise "PHASE 10 COMPLETE" --max-iterations 200

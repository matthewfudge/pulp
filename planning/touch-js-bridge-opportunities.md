# Touch → JS Bridge: Feature Opportunities

**Goal**: Web developers writing Pulp JS UIs should get multitouch, stylus, and gestures "for free" when their UI runs on iOS/iPadOS — no platform-specific code.

**Status**: The C++ View layer has `pointer_id` multitouch support, and iOS `window_host_ios.mm` calls `self.multipleTouchEnabled = YES`. But the JS bridge exposes none of this to JavaScript. There's also a pointer identity bug on iOS.

---

## Current State

| Layer | What exists | What's missing |
|-------|-------------|---------------|
| iOS UIKit (`window_host_ios.mm`) | `multipleTouchEnabled = YES`, loops over `NSSet<UITouch*>` | Stable pointer identity (NSSet is unordered, `pid++` resets each frame) |
| C++ MouseEvent (`input_events.hpp`) | `pointer_id`, `isTouch()`, modifiers touch flag (0x8000) | Pressure, tilt, azimuth, touch radius, predicted touches |
| JS bridge (`widget_bridge.cpp`) | `registerClick(id)`, `registerHover(id)` | No `pointerdown`/`pointermove`/`pointerup`, no touch events, no coordinates |
| JS event objects (`web-compat.js`) | `_makeEvent()` with `clientX`/`clientY` fields | Fields are always 0 — position data never flows from C++ to JS |

---

## Bug: Unstable Pointer Identity on iOS

**File**: `core/view/platform/ios/window_host_ios.mm` lines 72-122

```objc
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    int pid = 0;
    for (UITouch *touch in touches) {  // NSSet — ORDER IS NOT GUARANTEED
        me.pointer_id = pid++;          // Finger A gets pid=0 one frame, pid=1 the next
```

**Fix**: Use `std::unordered_map<void*, int>` keyed on UITouch pointer address. Assign stable IDs in `touchesBegan`, reuse in `touchesMoved`, remove in `touchesEnded`. Same fix needed in `plugin_view_host_ios.mm`.

---

## Opportunity 1: W3C Pointer Events in JS

Expose the existing C++ `pointer_id` data as standard **PointerEvent** objects in JavaScript. This is the single highest-value addition — web developers already know the API, and the C++ plumbing mostly exists.

**What web developers would get:**
```javascript
myCanvas.addEventListener('pointerdown', (e) => {
    console.log(e.pointerId);    // Stable per-finger ID
    console.log(e.pointerType);  // "touch", "mouse", or "pen"
    console.log(e.clientX, e.clientY);
    console.log(e.pressure);     // 0.0–1.0 (Apple Pencil or force touch)
    console.log(e.isPrimary);    // true for first finger
});
```

**Work required:**
1. Fix iOS pointer identity (see bug above)
2. Populate `clientX`/`clientY` from `MouseEvent.position` when dispatching to JS
3. Add `pointerId`, `pointerType`, `pressure`, `isPrimary` to `_makeEvent()`
4. Bridge `on_mouse_down`/`on_mouse_drag`/`on_mouse_up` to JS as `pointerdown`/`pointermove`/`pointerup`
5. Add `pointerenter`/`pointerleave`/`pointercancel` events
6. CSS `touch-action` property to control default handling

**Why Pointer Events, not Touch Events**: PointerEvents unify mouse, touch, and stylus into one API. They're the modern standard. TouchEvents are legacy. Pointer Events also degrade gracefully to mouse-only on desktop — same JS code works everywhere.

---

## Opportunity 2: Apple Pencil / Stylus Data

UITouch exposes rich stylus properties that would be valuable for audio UI (drawing envelopes, pressure-sensitive controls):

| UITouch property | Proposed JS field | Use case |
|------------------|-------------------|----------|
| `.force` / `.maximumPossibleForce` | `e.pressure` | Pressure-sensitive knobs, velocity curves |
| `.altitudeAngle` | `e.altitudeAngle` | Pencil tilt for brush/drawing tools |
| `.azimuthAngle(in:)` | `e.azimuthAngle` | Pencil rotation |
| `.type == .pencil` | `e.pointerType = "pen"` | Distinguish finger from pencil |
| `.estimatedProperties` | (internal) | Coalesced/predicted touch updates |

**Work required:**
1. Extend `MouseEvent` struct with `float pressure`, `float altitude_angle`, `float azimuth_angle`
2. Read UITouch properties in `touchesBegan`/`touchesMoved`
3. Expose through PointerEvent JS API (same as Opportunity 1)

---

## Opportunity 3: Gesture Recognition

High-level gestures that web developers expect, especially for multi-finger interaction:

| Gesture | JS event | Use case |
|---------|----------|----------|
| Pinch | `gesturestart`/`gesturechange`/`gestureend` with `e.scale` | Zoom waveform, resize elements |
| Rotate | Same events with `e.rotation` | Rotate knobs with two fingers |
| Long press | `contextmenu` or `longpress` | Show parameter menu |
| Swipe | `swipe` with direction | Page through presets |

**Two approaches:**
- **A) C++ gesture recognizer**: Analyze `pointer_id` streams, emit high-level events to JS. More reliable, but more C++ work.
- **B) JS polyfill**: Let a JS library analyze raw pointer events. Less native work, but heavier JS.

**Recommendation**: Start with approach A for pinch/rotate (these need native smoothness), let developers handle swipe/longpress in JS from raw pointer events.

---

## Opportunity 4: Coalesced & Predicted Touches

iOS provides `coalescedTouches(for:)` and `predictedTouches(for:)` on UIEvent. These are critical for low-latency drawing and smooth drag interactions:

- **Coalesced**: All touch samples between frames (120Hz+ on ProMotion iPads even when rendering at 60fps)
- **Predicted**: Where iOS thinks the finger is going (reduces perceived latency by ~1 frame)

**JS API** (matches W3C spec):
```javascript
canvas.addEventListener('pointermove', (e) => {
    for (const pt of e.getCoalescedEvents()) {
        drawLine(pt.clientX, pt.clientY);  // All intermediate points
    }
    const predicted = e.getPredictedEvents();
    // Use for tentative rendering
});
```

**Priority**: Medium. Very valuable for canvas drawing but not needed for standard widget interaction.

---

## Opportunity 5: Mouse Position in JS Events

Even before multitouch, `clientX`/`clientY` are always 0 in JS events today. Fixing this gives web developers coordinate-aware interactions on all platforms:

```javascript
// Works today: ❌ (clientX/clientY are always 0)
// Should work:
myElement.addEventListener('click', (e) => {
    console.log(e.clientX, e.clientY);  // Where the click happened
    console.log(e.offsetX, e.offsetY);  // Relative to element
});
```

**Work required**: When `widget_bridge.cpp` calls `__dispatch__`, include position data from the C++ `MouseEvent` and populate it in `_makeEvent()`.

**Priority**: High — this is a prerequisite for Opportunities 1-4 and fixes a gap that affects all platforms.

---

## Implementation Order

| Phase | What | Why | Depends on |
|-------|------|-----|------------|
| **P0** | Fix iOS pointer identity bug | Correctness — current multitouch is broken | — |
| **P1** | Pipe `clientX`/`clientY` through bridge to JS events | Prerequisite for everything else | — |
| **P2** | W3C PointerEvents (`pointerdown`/`pointermove`/`pointerup`) with `pointerId` | Highest-value feature — multitouch "just works" in JS | P0, P1 |
| **P2b** | Pointer capture (`setPointerCapture`/`releasePointerCapture`) | Required for correct drag behavior | P2 |
| **P3** | Stylus properties (`pressure`, `altitudeAngle`, `pointerType = "pen"`) | Differentiating for creative tools | P2 |
| **P4** | Native pinch/rotate gesture events (iOS + macOS trackpad) | Essential for touch-first UIs | P0 |
| **P5** | Coalesced/predicted touches | Polish for drawing-heavy UIs | P2 |
| **P6** | iPadOS hover for trackpad/mouse | Parity with macOS hover behavior | P2 |

**Critical path**: P0 → P1 → P2 → P2b. After P2b, a web developer's standard PointerEvent code runs correctly on desktop (mouse) and iOS (multitouch) with proper drag handling. Everything after that is progressive enhancement.

**Complete coverage after all phases**: Mouse, touch, stylus, gestures, hover, drag capture, sub-frame precision — across macOS, iOS, and iPadOS with cursor. The same JS code handles all platforms with no conditional logic.

---

## Opportunity 6: Pointer Capture

The W3C PointerEvent spec includes `setPointerCapture(pointerId)` / `releasePointerCapture(pointerId)`. When a user presses down on a knob and drags outside its bounds, the knob keeps receiving events because it "captured" the pointer. Without this, drag interactions break when the finger/mouse leaves the element bounds.

macOS `window_host_mac.mm` has a hardcoded `_dragTarget` that serves this purpose for mouse drags, but:
- It's not exposed to JS
- It's not cross-platform (iOS has no equivalent)
- It doesn't support multiple simultaneous captures (one per pointer)

**Work required:**
1. Add `captured_pointers_` map in C++ View: `pointer_id → View*`
2. Route events for captured pointers directly to the capturing view, bypassing hit-test
3. Expose `element.setPointerCapture(pointerId)` and `element.releasePointerCapture(pointerId)` in JS
4. Fire `gotpointercapture` / `lostpointercapture` events per spec
5. Implicit capture release on `pointerup` / `pointercancel`

**Priority**: High — essential for correct drag behavior. Practically required alongside P2 (PointerEvents).

---

## Opportunity 7: macOS Trackpad Gestures

macOS `window_host_mac.mm` only implements `scrollWheel:`. NSResponder also provides:
- `magnifyWithEvent:` — two-finger pinch (provides `magnification` delta)
- `rotateWithEvent:` — two-finger rotation (provides `rotation` delta in degrees)
- `smartMagnifyWithEvent:` — double-tap to zoom

These should map to the same gesture events as iOS (Opportunity 3), so the same JS code handles both:
- iPad two-finger pinch → `gesturechange` with `e.scale`
- Mac trackpad pinch → `gesturechange` with `e.scale`

**Work required:**
1. Add `magnifyWithEvent:` and `rotateWithEvent:` handlers to `PulpContentView` in `window_host_mac.mm`
2. Route through the same C++ gesture event path as iOS
3. No extra JS work — same events, same API

---

## Opportunity 8: iPadOS Hover (Trackpad/Mouse Connected)

iPadOS with a trackpad or mouse supports hover via `UIHoverGestureRecognizer` (iOS 13+). Currently `window_host_ios.mm` has zero hover support — `:hover` CSS and `pointerenter`/`pointerleave` events silently fail on iPad even when a cursor is connected.

**Work required:**
1. Add `UIHoverGestureRecognizer` to `PulpRootView` in `window_host_ios.mm`
2. Call `rootView->simulate_hover(pt)` on hover state changes (same as macOS does)
3. This feeds into existing hover infrastructure (`is_hovered()`, `on_hover_enter/leave` callbacks)

**Priority**: Medium — affects iPadOS users with Magic Keyboard/Trackpad.

---

## Testing Strategy

- **Headless**: Synthesize `MouseEvent` with various `pointer_id` values, verify JS receives correct `pointerId` in PointerEvent callbacks
- **iOS simulator**: UITouch sequences with known coordinates, verify stable identity across frames
- **Golden file**: Record a two-finger pinch sequence → verify gesture events fire with correct scale values
- **Cross-platform parity**: Same JS test code should produce same event sequence on macOS (mouse), iOS (touch), and headless (synthetic)

---

## Design Principle

**Write once, enhance everywhere.** A web developer writes standard PointerEvent handlers. On desktop they get mouse input. On iPad they get multitouch + stylus automatically. No `#ifdef`, no platform detection, no conditional code. The C++ bridge translates platform-specific input into the same W3C-compatible events.

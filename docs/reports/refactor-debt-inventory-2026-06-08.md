# Refactor Debt Inventory

Date: 2026-06-08

Scope: P1.5 from the refactor roadmap. This is a no-behavior-change inventory
for source markers that currently look alike but have different ownership:
behavior gaps, intentional no-ops, compatibility shims, deprecated paths, and
historical/test artifacts.

Baseline command:

```bash
rg -n "TODO|FIXME|HACK|DEPRECATED|deprecated|legacy_|workaround|shim" \
  core tools android packages test \
  --glob '!build*' --glob '!node_modules' --glob '!external' --glob '!third_party' -S
```

The narrowed source/test baseline contains 830 matching lines. The largest
source cluster is `core/view/src/widget_bridge.cpp` with 46 matching lines; most
of that cluster is web-compat and bridge-shim commentary, not open work.

## Behavior Gaps

These markers describe missing behavior or follow-up implementation work.

| Area | Anchor | Inventory note |
| --- | --- | --- |
| Android bridge bootstrap | `core/platform/src/android/jni_bridge.cpp:102` | Native init still logs startup but does not initialize engine subsystems. Needs owner before Android app lifecycle work expands. |
| Android memory pressure | `core/platform/src/android/lifecycle.cpp:22`, `:29`, `:30`, `:35` | Tiered memory callbacks exist, but actual GPU cache, texture atlas, sample cache, and DSP buffer release hooks are placeholders. |
| Android audio route recovery | `core/audio/platform/android/oboe_device.cpp:306`, `:312` | Oboe restart keeps prior sample rate/buffer size and lacks AudioManager device classification. |
| Android GPU policy | `android/app/src/main/kotlin/com/pulp/render/GpuDriverPolicy.kt:43` | Vulkan allow/deny policy needs real native GPU identity rather than preference/crash flags only. |
| Linux accessibility | `core/view/platform/linux/accessibility_linux.cpp:67-71`, `:113` | AT-SPI process registration exists, but per-view AtkObject, text/value roles, focus/value signals, and child-change events remain unwired. |
| Web MIDI output | `core/midi/src/web_midi.cpp:126` | `create_output()` returns `nullptr`; input exists but browser MIDI output is not implemented. |
| WidgetBridge animation | `core/view/src/widget_bridge.cpp:2105` | `animate()` applies final values immediately; FrameClock/value interpolation is not integrated. |
| WidgetBridge SVG path drawing | `core/view/src/widget_bridge.cpp:6912` | SVG-like path bridge entry parses arguments but does not render through CanvasWidget. |
| View skew transform | `core/view/src/view.cpp:224` | Skew is approximated by existing transform behavior; proper canvas skew/matrix support is still missing. |
| Bridge-native JS anchors | `core/view/src/design_codegen.cpp:650-654` | Web-compat codegen emits anchor trail, but bridge-native JS still lacks `setAnchor` dispatch in all create paths. |
| WAM host surface | `core/format/src/wasm/wam-plugin.js:43`, `:47`, `:121-123`, `:229` | State serialization, inter-plugin event routing, and parameter readback remain placeholder methods. |

## Intentional No-Ops

These markers should not be treated as bugs without new product scope.

| Area | Anchor | Inventory note |
| --- | --- | --- |
| Linux accessibility without AT-SPI | `core/view/platform/linux/accessibility_linux.cpp:80-100`, `:103-109` | Minimal/headless Linux builds intentionally keep a loadable no-op path when AT-SPI is unavailable. |
| CSS transform 3D forms | `core/view/js/web-compat-style-decl-transform.js:24-28` | `rotateX`, `rotateY`, `matrix3d`, and `perspective` are silent no-ops because the 2D View model has no 3D transform storage. |
| Browser compatibility listener aliases | `core/view/js/web-compat.js:1888-1889` | Deprecated `MediaQueryList.addListener/removeListener` aliases are expected by imported bundles and intentionally map to `onchange`. |

## Compatibility Shims To Keep

These markers preserve installed SDK, import, host, or web-compat behavior.
They need clear ownership, not cleanup.

| Area | Anchor | Inventory note |
| --- | --- | --- |
| DesignIR legacy bare-node parsing | `core/view/include/pulp/view/design_ir.hpp:415-422`, `core/view/src/design_ir_json.cpp:1397-1402`, `:2092-2097` | `legacy_field_shortcut` diagnostics document accepted legacy JSON shapes. Keep until external fixtures no longer rely on bare-node DesignIR input. |
| Codegen mode alias | `core/view/include/pulp/view/design_codegen.hpp:20-26` | `CodeGenMode::native` is a deprecated alias for `bridge_native_js`; keep for source compatibility while callers migrate. |
| Hosted editor void-pointer API | `core/host/include/pulp/host/plugin_slot.hpp:117-122`, `:148-176` | Deprecated `create_editor_view()` / `destroy_editor_view()` remain as fallback for existing slots while typed hosted-editor paths migrate. |
| Binding polling | `core/state/include/pulp/state/binding.hpp:132-140`, `core/view/include/pulp/view/param_attachment.hpp:121-126` | Polling is soft-deprecated but intentionally available for hosts without a vblank-driven render loop. |
| iOS deprecated UIKit calls | `core/platform/platform/ios/environment_ios.mm:48-109` | Deprecated API use is guarded for availability/backward compatibility; do not remove without a deployment-target audit. |
| Host quirks/workarounds | `core/format/src/host_quirks.cpp:321`, `core/format/include/pulp/format/host_quirks/bitwig.hpp:36-39` | Host-specific workarounds are compatibility policy, not cleanup debt. Validator/DAW evidence should govern changes. |
| Web-compat and React shims | `core/view/src/claude_bundle.cpp:743-814`, `tools/import-design/jsx-runtime/pulp-react-dom-shim.mjs:1-16`, `packages/pulp-react/src/host-config.ts:443-473` | Import/runtime shims bridge browser assumptions into Pulp. Keep unless a generated contract replaces them. |

## Deprecated Paths To Remove Later

These paths are removal candidates only after their replacement is universal.

| Area | Anchor | Exit condition |
| --- | --- | --- |
| Host editor void-pointer overrides | `core/host/include/pulp/host/plugin_slot.hpp:117-122` | Remove after every format slot overrides `create_hosted_editor()` and downstream hosted-editor tests no longer exercise the fallback. |
| `Binding::poll()` and `poll_bindings()` | `core/state/include/pulp/state/binding.hpp:132-140`, `core/view/include/pulp/view/param_attachment.hpp:121-126` | Remove only after every UI path has dirty-flag/vblank invalidation and hosts without `RenderLoop` have an alternate sync hook. |
| `CodeGenMode::native` alias | `core/view/include/pulp/view/design_codegen.hpp:20-26` | Remove after downstream codegen users are migrated to `bridge_native_js` and SDK compatibility policy permits a source break. |

## Historical And Test Artifacts

These should usually be ignored by implementation agents unless a related test
is failing.

| Area | Anchor | Inventory note |
| --- | --- | --- |
| Generated example TODO comments | `core/view/src/design_cpp_codegen.cpp:986`, `:998`, `:1026` and matching tests | These strings intentionally appear in generated C++ fixtures such as unbound knobs/meters. They are user-facing placeholders, not repo implementation TODOs. |
| Import-detect placeholder version | `tools/import-design/import_detect.cpp:541` | `TODO-set-version` is a fixture/sentinel candidate version string used by detection tests. |
| Web-compat orientation regression note | `core/view/js/web-compat-element.js:262-263` | The marker names a missing regression test, not a runtime behavior gap in the orientation resolver. |
| Browser-host demo placeholders | `tools/browser-host/index.html:225`, `:257` | Browser-host is a prototype/demo surface. Track separately from framework runtime debt. |
| Template component TODOs | `tools/templates/effect/processor.hpp.template:55`, `tools/add-component.py:81` | These are intentional scaffold prompts for users or component generation, not framework work items. |

## Follow-Up Ownership

- Android behavior gaps should stay with the Android platform lane and be
  validated with emulator/device smoke tests.
- Linux accessibility gaps should stay with the accessibility/platform lane and
  require AT-SPI/Orca-level validation, not just C++ unit tests.
- WidgetBridge animation/SVG gaps should be handled after the bridge manifest
  and registrar split, so moved registrations remain covered.
- Compatibility shims should be removed only through a migration PR that updates
  downstream validation expectations and SDK/source-compat notes.

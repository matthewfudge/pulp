# Moonbase Activation — Pulp licensing reference

A minimal Pulp effect that gates its audio on a [Moonbase](https://github.com/Moonbase-sh/moonbase-cpp)
license, with a **Pulp-native activation panel** (no WebView). When unlicensed it
outputs silence; once activated it passes audio through.

This is the reference integration for using Moonbase — a third-party
copyright-protection / license-activation service — to commercialize plugins
built on Pulp. Full guide: [`docs/guides/copyright-protection.md`](../../docs/guides/copyright-protection.md).

## What it shows

- **Drop-in upstream SDK, no fork.** Moonbase is consumed via `FetchContent` at a
  pinned tag (`v3.3.0`). Pulp owns no copy of the SDK.
- **Pulp HTTP transport, no libcurl.** `PulpMoonbaseHttpTransport` implements
  Moonbase's injected `http_transport` over Pulp's bundled `cpp-httplib`/mbedTLS
  stack (`MOONBASE_USE_CURL=OFF`).
- **RS256 validation via OpenSSL** (the upstream default). OpenSSL is required +
  linked by upstream v3.3.0 regardless, so v1 uses it directly; OS-native crypto
  (Security.framework / CNG) is a future option once upstream makes OpenSSL
  conditional on the backend.
- **Audio-thread gating** on a single `std::atomic<bool>` — the only thing the
  audio thread reads. All network/validation runs off the audio thread, owned by
  Moonbase.
- **`moonbase-pulp` User-Agent.** The integration tags its requests
  `moonbase-pulp/<version> (GenerousCorp Pulp; <OS>)`, mirroring Moonbase's JUCE
  module so they can attribute Pulp-driven traffic.
- **Theme-driven UI.** The panel resolves all colors from `pulp::view::Theme`
  tokens (Ink & Signal) and inherits the host plugin/app theme — no hard-coded
  Moonbase palette.

## Build & test

It is **off by default** (it fetches the Moonbase SDK and needs OpenSSL at
configure time — an upstream v3.3.0 CMake requirement).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPULP_BUILD_MOONBASE_EXAMPLE=ON
cmake --build build --target pulp-moonbase-activation-test
ctest --test-dir build -R moonbase --output-on-failure
```

OpenSSL is auto-discovered (Homebrew `openssl@3` on macOS); override with
`-DOPENSSL_ROOT_DIR=...`. For an offline build, point at a local checkout with
`-DMOONBASE_LOCAL_DIR=/path/to/moonbase-cpp` (a sibling `../moonbase-cpp` is
detected automatically).

## Use it for real

Replace the demo `endpoint`, `product_id`, and `public_key` in
`moonbase_activation_plugin.hpp` with your Moonbase product's values. The demo
public key is a throwaway key that only lets the SDK construct; it cannot
validate real Moonbase tokens.

## Files

| File | Role |
|------|------|
| `moonbase_pulp_transport.hpp` | `http_transport` over Pulp's cpp-httplib/mbedTLS |
| `moonbase_activation_controller.hpp` | Thin router over the Moonbase SDK + the `licensed` atomic + `moonbase-pulp` User-Agent |
| `moonbase_activation_view.hpp` | Native, theme-driven activation panel |
| `moonbase_activation_plugin.hpp` | The gated `Processor` |
| `test_moonbase_activation.cpp` | Network-free validation (User-Agent, gating, state machine, render) |

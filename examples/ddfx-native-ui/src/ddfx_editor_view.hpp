#pragma once
// Factory for the native Dream Date FX editor view (a pulp::view::DesignFrameView
// subclass). Defined in view.cpp, with NO dependency on any Processor, so the
// same builder serves both this sandbox (KnobProcessor::create_view) and the
// DreamDateFX JUCE plugin (mounted via pulp_embed_create_from_view). Loads the
// captured editor SVG, wires the controls, and returns the view (or nullptr if
// the DDFX_SVG asset is unavailable).

#include <pulp/view/view.hpp>
#include <memory>

namespace knobpg {
struct EffectHost;   // ddfx_host.hpp — routes structural effect actions to the engine
// `host` connects the view to the JUCE plugin's effect chain; null = standalone
// sandbox (rack is visual-only).
std::unique_ptr<pulp::view::View> make_ddfx_editor_view(EffectHost* host = nullptr);
}

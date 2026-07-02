#pragma once

// Minimal interface so the custom Pulp standalone window (PulpStandaloneApp.cpp)
// can drive the editor's fit-to-window scaling WITHOUT pulling in the whole
// PluginEditor.h include tree. InstrumentEditor implements it; the standalone
// window dynamic_casts the AudioProcessorEditor* to this and calls applyWindowScale.
// Intentionally dependency-free (no JUCE) so it's cheap to include on both sides.

namespace ddd
{
struct ScalableEditorHost
{
    virtual ~ScalableEditorHost() = default;

    /** Scale the editor to fit an available area (uniform, letterboxed). */
    virtual void applyWindowScale (int availableWidth, int availableHeight) = 0;
};
} // namespace ddd

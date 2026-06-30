// MoonbaseActivationPlugin — a minimal Pulp effect that is gated on a Moonbase
// license. When unlicensed it outputs silence; once activated it passes audio
// through. The editor is the Pulp-native activation panel (no WebView).
//
// This is a reference/validation example: replace the demo endpoint, product
// id, and public key with your Moonbase product's values. The public key below
// is a throwaway key that only lets the SDK construct — it cannot validate real
// Moonbase tokens.

#pragma once

#include "moonbase_activation_controller.hpp"
#include "moonbase_activation_view.hpp"

#include <pulp/format/processor.hpp>
#include <pulp/view/theme.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace moonbase_pulp {

// Replace these three with your Moonbase product's real values.
inline const char* demo_endpoint() { return "https://demo.moonbase.sh"; }
inline const char* demo_product_id() { return "pulp-moonbase-demo"; }
inline const char* demo_public_key()
{
    // A well-formed (throwaway) RS256 public key so the SDK constructs. Swap in
    // your product's real public key for live activation.
    return
        "-----BEGIN PUBLIC KEY-----\n"
        "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA08gy3e+K/RqcVWMWQVzh\n"
        "MYBSYn6OninSbQ4Zx5qFGfyxfRqMIWp+Gq3k0cCH94GjjGAajcwuQmAtTba3/C9m\n"
        "0NIqDUWN0eLldY5U7qN8+ZP+BnULvFIB592dfwJlqXJlWCKMEsZHDJn+KwjTL69R\n"
        "GFTPnC2ym7eq2g91LEB52zfzDzyd6LuIn4I+yPse/glUNBdDUWD4XVU1oZzE1Ay3\n"
        "7z5jBPUhBZSjMAsHCr+vADNhQzlemjTDF5HIH8gusmoD1XUWUNNCfB0nMsurbSt/\n"
        "+8gK4Bb1KCJrgD8j5VTDv04U/A/xne7mZhnh+msA5D6IXxx4rrFBFq0/ramPpewF\n"
        "ZwIDAQAB\n"
        "-----END PUBLIC KEY-----\n";
}

inline ActivationConfig demo_config()
{
    ActivationConfig config;
    config.endpoint = demo_endpoint();
    config.product_id = demo_product_id();
    config.public_key = demo_public_key();
    // Demo path (relative to the working directory). A real plugin should persist
    // under a per-user data directory, e.g. ~/Library/Application Support/<vendor>/.
    config.license_path = "moonbase-activation-demo.mb";
    return config;
}

class MoonbaseActivationPlugin : public pulp::format::Processor {
public:
    MoonbaseActivationPlugin() : controller_(demo_config()) {}

    explicit MoonbaseActivationPlugin(ActivationConfig config) : controller_(std::move(config)) {}

    pulp::format::PluginDescriptor descriptor() const override
    {
        return {"Moonbase Activation", "GenerousCorp", "com.generouscorp.moonbase-activation",
                "1.0.0", pulp::format::PluginCategory::Effect};
    }

    void define_parameters(pulp::state::StateStore&) override {}

    void prepare(const pulp::format::PrepareContext&) override {}

    void process(pulp::audio::BufferView<float>& out,
                 const pulp::audio::BufferView<const float>& in,
                 pulp::midi::MidiBuffer&, pulp::midi::MidiBuffer&,
                 const pulp::format::ProcessContext&) override
    {
        // The only audio-thread interaction with Moonbase: a single relaxed
        // atomic read. Unlicensed → silence; licensed → pass through.
        const bool licensed = controller_.licensed().load(std::memory_order_relaxed);
        const std::size_t channels = out.num_channels();
        const std::size_t frames = out.num_samples();
        for (std::size_t c = 0; c < channels; ++c) {
            auto o = out.channel(c);
            if (licensed && c < in.num_channels()) {
                auto i = in.channel(c);
                const std::size_t n = frames < i.size() ? frames : i.size();
                for (std::size_t s = 0; s < n; ++s) o[s] = i[s];
                for (std::size_t s = n; s < frames; ++s) o[s] = 0.0f;
            } else {
                for (auto& s : o) s = 0.0f;
            }
        }
    }

    pulp::format::ViewSize view_size() const override { return {420, 280, 360, 240, 640, 420}; }

    std::unique_ptr<pulp::view::View> create_view() override
    {
        // The panel is built for the controller's current screen. This reference
        // builds it once; a production editor rebuilds (or refreshes) it when the
        // controller transitions — e.g. drive poll_once() from a UI timer and
        // rebuild on a screen change.
        return build_activation_view(controller_, pulp::view::Theme::dark());
    }

    void on_view_opened(pulp::view::View&) override { controller_.start(); }

    MoonbaseActivationController& controller() noexcept { return controller_; }
    const MoonbaseActivationController& controller() const noexcept { return controller_; }

private:
    MoonbaseActivationController controller_;
};

} // namespace moonbase_pulp

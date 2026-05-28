#include "chainer-phase-f-hybrid.hpp"

#include <pulp/audio/buffer.hpp>
#include <pulp/format/processor.hpp>
#include <pulp/format/standalone.hpp>
#include <pulp/midi/buffer.hpp>
#include <pulp/runtime/log.hpp>
#include <pulp/view/design_import.hpp>
#include <pulp/view/view.hpp>
#include <pulp/view/widgets.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

class PreviewBindingContext final : public pulp::view::NativeImportBindingContext {
public:
    void reset() {
        choices_.clear();
        waveform_displays_.clear();
        meters_.clear();
        values_.clear();
    }

    void bind_knob(pulp::view::Knob& knob,
                   const pulp::view::NativeImportBindingDescriptor& descriptor) override {
        const auto key = std::string(descriptor.param_key);
        values_[key] = knob.value();
        knob.on_change = [this, key](float value) {
            values_[key] = value;
        };
    }

    void bind_fader(pulp::view::Fader& fader,
                    const pulp::view::NativeImportBindingDescriptor& descriptor) override {
        const auto key = std::string(descriptor.param_key);
        values_[key] = fader.value();
        fader.on_change = [this, key](float value) {
            values_[key] = value;
        };
    }

    void bind_xy_pad(pulp::view::XYPad& pad,
                     const pulp::view::NativeImportXYPadBindingDescriptor& descriptor) override {
        const auto x_key = std::string(descriptor.x_param_key);
        const auto y_key = std::string(descriptor.y_param_key);
        values_[x_key] = pad.x_value();
        values_[y_key] = pad.y_value();
        pad.on_change = [this, x_key, y_key](float x, float y) {
            values_[x_key] = x;
            values_[y_key] = y;
        };
    }

    void bind_toggle_button(pulp::view::ToggleButton& button,
                            const pulp::view::NativeImportBindingDescriptor& descriptor) override {
        const auto key = std::string(descriptor.param_key);
        values_[key] = button.is_on() ? 1.0f : 0.0f;
        button.on_toggle = [this, key](bool on) {
            values_[key] = on ? 1.0f : 0.0f;
        };
    }

    void bind_choice_button(pulp::view::ToggleButton& button,
                            const pulp::view::NativeImportChoiceBindingDescriptor& descriptor) override {
        choices_.push_back({
            std::string(descriptor.param_key),
            std::string(descriptor.choice_value),
            &button,
        });
        if (button.is_on())
            choice_values_[std::string(descriptor.param_key)] = std::string(descriptor.choice_value);

        const auto key = std::string(descriptor.param_key);
        const auto value = std::string(descriptor.choice_value);
        button.on_toggle = [this, key, value](bool) {
            select_choice(key, value);
        };
    }

    void bind_meter(pulp::view::Meter& meter,
                    const pulp::view::NativeImportMeterBindingDescriptor& descriptor) override {
        meters_.push_back({std::string(descriptor.channel), &meter});
    }

    void bind_waveform_display(
        pulp::view::WaveformView& waveform,
        const pulp::view::NativeImportWaveformBindingDescriptor& descriptor) override {
        waveform_displays_.push_back({std::string(descriptor.param_key), &waveform});
    }

    void prime_dynamic_surfaces() {
        for (auto& meter : meters_) {
            if (meter.widget == nullptr)
                continue;
            if (meter.channel == "R")
                meter.widget->set_level(0.42f, 0.50f);
            else
                meter.widget->set_level(0.78f, 0.88f);
        }
    }

private:
    struct Choice {
        std::string key;
        std::string value;
        pulp::view::ToggleButton* button = nullptr;
    };

    struct Meter {
        std::string channel;
        pulp::view::Meter* widget = nullptr;
    };

    struct WaveformDisplay {
        std::string key;
        pulp::view::WaveformView* widget = nullptr;
    };

    void select_choice(std::string_view key, std::string_view value) {
        for (auto& choice : choices_) {
            if (choice.key != key || choice.button == nullptr)
                continue;
            choice.button->set_on(choice.value == value);
        }
        choice_values_[std::string(key)] = std::string(value);
        for (auto& display : waveform_displays_) {
            if (display.key == key && display.widget != nullptr)
                display.widget->set_preview_shape(value);
        }
    }

    std::vector<Choice> choices_;
    std::vector<Meter> meters_;
    std::vector<WaveformDisplay> waveform_displays_;
    std::unordered_map<std::string, float> values_;
    std::unordered_map<std::string, std::string> choice_values_;
};

class PreviewProcessor final : public pulp::format::Processor {
public:
    pulp::format::PluginDescriptor descriptor() const override {
        return {
            .name = "Pulp Native UI Phase F Preview",
            .manufacturer = "Pulp",
            .bundle_id = "com.pulp.native-ui-phase-f-preview",
            .version = "0.1.0",
            .category = pulp::format::PluginCategory::Effect,
            .input_buses = {{"Audio In", 2}},
            .output_buses = {{"Audio Out", 2}},
        };
    }

    void define_parameters(pulp::state::StateStore&) override {}
    void prepare(const pulp::format::PrepareContext&) override {}

    void process(pulp::audio::BufferView<float>& output,
                 const pulp::audio::BufferView<const float>&,
                 pulp::midi::MidiBuffer&,
                 pulp::midi::MidiBuffer&,
                 const pulp::format::ProcessContext&) override {
        for (std::size_t channel = 0; channel < output.num_channels(); ++channel) {
            auto out = output.channel(channel);
            std::fill(out.begin(), out.end(), 0.0f);
        }
    }

    pulp::format::ViewSize view_size() const override {
        return {1280, 800, 900, 560, 1920, 1200};
    }

    std::unique_ptr<pulp::view::View> create_view() override {
        binding_context_.reset();
        auto root = pulp::test::phase_f_chainer_hybrid::build_chainer_phase_f_hybrid_ui();
        if (root != nullptr) {
            pulp::test::phase_f_chainer_hybrid::bind_chainer_phase_f_hybrid_ui(
                *root, binding_context_);
            binding_context_.prime_dynamic_surfaces();
        }
        return root;
    }

private:
    PreviewBindingContext binding_context_;
};

std::unique_ptr<pulp::format::Processor> create_preview_processor() {
    return std::make_unique<PreviewProcessor>();
}

}  // namespace

int main(int argc, char** argv) {
    pulp::runtime::log_info("Pulp Native UI Phase F Preview");

    pulp::format::StandaloneConfig config;
    config.output_channels = 2;
    config.input_channels = 0;
    config.show_settings_tab = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        constexpr std::string_view prefix = "--screenshot=";
        if (arg.rfind(prefix, 0) == 0) {
            config.headless = true;
            config.screenshot_path = arg.substr(prefix.size());
            config.screenshot_frame_delay = 6;
        }
    }

    pulp::format::StandaloneApp app(create_preview_processor);
    app.set_config(config);
    return app.run_with_editor(true) ? 0 : 1;
}

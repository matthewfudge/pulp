#include "chainer_native_ui_processor.hpp"

#include <pulp/view/buttons.hpp>
#include <pulp/view/text_editor.hpp>
#include <pulp/view/widgets.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>

namespace pulp::examples {
namespace {

enum ChainerParam : state::ParamID {
    kOscFreq = 1,
    kOscDetune,
    kOscShape,
    kOscWaveform,
    kEnvAttack,
    kEnvDecay,
    kEnvSustain,
    kEnvRelease,
    kXoverLo,
    kXoverHi,
    kMidBypass,
    kSideBypass,
    kMidWidth,
    kSideWidth,
    kSendLevel,
    kReturnLevel,
    kMasterOut,
    kSelectedModule,
};

struct ParamMap {
    std::string_view key;
    state::ParamID id;
};

constexpr std::array<ParamMap, 18> kParamMap{{
    {"osc_freq", kOscFreq},
    {"osc_detune", kOscDetune},
    {"osc_shape", kOscShape},
    {"osc_waveform", kOscWaveform},
    {"env_a", kEnvAttack},
    {"env_d", kEnvDecay},
    {"env_s", kEnvSustain},
    {"env_r", kEnvRelease},
    {"xover_lo", kXoverLo},
    {"xover_hi", kXoverHi},
    {"mid_bypass", kMidBypass},
    {"side_bypass", kSideBypass},
    {"ms_mid_width", kMidWidth},
    {"ms_side_width", kSideWidth},
    {"send_level", kSendLevel},
    {"return_level", kReturnLevel},
    {"master_out", kMasterOut},
    {"selected_mod", kSelectedModule},
}};

state::ParamID param_id_for_key(std::string_view key) {
    for (const auto& entry : kParamMap) {
        if (entry.key == key)
            return entry.id;
    }
    return 0;
}

float choice_value_for(std::string_view key, std::string_view value) {
    if (key == "osc_waveform") {
        if (value == "saw") return 0.0f;
        if (value == "sine") return 1.0f;
        if (value == "square") return 2.0f;
        if (value == "tri") return 3.0f;
    }
    if (key == "selected_mod") {
        if (value == "OSC") return 0.0f;
        if (value == "ENV") return 1.0f;
        if (value == "XOVER") return 2.0f;
        if (value == "FILT") return 3.0f;
        if (value == "DIST") return 4.0f;
        if (value == "MS" || value == "M/S") return 5.0f;
        if (value == "SUM") return 6.0f;
        if (value == "LIMIT") return 7.0f;
        if (value == "OUT") return 8.0f;
    }
    return 0.0f;
}

void add_normalized(state::StateStore& store,
                    state::ParamID id,
                    std::string name,
                    float default_value) {
    store.add_parameter({
        .id = id,
        .name = std::move(name),
        .unit = "",
        .range = {0.0f, 1.0f, std::clamp(default_value, 0.0f, 1.0f), 0.0f},
    });
}

}  // namespace

class ChainerNativeUiProcessor::BindingContext final
    : public view::NativeImportBindingContext {
public:
    void set_store(state::StateStore& store) { store_ = &store; }

    void reset() {
        choices_.clear();
        meters_.clear();
        waveform_displays_.clear();
        text_values_.clear();
        host_action_counts_.clear();
    }

    void bind_knob(view::Knob& knob,
                   const view::NativeImportBindingDescriptor& descriptor) override {
        const auto id = param_id_for_key(descriptor.param_key);
        if (store_ != nullptr && id != 0)
            knob.set_value(store_->get_normalized(id));
        knob.on_change = [this, id](float value) {
            if (store_ != nullptr && id != 0)
                store_->set_normalized(id, value);
        };
    }

    void bind_fader(view::Fader& fader,
                    const view::NativeImportBindingDescriptor& descriptor) override {
        const auto id = param_id_for_key(descriptor.param_key);
        if (store_ != nullptr && id != 0)
            fader.set_value(store_->get_normalized(id));
        fader.on_change = [this, id](float value) {
            if (store_ != nullptr && id != 0)
                store_->set_normalized(id, value);
        };
    }

    void bind_xy_pad(view::XYPad& pad,
                     const view::NativeImportXYPadBindingDescriptor& descriptor) override {
        const auto x_id = param_id_for_key(descriptor.x_param_key);
        const auto y_id = param_id_for_key(descriptor.y_param_key);
        if (store_ != nullptr) {
            if (x_id != 0)
                pad.set_x(store_->get_normalized(x_id));
            if (y_id != 0)
                pad.set_y(store_->get_normalized(y_id));
        }
        pad.on_change = [this, x_id, y_id](float x, float y) {
            if (store_ == nullptr)
                return;
            if (x_id != 0)
                store_->set_normalized(x_id, x);
            if (y_id != 0)
                store_->set_normalized(y_id, y);
        };
    }

    void bind_toggle_button(view::ToggleButton& button,
                            const view::NativeImportBindingDescriptor& descriptor) override {
        const auto id = param_id_for_key(descriptor.param_key);
        if (store_ != nullptr && id != 0)
            button.set_on(store_->get_value(id) >= 0.5f);
        button.on_toggle = [this, id](bool on) {
            if (store_ != nullptr && id != 0)
                store_->set_value(id, on ? 1.0f : 0.0f);
        };
    }

    void bind_choice_button(view::ToggleButton& button,
                            const view::NativeImportChoiceBindingDescriptor& descriptor) override {
        choices_.push_back({
            std::string(descriptor.param_key),
            std::string(descriptor.choice_value),
            &button,
        });

        const auto key = std::string(descriptor.param_key);
        const auto value = std::string(descriptor.choice_value);
        button.on_toggle = [this, key, value](bool) {
            select_choice(key, value);
        };
    }

    void bind_meter(view::Meter& meter,
                    const view::NativeImportMeterBindingDescriptor& descriptor) override {
        meters_.push_back({std::string(descriptor.channel), &meter});
    }

    void bind_waveform_display(
        view::WaveformView& waveform,
        const view::NativeImportWaveformBindingDescriptor& descriptor) override {
        waveform_displays_.push_back({std::string(descriptor.param_key), &waveform});
    }

    void bind_text_editor(view::TextEditor& editor,
                          const view::NativeImportTextBindingDescriptor& descriptor) override {
        const auto key = std::string(descriptor.value_key);
        if (!key.empty())
            text_values_[key] = editor.text();
        editor.on_change = [this, key](const std::string& text) {
            if (!key.empty())
                text_values_[key] = text;
        };
    }

    void bind_host_action(view::TextButton& button,
                          const view::NativeImportHostActionDescriptor& descriptor) override {
        const auto action = std::string(descriptor.action);
        host_action_counts_.try_emplace(action, 0);
        button.on_click = [this, action] {
            ++host_action_counts_[action];
        };
    }

    void sync_choices_from_store() {
        if (store_ == nullptr)
            return;
        for (auto& choice : choices_) {
            auto* button = choice.button;
            if (button == nullptr)
                continue;
            const auto id = param_id_for_key(choice.key);
            const auto expected = choice_value_for(choice.key, choice.value);
            const auto current = id == 0 ? 0.0f : store_->get_value(id);
            button->set_on(std::abs(current - expected) < 0.5f);
        }
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
        view::ToggleButton* button = nullptr;
    };

    struct Meter {
        std::string channel;
        view::Meter* widget = nullptr;
    };

    struct WaveformDisplay {
        std::string key;
        view::WaveformView* widget = nullptr;
    };

    void select_choice(std::string_view key, std::string_view value) {
        for (auto& choice : choices_) {
            if (choice.key != key || choice.button == nullptr)
                continue;
            choice.button->set_on(choice.value == value);
        }
        for (auto& display : waveform_displays_) {
            if (display.key == key && display.widget != nullptr)
                display.widget->set_preview_shape(value);
        }
        if (store_ != nullptr) {
            const auto id = param_id_for_key(key);
            if (id != 0)
                store_->set_value(id, choice_value_for(key, value));
        }
    }

    state::StateStore* store_ = nullptr;
    std::vector<Choice> choices_;
    std::vector<Meter> meters_;
    std::vector<WaveformDisplay> waveform_displays_;
    std::unordered_map<std::string, std::string> text_values_;
    std::unordered_map<std::string, int> host_action_counts_;
};

format::PluginDescriptor ChainerNativeUiProcessor::descriptor() const {
    return {
        .name = "PulpChainerNativeUi",
        .manufacturer = "Pulp",
        .bundle_id = "com.pulp.chainer-native-ui",
        .version = "0.1.0",
        .category = format::PluginCategory::Effect,
        .input_buses = {{"Audio In", 2}},
        .output_buses = {{"Audio Out", 2}},
        .accepts_midi = false,
        .produces_midi = false,
        .tail_samples = 0,
    };
}

void ChainerNativeUiProcessor::define_parameters(state::StateStore& store) {
    add_normalized(store, kOscFreq, "OSC Frequency", 0.32f);
    add_normalized(store, kOscDetune, "OSC Detune", 0.42f);
    add_normalized(store, kOscShape, "OSC Shape", 0.62f);
    store.add_parameter({.id = kOscWaveform,
                         .name = "OSC Waveform",
                         .unit = "",
                         .range = {0.0f, 3.0f, 0.0f, 1.0f}});
    add_normalized(store, kEnvAttack, "Envelope Attack", 0.18f);
    add_normalized(store, kEnvDecay, "Envelope Decay", 0.44f);
    add_normalized(store, kEnvSustain, "Envelope Sustain", 0.76f);
    add_normalized(store, kEnvRelease, "Envelope Release", 0.30f);
    add_normalized(store, kXoverLo, "Crossover Low", 0.24f);
    add_normalized(store, kXoverHi, "Crossover High", 0.68f);
    store.add_parameter({.id = kMidBypass,
                         .name = "Mid",
                         .unit = "",
                         .range = {0.0f, 1.0f, 1.0f, 1.0f}});
    store.add_parameter({.id = kSideBypass,
                         .name = "Side",
                         .unit = "",
                         .range = {0.0f, 1.0f, 0.0f, 1.0f}});
    add_normalized(store, kMidWidth, "Mid Width", 0.55f);
    add_normalized(store, kSideWidth, "Side Width", 0.48f);
    add_normalized(store, kSendLevel, "Send Level", 0.30f);
    add_normalized(store, kReturnLevel, "Return Level", 0.60f);
    add_normalized(store, kMasterOut, "Master Out", 0.58f);
    store.add_parameter({.id = kSelectedModule,
                         .name = "Selected Module",
                         .unit = "",
                         .range = {0.0f, 8.0f, 6.0f, 1.0f}});
}

void ChainerNativeUiProcessor::process(audio::BufferView<float>& output,
                                       const audio::BufferView<const float>& input,
                                       midi::MidiBuffer&,
                                       midi::MidiBuffer&,
                                       const format::ProcessContext&) {
    const auto channels = std::min(output.num_channels(), input.num_channels());
    for (std::size_t ch = 0; ch < channels; ++ch) {
        auto* out = output.channel_ptr(ch);
        if (out == nullptr)
            continue;
        const auto* in = input.channel_ptr(ch);
        if (in == nullptr) {
            std::fill_n(out, output.num_samples(), 0.0f);
            continue;
        }
        std::copy_n(in, output.num_samples(), out);
    }
    for (std::size_t ch = channels; ch < output.num_channels(); ++ch) {
        auto* out = output.channel_ptr(ch);
        if (out != nullptr)
            std::fill_n(out, output.num_samples(), 0.0f);
    }
}

std::unique_ptr<view::View> ChainerNativeUiProcessor::create_view() {
    auto root = test::phase_f_chainer_hybrid::build_chainer_phase_f_hybrid_ui();
    if (!root)
        return nullptr;

    auto& ctx = binding_context();
    ctx.reset();
    ctx.set_store(state());
    test::phase_f_chainer_hybrid::bind_chainer_phase_f_hybrid_ui(*root, ctx);
    ctx.sync_choices_from_store();
    ctx.prime_dynamic_surfaces();
    root->set_requires_gpu_host(true);
    return root;
}

void ChainerNativeUiProcessor::on_view_closed(view::View&) {
    if (binding_context_)
        binding_context_->reset();
}

ChainerNativeUiProcessor::BindingContext& ChainerNativeUiProcessor::binding_context() {
    if (!binding_context_)
        binding_context_ = std::make_unique<BindingContext>();
    return *binding_context_;
}

std::unique_ptr<format::Processor> create_chainer_native_ui_processor() {
    return std::make_unique<ChainerNativeUiProcessor>();
}

}  // namespace pulp::examples

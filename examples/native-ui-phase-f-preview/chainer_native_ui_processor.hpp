#pragma once

#include "chainer-phase-f-hybrid.hpp"

#include <pulp/format/processor.hpp>
#include <pulp/view/design_import.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pulp::examples {

class ChainerNativeUiProcessor final : public format::Processor {
public:
    format::PluginDescriptor descriptor() const override;
    void define_parameters(state::StateStore& store) override;
    void prepare(const format::PrepareContext&) override {}
    void release() override {}
    void process(audio::BufferView<float>& output,
                 const audio::BufferView<const float>& input,
                 midi::MidiBuffer&,
                 midi::MidiBuffer&,
                 const format::ProcessContext&) override;

    std::pair<uint32_t, uint32_t> editor_size() const override { return {900, 520}; }
    std::unique_ptr<view::View> create_view() override;
    void on_view_closed(view::View&) override;

private:
    class BindingContext;

    BindingContext& binding_context();

    std::unique_ptr<BindingContext> binding_context_;
};

std::unique_ptr<format::Processor> create_chainer_native_ui_processor();

}  // namespace pulp::examples

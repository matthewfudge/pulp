#include <pulp/view/widget_bridge.hpp>
#include <cmath>

namespace pulp::view {

WidgetBridge::WidgetBridge(ScriptEngine& engine, View& root, state::StateStore& store)
    : engine_(engine), root_(root), store_(store) {
    register_api();
}

void WidgetBridge::load_script(const std::string& code) {
    engine_.evaluate(code);
}

View* WidgetBridge::widget(const std::string& id) {
    auto it = widgets_.find(id);
    return it != widgets_.end() ? it->second : nullptr;
}

void WidgetBridge::sync_from_store() {
    for (auto& [id, view] : widgets_) {
        if (auto* knob = dynamic_cast<Knob*>(view)) {
            // Try to find a parameter matching this widget's id
            // Convention: widget id matches parameter name
            for (size_t i = 0; i < store_.param_count(); ++i) {
                auto* info = &store_.all_params()[i];
                if (info && info->name == id) {
                    knob->set_value(store_.get_normalized(info->id));
                    break;
                }
            }
        } else if (auto* fader = dynamic_cast<Fader*>(view)) {
            for (size_t i = 0; i < store_.param_count(); ++i) {
                auto* info = &store_.all_params()[i];
                if (info && info->name == id) {
                    fader->set_value(store_.get_normalized(info->id));
                    break;
                }
            }
        } else if (auto* toggle = dynamic_cast<Toggle*>(view)) {
            for (size_t i = 0; i < store_.param_count(); ++i) {
                auto* info = &store_.all_params()[i];
                if (info && info->name == id) {
                    toggle->set_on(store_.get_normalized(info->id) > 0.5f);
                    break;
                }
            }
        }
    }
}

void WidgetBridge::register_api() {
    // createKnob(id, x, y, w, h) -> creates a Knob widget
    engine_.register_function("createKnob", [this](choc::javascript::ArgumentList args) {
        auto id = args.get<std::string>(0, "");
        auto x = args.get<double>(1, 0);
        auto y = args.get<double>(2, 0);
        auto w = args.get<double>(3, 48);
        auto h = args.get<double>(4, 48);

        auto knob = std::make_unique<Knob>();
        knob->set_bounds({static_cast<float>(x), static_cast<float>(y),
                         static_cast<float>(w), static_cast<float>(h)});
        knob->set_label(id);
        knob->set_id(id);

        auto* ptr = knob.get();
        widgets_[id] = ptr;
        root_.add_child(std::move(knob));

        return choc::value::createString(id);
    });

    // createFader(id, x, y, w, h, orientation) -> creates a Fader widget
    engine_.register_function("createFader", [this](choc::javascript::ArgumentList args) {
        auto id = args.get<std::string>(0, "");
        auto x = args.get<double>(1, 0);
        auto y = args.get<double>(2, 0);
        auto w = args.get<double>(3, 24);
        auto h = args.get<double>(4, 200);
        auto horiz = args.get<std::string>(5, "vertical");

        auto fader = std::make_unique<Fader>();
        fader->set_bounds({static_cast<float>(x), static_cast<float>(y),
                          static_cast<float>(w), static_cast<float>(h)});
        fader->set_label(id);
        fader->set_id(id);
        if (horiz == "horizontal")
            fader->set_orientation(Fader::Orientation::horizontal);

        auto* ptr = fader.get();
        widgets_[id] = ptr;
        root_.add_child(std::move(fader));

        return choc::value::createString(id);
    });

    // createToggle(id, x, y, w, h) -> creates a Toggle widget
    engine_.register_function("createToggle", [this](choc::javascript::ArgumentList args) {
        auto id = args.get<std::string>(0, "");
        auto x = args.get<double>(1, 0);
        auto y = args.get<double>(2, 0);
        auto w = args.get<double>(3, 50);
        auto h = args.get<double>(4, 30);

        auto toggle = std::make_unique<Toggle>();
        toggle->set_bounds({static_cast<float>(x), static_cast<float>(y),
                           static_cast<float>(w), static_cast<float>(h)});
        toggle->set_label(id);
        toggle->set_id(id);

        auto* ptr = toggle.get();
        widgets_[id] = ptr;
        root_.add_child(std::move(toggle));

        return choc::value::createString(id);
    });

    // createLabel(id, text, x, y, w, h) -> creates a Label widget
    engine_.register_function("createLabel", [this](choc::javascript::ArgumentList args) {
        auto id = args.get<std::string>(0, "");
        auto text = args.get<std::string>(1, "");
        auto x = args.get<double>(2, 0);
        auto y = args.get<double>(3, 0);
        auto w = args.get<double>(4, 100);
        auto h = args.get<double>(5, 20);

        auto label = std::make_unique<Label>(text);
        label->set_bounds({static_cast<float>(x), static_cast<float>(y),
                          static_cast<float>(w), static_cast<float>(h)});
        label->set_id(id);

        auto* ptr = label.get();
        widgets_[id] = ptr;
        root_.add_child(std::move(label));

        return choc::value::createString(id);
    });

    // setValue(id, value) -> set widget normalized value
    engine_.register_function("setValue", [this](choc::javascript::ArgumentList args) {
        auto id = args.get<std::string>(0, "");
        auto value = args.get<double>(1, 0);

        auto it = widgets_.find(id);
        if (it == widgets_.end()) return choc::value::Value();

        if (auto* knob = dynamic_cast<Knob*>(it->second))
            knob->set_value(static_cast<float>(value));
        else if (auto* fader = dynamic_cast<Fader*>(it->second))
            fader->set_value(static_cast<float>(value));
        else if (auto* toggle = dynamic_cast<Toggle*>(it->second))
            toggle->set_on(value > 0.5);

        return choc::value::Value();
    });

    // getValue(id) -> get widget normalized value
    engine_.register_function("getValue", [this](choc::javascript::ArgumentList args) {
        auto id = args.get<std::string>(0, "");

        auto it = widgets_.find(id);
        if (it == widgets_.end()) return choc::value::createFloat64(0);

        if (auto* knob = dynamic_cast<Knob*>(it->second))
            return choc::value::createFloat64(knob->value());
        if (auto* fader = dynamic_cast<Fader*>(it->second))
            return choc::value::createFloat64(fader->value());
        if (auto* toggle = dynamic_cast<Toggle*>(it->second))
            return choc::value::createFloat64(toggle->is_on() ? 1.0 : 0.0);

        return choc::value::createFloat64(0);
    });

    // getParam(name) -> get parameter value from store (normalized)
    engine_.register_function("getParam", [this](choc::javascript::ArgumentList args) {
        auto name = args.get<std::string>(0, "");

        for (size_t i = 0; i < store_.param_count(); ++i) {
            auto* info = &store_.all_params()[i];
            if (info && info->name == name) {
                return choc::value::createFloat64(store_.get_normalized(info->id));
            }
        }
        return choc::value::createFloat64(0);
    });

    // setParam(name, normalized_value) -> set parameter in store
    engine_.register_function("setParam", [this](choc::javascript::ArgumentList args) {
        auto name = args.get<std::string>(0, "");
        auto value = args.get<double>(1, 0);

        for (size_t i = 0; i < store_.param_count(); ++i) {
            auto* info = &store_.all_params()[i];
            if (info && info->name == name) {
                store_.set_normalized(info->id, static_cast<float>(value));
                break;
            }
        }
        return choc::value::Value();
    });
}

} // namespace pulp::view

#pragma once

#include <pulp/view/script_engine.hpp>
#include <pulp/view/view.hpp>
#include <pulp/view/widgets.hpp>
#include <pulp/view/theme.hpp>
#include <pulp/state/store.hpp>
#include <string>
#include <unordered_map>
#include <memory>

namespace pulp::view {

// Bridges JS scripts to the Pulp widget system
// Registers native functions that JS code calls to create and configure widgets
class WidgetBridge {
public:
    WidgetBridge(ScriptEngine& engine, View& root, state::StateStore& store);

    // Load and execute a UI script
    void load_script(const std::string& code);

    // Get a widget by its JS-assigned ID
    View* widget(const std::string& id);

    // Sync all widget values from the parameter store
    void sync_from_store();

private:
    ScriptEngine& engine_;
    View& root_;
    state::StateStore& store_;

    // Track widgets by ID for JS access
    std::unordered_map<std::string, View*> widgets_;

    void register_api();
};

} // namespace pulp::view

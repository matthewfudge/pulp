#include <catch2/catch_test_macros.hpp>
#include <pulp/view/view.hpp>
#include <pulp/view/script_engine.hpp>
#include <pulp/view/widget_bridge.hpp>
#include <pulp/view/theme.hpp>
#include <pulp/state/store.hpp>
#include <iostream>
using namespace pulp::view;
using namespace pulp;

struct Env {
    View root;
    state::StateStore store;
    ScriptEngine engine;
    std::unique_ptr<WidgetBridge> bridge;
    Env() {
        root.set_bounds({0,0,400,300});
        root.set_theme(Theme::dark());
        engine.set_log_callback([](std::string_view, std::string_view msg) {
            std::cerr << "[JS] " << msg << "\n";
        });
        bridge = std::make_unique<WidgetBridge>(engine, root, store);
    }
};

TEST_CASE("trace _flushAll detail") {
    Env e;
    // Monkey-patch _flushAll with logging
    e.bridge->load_script(
        "var origFlush = CSSStyleDeclaration.prototype._flushAll;"
        "CSSStyleDeclaration.prototype._flushAll = function() {"
        "  console.log('FLUSH: keys=' + Object.keys(this._props).join(','));"
        "  console.log('FLUSH: nativeCreated=' + this._el._nativeCreated);"
        "  origFlush.call(this);"
        "  console.log('FLUSH: done');"
        "};"
    );
    e.bridge->load_script(
        "var d = document.createElement('div');"
        "d.style.width = '200px';"
        "document.body.appendChild(d);"
    );
    auto id = std::string(e.engine.evaluate("d._id").getWithDefault<std::string_view>(""));
    auto* w = e.bridge->widget(id);
    REQUIRE(w != nullptr);
    std::cerr << "preferred_width = " << w->flex().preferred_width << "\n";
    REQUIRE(w->flex().preferred_width == 200.0f);
}

#include <catch2/catch_test_macros.hpp>
#include <pulp/view/web_view.hpp>

using namespace pulp::view;

TEST_CASE("WebViewPanel creation", "[view][webview]") {
    // WebView creation requires a message loop / GUI context.
    // In headless test environments, create() may return nullptr.
    // This test validates the API surface compiles and the factory works.

    SECTION("Default options") {
        WebViewOptions opts;
        REQUIRE(opts.enable_debug == false);
        REQUIRE(opts.transparent_background == false);
        REQUIRE(opts.accept_first_click == true);
        REQUIRE(opts.custom_user_agent.empty());
    }

    SECTION("Factory returns non-null in GUI context or null in headless") {
        auto panel = WebViewPanel::create();
        // In CI / headless: nullptr is expected
        // In GUI context: non-null
        // Either way, no crash
        if (panel) {
            REQUIRE(panel->native_handle() != nullptr);
        }
    }
}

TEST_CASE("WebViewPanel HTML content", "[view][webview]") {
    auto panel = WebViewPanel::create();
    if (!panel) {
        SKIP("WebView not available in headless environment");
        return;
    }

    SECTION("set_html does not crash") {
        panel->set_html("<html><body><h1>Test</h1></body></html>");
    }

    SECTION("evaluate_js does not crash") {
        panel->evaluate_js("1 + 1");
    }

    SECTION("navigate does not crash") {
        panel->navigate("about:blank");
    }

    SECTION("bind callback") {
        panel->bind("testFunc", [](const std::string& args) -> std::string {
            return "\"ok\"";
        });
    }
}

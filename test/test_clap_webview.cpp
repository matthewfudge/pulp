#include <catch2/catch_test_macros.hpp>
#include <pulp/format/web/clap_webview.hpp>
#include <string>

using namespace pulp::format::wclap;

TEST_CASE("WebviewProvider interface", "[format][webview]") {
    // Default WebviewProvider behavior
    class TestProvider : public WebviewProvider {
    public:
        WebviewContent get_webview_content() const override {
            return {
                .url = "",
                .html = "<h1>Test</h1>",
                .preferred_width = 500,
                .preferred_height = 400,
                .resizable = true,
            };
        }
    };

    TestProvider provider;
    auto content = provider.get_webview_content();
    REQUIRE(content.html == "<h1>Test</h1>");
    REQUIRE(content.preferred_width == 500);
    REQUIRE(content.preferred_height == 400);
    REQUIRE(content.resizable == true);
}

TEST_CASE("WebviewProvider message callback", "[format][webview]") {
    class TestProvider : public WebviewProvider {
    public:
        WebviewContent get_webview_content() const override {
            return { .html = "<p>hello</p>" };
        }
        std::string last_message;
        void on_webview_message(const std::string& msg) override {
            last_message = msg;
        }
    };

    TestProvider provider;
    provider.on_webview_message("{\"param\":\"gain\",\"value\":0.5}");
    REQUIRE(provider.last_message == "{\"param\":\"gain\",\"value\":0.5}");
}

TEST_CASE("generate_webview_html produces valid HTML", "[format][webview]") {
    std::string params_json = R"([
        {"id":"0","label":"Gain","type":"float","defaultValue":0,"minValue":-60,"maxValue":24,"unit":"dB"},
        {"id":"1","label":"Bypass","type":"boolean","defaultValue":0,"minValue":0,"maxValue":1}
    ])";

    auto html = generate_webview_html("TestPlugin", params_json);

    // Basic structure
    REQUIRE(html.find("<!DOCTYPE html>") != std::string::npos);
    REQUIRE(html.find("<title>TestPlugin</title>") != std::string::npos);
    REQUIRE(html.find("TestPlugin") != std::string::npos);

    // Parameter JSON is embedded
    REQUIRE(html.find("\"label\":\"Gain\"") != std::string::npos);
    REQUIRE(html.find("\"label\":\"Bypass\"") != std::string::npos);

    // Has interactive elements
    REQUIRE(html.find("createParamUI") != std::string::npos);
    REQUIRE(html.find("sendParam") != std::string::npos);
    REQUIRE(html.find("postMessage") != std::string::npos);

    // Has styling
    REQUIRE(html.find("font-family") != std::string::npos);
    REQUIRE(html.find("#1e1e2e") != std::string::npos); // Catppuccin background
}

TEST_CASE("generate_webview_html handles empty params", "[format][webview]") {
    auto html = generate_webview_html("EmptyPlugin", "[]");
    REQUIRE(html.find("EmptyPlugin") != std::string::npos);
    REQUIRE(html.find("<!DOCTYPE html>") != std::string::npos);
}

TEST_CASE("CLAP webview constants", "[format][webview]") {
    REQUIRE(std::string(CLAP_EXT_WEBVIEW) == "clap.webview/1");
    REQUIRE(std::string(CLAP_WINDOW_API_WEBVIEW) == "webview");
}

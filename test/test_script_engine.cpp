#include <catch2/catch_test_macros.hpp>
#include <pulp/view/script_engine.hpp>
#include <string>
#include <vector>

using namespace pulp::view;

TEST_CASE("ScriptEngine creation", "[view][script]") {
    ScriptEngine engine;
    REQUIRE(static_cast<bool>(engine));
}

TEST_CASE("ScriptEngine evaluate expressions", "[view][script]") {
    ScriptEngine engine;

    SECTION("Integer arithmetic") {
        auto result = engine.evaluate("2 + 3");
        REQUIRE(result.getWithDefault<int>(0) == 5);
    }

    SECTION("String concatenation") {
        auto result = engine.evaluate("'hello' + ' ' + 'world'");
        REQUIRE(result.toString() == "hello world");
    }

    SECTION("Boolean logic") {
        auto result = engine.evaluate("true && !false");
        REQUIRE(result.getWithDefault<bool>(false) == true);
    }

    SECTION("Float arithmetic") {
        auto result = engine.evaluate("3.14 * 2");
        auto val = result.getWithDefault<double>(0.0);
        REQUIRE(val > 6.27);
        REQUIRE(val < 6.29);
    }

    SECTION("Parse error throws") {
        REQUIRE_THROWS_AS(engine.evaluate("function {{{"), choc::javascript::Error);
    }
}

TEST_CASE("ScriptEngine register native function", "[view][script]") {
    ScriptEngine engine;

    SECTION("Function with no args") {
        engine.register_function("getAnswer", [](choc::javascript::ArgumentList) {
            return choc::value::createInt32(42);
        });
        auto result = engine.evaluate("getAnswer()");
        REQUIRE(result.getWithDefault<int>(0) == 42);
    }

    SECTION("Function with args") {
        engine.register_function("add", [](choc::javascript::ArgumentList args) {
            auto a = args.get<double>(0, 0.0);
            auto b = args.get<double>(1, 0.0);
            return choc::value::createFloat64(a + b);
        });
        auto result = engine.evaluate("add(10, 32)");
        REQUIRE(result.getWithDefault<int>(0) == 42);
    }

    SECTION("Function returning string") {
        engine.register_function("greet", [](choc::javascript::ArgumentList args) {
            auto name = args.get<std::string>(0, "world");
            return choc::value::createString("hello " + name);
        });
        auto result = engine.evaluate("greet('pulp')");
        REQUIRE(result.toString() == "hello pulp");
    }
}

TEST_CASE("ScriptEngine invoke JS functions", "[view][script]") {
    ScriptEngine engine;

    engine.evaluate("function double(x) { return x * 2; }");
    auto result = engine.invoke("double", 21);
    REQUIRE(result.getWithDefault<int>(0) == 42);
}

TEST_CASE("ScriptEngine console logging", "[view][script]") {
    ScriptEngine engine;

    std::vector<std::pair<std::string, std::string>> logs;
    engine.set_log_callback([&](std::string_view level, std::string_view message) {
        logs.emplace_back(std::string(level), std::string(message));
    });

    engine.evaluate("console.log('hello')");
    engine.evaluate("console.warn('warning')");
    engine.evaluate("console.error('bad')");

    REQUIRE(logs.size() == 3);
    REQUIRE(logs[0].first == "log");
    REQUIRE(logs[0].second == "hello");
    REQUIRE(logs[1].first == "warn");
    REQUIRE(logs[1].second == "warning");
    REQUIRE(logs[2].first == "error");
    REQUIRE(logs[2].second == "bad");
}

TEST_CASE("ScriptEngine parameter binding pattern", "[view][script]") {
    ScriptEngine engine;

    // Simulate a parameter store with native functions
    float gain_value = 0.5f;

    engine.register_function("getParam", [&](choc::javascript::ArgumentList args) {
        auto name = args.get<std::string>(0, "");
        if (name == "gain") return choc::value::createFloat64(gain_value);
        return choc::value::Value();
    });

    engine.register_function("setParam", [&](choc::javascript::ArgumentList args) {
        auto name = args.get<std::string>(0, "");
        auto value = args.get<double>(1, 0.0);
        if (name == "gain") gain_value = static_cast<float>(value);
        return choc::value::Value();
    });

    // JS code can read and write parameters
    auto result = engine.evaluate("getParam('gain')");
    REQUIRE(result.getWithDefault<double>(0.0) > 0.49);
    REQUIRE(result.getWithDefault<double>(0.0) < 0.51);

    engine.evaluate("setParam('gain', 0.75)");
    REQUIRE(gain_value > 0.74f);
    REQUIRE(gain_value < 0.76f);
}

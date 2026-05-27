// RelativePoint / RelativeExpression tests.
//
// Covers:
//   1. Lexer/parser edge cases (whitespace, unary minus, parens, qualified
//      identifiers, leading-dot numbers).
//   2. Evaluator correctness for common layout idioms (right-aligned,
//      centered, sibling-relative).
//   3. Error reporting on malformed input (column-bearing messages,
//      undefined named rects, divide-by-zero).
//   4. to_string round-trip preserves semantics (re-parse + re-eval
//      matches original).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <pulp/view/relative_point.hpp>

using namespace pulp::view;
using Catch::Matchers::WithinAbs;

namespace {
Rect parent() { return Rect{0, 0, 200, 100}; }
}  // namespace

TEST_CASE("RelativeExpression parses numeric literals", "[relpoint][parse]") {
    REQUIRE(RelativeExpression::evaluate("42",      parent()) == 42.0f);
    REQUIRE(RelativeExpression::evaluate("3.5",     parent()) == 3.5f);
    REQUIRE(RelativeExpression::evaluate(".75",     parent()) == 0.75f);
    REQUIRE(RelativeExpression::evaluate("  100 ",  parent()) == 100.0f);
}

TEST_CASE("RelativeExpression parses built-in identifiers", "[relpoint][parse]") {
    Rect p = parent();  // x=0 y=0 w=200 h=100
    REQUIRE(RelativeExpression::evaluate("left",    p) == 0.0f);
    REQUIRE(RelativeExpression::evaluate("right",   p) == 200.0f);
    REQUIRE(RelativeExpression::evaluate("top",     p) == 0.0f);
    REQUIRE(RelativeExpression::evaluate("bottom",  p) == 100.0f);
    REQUIRE(RelativeExpression::evaluate("width",   p) == 200.0f);
    REQUIRE(RelativeExpression::evaluate("height",  p) == 100.0f);
    REQUIRE(RelativeExpression::evaluate("centerx", p) == 100.0f);
    REQUIRE(RelativeExpression::evaluate("centery", p) == 50.0f);
}

TEST_CASE("RelativeExpression — common layout idioms", "[relpoint][eval]") {
    Rect p = parent();
    // right-align with 10px margin
    REQUIRE(RelativeExpression::evaluate("right - 10",      p) == 190.0f);
    // center horizontally
    REQUIRE(RelativeExpression::evaluate("centerx",         p) == 100.0f);
    // half-width
    REQUIRE(RelativeExpression::evaluate("width / 2",       p) == 100.0f);
    // golden-ratio split
    REQUIRE_THAT(RelativeExpression::evaluate("width * 0.618", p),
                 WithinAbs(123.6f, 1e-4f));
}

TEST_CASE("RelativeExpression — operator precedence and parens", "[relpoint][eval]") {
    Rect p = parent();
    // 2 + 3 * 4 = 14 (not 20)
    REQUIRE(RelativeExpression::evaluate("2 + 3 * 4",       p) == 14.0f);
    REQUIRE(RelativeExpression::evaluate("(2 + 3) * 4",     p) == 20.0f);
    // unary minus
    REQUIRE(RelativeExpression::evaluate("-10",             p) == -10.0f);
    REQUIRE(RelativeExpression::evaluate("right + -10",     p) == 190.0f);
    // nested parens
    REQUIRE(RelativeExpression::evaluate("((width - 20) / 2) + 5", p) == 95.0f);
}

TEST_CASE("RelativeExpression — qualified names from named rects",
          "[relpoint][named]") {
    Rect p = parent();
    std::map<std::string, Rect> env;
    env["prev"]   = Rect{10, 20, 60, 30};   // right=70, bottom=50, centery=35
    env["sibling"] = Rect{80, 40, 50, 25};   // left=80, top=40

    REQUIRE(RelativeExpression::evaluate("prev.right + 8",         p, env) == 78.0f);
    REQUIRE(RelativeExpression::evaluate("sibling.left - 4",       p, env) == 76.0f);
    REQUIRE(RelativeExpression::evaluate("prev.centery",           p, env) == 35.0f);
    // parent.foo == foo
    REQUIRE(RelativeExpression::evaluate("parent.width",           p, env) == 200.0f);
}

TEST_CASE("RelativeExpression — error reporting", "[relpoint][error]") {
    SECTION("empty expression") {
        REQUIRE_THROWS_AS(RelativeExpression::parse(""), std::invalid_argument);
    }
    SECTION("trailing garbage") {
        REQUIRE_THROWS_AS(RelativeExpression::parse("10 +"), std::invalid_argument);
        REQUIRE_THROWS_AS(RelativeExpression::parse("right 10"), std::invalid_argument);
    }
    SECTION("unbalanced paren") {
        REQUIRE_THROWS_AS(RelativeExpression::parse("(width + 10"), std::invalid_argument);
    }
    SECTION("unknown character") {
        REQUIRE_THROWS_AS(RelativeExpression::parse("width @ 10"), std::invalid_argument);
    }
    SECTION("qualified without member") {
        REQUIRE_THROWS_AS(RelativeExpression::parse("prev."), std::invalid_argument);
    }
    SECTION("unknown built-in member") {
        auto e = RelativeExpression::parse("nope");
        REQUIRE_THROWS_AS(e.evaluate(parent()), std::runtime_error);
    }
    SECTION("undefined named rect") {
        auto e = RelativeExpression::parse("ghost.right");
        REQUIRE_THROWS_AS(e.evaluate(parent()), std::runtime_error);
    }
    SECTION("divide by zero") {
        auto e = RelativeExpression::parse("10 / 0");
        REQUIRE_THROWS_AS(e.evaluate(parent()), std::runtime_error);
    }
    SECTION("error message carries column number") {
        try {
            RelativeExpression::parse("right @ 10");
            FAIL("expected throw");
        } catch (const std::invalid_argument& e) {
            std::string what = e.what();
            REQUIRE(what.find("column") != std::string::npos);
        }
    }
}

TEST_CASE("RelativeExpression — to_string round-trips", "[relpoint][serialize]") {
    auto roundtrip = [](const std::string& src) {
        auto e1 = RelativeExpression::parse(src);
        auto s  = e1.to_string();
        auto e2 = RelativeExpression::parse(s);
        // Numerical equivalence is the contract (formatting may differ).
        Rect p = parent();
        std::map<std::string, Rect> env{ {"prev", Rect{10, 20, 60, 30}} };
        REQUIRE(e1.evaluate(p, env) == e2.evaluate(p, env));
    };
    roundtrip("right - 10");
    roundtrip("(2 + 3) * 4");
    roundtrip("-width / 2 + centerx");
    roundtrip("prev.right + 8");
    roundtrip("width * 0.618");
}

TEST_CASE("RelativePoint — x/y pair parsing and eval", "[relpoint][pair]") {
    Rect p = parent();
    auto rp = RelativePoint::parse("right - 10, centery");
    Point pt = rp.evaluate(p);
    REQUIRE(pt.x == 190.0f);
    REQUIRE(pt.y == 50.0f);
}

TEST_CASE("RelativePoint — error when comma missing", "[relpoint][pair][error]") {
    REQUIRE_THROWS_AS(RelativePoint::parse("right - 10"), std::invalid_argument);
}

TEST_CASE("RelativePoint — both axes resolve named rects", "[relpoint][pair][named]") {
    Rect p = parent();
    std::map<std::string, Rect> env{
        {"prev", Rect{10, 20, 60, 30}},
    };
    auto rp = RelativePoint::parse("prev.right + 4, prev.bottom + 6");
    Point pt = rp.evaluate(p, env);
    REQUIRE(pt.x == 74.0f);
    REQUIRE(pt.y == 56.0f);
}

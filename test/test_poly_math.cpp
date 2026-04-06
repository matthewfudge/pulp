#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <pulp/signal/poly_math.hpp>
#include <cmath>

using namespace pulp::signal;
using Catch::Matchers::WithinAbs;

TEST_CASE("Polynomial eval constant", "[signal][poly]") {
    REQUIRE_THAT(Polynomial::eval({5.0f}, 3.0f), WithinAbs(5.0, 0.001));
}

TEST_CASE("Polynomial eval linear", "[signal][poly]") {
    // 2x + 3: coeffs[0]=3, coeffs[1]=2
    REQUIRE_THAT(Polynomial::eval({3.0f, 2.0f}, 4.0f), WithinAbs(11.0, 0.001));
}

TEST_CASE("Polynomial eval quadratic", "[signal][poly]") {
    // x^2 - 3x + 2 at x=5: 25 - 15 + 2 = 12
    REQUIRE_THAT(Polynomial::eval({2.0f, -3.0f, 1.0f}, 5.0f), WithinAbs(12.0, 0.001));
}

TEST_CASE("Polynomial roots_quadratic real roots", "[signal][poly]") {
    // x^2 - 3x + 2 = (x-1)(x-2)
    auto [r1, r2] = Polynomial::roots_quadratic(1.0f, -3.0f, 2.0f);
    float a = r1.real(), b = r2.real();
    if (a > b) std::swap(a, b);
    REQUIRE_THAT(a, WithinAbs(1.0, 0.001));
    REQUIRE_THAT(b, WithinAbs(2.0, 0.001));
    REQUIRE_THAT(r1.imag(), WithinAbs(0.0, 0.001));
}

TEST_CASE("Polynomial roots_quadratic complex roots", "[signal][poly]") {
    // x^2 + 1 = 0 → roots = ±i
    auto [r1, r2] = Polynomial::roots_quadratic(1.0f, 0.0f, 1.0f);
    REQUIRE_THAT(r1.real(), WithinAbs(0.0, 0.001));
    REQUIRE(std::abs(r1.imag()) > 0.9f);
}

TEST_CASE("Polynomial multiply", "[signal][poly]") {
    // (x + 1)(x + 2) = x^2 + 3x + 2
    auto result = Polynomial::multiply({1.0f, 1.0f}, {2.0f, 1.0f});
    REQUIRE(result.size() == 3);
    REQUIRE_THAT(result[0], WithinAbs(2.0, 0.001)); // constant
    REQUIRE_THAT(result[1], WithinAbs(3.0, 0.001)); // x
    REQUIRE_THAT(result[2], WithinAbs(1.0, 0.001)); // x^2
}

TEST_CASE("Polynomial add", "[signal][poly]") {
    auto result = Polynomial::add({1.0f, 2.0f}, {3.0f, 4.0f, 5.0f});
    REQUIRE(result.size() == 3);
    REQUIRE_THAT(result[0], WithinAbs(4.0, 0.001));
    REQUIRE_THAT(result[1], WithinAbs(6.0, 0.001));
    REQUIRE_THAT(result[2], WithinAbs(5.0, 0.001));
}

TEST_CASE("Polynomial derivative", "[signal][poly]") {
    // d/dx (3x^2 + 2x + 1) = 6x + 2
    auto d = Polynomial::derivative({1.0f, 2.0f, 3.0f});
    REQUIRE(d.size() == 2);
    REQUIRE_THAT(d[0], WithinAbs(2.0, 0.001));
    REQUIRE_THAT(d[1], WithinAbs(6.0, 0.001));
}

TEST_CASE("Polynomial scale", "[signal][poly]") {
    auto s = Polynomial::scale({1.0f, 2.0f, 3.0f}, 2.0f);
    REQUIRE_THAT(s[0], WithinAbs(2.0, 0.001));
    REQUIRE_THAT(s[1], WithinAbs(4.0, 0.001));
    REQUIRE_THAT(s[2], WithinAbs(6.0, 0.001));
}

TEST_CASE("Mat2 identity and multiply", "[signal][matrix]") {
    auto id = Mat2::identity();
    Mat2 a{{{2, 3}, {1, 4}}};
    auto r = id * a;
    REQUIRE_THAT(r.m[0][0], WithinAbs(2.0, 0.001));
    REQUIRE_THAT(r.m[1][1], WithinAbs(4.0, 0.001));
}

TEST_CASE("Mat2 determinant", "[signal][matrix]") {
    Mat2 a{{{2, 3}, {1, 4}}};
    REQUIRE_THAT(a.determinant(), WithinAbs(5.0, 0.001)); // 2*4 - 3*1
}

TEST_CASE("Mat2 inverse", "[signal][matrix]") {
    Mat2 a{{{2, 3}, {1, 4}}};
    auto inv = a.inverse();
    auto product = a * inv;
    REQUIRE_THAT(product.m[0][0], WithinAbs(1.0, 0.001));
    REQUIRE_THAT(product.m[0][1], WithinAbs(0.0, 0.001));
    REQUIRE_THAT(product.m[1][0], WithinAbs(0.0, 0.001));
    REQUIRE_THAT(product.m[1][1], WithinAbs(1.0, 0.001));
}

TEST_CASE("Mat3 determinant", "[signal][matrix]") {
    auto id = Mat3::identity();
    REQUIRE_THAT(id.determinant(), WithinAbs(1.0, 0.001));
}

TEST_CASE("Polynomial eval_complex", "[signal][poly]") {
    // p(z) = z + 1 at z = i: result = 1 + i
    auto result = Polynomial::eval_complex({1.0f, 1.0f}, {0.0f, 1.0f});
    REQUIRE_THAT(result.real(), WithinAbs(1.0, 0.001));
    REQUIRE_THAT(result.imag(), WithinAbs(1.0, 0.001));
}

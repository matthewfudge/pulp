#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <pulp/view/animator_set.hpp>
#include <cmath>

using namespace pulp::view;

TEST_CASE("AnimatorSetBuilder then creates sequential tween", "[view][animation]") {
    float value = 0.0f;
    auto runner = AnimatorSetBuilder()
        .then(0.0f, 1.0f, 0.5f, [&](float v) { value = v; })
        .build_runner();

    REQUIRE_FALSE(runner.finished());
    runner.advance(0.25f);
    REQUIRE(value > 0.0f);
    REQUIRE(value < 1.0f);

    runner.advance(0.3f);
    REQUIRE(value == Catch::Approx(1.0f));
    REQUIRE(runner.finished());
}

TEST_CASE("AnimatorSetBuilder sequential tweens run in order", "[view][animation]") {
    float a = 0, b = 0;
    auto runner = AnimatorSetBuilder()
        .then(0.0f, 10.0f, 0.2f, [&](float v) { a = v; })
        .then(0.0f, 20.0f, 0.3f, [&](float v) { b = v; })
        .build_runner();

    runner.advance(0.2f);
    REQUIRE(a == Catch::Approx(10.0f));

    runner.advance(0.3f);
    REQUIRE(b == Catch::Approx(20.0f));
}

TEST_CASE("AnimatorSetBuilder delay", "[view][animation]") {
    float v = 0;
    auto runner = AnimatorSetBuilder()
        .delay(0.5f)
        .then(0.0f, 1.0f, 0.1f, [&](float val) { v = val; })
        .build_runner();

    runner.advance(0.3f);
    REQUIRE(v == Catch::Approx(0.0f));  // still in delay

    runner.advance(0.3f);  // past delay, into tween
    REQUIRE(v > 0.0f);
}

TEST_CASE("AnimatorSetBuilder reset", "[view][animation]") {
    float v = 0;
    auto runner = AnimatorSetBuilder()
        .then(0.0f, 1.0f, 0.3f, [&](float val) { v = val; })
        .build_runner();

    runner.advance(0.3f);
    REQUIRE(runner.finished());

    runner.reset();
    REQUIRE_FALSE(runner.finished());
}

// ── Vector3D ────────────────────────────────────────────────────────────

TEST_CASE("Vector3D arithmetic", "[view][3d]") {
    Vector3D a{1, 2, 3};
    Vector3D b{4, 5, 6};

    auto sum = a + b;
    REQUIRE(sum.x == Catch::Approx(5.0f));
    REQUIRE(sum.y == Catch::Approx(7.0f));

    auto scaled = a * 2.0f;
    REQUIRE(scaled.z == Catch::Approx(6.0f));

    REQUIRE(a.length() == Catch::Approx(std::sqrt(14.0f)));

    auto n = a.normalized();
    REQUIRE(n.length() == Catch::Approx(1.0f).margin(0.001f));
}

TEST_CASE("Vector3D dot and cross product", "[view][3d]") {
    Vector3D x{1, 0, 0};
    Vector3D y{0, 1, 0};

    REQUIRE(x.dot(y) == Catch::Approx(0.0f));
    REQUIRE(x.dot(x) == Catch::Approx(1.0f));

    auto cross = x.cross(y);
    REQUIRE(cross.z == Catch::Approx(1.0f));
}

// ── Quaternion ──────────────────────────────────────────────────────────

TEST_CASE("Quaternion identity", "[view][3d]") {
    auto q = Quaternion::identity();
    REQUIRE(q.w == Catch::Approx(1.0f));
    REQUIRE(q.length() == Catch::Approx(1.0f));
}

TEST_CASE("Quaternion from axis angle", "[view][3d]") {
    auto q = Quaternion::from_axis_angle({0, 1, 0}, 3.14159f);
    REQUIRE(q.length() == Catch::Approx(1.0f).margin(0.01f));
}

TEST_CASE("Quaternion multiply identity", "[view][3d]") {
    auto q = Quaternion::from_axis_angle({1, 0, 0}, 0.5f);
    auto id = Quaternion::identity();
    auto result = q * id;
    REQUIRE(result.w == Catch::Approx(q.w).margin(0.001f));
}

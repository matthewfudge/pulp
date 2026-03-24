#include <catch2/catch_test_macros.hpp>
#include <pulp/platform/platform.hpp>

using namespace pulp::platform;

TEST_CASE("Platform detection", "[platform]") {
    SECTION("Current OS is known") {
        REQUIRE(current_os != OS::Unknown);
    }

    SECTION("At least one platform flag is true") {
        REQUIRE((is_macos || is_ios || is_windows || is_linux || is_android));
    }

    SECTION("Desktop or mobile is set") {
        REQUIRE((is_desktop || is_mobile));
    }

    SECTION("Architecture is known") {
        REQUIRE(current_arch != Arch::Unknown);
    }

    SECTION("64-bit detection") {
        REQUIRE(is_64bit);  // We only target 64-bit
    }

    SECTION("Compiler is known") {
        REQUIRE(current_compiler != Compiler::Unknown);
    }

    SECTION("Endianness is consistent") {
        REQUIRE((is_little_endian || is_big_endian));
        REQUIRE(is_little_endian != is_big_endian);
    }
}

#ifdef __APPLE__
TEST_CASE("Apple-specific detection", "[platform]") {
    REQUIRE(is_apple);
    REQUIRE((is_macos || is_ios));
    REQUIRE((current_compiler == Compiler::AppleClang || current_compiler == Compiler::Clang));
}
#endif

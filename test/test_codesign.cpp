#include <catch2/catch_test_macros.hpp>
#include <pulp/ship/codesign.hpp>
#include <fstream>
#include <cstdlib>

using namespace pulp::ship;

TEST_CASE("check_codesign on system binary", "[ship][codesign]") {
#ifdef __APPLE__
    // /usr/bin/true is always signed on macOS
    auto info = check_codesign("/usr/bin/true");
    REQUIRE(info.is_signed);
    REQUIRE(info.is_valid);
    REQUIRE_FALSE(info.identity.empty());
#else
    auto info = check_codesign("/usr/bin/true");
    REQUIRE_FALSE(info.is_signed);
#endif
}

TEST_CASE("check_codesign on nonexistent path", "[ship][codesign]") {
    auto info = check_codesign("/nonexistent/binary");
    REQUIRE_FALSE(info.is_signed);
    REQUIRE_FALSE(info.is_valid);
}

TEST_CASE("list_signing_identities does not crash", "[ship][codesign]") {
    auto ids = list_signing_identities();
    // May be empty on CI, but should not crash
    REQUIRE(true);
}

TEST_CASE("default_audio_entitlements returns valid plist", "[ship][codesign]") {
    auto plist = default_audio_entitlements();
#ifdef __APPLE__
    REQUIRE_FALSE(plist.empty());
    REQUIRE(plist.find("audio-input") != std::string::npos);
    REQUIRE(plist.find("network.client") != std::string::npos);
    REQUIRE(plist.find("</plist>") != std::string::npos);
#else
    // Non-Apple returns empty
    REQUIRE(plist.empty());
#endif
}

TEST_CASE("create_dmg produces a file from valid source", "[ship][codesign]") {
#ifdef __APPLE__
    // Create a temp directory as source
    std::string tmp = "/tmp/pulp-dmg-test-src";
    std::system(("mkdir -p " + tmp).c_str());
    std::system(("touch " + tmp + "/test.txt").c_str());

    std::string dmg_path = "/tmp/pulp-test-output.dmg";
    bool ok = create_dmg(tmp, dmg_path, "PulpTest");
    REQUIRE(ok);

    // Verify the DMG file exists
    std::ifstream f(dmg_path);
    REQUIRE(f.good());

    // Cleanup
    std::system(("rm -rf " + tmp).c_str());
    std::system(("rm -f " + dmg_path).c_str());
#endif
}

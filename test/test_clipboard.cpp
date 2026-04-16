#include <catch2/catch_test_macros.hpp>
#include <pulp/platform/clipboard.hpp>

using namespace pulp::platform;

// #300 — tests no longer assume every platform has a real clipboard.
// Linux needs xclip/wl-copy on PATH; without them set_text returns
// false (not fake-success). Android needs a registered bridge;
// without it the clipboard is unsupported. These tests accept
// either a working clipboard OR an honest "unsupported" and only
// fail if a clipboard call fake-succeeds.

TEST_CASE("Clipboard text round-trip or honest unsupported", "[platform][clipboard]") {
    const bool ok = Clipboard::set_text("pulp test clipboard");
    if (!ok) {
        // Honest failure on this platform (no xclip/wl-copy or no
        // Android bridge). Round-trip then MUST report no text.
        auto maybe_text = Clipboard::get_text();
        // Either the OS clipboard has something else (from a prior
        // test run or another app) → accept anything EXCEPT our
        // specific string, which would indicate the earlier set_text
        // silently took effect. Or the clipboard is plain unsupported.
        if (maybe_text.has_value()) {
            REQUIRE(*maybe_text != "pulp test clipboard");
        }
        return;
    }

    // set_text succeeded → the platform has a real clipboard
    // (macOS/iOS native, Windows native, Linux with xclip/wl-copy,
    // or Android with a bridge). Round-trip must work.
    REQUIRE(Clipboard::has_text());
    auto text = Clipboard::get_text();
    REQUIRE(text.has_value());
    REQUIRE(text.value() == "pulp test clipboard");
}

TEST_CASE("Clipboard binary data — mac/win supported, others explicit unsupported",
          "[platform][clipboard]") {
    std::vector<uint8_t> data = {0x50, 0x55, 0x4C, 0x50}; // "PULP"
    const bool ok = Clipboard::set_data("com.pulp.test", data);

    if (!ok) {
        // Linux / Android return false explicitly — binary clipboard
        // is unsupported. get_data must also report unsupported.
        auto r = Clipboard::get_data("com.pulp.test");
        REQUIRE_FALSE(r.has_value());
        return;
    }

    // mac/win accepted the write → read must match.
    auto result = Clipboard::get_data("com.pulp.test");
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 4);
    REQUIRE((*result)[0] == 0x50);
    REQUIRE((*result)[3] == 0x50);
}

TEST_CASE("Clipboard missing data returns nullopt", "[platform][clipboard]") {
    auto result = Clipboard::get_data("com.pulp.nonexistent.type");
    REQUIRE_FALSE(result.has_value());
}

// #300: Android-bridge registration API is public on every platform
// so callers don't need #ifdef guards. On non-Android builds it is
// a no-op; on Android it routes Clipboard calls through the
// registered ClipboardManager bridge. Either way, installing and
// clearing a bridge must not crash or fake-succeed.
TEST_CASE("Clipboard Android bridge registration is safe on all platforms",
          "[platform][clipboard][issue-300]") {
    bool set_called = false;
    Clipboard::AndroidBridge bridge;
    bridge.set_text = [&](const std::string&) { set_called = true; return true; };
    bridge.has_text = []() { return false; };
    bridge.get_text = []() -> std::optional<std::string> { return std::nullopt; };

    // Registering + clearing must not throw or alter public API shape.
    Clipboard::set_android_bridge(bridge);
    Clipboard::clear_android_bridge();

#if defined(__ANDROID__)
    // On Android, with no bridge installed after clear_android_bridge(),
    // set_text must return false (not fake-succeed).
    REQUIRE_FALSE(Clipboard::set_text("should-not-land"));
    REQUIRE_FALSE(Clipboard::has_text());
    REQUIRE_FALSE(Clipboard::get_text().has_value());

    // Reinstall → calls route through.
    Clipboard::set_android_bridge(bridge);
    REQUIRE(Clipboard::set_text("via-bridge"));
    REQUIRE(set_called);
    Clipboard::clear_android_bridge();
#else
    // On non-Android platforms, these calls are no-ops and the
    // flag must not have been flipped.
    REQUIRE_FALSE(set_called);
#endif
}

#ifdef __APPLE__
TEST_CASE("Clipboard text reflects pasteboard contents, not stale fallback", "[platform][clipboard]") {
    REQUIRE(Clipboard::set_text("pulp stale text"));
    REQUIRE(Clipboard::set_data("com.pulp.test.binary", {0x01, 0x02, 0x03}));

    REQUIRE_FALSE(Clipboard::has_text());
    REQUIRE_FALSE(Clipboard::get_text().has_value());
}

TEST_CASE("Clipboard data reflects pasteboard contents, not stale fallback", "[platform][clipboard]") {
    REQUIRE(Clipboard::set_data("com.pulp.test.binary", {0xAA, 0xBB}));
    REQUIRE(Clipboard::set_text("plain text only"));

    REQUIRE_FALSE(Clipboard::get_data("com.pulp.test.binary").has_value());
}
#endif

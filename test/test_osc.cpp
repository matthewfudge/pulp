#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <pulp/osc/osc.hpp>
#include <thread>
#include <chrono>

using namespace pulp::osc;
using Catch::Matchers::WithinAbs;

// ── Message ──────────────────────────────────────────────────────────────────

TEST_CASE("OSC Message construction", "[osc][message]") {
    Message msg("/synth/freq");
    msg.add(440.0f).add(42).add(std::string("hello"));

    REQUIRE(msg.address == "/synth/freq");
    REQUIRE(msg.args.size() == 3);
    REQUIRE_THAT(msg.get_float(0), WithinAbs(440.0, 0.01));
    REQUIRE(msg.get_int(1) == 42);
    REQUIRE(msg.get_string(2) == "hello");
}

TEST_CASE("OSC Message get with defaults", "[osc][message]") {
    Message msg("/test");
    msg.add(10);

    REQUIRE(msg.get_int(0) == 10);
    REQUIRE(msg.get_int(5, -1) == -1);      // Out of range
    REQUIRE(msg.get_float(0, 99.0f) == 99.0f); // Wrong type
    REQUIRE(msg.get_string(0, "nope") == "nope");
}

// ── Encoding/Decoding ────────────────────────────────────────────────────────

TEST_CASE("OSC encode/decode round-trip int+float", "[osc][codec]") {
    Message msg("/gain");
    msg.add(-12).add(0.75f);

    auto data = encode(msg);
    REQUIRE(!data.empty());

    auto decoded = decode(data.data(), data.size());
    REQUIRE(decoded.address == "/gain");
    REQUIRE(decoded.args.size() == 2);
    REQUIRE(decoded.get_int(0) == -12);
    REQUIRE_THAT(decoded.get_float(1), WithinAbs(0.75, 0.001));
}

TEST_CASE("OSC encode/decode round-trip string", "[osc][codec]") {
    Message msg("/label");
    msg.add(std::string("Pulp Framework"));

    auto data = encode(msg);
    auto decoded = decode(data.data(), data.size());

    REQUIRE(decoded.address == "/label");
    REQUIRE(decoded.get_string(0) == "Pulp Framework");
}

TEST_CASE("OSC encode/decode round-trip blob", "[osc][codec]") {
    Message msg("/data");
    std::vector<uint8_t> blob = {0xDE, 0xAD, 0xBE, 0xEF};
    msg.add(blob);

    auto data = encode(msg);
    auto decoded = decode(data.data(), data.size());

    REQUIRE(decoded.address == "/data");
    REQUIRE(decoded.args.size() == 1);
    auto& result = std::get<std::vector<uint8_t>>(decoded.args[0]);
    REQUIRE(result.size() == 4);
    REQUIRE(result[0] == 0xDE);
    REQUIRE(result[3] == 0xEF);
}

TEST_CASE("OSC encode/decode mixed types", "[osc][codec]") {
    Message msg("/mix");
    msg.add(1).add(2.5f).add(std::string("test")).add(3);

    auto data = encode(msg);
    auto decoded = decode(data.data(), data.size());

    REQUIRE(decoded.address == "/mix");
    REQUIRE(decoded.args.size() == 4);
    REQUIRE(decoded.get_int(0) == 1);
    REQUIRE_THAT(decoded.get_float(1), WithinAbs(2.5, 0.001));
    REQUIRE(decoded.get_string(2) == "test");
    REQUIRE(decoded.get_int(3) == 3);
}

TEST_CASE("OSC decode invalid data", "[osc][codec]") {
    uint8_t bad[] = {0x00, 0x01, 0x02};
    auto msg = decode(bad, 3);
    REQUIRE(msg.address.empty());
}

TEST_CASE("OSC 4-byte alignment", "[osc][codec]") {
    // Short address "/a" should still produce valid encoding
    Message msg("/a");
    msg.add(42);

    auto data = encode(msg);
    REQUIRE(data.size() % 4 == 0); // All OSC data is 4-byte aligned

    auto decoded = decode(data.data(), data.size());
    REQUIRE(decoded.address == "/a");
    REQUIRE(decoded.get_int(0) == 42);
}

// ── UDP sender/receiver ─────────────────────────────────────────────────────

TEST_CASE("OSC sender/receiver loopback", "[osc][udp]") {
    Message received_msg;
    bool got_message = false;

    Receiver rx;
    bool listening = rx.listen(9876, [&](const Message& msg) {
        received_msg = msg;
        got_message = true;
    });
    REQUIRE(listening);
    REQUIRE(rx.is_listening());

    // Give receiver time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    Sender tx;
    REQUIRE(tx.connect("127.0.0.1", 9876));
    REQUIRE(tx.is_connected());

    Message msg("/test/ping");
    msg.add(42).add(3.14f);
    REQUIRE(tx.send(msg));

    // Wait for message
    for (int i = 0; i < 20 && !got_message; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    REQUIRE(got_message);
    REQUIRE(received_msg.address == "/test/ping");
    REQUIRE(received_msg.get_int(0) == 42);
    REQUIRE_THAT(received_msg.get_float(1), WithinAbs(3.14, 0.01));

    tx.disconnect();
    rx.stop();
    REQUIRE_FALSE(tx.is_connected());
    REQUIRE_FALSE(rx.is_listening());
}

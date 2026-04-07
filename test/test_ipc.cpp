#include <catch2/catch_test_macros.hpp>
#include <pulp/events/interprocess_connection.hpp>
#include <pulp/runtime/temporary_file.hpp>
#include <thread>
#include <chrono>
#include <atomic>

using namespace pulp::events;
using namespace pulp::runtime;

// ── InterprocessConnection via named pipe ───────────────────────────────

TEST_CASE("IPC message framing", "[events][ipc]") {
    // Verify the connection won't send when disconnected
    InterprocessConnection conn;
    REQUIRE_FALSE(conn.send_message("test message"));
    REQUIRE_FALSE(conn.send_message(std::string_view("binary data")));
}

TEST_CASE("IPC connection state", "[events][ipc]") {
    InterprocessConnection conn;
    REQUIRE(conn.state() == IpcState::Disconnected);
    REQUIRE_FALSE(conn.is_connected());
}

TEST_CASE("IPC connect to nonexistent fails", "[events][ipc]") {
    InterprocessConnection conn;
    bool ok = conn.connect("/tmp/pulp_nonexistent_pipe_12345", IpcTransport::NamedPipe);
    REQUIRE_FALSE(ok);
    REQUIRE(conn.state() == IpcState::Error);
}

TEST_CASE("IPC send while disconnected returns false", "[events][ipc]") {
    InterprocessConnection conn;
    REQUIRE_FALSE(conn.send_message("test"));
}

TEST_CASE("IPC lambda callbacks settable", "[events][ipc]") {
    InterprocessConnection conn;
    bool connected_fired = false;
    bool disconnected_fired = false;

    conn.on_connected = [&]() { connected_fired = true; };
    conn.on_disconnected = [&]() { disconnected_fired = true; };
    conn.on_text_message = [](std::string_view) {};

    // Callbacks are set but won't fire without actual connection
    REQUIRE_FALSE(connected_fired);
}

// ── InterprocessConnectionServer ────────────────────────────────────────

TEST_CASE("IPC server initial state", "[events][ipc]") {
    InterprocessConnectionServer server;
    REQUIRE_FALSE(server.is_running());
}

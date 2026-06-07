#pragma once

// Shared loopback-HTTP capability probe for tests.
//
// Pulp's self-hosted macOS CI runner sits behind a host firewall (LuLu) that
// lets a server bind 127.0.0.1 but blocks the loopback *connect*. Round-trip
// HTTP tests therefore time out and fail through no fault of the code under
// test — the bind-only skip guards some tests already use do not catch this,
// because the bind succeeds; it is the connect that is denied.
//
// Probe the capability ONCE: stand up a tiny httplib server and try to reach it
// over loopback. If the round trip works, the caller runs its tests normally
// (so a real regression on a machine where loopback works still fails). If it
// does not, the caller SKIPs — turning an environment limitation into a skip,
// not a red lane. The result is cached for the lifetime of the test binary.

#include "../external/cpp-httplib/httplib.h"

#include <pulp/runtime/http.hpp>  // pulp::runtime::http_get — the stack the tests use

#include <chrono>
#include <string>
#include <thread>

namespace pulp_test {

// Probe with Pulp's OWN http_get against a loopback server — NOT httplib's client.
// A host firewall (LuLu) decides per outbound connection, and it can allow httplib's
// connect while denying Pulp's socket stack, so a probe that uses httplib would wrongly
// report "available" and the round-trip tests would still fail. Exercising the same path
// the tests use makes the skip decision match reality.
inline bool loopback_http_available() {
    static const bool ok = [] {
        httplib::Server server;
        server.Get("/__probe", [](const httplib::Request&, httplib::Response& r) {
            r.set_content("ok", "text/plain");
        });
        const int port = server.bind_to_any_port("127.0.0.1");
        if (port == 0) return false;  // cannot even bind — definitely unavailable

        std::thread worker([&] { server.listen_after_bind(); });
        const auto running_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        while (!server.is_running() && std::chrono::steady_clock::now() < running_deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

        const std::string url = "http://127.0.0.1:" + std::to_string(port) + "/__probe";
        bool reached = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline) {
            const auto resp = pulp::runtime::http_get(url, 1);
            if (resp.ok() && resp.body == "ok") {
                reached = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        server.stop();
        if (worker.joinable()) worker.join();
        return reached;
    }();
    return ok;
}

}  // namespace pulp_test

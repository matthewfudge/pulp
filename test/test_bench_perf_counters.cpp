// Zero-copy benchmark perf-counter unit tests (Slice 0 of #516).
//
// Hammers the atomic accumulators from multiple writer threads to
// verify that fetch_add on std::atomic<double> coalesces correctly and
// that `snapshot_and_reset` doesn't drop samples in the exchange.
//
// Counters are tooling-only, so the entire test is gated on
// PULP_BENCHMARK via test/CMakeLists.txt.

#include <pulp/render/bench/perf_counters.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <thread>
#include <vector>

using namespace pulp::render::bench;

TEST_CASE("PerfCounters accumulate across 8 writer threads", "[bench][perf-counters]") {
    PerfCounters counters;
    counters.reset();

    constexpr int kNumThreads = 8;
    constexpr int kIters = 1000;

    std::vector<std::thread> writers;
    writers.reserve(kNumThreads);

    for (int t = 0; t < kNumThreads; ++t) {
        writers.emplace_back([&counters]() {
            for (int i = 0; i < kIters; ++i) {
                // Each thread adds exactly 1.0 µs per iteration into
                // the audio_copy bucket. Final expected total is
                // kNumThreads * kIters * 1.0 = 8000.0.
                counters.audio_copy_total_us.fetch_add(
                    1.0, std::memory_order_relaxed);
            }
        });
    }

    for (auto& th : writers) th.join();

    REQUIRE(counters.audio_copy_total_us.load(std::memory_order_relaxed) ==
            static_cast<double>(kNumThreads * kIters));
}

TEST_CASE("PerfCounters snapshot_and_reset returns + zeros atomically",
          "[bench][perf-counters]") {
    PerfCounters counters;
    counters.reset();

    counters.audio_copy_total_us.fetch_add(42.5, std::memory_order_relaxed);
    counters.triplebuffer_publish_total_us.fetch_add(3.25, std::memory_order_relaxed);
    counters.gpu_upload_total_us.fetch_add(18.0, std::memory_order_relaxed);
    counters.gpu_readback_total_us.fetch_add(0.75, std::memory_order_relaxed);
    counters.gpu_dispatch_total_us.fetch_add(45.0, std::memory_order_relaxed);
    counters.total_frame_total_us.fetch_add(120.0, std::memory_order_relaxed);
    counters.cpu_to_gpu_bytes_total.fetch_add(32768.0, std::memory_order_relaxed);
    counters.gpu_to_cpu_bytes_total.fetch_add(0.0, std::memory_order_relaxed);
    counters.sample_count.fetch_add(1.0, std::memory_order_relaxed);

    auto snap = counters.snapshot_and_reset();

    REQUIRE(snap.audio_copy_total_us == 42.5);
    REQUIRE(snap.triplebuffer_publish_total_us == 3.25);
    REQUIRE(snap.gpu_upload_total_us == 18.0);
    REQUIRE(snap.gpu_readback_total_us == 0.75);
    REQUIRE(snap.gpu_dispatch_total_us == 45.0);
    REQUIRE(snap.total_frame_total_us == 120.0);
    REQUIRE(snap.cpu_to_gpu_bytes_total == 32768.0);
    REQUIRE(snap.gpu_to_cpu_bytes_total == 0.0);
    REQUIRE(snap.sample_count == 1.0);

    // Everything should be zero after the snapshot.
    REQUIRE(counters.audio_copy_total_us.load(std::memory_order_relaxed) == 0.0);
    REQUIRE(counters.triplebuffer_publish_total_us.load(std::memory_order_relaxed) == 0.0);
    REQUIRE(counters.gpu_upload_total_us.load(std::memory_order_relaxed) == 0.0);
    REQUIRE(counters.gpu_readback_total_us.load(std::memory_order_relaxed) == 0.0);
    REQUIRE(counters.gpu_dispatch_total_us.load(std::memory_order_relaxed) == 0.0);
    REQUIRE(counters.total_frame_total_us.load(std::memory_order_relaxed) == 0.0);
    REQUIRE(counters.cpu_to_gpu_bytes_total.load(std::memory_order_relaxed) == 0.0);
    REQUIRE(counters.gpu_to_cpu_bytes_total.load(std::memory_order_relaxed) == 0.0);
    REQUIRE(counters.sample_count.load(std::memory_order_relaxed) == 0.0);
}

TEST_CASE("PerfCounters now_us advances monotonically on the same thread",
          "[bench][perf-counters]") {
    const double t0 = now_us();
    const double t1 = now_us();
    REQUIRE(t1 >= t0);
}

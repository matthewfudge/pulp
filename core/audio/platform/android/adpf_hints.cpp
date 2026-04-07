#if defined(__ANDROID__)

#include <android/log.h>
#include <chrono>
#include <cstdint>

// ADPF (Android Dynamic Performance Framework) requires API 31+.
// We check at runtime and gracefully degrade on older devices.
#if __ANDROID_API__ >= 31
#include <android/performance_hint.h>
#define PULP_HAS_ADPF 1
#else
#define PULP_HAS_ADPF 0
#endif

#include <sys/types.h>
#include <unistd.h>

#define PULP_LOG_TAG "Pulp"
#define PULP_LOGI(...) __android_log_print(ANDROID_LOG_INFO, PULP_LOG_TAG, __VA_ARGS__)
#define PULP_LOGW(...) __android_log_print(ANDROID_LOG_WARN, PULP_LOG_TAG, __VA_ARGS__)

namespace pulp::audio {

// ── ADPF Performance Hint Session ─────────────────────────────────────────
// Reports audio callback execution times and deadlines to the OS scheduler.
// This allows the OS to:
// - Assign the audio thread to performance ("Big") CPU cores
// - Scale CPU clock appropriately without thermal runaway
// - Avoid power-saving throttling during active DSP work
//
// Graceful degradation: if ADPF is unavailable (API < 31 or unsupported device),
// init() returns false and all subsequent calls are no-ops.

class AdpfHintSession {
public:
    // Initialize the hint session for the current thread.
    // Call from the audio callback thread on the first callback.
    // target_duration_ns: the audio buffer duration in nanoseconds.
    bool init(int64_t target_duration_ns) {
#if PULP_HAS_ADPF
        manager_ = APerformanceHint_getManager();
        if (!manager_) {
            PULP_LOGW("ADPF: APerformanceHint_getManager() returned null — not available");
            return false;
        }

        target_duration_ns_ = target_duration_ns;

        pid_t tids[] = { gettid() };
        session_ = APerformanceHint_createSession(manager_, tids, 1, target_duration_ns_);
        if (!session_) {
            PULP_LOGW("ADPF: Failed to create performance hint session");
            return false;
        }

        PULP_LOGI("ADPF: Hint session created (target=%lldns = %.1fms)",
                  static_cast<long long>(target_duration_ns_),
                  target_duration_ns_ / 1e6);
        return true;
#else
        (void)target_duration_ns;
        return false;
#endif
    }

    // Report the actual work duration of the audio callback.
    // Call at the END of every onAudioReady callback.
    void report_actual_duration(int64_t actual_ns) {
#if PULP_HAS_ADPF
        if (session_) {
            APerformanceHint_reportActualWorkDuration(session_, actual_ns);
        }
#else
        (void)actual_ns;
#endif
    }

    // Update the target duration when buffer size or sample rate changes.
    void update_target(int64_t new_target_ns) {
#if PULP_HAS_ADPF
        if (session_) {
            target_duration_ns_ = new_target_ns;
            APerformanceHint_updateTargetWorkDuration(session_, new_target_ns);
            PULP_LOGI("ADPF: Target updated to %lldns (%.1fms)",
                      static_cast<long long>(new_target_ns), new_target_ns / 1e6);
        }
#else
        (void)new_target_ns;
#endif
    }

    ~AdpfHintSession() {
#if PULP_HAS_ADPF
        if (session_) {
            APerformanceHint_closeSession(session_);
        }
#endif
    }

    // Non-copyable, movable
    AdpfHintSession() = default;
    AdpfHintSession(const AdpfHintSession&) = delete;
    AdpfHintSession& operator=(const AdpfHintSession&) = delete;
    AdpfHintSession(AdpfHintSession&& other) noexcept
        : manager_(other.manager_), session_(other.session_),
          target_duration_ns_(other.target_duration_ns_) {
        other.session_ = nullptr;
    }
    AdpfHintSession& operator=(AdpfHintSession&& other) noexcept {
#if PULP_HAS_ADPF
        if (session_) APerformanceHint_closeSession(session_);
#endif
        manager_ = other.manager_;
        session_ = other.session_;
        target_duration_ns_ = other.target_duration_ns_;
        other.session_ = nullptr;
        return *this;
    }

    bool is_active() const {
#if PULP_HAS_ADPF
        return session_ != nullptr;
#else
        return false;
#endif
    }

    // Calculate target duration from buffer size and sample rate
    static int64_t calculate_target_ns(int32_t buffer_size, int32_t sample_rate) {
        return static_cast<int64_t>(buffer_size) * 1'000'000'000LL / sample_rate;
    }

private:
#if PULP_HAS_ADPF
    APerformanceHintManager* manager_ = nullptr;
    APerformanceHintSession* session_ = nullptr;
#endif
    int64_t target_duration_ns_ = 0;
};

} // namespace pulp::audio

#endif // __ANDROID__

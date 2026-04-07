#if defined(__ANDROID__)

#include <pulp/platform/android/jni.hpp>
#include <android/log.h>
#include <string>
#include <vector>
#include <mutex>

#define PULP_LOG_TAG "Pulp"
#define PULP_LOGI(...) __android_log_print(ANDROID_LOG_INFO, PULP_LOG_TAG, __VA_ARGS__)

namespace pulp::midi {

// ── Android MIDI Device Registry ──────────────────────────────────────────
// Tracks connected MIDI devices as reported by the Kotlin MidiManager.
// Device discovery callbacks come from the main/Kotlin thread.
// MIDI data is routed to the audio thread via a lock-free SpscQueue
// (not implemented here — wired by the standalone adapter).

struct MidiDeviceEntry {
    int id;
    std::string name;
};

// Device list is accessed from the main thread only (JNI callbacks).
// Audio thread accesses MIDI data via SpscQueue, not this list.
static std::vector<MidiDeviceEntry> g_devices;
static std::mutex g_devices_mutex;  // OK: only main thread, never audio thread

void on_device_added(int id, const std::string& name) {
    std::lock_guard lock(g_devices_mutex);
    g_devices.push_back({id, name});
    PULP_LOGI("MIDI device added: %s (id=%d), total=%zu", name.c_str(), id, g_devices.size());
}

void on_device_removed(int id) {
    std::lock_guard lock(g_devices_mutex);
    g_devices.erase(
        std::remove_if(g_devices.begin(), g_devices.end(),
                       [id](const MidiDeviceEntry& e) { return e.id == id; }),
        g_devices.end()
    );
    PULP_LOGI("MIDI device removed: id=%d, remaining=%zu", id, g_devices.size());
}

std::vector<MidiDeviceEntry> get_devices() {
    std::lock_guard lock(g_devices_mutex);
    return g_devices;
}

} // namespace pulp::midi

// ── JNI Exports ───────────────────────────────────────────────────────────

extern "C" JNIEXPORT void JNICALL
Java_com_pulp_midi_PulpMidiManager_nativeOnDeviceAdded(
    JNIEnv* env, jobject thiz, jint id, jstring name) {
    try {
        const char* name_str = env->GetStringUTFChars(name, nullptr);
        if (name_str) {
            pulp::midi::on_device_added(id, name_str);
            env->ReleaseStringUTFChars(name, name_str);
        }
    } catch (const std::exception& e) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    } catch (...) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                      "Unknown C++ exception in nativeOnDeviceAdded");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_pulp_midi_PulpMidiManager_nativeOnDeviceRemoved(
    JNIEnv* env, jobject thiz, jint id) {
    try {
        pulp::midi::on_device_removed(id);
    } catch (const std::exception& e) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    } catch (...) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                      "Unknown C++ exception in nativeOnDeviceRemoved");
    }
}

#endif // __ANDROID__

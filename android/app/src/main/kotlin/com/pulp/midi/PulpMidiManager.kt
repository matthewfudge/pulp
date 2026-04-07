package com.pulp.midi

import android.content.Context
import android.media.midi.MidiDeviceInfo
import android.media.midi.MidiManager
import android.os.Build
import android.util.Log
import com.pulp.PulpApplication

/**
 * Kotlin-side MIDI device management.
 * Handles device discovery, open/close, and BLE MIDI pairing.
 * MIDI data flows to C++ via JNI callbacks on a lock-free queue.
 */
class PulpMidiManager(context: Context) {

    private val midiManager = context.getSystemService(MidiManager::class.java)

    init {
        midiManager?.registerDeviceCallback(object : MidiManager.DeviceCallback() {
            override fun onDeviceAdded(device: MidiDeviceInfo) {
                val name = device.properties.getString(MidiDeviceInfo.PROPERTY_NAME) ?: "Unknown"
                Log.i(PulpApplication.LOG_TAG, "MIDI device added: $name (id=${device.id})")
                nativeOnDeviceAdded(device.id, name)
            }

            override fun onDeviceRemoved(device: MidiDeviceInfo) {
                val name = device.properties.getString(MidiDeviceInfo.PROPERTY_NAME) ?: "Unknown"
                Log.i(PulpApplication.LOG_TAG, "MIDI device removed: $name (id=${device.id})")
                nativeOnDeviceRemoved(device.id)
            }
        }, null)
    }

    fun getTransportType(device: MidiDeviceInfo): Int {
        return if (Build.VERSION.SDK_INT >= 33) {
            device.defaultProtocol
        } else {
            1  // assume bytestream
        }
    }

    private external fun nativeOnDeviceAdded(id: Int, name: String)
    private external fun nativeOnDeviceRemoved(id: Int)
}

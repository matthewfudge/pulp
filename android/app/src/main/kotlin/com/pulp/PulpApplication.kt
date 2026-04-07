package com.pulp

import android.app.Application
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner

class PulpApplication : Application(), LifecycleEventObserver {

    override fun onCreate() {
        super.onCreate()
        System.loadLibrary("pulp")
        ProcessLifecycleOwner.get().lifecycle.addObserver(this)
    }

    override fun onStateChanged(source: LifecycleOwner, event: Lifecycle.Event) {
        // Process-level lifecycle — not per-activity
    }

    companion object {
        const val LOG_TAG = "Pulp"
    }
}

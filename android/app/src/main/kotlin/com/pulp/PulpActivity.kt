package com.pulp

import android.content.ComponentCallbacks2
import android.content.res.Configuration
import android.os.Bundle
import android.util.Log
import android.widget.FrameLayout
import androidx.activity.ComponentActivity
import com.pulp.render.PulpSurfaceView

class PulpActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Log.i(PulpApplication.LOG_TAG, "PulpActivity.onCreate")

        if (PulpApplication.nativeLoaded) nativeOnForeground()

        // Fullscreen Pulp rendering surface
        val surfaceView = PulpSurfaceView(this)
        val frame = FrameLayout(this)
        frame.addView(surfaceView)
        setContentView(frame)
    }

    override fun onResume() {
        super.onResume()
        if (PulpApplication.nativeLoaded) nativeOnForeground()
    }

    override fun onPause() {
        super.onPause()
        if (PulpApplication.nativeLoaded) nativeOnBackground()
    }

    override fun onDestroy() {
        if (PulpApplication.nativeLoaded) nativeOnShutdown()
        super.onDestroy()
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        val dm = resources.displayMetrics
        val dark = newConfig.uiMode and Configuration.UI_MODE_NIGHT_MASK ==
                Configuration.UI_MODE_NIGHT_YES
        if (PulpApplication.nativeLoaded)
            nativeOnDisplayChanged(dm.widthPixels, dm.heightPixels, dm.density, dark)
    }

    override fun onTrimMemory(level: Int) {
        super.onTrimMemory(level)
        val pressureLevel = when (level) {
            ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN -> 0
            ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW,
            ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL -> 1
            ComponentCallbacks2.TRIM_MEMORY_BACKGROUND,
            ComponentCallbacks2.TRIM_MEMORY_MODERATE,
            ComponentCallbacks2.TRIM_MEMORY_COMPLETE -> 2
            else -> return
        }
        if (PulpApplication.nativeLoaded) nativeOnMemoryPressure(pressureLevel)
    }

    private external fun nativeOnForeground()
    private external fun nativeOnBackground()
    private external fun nativeOnShutdown()
    private external fun nativeOnMemoryPressure(level: Int)
    private external fun nativeOnDisplayChanged(w: Int, h: Int, density: Float, dark: Boolean)
    external fun nativeOnPermissionResult(permission: Int, granted: Boolean)
}

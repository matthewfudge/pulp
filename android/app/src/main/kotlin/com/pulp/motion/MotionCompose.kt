package com.pulp.motion

import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.layout.boundsInWindow
import androidx.compose.ui.layout.onGloballyPositioned

/**
 * Jetpack Compose analog of `View.pulpMotionTrace(...)`.
 *
 * Attach to any Composable that has a layout slot:
 *
 * ```kotlin
 * Box(
 *     Modifier
 *         .size(120.dp)
 *         .pulpMotionGeometry("Card") {
 *             +Trace.value("opacity", opacity)
 *             +Trace.geometry("frame")
 *         }
 * ) { ... }
 * ```
 *
 * Behavior:
 *
 *  - When [PulpMotion.isTracingEnabled] is false, the modifier returns
 *    the receiver unchanged — zero Compose-side overhead beyond a
 *    single branch in `composed { }`.
 *  - When tracing is on, registers a geometry trace via
 *    [PulpMotion.registerGeometryTrace], plumbs `boundsInWindow()`
 *    deltas from `Modifier.onGloballyPositioned` into
 *    [PulpMotion.updateGeometry], and detaches in `DisposableEffect`
 *    when the composable leaves composition.
 *  - Initial `MotionMetric.Value` declarations publish once at attach
 *    time, mirroring `View.pulpMotionTrace` and the Swift modifier.
 *
 * Lives in the app source set rather than a separate Gradle module —
 * the Android app already pulls in Compose deps. If Pulp ever ships a
 * non-Compose Android SDK artifact, this file is the one to split.
 */
fun Modifier.pulpMotionGeometry(
    name: String,
    fps: Int = PulpMotion.DEFAULT_FPS,
    build: MotionTraceScope.() -> Unit,
): Modifier = composed {
    if (!PulpMotion.isTracingEnabled) return@composed this

    // `remember` the parsed metric set so the trace block runs once
    // per composition entry, matching how a `View.pulpMotionTrace`
    // caller would attach exactly once in `onAttachedToWindow`.
    val metrics = remember(name) { motionTrace(build) }
    val traceIdHolder = remember(name) { mutableIntStateOf(0) }
    val geometryMetricNames = remember(metrics) {
        metrics.mapNotNull { metric ->
            when (metric) {
                is MotionMetric.Geometry -> metric.name
                is MotionMetric.ScrollGeometry -> metric.name
                else -> null
            }
        }.ifEmpty { listOf("frame") }
    }

    DisposableEffect(name, fps) {
        val id = PulpMotion.registerGeometryTrace(name, fps)
        traceIdHolder.intValue = id

        if (id != 0) {
            PulpMotion.withProvenance(kind = "android", id = name) {
                for (m in metrics) {
                    if (m is MotionMetric.Value) {
                        PulpMotion.publishValue(
                            view = name,
                            metric = m.name,
                            value = m.value,
                            epsilon = m.epsilon,
                            precision = m.precision,
                        )
                    }
                }
            }
        }

        onDispose {
            val current = traceIdHolder.intValue
            traceIdHolder.intValue = 0
            PulpMotion.detachTrace(current)
        }
    }

    this.onGloballyPositioned { coords ->
        val id = traceIdHolder.intValue
        if (id == 0) return@onGloballyPositioned
        val r: Rect = coords.boundsInWindow()
        for (mName in geometryMetricNames) {
            PulpMotion.updateGeometry(
                traceId = id,
                metricName = mName,
                minX = r.left.toDouble(),
                minY = r.top.toDouble(),
                width = r.width.toDouble(),
                height = r.height.toDouble(),
            )
        }
    }
}

/**
 * Composition-local helper that mirrors [PulpMotion.publishValue]
 * inside `@Composable` code — handy when wiring an existing
 * `animateFloatAsState` value into the motion stream without writing
 * a side-effect block by hand.
 *
 * ```kotlin
 * val opacity by animateFloatAsState(targetValue = visible.toFloat())
 * pulpMotionPublish("Card", "opacity", opacity.toDouble())
 * ```
 */
@Composable
fun pulpMotionPublish(
    view: String,
    metric: String,
    value: Double,
    epsilon: Double = PulpMotion.DEFAULT_EPSILON,
    precision: Int = PulpMotion.DEFAULT_PRECISION,
) {
    if (!PulpMotion.isTracingEnabled) return
    // We deliberately do NOT remember the previous value — the
    // Coordinator already deduplicates inside its publish channel via
    // the per-metric epsilon, so a re-publish on every recomposition
    // is cheap and correct.
    PulpMotion.publishValue(view, metric, value, epsilon, precision)
}

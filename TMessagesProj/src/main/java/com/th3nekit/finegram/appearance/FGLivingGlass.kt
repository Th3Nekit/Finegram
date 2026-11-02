package com.th3nekit.finegram.appearance

import android.content.Context
import android.graphics.LinearGradient
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.Shader
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.os.Build
import android.os.SystemClock
import android.view.Surface
import android.view.View
import android.view.WindowManager
import com.th3nekit.finegram.core.configs.FinegramChatsConfig
import org.telegram.messenger.ApplicationLoader
import org.telegram.messenger.LiteMode
import java.util.Collections
import java.util.WeakHashMap
import kotlin.math.abs

object FGLivingGlass : SensorEventListener {

    private const val SMOOTHING = 0.06f
    private const val STEP = 0.035f
    private const val MIN_FRAME_MS = 90L
    private const val FULL_TILT = 6f
    private const val TRAVEL = 0.42f
    private const val SPREAD = 0.8f
    private const val EDGE_ALPHA = 0x55
    private const val CHECK_INTERVAL_MS = 1000L

    private val hosts: MutableSet<View> = Collections.newSetFromMap(WeakHashMap())
    private val shaders = HashMap<Int, LinearGradient>()
    private val matrix = Matrix()
    private var sensorManager: SensorManager? = null
    private var running = false
    private var unavailable = false
    private var smoothed = 0f
    private var tilt = 0f
    private var lastCheck = 0L
    private var lastPublish = 0L
    private var rotation = Surface.ROTATION_0

    @JvmStatic
    var stamp = 0
        private set

    @JvmStatic
    fun track(view: View?) {
        if (view != null) {
            hosts.add(view)
        }
    }

    private fun enabled(): Boolean =
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.O &&
            FinegramChatsConfig.livingGlass &&
            FinegramChatsConfig.glareEffects &&
            !LiteMode.isPowerSaverApplied()

    @JvmStatic
    fun apply(paint: Paint, left: Float, right: Float, top: Boolean): Boolean {
        if (!enabled()) {
            if (running) {
                stop()
            }
            return false
        }
        if (!running && !unavailable) {
            start()
        }
        val width = right - left
        if (width <= 0f) {
            return false
        }
        val rgb = paint.color and 0xFFFFFF
        val shader = shaders.getOrPut(rgb) {
            LinearGradient(
                -1f, 0f, 1f, 0f,
                intArrayOf(rgb or (EDGE_ALPHA shl 24), rgb or (0xFF shl 24), rgb or (EDGE_ALPHA shl 24)),
                null, Shader.TileMode.CLAMP
            )
        }
        val center = left + width * (0.5f + (if (top) TRAVEL else -TRAVEL) * tilt)
        matrix.setScale(width * SPREAD, 1f)
        matrix.postTranslate(center, 0f)
        shader.setLocalMatrix(matrix)
        paint.shader = shader
        return true
    }

    @JvmStatic
    fun clear(paint: Paint) {
        paint.shader = null
    }

    private fun start() {
        val manager = sensorManager
            ?: (ApplicationLoader.applicationContext.getSystemService(Context.SENSOR_SERVICE) as? SensorManager)?.also { sensorManager = it }
        val sensor = manager?.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
        if (manager == null || sensor == null) {
            unavailable = true
            return
        }
        updateRotation()
        lastCheck = SystemClock.uptimeMillis()
        running = manager.registerListener(this, sensor, SensorManager.SENSOR_DELAY_UI)
        unavailable = !running
    }

    private fun stop() {
        sensorManager?.unregisterListener(this)
        running = false
    }

    private fun updateRotation() {
        val manager = ApplicationLoader.applicationContext.getSystemService(Context.WINDOW_SERVICE) as? WindowManager
        @Suppress("DEPRECATION")
        rotation = manager?.defaultDisplay?.rotation ?: Surface.ROTATION_0
    }

    private fun anyHostVisible(): Boolean =
        hosts.any { it.isAttachedToWindow && it.windowVisibility == View.VISIBLE }

    override fun onSensorChanged(event: SensorEvent) {
        val x = event.values[0]
        val y = event.values[1]
        val axis = when (rotation) {
            Surface.ROTATION_90 -> -y
            Surface.ROTATION_180 -> x
            Surface.ROTATION_270 -> y
            else -> -x
        }
        smoothed += (axis - smoothed) * SMOOTHING
        val next = (smoothed / FULL_TILT).coerceIn(-1f, 1f)
        val now = SystemClock.uptimeMillis()
        if (abs(next - tilt) >= STEP && now - lastPublish >= MIN_FRAME_MS) {
            lastPublish = now
            tilt = next
            stamp++
            val iterator = hosts.iterator()
            while (iterator.hasNext()) {
                val view = iterator.next()
                if (view.isAttachedToWindow && view.windowVisibility == View.VISIBLE) {
                    view.invalidate()
                }
            }
        }
        if (now - lastCheck >= CHECK_INTERVAL_MS) {
            lastCheck = now
            updateRotation()
            if (!enabled() || !anyHostVisible()) {
                stop()
            }
        }
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {
    }
}

/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.configs

import android.app.Activity
import android.content.SharedPreferences
import org.telegram.messenger.ApplicationLoader
import org.telegram.messenger.SharedConfig
import com.th3nekit.finegram.camera.CameraXUtils
import com.th3nekit.finegram.preferences.boolean
import com.th3nekit.finegram.preferences.int
import com.th3nekit.finegram.preferences.string
import java.util.Calendar

object FinegramCameraConfig {

    private fun defaultCameraType(): Int = try {
        if (com.th3nekit.finegram.camera.CameraXUtils.isCameraXSupported()) CAMERA_X else TELEGRAM_CAMERA
    } catch (e: Throwable) {
        TELEGRAM_CAMERA
    }

    private val sharedPreferences: SharedPreferences = ApplicationLoader.applicationContext.getSharedPreferences("mainconfig", Activity.MODE_PRIVATE)

    const val TELEGRAM_CAMERA = 0
    const val CAMERA_X = 1
    const val CAMERA_2 = 2
    const val SYSTEM_CAMERA = 3

    var videoMessageWatermark by sharedPreferences.boolean("FG_VideoMessageWatermark", false)

    var cameraType by sharedPreferences.int("CP_CameraType", defaultCameraType())

    var disableAttachCamera by sharedPreferences.boolean("CP_DisableAttachCam", true)
    var useDualCamera by sharedPreferences.boolean("CP_UseDualCameraX", false)

    const val Camera16to9 = 0
    const val Camera4to3 = 1
    const val Camera1to1 = 2
    const val CameraAspectDefault = 3
    var cameraAspectRatio by sharedPreferences.int("CP_CameraAspectRatio", CameraAspectDefault)

    var cameraResolution by sharedPreferences.int("CP_CameraResolution", -1)
    var startFromUltraWideCam by sharedPreferences.boolean("CP_StartFromUltraWideCam", true)

    const val CameraXFpsRangeDefault = 0
    const val CameraXFpsRange25to30 = 1
    const val CameraXFpsRange30to30 = 2
    const val CameraXFpsRange30to60 = 3
    const val CameraXFpsRange60to60 = 4
    var cameraXFpsRange by sharedPreferences.int("CP_CameraXFpsRangeValueF",
        if (SharedConfig.getDevicePerformanceClass() >= SharedConfig.PERFORMANCE_CLASS_AVERAGE) CameraXFpsRange25to30 else CameraXFpsRangeDefault)

    var videoStabilisation by sharedPreferences.boolean("CP_VideoStabilisation", false)
    var opticalStabilisation by sharedPreferences.boolean("CP_OpticalStabilisation", false)
    var continuousAutofocus by sharedPreferences.boolean("CP_ContinuousAutofocus", false)
    var noiceReduction by sharedPreferences.boolean("CP_NoiceReduction", false)
    var faceDetection by sharedPreferences.boolean("CP_FaceDetection", false)
    var bokehEffect by sharedPreferences.boolean("CP_BokehEffect", false)

    var lensSwitcher by sharedPreferences.boolean("CP_LensSwitcher", true)

    var centerCameraControlButtons by sharedPreferences.boolean("CP_CenterCameraControlButtons", true)
    var cameraControlButtonsRight by sharedPreferences.boolean("FG_CameraControlsRight", false)
    var cameraExposureIndex by sharedPreferences.int("FG_CameraExposureIndex", 0)
    var roundZoomScale by sharedPreferences.boolean("FG_RoundZoomScale", true)
    var smoothCameraModuleTransitions by sharedPreferences.boolean("FG_SmoothCameraTransitions", false)
    var roundCamLogicalDisabled by sharedPreferences.boolean("FG_RoundLogicalDisabled", false)
    var newCameraAudio by sharedPreferences.boolean("FG_NewCameraAudio", false)

    const val EXPOSURE_SLIDER_NONE = 0
    const val EXPOSURE_SLIDER_BOTTOM = 1
    const val EXPOSURE_SLIDER_RIGHT = 2
    const val EXPOSURE_SLIDER_LEFT = 3
    var exposureSlider by sharedPreferences.int("CP_ExposureSlider", EXPOSURE_SLIDER_RIGHT)

    const val ROUND_CAMERA_FRONT = 0
    const val ROUND_CAMERA_REAR = 1
    const val ROUND_CAMERA_LAST = 2
    const val ROUND_CAMERA_ASK = 3
    var roundStartCamera by sharedPreferences.int("FG_RoundStartCamera", ROUND_CAMERA_ASK)

    var manualCameraId by sharedPreferences.boolean("FG_ManualCameraId", false)
    var manualCameraIdFront by sharedPreferences.string("FG_ManualCameraIdFront", "")
    var manualCameraIdRear by sharedPreferences.string("FG_ManualCameraIdRear", "")

    @JvmStatic
    fun manualIdFor(front: Boolean): String {
        if (!manualCameraId) return ""
        return if (front) manualCameraIdFront else manualCameraIdRear
    }
    var roundLastRear by sharedPreferences.boolean("FG_RoundLastRear", false)

    fun startWithRear(): Boolean = when (roundStartCamera) {
        ROUND_CAMERA_REAR -> true
        ROUND_CAMERA_LAST, ROUND_CAMERA_ASK -> roundLastRear
        else -> false
    }

    private var pendingRoundRear: Boolean? = null

    fun setPendingRoundRear(rear: Boolean) {
        pendingRoundRear = rear
    }

    fun takePendingRoundRear(): Boolean? {
        val value = pendingRoundRear
        pendingRoundRear = null
        return value
    }

    var videoMessagesResolution by sharedPreferences.int("FG_Round_Video_Resolution", 512)
    var videoMessagesHintCount by sharedPreferences.int("FG_Round_Flash_Hint_Count", 0)

    fun checkVideoMessagesHint() {
        try {
            val calendar = Calendar.getInstance()
            val dayOfMonth = calendar.get(Calendar.DAY_OF_MONTH)

            if (dayOfMonth == 1) {
                videoMessagesHintCount = 0
            }
        } catch (_: Exception) {}
    }

    var videoMessagesBitrateKbps by sharedPreferences.int("FG_RoundVideoBitrate", 1000)
    var videoMessagesAudioBitrateKbps by sharedPreferences.int("FG_RoundAudioBitrate", 64)
}

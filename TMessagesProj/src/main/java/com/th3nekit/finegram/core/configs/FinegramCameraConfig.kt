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

    /** CameraX там, где он не тормозит; иначе встроенная камера Telegram. */
    private fun defaultCameraType(): Int = try {
        if (com.th3nekit.finegram.camera.CameraXUtils.isCameraXSupported()) CAMERA_X else TELEGRAM_CAMERA
    } catch (e: Throwable) {
        TELEGRAM_CAMERA
    }


    private val sharedPreferences: SharedPreferences = ApplicationLoader.applicationContext.getSharedPreferences("mainconfig", Activity.MODE_PRIVATE)

    /** Camera type start */
    const val TELEGRAM_CAMERA = 0
    const val CAMERA_X = 1
    const val CAMERA_2 = 2
    const val SYSTEM_CAMERA = 3
    /**
     * Водяной знак на записываемых видеосообщениях. В Cherrygram он стоял всегда и уходил
     * собеседникам вместе с кружком; здесь это выбор пользователя, по умолчанию выключено.
     */
    var videoMessageWatermark by sharedPreferences.boolean("FG_VideoMessageWatermark", false)

    /**
     * Какой камерой снимать.
     *
     * По умолчанию — родная камера Telegram: она работает везде. CameraX в нашей сборке
     * пока не доведён до конца, и ставить его по умолчанию нельзя — кружки просто не
     * записывались бы.
     */
    /**
     * Какой камерой снимать.
     *
     * Раньше по умолчанию стояла встроенная камера Telegram, и весь наш выбор объектива —
     * поиск широкого, отсев макро, стабилизация, режимы расширений — просто не работал:
     * им управляет только путь CameraX. Теперь на устройствах, которые CameraX тянут, он и
     * стоит по умолчанию; на слабых остаётся камера Telegram, как и было.
     *
     * Однажды выбранное человеком значение не трогаем — оно уже лежит в хранилище.
     */
    var cameraType by sharedPreferences.int("CP_CameraType", defaultCameraType())
    /** Camera type finish */

    /** Camera start */
    var disableAttachCamera by sharedPreferences.boolean("CP_DisableAttachCam", true)
    var useDualCamera by sharedPreferences.boolean("CP_UseDualCameraX", false)

    const val Camera16to9 = 0
    const val Camera4to3 = 1
    const val Camera1to1 = 2
    const val CameraAspectDefault = 3
    var cameraAspectRatio by sharedPreferences.int("CP_CameraAspectRatio", CameraAspectDefault)
    /** Camera finish */

    /** Videomessages start */
    var cameraResolution by sharedPreferences.int("CP_CameraResolution", -1)
    var startFromUltraWideCam by sharedPreferences.boolean("CP_StartFromUltraWideCam", true)

    /** CameraX FPS start */
    const val CameraXFpsRangeDefault = 0
    const val CameraXFpsRange25to30 = 1
    const val CameraXFpsRange30to30 = 2
    const val CameraXFpsRange30to60 = 3
    const val CameraXFpsRange60to60 = 4
    var cameraXFpsRange by sharedPreferences.int("CP_CameraXFpsRangeValueF",
        if (SharedConfig.getDevicePerformanceClass() >= SharedConfig.PERFORMANCE_CLASS_AVERAGE) CameraXFpsRange25to30 else CameraXFpsRangeDefault)
    /** CameraX FPS finish */

    var videoStabilisation by sharedPreferences.boolean("CP_VideoStabilisation", false)
    var opticalStabilisation by sharedPreferences.boolean("CP_OpticalStabilisation", false)
    var continuousAutofocus by sharedPreferences.boolean("CP_ContinuousAutofocus", false)
    var noiceReduction by sharedPreferences.boolean("CP_NoiceReduction", false)
    var faceDetection by sharedPreferences.boolean("CP_FaceDetection", false)
    var bokehEffect by sharedPreferences.boolean("CP_BokehEffect", false)

    /**
     * Переключатель объективов под кадром: 0,5x / 1x / 2x одним касанием.
     * Работает только на CameraX — только он сообщает настоящие кратности.
     */
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

    /**
     * С какой камеры начинается кружок: фронтальной, основной, той, что была в прошлый
     * раз, или спросить. «Спросить» — меню прямо на зажатой кнопке записи: палец ведут
     * на нужную камеру и отпускают, запись идёт с неё.
     */
    const val ROUND_CAMERA_FRONT = 0
    const val ROUND_CAMERA_REAR = 1
    const val ROUND_CAMERA_LAST = 2
    const val ROUND_CAMERA_ASK = 3
    var roundStartCamera by sharedPreferences.int("FG_RoundStartCamera", ROUND_CAMERA_ASK)

    /**
     * Назвать камеру самому, а не искать её по признаку «передняя/задняя».
     *
     * На части устройств под одним признаком числится несколько объективов, и клиент
     * берёт не тот: вместо обычной камеры открывается широкоугольная или макро. Номер
     * снимает этот спор. Пустая строка означает «искать как обычно».
     */
    var manualCameraId by sharedPreferences.boolean("FG_ManualCameraId", false)
    var manualCameraIdFront by sharedPreferences.string("FG_ManualCameraIdFront", "")
    var manualCameraIdRear by sharedPreferences.string("FG_ManualCameraIdRear", "")

    /** Номер камеры, заданный вручную, или пусто — тогда ищем сами. */
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

    /** Выбор из меню на кнопке записи. Действует на одну запись и сразу забывается. */
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
    /** Videomessages finish */


    /**
     * Качество кружков.
     *
     * Telegram жмёт кружки одинаково для всех, и на хорошей камере это
     * видно. Значения в килобитах в секунду; звук отдельно, потому что голос страдает от
     * сжатия заметнее картинки.
     */
    var videoMessagesBitrateKbps by sharedPreferences.int("FG_RoundVideoBitrate", 1000)
    var videoMessagesAudioBitrateKbps by sharedPreferences.int("FG_RoundAudioBitrate", 64)
}

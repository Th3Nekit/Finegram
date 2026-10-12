/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core

import android.content.Context
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.view.View
import android.view.ViewGroup
import androidx.core.view.children
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.ApplicationLoader
import com.th3nekit.finegram.core.configs.FinegramChatsConfig

object VibrateUtil {

    lateinit var vibrator: Vibrator

    fun disableHapticFeedback(view: View) {
        view.isHapticFeedbackEnabled = false
        (view as? ViewGroup)?.children?.forEach(VibrateUtil::disableHapticFeedback)
    }

    @JvmOverloads
    fun vibrate(time: Long = 200L) {

        if (FinegramChatsConfig.disableVibration) return

        if (!VibrateUtil::vibrator.isInitialized) {
            vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val vibratorManager =
                    ApplicationLoader.applicationContext.getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as VibratorManager
                vibratorManager.defaultVibrator
            } else {
                @Suppress("DEPRECATION")
                ApplicationLoader.applicationContext.getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
            }
        }

        if (!vibrator.hasVibrator()) return

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            runCatching {
                val effect = VibrationEffect.createOneShot(time, VibrationEffect.DEFAULT_AMPLITUDE)
                vibrator.vibrate(effect, null)
            }
        } else {
            runCatching {
                @Suppress("DEPRECATION")
                vibrator.vibrate(time)
            }
        }
    }

    fun makeClickVibration() {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                val vibrator = AndroidUtilities.getVibrator()
                val vibrationEffect = VibrationEffect.createPredefined(
                    VibrationEffect.EFFECT_CLICK
                )
                vibrator.cancel()
                vibrator.vibrate(vibrationEffect)
            }
        } catch (ignore: Exception) { }
    }

    fun makeWaveVibration() {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                val vibrator = AndroidUtilities.getVibrator()
                val vibrationEffect = VibrationEffect.createWaveform(
                    longArrayOf(75, 10, 5, 10),
                    intArrayOf(5, 20, 90, 20),
                    -1
                )
                vibrator.cancel()
                vibrator.vibrate(vibrationEffect)
            }
        } catch (ignore: Exception) { }
    }

}
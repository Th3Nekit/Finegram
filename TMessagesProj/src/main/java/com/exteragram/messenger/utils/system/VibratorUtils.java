/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.utils.system;

import android.content.Context;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.HapticFeedbackConstants;
import android.view.View;

import com.th3nekit.finegram.core.configs.FinegramChatsConfig;

import org.telegram.messenger.ApplicationLoader;

/**
 * Отдача для плагинов: короткая вибрация и тип отдачи для долгого нажатия.
 *
 * Всё проходит через нашу настройку «без вибрации»: если человек её выключил, плагин тоже
 * молчит.
 */
public final class VibratorUtils {

    private static final long DEFAULT_MS = 30;

    private static boolean allowed() {
        return !FinegramChatsConfig.INSTANCE.getDisableVibration();
    }

    private static Vibrator vibrator() {
        return (Vibrator) ApplicationLoader.applicationContext.getSystemService(Context.VIBRATOR_SERVICE);
    }

    /**
     * Вид отдачи для {@code View.performHapticFeedback}. Отказать тут нельзя — номер всё
     * равно уйдёт в систему, — поэтому при выключенной вибрации отдаём самый тихий вид.
     */
    public static int getType(int type) {
        return allowed() ? type : HapticFeedbackConstants.CLOCK_TICK;
    }

    public static void vibrate() {
        vibrate(DEFAULT_MS);
    }

    public static void vibrate(long millis) {
        if (!allowed()) {
            return;
        }
        final Vibrator vibrator = vibrator();
        if (vibrator == null || !vibrator.hasVibrator()) {
            return;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            vibrator.vibrate(VibrationEffect.createOneShot(millis, VibrationEffect.DEFAULT_AMPLITUDE));
        } else {
            vibrator.vibrate(millis);
        }
    }

    public static void vibrateEffect(VibrationEffect effect) {
        if (!allowed() || effect == null || Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return;
        }
        final Vibrator vibrator = vibrator();
        if (vibrator != null && vibrator.hasVibrator()) {
            vibrator.vibrate(effect);
        }
    }

    /** Отключить системную отдачу у вида: плагин рисует свою. */
    public static void disableHapticFeedback(View view) {
        if (view != null) {
            view.setHapticFeedbackEnabled(false);
        }
    }

    private VibratorUtils() {
    }
}

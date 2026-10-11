/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.chats.ui;

import android.app.Activity;
import android.graphics.RenderEffect;
import android.graphics.Shader;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

import androidx.annotation.RequiresApi;

public class WindowBlurHelper {

    @RequiresApi(Build.VERSION_CODES.S)
    public void setWindowBlur(
            Activity activity,
            boolean enable,
            boolean hideStatusBar,
            float windowBlurRadius,
            float windowDimAlpha
    ) {
        if (activity == null) return;

        Window window = activity.getWindow();
        View root = window.getDecorView();

        hideStatusBar(window, hideStatusBar);

        if (enable) {
            root.setRenderEffect(
                    RenderEffect.createBlurEffect(windowBlurRadius, windowBlurRadius, Shader.TileMode.DECAL)
            );

            int alpha = (int) (Math.max(0f, Math.min(1f, windowDimAlpha)) * 255);
            root.setForeground(new ColorDrawable(alpha << 24));

        } else {
            root.setRenderEffect(null);
            root.setForeground(null);
        }
    }

    public static void hideStatusBar(Window window, boolean hide) {
        if (hide) {
            window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        } else {
            window.clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        }
    }

}

package com.th3nekit.finegram.chats.ui;

import android.view.View;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.SharedConfig;
import org.telegram.ui.Components.CubicBezierInterpolator;

public final class MessageMenuMotion {
    public static boolean prepare(View menu, boolean outgoing) {
        menu.animate().cancel();
        boolean enabled = SharedConfig.animationsEnabled()
                && !AndroidUtilities.isAccessibilityTouchExplorationEnabled();
        menu.setPivotX(outgoing ? menu.getMeasuredWidth() : 0);
        menu.setPivotY(0);
        menu.setAlpha(enabled ? 0 : 1);
        menu.setScaleX(enabled ? 0.97f : 1);
        menu.setScaleY(enabled ? 0.97f : 1);
        menu.setTranslationY(enabled ? -AndroidUtilities.dp(4) : 0);
        return enabled;
    }

    public static void start(View menu) {
        menu.animate().alpha(1).scaleX(1).scaleY(1).translationY(0)
                .setDuration(180).setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT).start();
    }

    public static void cancel(View menu) {
        menu.animate().cancel();
    }
}

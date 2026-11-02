package com.th3nekit.finegram.chats.ui;

import android.animation.ValueAnimator;
import android.os.Build;
import android.view.View;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.SharedConfig;

public final class MessageMenuMotion {
    public static boolean isEnabled() {
        return SharedConfig.animationsEnabled()
                && !AndroidUtilities.isAccessibilityTouchExplorationEnabled()
                && (Build.VERSION.SDK_INT < 26 || ValueAnimator.areAnimatorsEnabled());
    }

    public static void prepare(View menu, View reactions, boolean outgoing) {
        cancel(menu);
        menu.setPivotX(outgoing ? menu.getMeasuredWidth() : 0);
        menu.setPivotY(0);
        if (reactions != null) {
            cancel(reactions);
            reactions.setPivotX(outgoing ? reactions.getMeasuredWidth() : 0);
            reactions.setPivotY(reactions.getMeasuredHeight());
        }
        apply(menu, reactions, isEnabled() ? 0 : 1);
    }

    public static void apply(View menu, View reactions, float progress) {
        float p = Float.isNaN(progress) ? 0 : Math.max(0, Math.min(1, progress));
        menu.setAlpha(Math.min(1, p * 8.4f));
        menu.setScaleX(0.01f + 0.99f * p);
        menu.setScaleY(0.01f + 0.99f * p);
        menu.setTranslationY(0);
        if (reactions != null) {
            reactions.setAlpha(p);
            reactions.setScaleX(0.98f + 0.02f * p);
            reactions.setScaleY(0.98f + 0.02f * p);
            reactions.setTranslationY(AndroidUtilities.dp(4) * (1 - p));
        }
    }

    public static long duration(float from, float to) {
        return Math.round((to > from ? 420 : 200) * Math.min(1, Math.abs(to - from)));
    }

    public static float spring(float fraction) {
        float t = Math.max(0, Math.min(1, fraction));
        double end = 1 - 11 * Math.exp(-10);
        return (float) ((1 - (1 + 10 * t) * Math.exp(-10 * t)) / end);
    }

    public static void cancel(View menu) {
        menu.animate().cancel();
    }
}

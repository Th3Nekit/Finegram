package org.telegram.ui.Components;

import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.StateListAnimator;
import android.animation.ValueAnimator;
import android.view.View;
import android.view.animation.OvershootInterpolator;

import java.util.function.Consumer;

public class ScaleStateListAnimator {

    public static void apply(View view) {
        apply(view, .1f, 1.5f);
    }

    public static void apply(View view, float scale, float tension) {
        apply(view, scale, tension, null, null);
    }

    public static void apply(View view, float scale, float tension, Consumer<Float> onPress, Consumer<Float> onRelease) {
        if (view == null) {
            return;
        }

        AnimatorSet pressedAnimator = new AnimatorSet();
        pressedAnimator.playTogether(
                ObjectAnimator.ofFloat(view, View.SCALE_X, 1f - scale),
                ObjectAnimator.ofFloat(view, View.SCALE_Y, 1f - scale)
        );
        pressedAnimator.setDuration(80);

        AnimatorSet defaultAnimator = new AnimatorSet();
        defaultAnimator.playTogether(
                ObjectAnimator.ofFloat(view, View.SCALE_X, 1f),
                ObjectAnimator.ofFloat(view, View.SCALE_Y, 1f)
        );
        defaultAnimator.setInterpolator(new OvershootInterpolator(tension));
        defaultAnimator.setDuration(350);

        StateListAnimator scaleStateListAnimator = new StateListAnimator();

        if (onPress != null) {
            ValueAnimator progress = ValueAnimator.ofFloat(0f, 1f);
            progress.setDuration(80);
            progress.addUpdateListener(a -> onPress.accept((Float) a.getAnimatedValue()));
            AnimatorSet set = new AnimatorSet();
            set.playTogether(pressedAnimator, progress);
            scaleStateListAnimator.addState(new int[]{android.R.attr.state_pressed}, set);
        } else {
            scaleStateListAnimator.addState(new int[]{android.R.attr.state_pressed}, pressedAnimator);
        }
        if (onRelease != null) {
            ValueAnimator progress = ValueAnimator.ofFloat(1f, 0f);
            progress.setDuration(350);
            progress.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
            progress.addUpdateListener(a -> onRelease.accept((Float) a.getAnimatedValue()));
            AnimatorSet set = new AnimatorSet();
            set.playTogether(defaultAnimator, progress);
            scaleStateListAnimator.addState(new int[0], set);
        } else {
            scaleStateListAnimator.addState(new int[0], defaultAnimator);
        }

        view.setStateListAnimator(scaleStateListAnimator);
    }

    public static void reset(View view) {
        view.setStateListAnimator(null);
    }

}

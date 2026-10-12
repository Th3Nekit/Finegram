package org.telegram.ui.iv;

import android.animation.ValueAnimator;

import org.telegram.ui.Components.CubicBezierInterpolator;

class RichBlockInset {

    interface Applier {
        void apply(int px);
    }

    private long boundRowId = Long.MIN_VALUE;
    private int currentPx = -1;
    private ValueAnimator animator;

    int current() {
        return Math.max(0, currentPx);
    }

    void apply(BlockRow row, Applier applier) {
        apply(row, applier, true);
    }

    void apply(BlockRow row, Applier applier, boolean animated) {
        final int target = RichBlockChrome.insetFor(row);
        final long rid = row != null ? row.id : Long.MIN_VALUE;
        final boolean sameBlock = rid == boundRowId && currentPx >= 0;
        boundRowId = rid;
        if (animator != null) {
            animator.cancel();
            animator = null;
        }
        if (animated && sameBlock && currentPx != target) {
            final int from = currentPx;
            final ValueAnimator a = ValueAnimator.ofInt(from, target);
            a.addUpdateListener(anim -> {
                currentPx = (int) anim.getAnimatedValue();
                applier.apply(currentPx);
            });
            a.setInterpolator(CubicBezierInterpolator.DEFAULT);
            a.setDuration(200);
            animator = a;
            a.start();
        } else {
            currentPx = target;
            applier.apply(target);
        }
    }
}

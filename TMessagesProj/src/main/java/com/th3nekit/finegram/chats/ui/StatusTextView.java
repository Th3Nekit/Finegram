package com.th3nekit.finegram.chats.ui;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.provider.Settings;
import android.text.Spanned;
import android.text.TextUtils;
import android.view.accessibility.AccessibilityManager;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.ui.ActionBar.SimpleTextView;
import org.telegram.ui.Components.AnimatedTextView;
import org.telegram.ui.Components.CubicBezierInterpolator;

public class StatusTextView extends SimpleTextView {
    private static final CubicBezierInterpolator TRANSITION = new CubicBezierInterpolator(.22, 1, .36, 1);
    private final AnimatedTextView.AnimatedTextDrawable status = new AnimatedTextView.AnimatedTextDrawable(false, false, false);
    private CharSequence pending;
    private boolean plain;

    public StatusTextView(Context context) {
        super(context);
        status.centerY = false;
        status.setIncludeFontPadding(false);
        status.setCallback(this);
        status.setOverrideFullWidth(Math.max(1, AndroidUtilities.displaySize.x));
        status.setOnAnimationFinishListener(() -> {
            CharSequence next = pending;
            pending = null;
            if (next != null) transition(next, canAnimate());
        });
    }

    @Override
    public boolean setText(CharSequence value, boolean force) {
        boolean changed = super.setText(value, force);
        if (status == null) return changed;
        CharSequence next = value == null ? "" : value;
        boolean previousPlain = plain;
        plain = !(next instanceof Spanned) || ((Spanned) next).getSpans(0, next.length(), Object.class).length == 0;
        plain &= !AndroidUtilities.isRTL(next) && !next.toString().contains("**oo**");
        String text = next.toString();
        if (!changed && TextUtils.equals(status.getText(), text) && pending == null) return false;
        if (!plain || !previousPlain || !canAnimate() || TextUtils.isEmpty(text) || TextUtils.isEmpty(status.getText())) {
            snap(text);
        } else if (status.isAnimating()) {
            pending = TextUtils.equals(status.getText(), text) ? null : text;
        } else {
            transition(text, true);
        }
        return changed;
    }

    private boolean canAnimate() {
        if (!isAttachedToWindow() || !isShown() || getAlpha() == 0 || getWindowVisibility() != VISIBLE) return false;
        if (Build.VERSION.SDK_INT >= 26) {
            if (!ValueAnimator.areAnimatorsEnabled()) return false;
        } else if (Settings.Global.getFloat(getContext().getContentResolver(), Settings.Global.ANIMATOR_DURATION_SCALE, 1f) == 0) {
            return false;
        }
        AccessibilityManager manager = (AccessibilityManager) getContext().getSystemService(Context.ACCESSIBILITY_SERVICE);
        return manager == null || !manager.isTouchExplorationEnabled();
    }

    private void transition(CharSequence next, boolean animate) {
        boolean counter = StatusTextTransition.isCounterChange(status.getText(), next);
        status.updateAll = !counter;
        status.preserveCommonEnds = counter;
        status.setHacks(false, false, false, counter);
        status.setAnimationProperties(counter ? 1f : 0f, 0, 460, 0, TRANSITION);
        status.copyStylesFrom(getTextPaint());
        status.setText(next, animate, false);
    }

    private void snap(CharSequence text) {
        pending = null;
        status.cancelAnimation();
        transition(text, false);
    }

    @Override
    protected void drawLayout(Canvas canvas) {
        if (!plain || !status.isAnimating() || getLayout() == null) {
            super.drawLayout(canvas);
            return;
        }
        status.copyStylesFrom(getTextPaint());
        status.setBounds(0, 0, getLayout().getWidth(), getLayout().getHeight());
        status.draw(canvas);
    }

    @Override
    public void invalidateDrawable(Drawable drawable) {
        if (drawable == status) {
            invalidate();
        } else {
            super.invalidateDrawable(drawable);
        }
    }

    @Override
    protected boolean verifyDrawable(Drawable drawable) {
        return drawable == status || super.verifyDrawable(drawable);
    }

    @Override
    protected void onDetachedFromWindow() {
        snap(getText().toString());
        super.onDetachedFromWindow();
    }

    @Override
    protected void onVisibilityChanged(android.view.View changedView, int visibility) {
        super.onVisibilityChanged(changedView, visibility);
        if (status != null && visibility != VISIBLE) snap(getText().toString());
    }

    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        if (status != null && visibility != VISIBLE) snap(getText().toString());
    }
}

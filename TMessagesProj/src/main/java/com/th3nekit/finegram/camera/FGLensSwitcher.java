/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.camera;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.NonNull;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.ui.Components.CubicBezierInterpolator;

import java.util.ArrayList;
import java.util.Locale;

public class FGLensSwitcher extends View {

    public interface Delegate {

        void onLensSelected(float ratio);
    }

    private static final int PILL_SIZE_DP = 34;
    private static final int PILL_GAP_DP = 4;
    private static final int CAPSULE_PADDING_DP = 5;
    private static final long MOVE_MS = 240;
    private static final long PRESS_MS = 160;

    private final Paint capsulePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint pillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF capsule = new RectF();

    private final ArrayList<Float> ratios = new ArrayList<>();
    private final ArrayList<String> labels = new ArrayList<>();

    private int selected = 0;
    private float highlight = 0f;
    private ValueAnimator highlightAnimator;

    private int pressedIndex = -1;
    private float pressScale = 1f;
    private ValueAnimator pressAnimator;

    private float currentRatio = 1f;
    private float knownMin = Float.NaN;
    private float knownMax = Float.NaN;

    private float baseRatio = 1f;

    private Delegate delegate;

    public FGLensSwitcher(Context context) {
        super(context);
        capsulePaint.setColor(0x66000000);
        pillPaint.setColor(0x3dffffff);
        textPaint.setTextAlign(Paint.Align.CENTER);
        textPaint.setTypeface(AndroidUtilities.bold());
        textPaint.setTextSize(dp(12));
        setContentDescription(getContext().getString(org.telegram.messenger.R.string.CP_LensSwitcher));
    }

    public void setDelegate(Delegate delegate) {
        this.delegate = delegate;
    }

    public boolean setZoomRange(float min, float max, float base) {
        final float safeBase = base > 0.01f ? base : 1f;
        if (!Float.isNaN(knownMin) && Math.abs(knownMin - min) < 0.01f
                && Math.abs(knownMax - max) < 0.01f && Math.abs(baseRatio - safeBase) < 0.01f) {
            return ratios.size() > 1;
        }
        knownMin = min;
        knownMax = max;
        baseRatio = safeBase;

        ratios.clear();
        labels.clear();
        if (min > 0f && min < baseRatio * 0.95f) {
            ratios.add(min);
        }
        ratios.add(baseRatio);
        if (max >= baseRatio * 1.95f) {
            ratios.add(baseRatio * 2f);
        }
        for (float ratio : ratios) {
            labels.add(formatRatio(ratio / baseRatio));
        }

        selected = ratios.indexOf(baseRatio);
        if (selected < 0) {
            selected = 0;
        }
        highlight = selected;
        currentRatio = ratios.get(selected);
        requestLayout();
        invalidate();
        return ratios.size() > 1;
    }

    public void setCurrentRatio(float ratio) {
        if (ratios.isEmpty() || Math.abs(currentRatio - ratio) < 0.01f) {
            return;
        }
        currentRatio = ratio;
        int nearest = 0;
        for (int i = 1; i < ratios.size(); i++) {
            if (ratio >= ratios.get(i) - 0.01f) {
                nearest = i;
            }
        }
        if (nearest != selected) {
            selected = nearest;
            animateHighlight();
        }
        invalidate();
    }

    private void animateHighlight() {
        if (highlightAnimator != null) {
            highlightAnimator.cancel();
        }
        highlightAnimator = ValueAnimator.ofFloat(highlight, selected);
        highlightAnimator.setDuration(MOVE_MS);
        highlightAnimator.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
        highlightAnimator.addUpdateListener(animation -> {
            highlight = (float) animation.getAnimatedValue();
            invalidate();
        });
        highlightAnimator.start();
    }

    private void animatePress(boolean pressed) {
        if (pressAnimator != null) {
            pressAnimator.cancel();
        }
        pressAnimator = ValueAnimator.ofFloat(pressScale, pressed ? 0.93f : 1f);
        pressAnimator.setDuration(PRESS_MS);
        pressAnimator.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
        pressAnimator.addUpdateListener(animation -> {
            pressScale = (float) animation.getAnimatedValue();
            invalidate();
        });
        pressAnimator.start();
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (highlightAnimator != null) {
            highlightAnimator.cancel();
            highlightAnimator = null;
        }
        if (pressAnimator != null) {
            pressAnimator.cancel();
            pressAnimator = null;
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        layoutCapsule();
    }

    private void layoutCapsule() {
        int count = Math.max(1, ratios.size());
        float width = count * dp(PILL_SIZE_DP) + (count - 1) * dp(PILL_GAP_DP) + 2 * dp(CAPSULE_PADDING_DP);
        float height = dp(PILL_SIZE_DP) + 2 * dp(CAPSULE_PADDING_DP);
        float centerX = getMeasuredWidth() / 2f;
        float centerY = getMeasuredHeight() / 2f;
        capsule.set(centerX - width / 2f, centerY - height / 2f, centerX + width / 2f, centerY + height / 2f);
    }

    private float pillCenterX(float index) {
        return capsule.left + dp(CAPSULE_PADDING_DP) + dp(PILL_SIZE_DP) / 2f
                + index * (dp(PILL_SIZE_DP) + dp(PILL_GAP_DP));
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (ratios.size() < 2) {
            return;
        }
        if (capsule.isEmpty()) {
            layoutCapsule();
        }
        canvas.drawRoundRect(capsule, capsule.height() / 2f, capsule.height() / 2f, capsulePaint);

        float radius = dp(PILL_SIZE_DP) / 2f;
        float centerY = capsule.centerY();
        float highlightScale = pressedIndex >= 0 && Math.abs(pressedIndex - highlight) < 0.5f ? pressScale : 1f;
        canvas.drawCircle(pillCenterX(highlight), centerY, radius * highlightScale, pillPaint);

        for (int i = 0; i < ratios.size(); i++) {
            boolean active = i == selected;
            String label = active ? formatRatio(currentRatio / baseRatio) + "x" : labels.get(i);
            textPaint.setColor(active ? 0xffffd60a : 0xe6ffffff);
            textPaint.setTextSize(dp(active ? 12.5f : 12));
            float scale = i == pressedIndex ? pressScale : 1f;
            float x = pillCenterX(i);
            canvas.save();
            canvas.scale(scale, scale, x, centerY);
            canvas.drawText(label, x, centerY - (textPaint.descent() + textPaint.ascent()) / 2f, textPaint);
            canvas.restore();
        }
    }

    @Override
    public boolean onTouchEvent(@NonNull MotionEvent event) {
        if (ratios.size() < 2) {
            return false;
        }
        int action = event.getAction();
        if (action == MotionEvent.ACTION_DOWN) {
            int index = indexAt(event.getX(), event.getY());
            if (index < 0) {
                return false;
            }
            pressedIndex = index;
            animatePress(true);
            return true;
        }
        if (pressedIndex < 0) {
            return false;
        }
        if (action == MotionEvent.ACTION_UP) {
            int index = indexAt(event.getX(), event.getY());
            if (index == pressedIndex) {
                select(index);
            }
            pressedIndex = -1;
            animatePress(false);
            return true;
        }
        if (action == MotionEvent.ACTION_CANCEL) {
            pressedIndex = -1;
            animatePress(false);
            return true;
        }
        return true;
    }

    private void select(int index) {
        performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP, HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING);
        selected = index;
        currentRatio = ratios.get(index);
        animateHighlight();
        if (delegate != null) {
            delegate.onLensSelected(ratios.get(index));
        }
    }

    private int indexAt(float x, float y) {
        if (y < capsule.top - dp(6) || y > capsule.bottom + dp(6)) {
            return -1;
        }
        float half = dp(PILL_SIZE_DP + PILL_GAP_DP) / 2f;
        for (int i = 0; i < ratios.size(); i++) {
            if (Math.abs(x - pillCenterX(i)) <= half) {
                return i;
            }
        }
        return -1;
    }

    private static String formatRatio(float ratio) {
        if (Math.abs(ratio - Math.round(ratio)) < 0.05f) {
            return String.valueOf(Math.round(ratio));
        }
        return String.format(Locale.getDefault(), "%.1f", ratio);
    }
}

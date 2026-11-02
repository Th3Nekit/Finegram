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
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.os.Bundle;
import android.view.HapticFeedbackConstants;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.accessibility.AccessibilityNodeInfo;

import androidx.annotation.NonNull;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.customview.widget.ExploreByTouchHelper;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LiteMode;
import org.telegram.messenger.SharedConfig;
import org.telegram.ui.Components.CubicBezierInterpolator;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Locale;
import java.util.List;

public class FGLensSwitcher extends View {

    public interface Delegate {
        void onLensSelected(float ratio);
    }

    private static final int PILL_SIZE_DP = 34;
    private static final int TOUCH_SIZE_DP = 48;
    private static final int CAPSULE_PADDING_DP = 5;
    private static final long MOVE_MS = 240;
    private static final long PRESS_MS = 160;

    private final Paint capsulePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint pillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF capsule = new RectF();
    private final LensAccessibility accessibility;
    private final int touchSlop;

    private final ArrayList<Float> ratios = new ArrayList<>();
    private final ArrayList<String> labels = new ArrayList<>();

    private int selected = 0;
    private float highlight = 0f;
    private ValueAnimator highlightAnimator;

    private int pressedIndex = -1;
    private float pressScale = 1f;
    private ValueAnimator pressAnimator;
    private float scrollOffset;
    private float downX;
    private float downScroll;
    private boolean dragging;
    private boolean gestureActive;

    private float currentRatio = 1f;
    private float knownMin = Float.NaN;
    private float knownMax = Float.NaN;

    private float[] knownShortcuts = new float[0];

    private Delegate delegate;

    public FGLensSwitcher(Context context) {
        super(context);
        capsulePaint.setColor(0x66000000);
        pillPaint.setColor(0x3dffffff);
        textPaint.setTextAlign(Paint.Align.CENTER);
        textPaint.setTypeface(AndroidUtilities.bold());
        textPaint.setTextSize(dp(12));
        touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        accessibility = new LensAccessibility();
        ViewCompat.setAccessibilityDelegate(this, accessibility);
        setFocusable(true);
        setContentDescription(getContext().getString(org.telegram.messenger.R.string.CP_LensSwitcher));
    }

    public void setDelegate(Delegate delegate) {
        this.delegate = delegate;
    }

    @Override
    public void setEnabled(boolean enabled) {
        if (!enabled) cancelPress();
        super.setEnabled(enabled);
        if (accessibility != null) accessibility.invalidateRoot();
        invalidate();
    }

    public boolean setZoomRange(float min, float max, float[] shortcuts, float current) {
        float[] available = CameraXZoomShortcuts.select(min, max, shortcuts);
        if (!Float.isNaN(knownMin) && Math.abs(knownMin - min) < 0.01f
                && Math.abs(knownMax - max) < 0.01f && Arrays.equals(knownShortcuts, available)) {
            setCurrentRatio(current);
            return ratios.size() > 1;
        }
        knownMin = min;
        knownMax = max;
        knownShortcuts = available;
        cancelPress();

        ratios.clear();
        labels.clear();
        for (float ratio : available) {
            ratios.add(ratio);
            labels.add(formatRatio(ratio));
        }

        if (highlightAnimator != null) highlightAnimator.cancel();
        selected = 0;
        currentRatio = Float.isFinite(current) ? current : 1f;
        for (int i = 1; i < ratios.size(); i++) {
            if (currentRatio >= ratios.get(i) - 0.01f) selected = i;
        }
        highlight = selected;
        scrollOffset = 0f;
        layoutCapsule();
        revealSelected();
        accessibility.invalidateRoot();
        requestLayout();
        invalidate();
        return ratios.size() > 1;
    }

    public void setCurrentRatio(float ratio) {
        if (ratios.isEmpty() || !Float.isFinite(ratio) || Math.abs(currentRatio - ratio) < 0.01f) {
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
            revealSelected();
            animateHighlight();
        }
        accessibility.invalidateRoot();
        invalidate();
    }

    private void animateHighlight() {
        if (highlightAnimator != null) {
            highlightAnimator.cancel();
            highlightAnimator = null;
        }
        if (!animationsEnabled()) {
            highlight = selected;
            invalidate();
            return;
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
            pressAnimator = null;
        }
        if (!animationsEnabled()) {
            pressScale = 1f;
            invalidate();
            return;
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

    private boolean animationsEnabled() {
        return isAttachedToWindow() && SharedConfig.animationsEnabled() && !LiteMode.isPowerSaverApplied()
                && (Build.VERSION.SDK_INT < 26 || ValueAnimator.areAnimatorsEnabled());
    }

    private void cancelPress() {
        pressedIndex = -1;
        dragging = false;
        gestureActive = false;
        if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(false);
        if (pressAnimator != null) {
            pressAnimator.cancel();
            pressAnimator = null;
        }
        pressScale = 1f;
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        cancelPress();
        if (highlightAnimator != null) {
            highlightAnimator.cancel();
            highlightAnimator = null;
        }
        if (pressAnimator != null) {
            pressAnimator.cancel();
            pressAnimator = null;
        }
        highlight = selected;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        layoutCapsule();
        revealSelected();
    }

    private void layoutCapsule() {
        int count = Math.max(1, ratios.size());
        float width = Math.min(count * dp(TOUCH_SIZE_DP) + 2 * dp(CAPSULE_PADDING_DP),
                Math.max(0, getMeasuredWidth() - dp(16)));
        float height = dp(PILL_SIZE_DP) + 2 * dp(CAPSULE_PADDING_DP);
        float centerX = getMeasuredWidth() / 2f;
        float centerY = getMeasuredHeight() / 2f;
        capsule.set(centerX - width / 2f, centerY - height / 2f, centerX + width / 2f, centerY + height / 2f);
        scrollOffset = Math.max(0f, Math.min(maxScroll(), scrollOffset));
    }

    private float pillCenterX(float index) {
        return capsule.left + dp(CAPSULE_PADDING_DP) + dp(TOUCH_SIZE_DP) / 2f
                + index * dp(TOUCH_SIZE_DP) - scrollOffset;
    }

    private float maxScroll() {
        return Math.max(0f, ratios.size() * dp(TOUCH_SIZE_DP)
                + 2 * dp(CAPSULE_PADDING_DP) - capsule.width());
    }

    private void revealSelected() {
        float left = selected * dp(TOUCH_SIZE_DP);
        float viewport = Math.max(0, capsule.width() - 2 * dp(CAPSULE_PADDING_DP));
        if (left < scrollOffset) scrollOffset = left;
        else if (left + dp(TOUCH_SIZE_DP) > scrollOffset + viewport) {
            scrollOffset = left + dp(TOUCH_SIZE_DP) - viewport;
        }
        scrollOffset = Math.max(0f, Math.min(maxScroll(), scrollOffset));
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
        canvas.save();
        canvas.clipRect(capsule);

        float radius = dp(PILL_SIZE_DP) / 2f;
        float centerY = capsule.centerY();
        float highlightScale = pressedIndex >= 0 && Math.abs(pressedIndex - highlight) < 0.5f ? pressScale : 1f;
        canvas.drawCircle(pillCenterX(highlight), centerY, radius * highlightScale, pillPaint);

        for (int i = 0; i < ratios.size(); i++) {
            boolean active = i == selected;
            String label = active ? formatRatio(currentRatio) + "x" : labels.get(i);
            textPaint.setColor(active ? 0xffffd60a : 0xe6ffffff);
            textPaint.setTextSize(dp(active ? 12.5f : 12));
            float scale = i == pressedIndex ? pressScale : 1f;
            float x = pillCenterX(i);
            canvas.save();
            canvas.scale(scale, scale, x, centerY);
            canvas.drawText(label, x, centerY - (textPaint.descent() + textPaint.ascent()) / 2f, textPaint);
            canvas.restore();
        }
        canvas.restore();
    }

    @Override
    public boolean onTouchEvent(@NonNull MotionEvent event) {
        if (!isEnabled() || ratios.size() < 2) {
            return false;
        }
        int action = event.getActionMasked();
        if (event.getPointerCount() > 1 || action == MotionEvent.ACTION_CANCEL) {
            cancelPress();
            invalidate();
            return true;
        }
        if (action == MotionEvent.ACTION_DOWN) {
            int index = indexAt(event.getX(), event.getY());
            if (index < 0) {
                return false;
            }
            pressedIndex = index;
            downX = event.getX();
            downScroll = scrollOffset;
            dragging = false;
            gestureActive = true;
            animatePress(true);
            return true;
        }
        if (!gestureActive) return false;
        if (action == MotionEvent.ACTION_MOVE && maxScroll() > 0f
                && (dragging || Math.abs(event.getX() - downX) > touchSlop)) {
            dragging = true;
            pressedIndex = -1;
            scrollOffset = Math.max(0f, Math.min(maxScroll(), downScroll + downX - event.getX()));
            if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(true);
            invalidate();
            accessibility.invalidateRoot();
            return true;
        }
        if (dragging && action == MotionEvent.ACTION_UP) {
            cancelPress();
            invalidate();
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
            cancelPress();
            animatePress(false);
            return true;
        }
        return true;
    }

    private void select(int index) {
        performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP);
        selected = index;
        currentRatio = ratios.get(index);
        revealSelected();
        animateHighlight();
        accessibility.invalidateRoot();
        if (delegate != null) {
            delegate.onLensSelected(ratios.get(index));
        }
    }

    private int indexAt(float x, float y) {
        if (!isEnabled() || x < capsule.left || x >= capsule.right || y < 0 || y >= getHeight()) {
            return -1;
        }
        float half = dp(TOUCH_SIZE_DP) / 2f;
        for (int i = 0; i < ratios.size(); i++) {
            if (x >= pillCenterX(i) - half && x < pillCenterX(i) + half) {
                return i;
            }
        }
        return -1;
    }

    @Override
    public boolean dispatchHoverEvent(MotionEvent event) {
        return accessibility.dispatchHoverEvent(event) || super.dispatchHoverEvent(event);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        return accessibility.dispatchKeyEvent(event) || super.dispatchKeyEvent(event);
    }

    @Override
    protected void onFocusChanged(boolean gainFocus, int direction, Rect previouslyFocusedRect) {
        super.onFocusChanged(gainFocus, direction, previouslyFocusedRect);
        accessibility.onFocusChanged(gainFocus, direction, previouslyFocusedRect);
    }

    private final class LensAccessibility extends ExploreByTouchHelper {
        LensAccessibility() { super(FGLensSwitcher.this); }

        @Override
        protected int getVirtualViewAt(float x, float y) {
            int index = indexAt(x, y);
            return index < 0 ? INVALID_ID : index;
        }

        @Override
        protected void getVisibleVirtualViews(List<Integer> ids) {
            if (ratios.size() > 1) for (int i = 0; i < ratios.size(); i++) ids.add(i);
        }

        @Override
        protected void onPopulateNodeForVirtualView(int id, AccessibilityNodeInfoCompat node) {
            if (id < 0 || id >= ratios.size()) {
                node.setContentDescription("");
                node.setBoundsInParent(new Rect());
                return;
            }
            node.setClassName("android.widget.Button");
            node.setContentDescription(labels.get(id) + "×");
            node.setSelected(id == selected);
            node.setEnabled(isEnabled());
            node.setFocusable(true);
            node.setClickable(true);
            float half = dp(TOUCH_SIZE_DP) / 2f;
            node.setBoundsInParent(new Rect(Math.round(pillCenterX(id) - half), 0,
                    Math.round(pillCenterX(id) + half), getHeight()));
            if (isEnabled()) node.addAction(AccessibilityNodeInfo.ACTION_CLICK);
        }

        @Override
        protected boolean onPerformActionForVirtualView(int id, int action, Bundle args) {
            if (!isEnabled() || id < 0 || id >= ratios.size()
                    || action != AccessibilityNodeInfo.ACTION_CLICK) return false;
            select(id);
            return true;
        }
    }

    private static String formatRatio(float ratio) {
        if (Math.abs(ratio - Math.round(ratio)) < 0.05f) {
            return String.valueOf(Math.round(ratio));
        }
        return String.format(Locale.getDefault(), "%.1f", ratio);
    }
}

/*
 * Finegram plugin store.
 * Adapted from Kangel Plugins Manager (KangelPlugins).
 * Upstream: https://git.kangel.xyz/KangelPlugins/PluginManager
 * Licensed under GNU GPL v3; see LICENSE.PluginManager and NOTICE.
 */

package com.th3nekit.finegram.store;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.View;
import android.widget.HorizontalScrollView;

import androidx.annotation.NonNull;
import androidx.core.graphics.ColorUtils;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.CubicBezierInterpolator;

import java.util.ArrayList;
import java.util.List;

public class FGStoreFilters extends HorizontalScrollView {

    public interface Delegate {
        void onPicked(int index);
    }

    private static final long MOVE_MS = 240;
    private static final long PRESS_MS = 160;
    private static final int PADDING_DP = 14;
    private static final int GAP_DP = 6;
    private static final int EDGE_DP = 16;
    private static final int PILL_DP = 32;
    private static final int HEIGHT_DP = 52;

    private final Strip strip;

    public FGStoreFilters(Context context) {
        super(context);
        setHorizontalScrollBarEnabled(false);
        setOverScrollMode(OVER_SCROLL_NEVER);

        setFocusable(false);
        setDescendantFocusability(FOCUS_BLOCK_DESCENDANTS);
        strip = new Strip(context);
        addView(strip, new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT));
    }

    public void setDelegate(Delegate delegate) {
        strip.delegate = delegate;
    }

    public void setTitles(List<String> titles, int selected) {
        strip.setTitles(titles, selected);
        requestLayout();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec,
                MeasureSpec.makeMeasureSpec(dp(HEIGHT_DP), MeasureSpec.EXACTLY));
    }

    private class Strip extends View {

        private final Paint pillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint restPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF pill = new RectF();

        private final ArrayList<String> titles = new ArrayList<>();
        private final ArrayList<Float> widths = new ArrayList<>();

        private int selected;
        private float highlight;
        private ValueAnimator highlightAnimator;

        private int pressedIndex = -1;
        private float pressScale = 1f;
        private ValueAnimator pressAnimator;

        private Delegate delegate;

        Strip(Context context) {
            super(context);
            textPaint.setTypeface(AndroidUtilities.bold());
            textPaint.setTextSize(dp(13));

            textPaint.setTextAlign(Paint.Align.CENTER);
        }

        void setTitles(List<String> list, int picked) {
            final boolean same = titles.equals(list);
            if (!same) {
                titles.clear();
                widths.clear();
                titles.addAll(list);
                for (String title : list) {
                    widths.add(textPaint.measureText(title) + dp(2 * PADDING_DP));
                }
                requestLayout();
            }
            final int wanted = Math.max(0, Math.min(picked, titles.size() - 1));
            if (!same) {
                selected = wanted;
                highlight = wanted;
            } else if (wanted != selected) {

                selected = wanted;
                animateHighlight();
                scrollToSelected();
            }
            invalidate();
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            float total = 0;
            for (float width : widths) {
                total += width + dp(GAP_DP);
            }
            setMeasuredDimension((int) (total + dp(2 * EDGE_DP)), dp(HEIGHT_DP));
        }

        private float leftOf(float index) {
            float left = dp(EDGE_DP);
            int whole = (int) index;
            for (int i = 0; i < whole && i < widths.size(); i++) {
                left += widths.get(i) + dp(GAP_DP);
            }
            if (whole < widths.size() && index > whole) {
                left += (widths.get(whole) + dp(GAP_DP)) * (index - whole);
            }
            return left;
        }

        private float widthAt(float index) {
            if (widths.isEmpty()) {
                return 0;
            }
            int whole = Math.min(widths.size() - 1, (int) index);
            int next = Math.min(widths.size() - 1, whole + 1);
            float part = index - whole;
            return widths.get(whole) * (1 - part) + widths.get(next) * part;
        }

        @Override
        protected void onDraw(@NonNull Canvas canvas) {
            if (titles.isEmpty()) {
                return;
            }
            final float top = (getMeasuredHeight() - dp(PILL_DP)) / 2f;
            final float scale = pressedIndex >= 0 && Math.abs(pressedIndex - highlight) < 0.5f
                    ? pressScale : 1f;

            final int restColor = Theme.getColor(Theme.key_windowBackgroundWhiteGrayText2);
            final Runnable drawSelected = () -> {
                pill.set(leftOf(highlight), top, leftOf(highlight) + widthAt(highlight), top + dp(PILL_DP));
                pillPaint.setColor(Theme.getColor(Theme.key_featuredStickers_addButton));
                canvas.save();
                canvas.scale(scale, scale, pill.centerX(), pill.centerY());
                canvas.drawRoundRect(pill, dp(PILL_DP / 2f), dp(PILL_DP / 2f), pillPaint);
                canvas.restore();
            };
            restPaint.setColor(Theme.multAlpha(restColor, 0.12f));
            for (int i = 0; i < titles.size(); i++) {
                if (Math.abs(i - highlight) < 0.999f) {
                    continue;
                }
                pill.set(leftOf(i), top, leftOf(i) + widths.get(i), top + dp(PILL_DP));
                canvas.drawRoundRect(pill, dp(PILL_DP / 2f), dp(PILL_DP / 2f), restPaint);
            }
            drawSelected.run();

            for (int i = 0; i < titles.size(); i++) {

                final float active = Math.max(0, 1 - Math.abs(i - highlight));
                textPaint.setColor(ColorUtils.blendARGB(restColor, 0xFFFFFFFF, active));
                final float left = leftOf(i);
                final float centerX = left + widths.get(i) / 2f;
                final float centerY = getMeasuredHeight() / 2f
                        - (textPaint.descent() + textPaint.ascent()) / 2f;
                final float itemScale = i == pressedIndex ? pressScale : 1f;
                canvas.save();
                canvas.scale(itemScale, itemScale, centerX, getMeasuredHeight() / 2f);
                canvas.drawText(titles.get(i), centerX, centerY, textPaint);
                canvas.restore();
            }
        }

        @Override
        public boolean onTouchEvent(@NonNull MotionEvent event) {
            if (titles.isEmpty()) {
                return false;
            }
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    pressedIndex = indexAt(event.getX());
                    if (pressedIndex < 0) {
                        return false;
                    }
                    animatePress(true);
                    return true;
                case MotionEvent.ACTION_UP:
                    if (pressedIndex >= 0 && pressedIndex == indexAt(event.getX())) {
                        pick(pressedIndex);
                    }
                    pressedIndex = -1;
                    animatePress(false);
                    return true;
                case MotionEvent.ACTION_CANCEL:
                    pressedIndex = -1;
                    animatePress(false);
                    return true;
            }
            return pressedIndex >= 0;
        }

        private int indexAt(float x) {
            float left = dp(EDGE_DP);
            for (int i = 0; i < widths.size(); i++) {
                if (x >= left && x <= left + widths.get(i)) {
                    return i;
                }
                left += widths.get(i) + dp(GAP_DP);
            }
            return -1;
        }

        private void pick(int index) {
            if (index == selected) {
                return;
            }
            selected = index;
            animateHighlight();
            scrollToSelected();
            if (delegate != null) {
                delegate.onPicked(index);
            }
        }

        private void scrollToSelected() {
            final float left = leftOf(selected);
            final float right = left + widths.get(selected);
            final int visibleLeft = getScrollX();
            final int visibleRight = visibleLeft + FGStoreFilters.this.getMeasuredWidth();
            if (left < visibleLeft + dp(EDGE_DP)) {
                FGStoreFilters.this.smoothScrollTo((int) left - dp(EDGE_DP), 0);
            } else if (right > visibleRight - dp(EDGE_DP)) {
                FGStoreFilters.this.smoothScrollTo(
                        (int) right - FGStoreFilters.this.getMeasuredWidth() + dp(EDGE_DP), 0);
            }
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
            pressAnimator = ValueAnimator.ofFloat(pressScale, pressed ? 0.94f : 1f);
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
    }
}

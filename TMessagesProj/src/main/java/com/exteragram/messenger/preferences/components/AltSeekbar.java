/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.preferences.components;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.content.Context;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.HapticFeedbackConstants;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.SeekBarView;

public class AltSeekbar extends FrameLayout {

    public interface OnDrag {
        void run(float value);
    }

    private static final int MAX_STEPS = 100;

    protected float currentValue;
    protected final TextView leftTextView;
    protected final TextView rightTextView;
    public SeekBarView seekBarView;

    private final TextView titleView;
    private final TextView valueView;
    private final OnDrag onDrag;
    private final int min;
    private final int max;
    private final String title;
    private boolean atEdge;

    public AltSeekbar(Context context, OnDrag onDrag, int min, int max, String title, String left, String right) {
        super(context);
        this.onDrag = onDrag;
        this.min = Math.min(min, max);
        this.max = Math.max(min, max);
        this.title = title;
        this.currentValue = this.min;

        titleView = new TextView(context);
        titleView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 16);
        titleView.setLines(1);
        addView(titleView, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT, LayoutHelper.WRAP_CONTENT,
                Gravity.LEFT | Gravity.TOP, 21, 14, 80, 0));

        valueView = new TextView(context);
        valueView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 16);
        valueView.setTypeface(AndroidUtilities.bold());
        valueView.setLines(1);
        addView(valueView, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT, LayoutHelper.WRAP_CONTENT,
                Gravity.RIGHT | Gravity.TOP, 0, 14, 21, 0));

        seekBarView = new SeekBarView(context);
        seekBarView.setReportChanges(true);
        final int range = this.max - this.min;
        if (range > 0 && range <= MAX_STEPS) {
            seekBarView.setSeparatorsCount(range + 1);
        }
        seekBarView.setDelegate(new SeekBarView.SeekBarViewDelegate() {
            @Override
            public void onSeekBarDrag(boolean stop, float progress) {
                applyFromSeekBar(progress);
            }

            @Override
            public void onSeekBarPressed(boolean pressed) {
            }

            @Override
            public CharSequence getContentDescription() {
                return getTextForHeader();
            }

            @Override
            public int getStepsCount() {
                return range > 0 && range <= MAX_STEPS ? range : 0;
            }
        });
        addView(seekBarView, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, 38,
                Gravity.LEFT | Gravity.TOP, 6, 40, 6, 0));

        leftTextView = new TextView(context);
        leftTextView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 13);
        leftTextView.setText(left);
        addView(leftTextView, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT, LayoutHelper.WRAP_CONTENT,
                Gravity.LEFT | Gravity.TOP, 21, 80, 0, 12));

        rightTextView = new TextView(context);
        rightTextView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 13);
        rightTextView.setText(right);
        addView(rightTextView, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT, LayoutHelper.WRAP_CONTENT,
                Gravity.RIGHT | Gravity.TOP, 0, 80, 21, 12));

        updateStyle();
        updateHeader(currentValue);
    }

    private void applyFromSeekBar(float progress) {
        float value = min + (max - min) * progress;
        if (max - min <= MAX_STEPS) {
            value = Math.round(value);
        }
        if (value == currentValue) {
            return;
        }
        currentValue = value;
        updateHeader(value);
        final boolean edge = value <= min || value >= max;
        if (edge && !atEdge && useExactEndpointHaptic()) {
            performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP);
        }
        atEdge = edge;
        if (onDrag != null) {
            onDrag.run(value);
        }
    }

    public void setProgress(float value) {
        currentValue = Math.max(min, Math.min(max, value));
        final float range = max - min;
        seekBarView.setProgress(range <= 0 ? 0f : (currentValue - min) / range);
        updateHeader(currentValue);
    }

    public void updateHeader(float value) {
        titleView.setText(title);
        valueView.setText(getTextForHeader());
    }

    public CharSequence getTextForHeader() {
        if (currentValue == Math.rint(currentValue)) {
            return String.valueOf((int) currentValue);
        }
        return String.format(java.util.Locale.US, "%.1f", currentValue);
    }

    public void updateStyle() {
        titleView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlackText));
        valueView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteValueText));
        leftTextView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteGrayText));
        rightTextView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteGrayText));
        seekBarView.setInnerColor(Theme.getColor(Theme.key_player_progressBackground));
        seekBarView.setOuterColor(Theme.getColor(Theme.key_player_progress));
        seekBarView.invalidate();
    }

    public boolean useExactEndpointHaptic() {
        return true;
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(dp(108), MeasureSpec.EXACTLY));
    }
}

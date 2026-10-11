/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences.cells;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.text.TextPaint;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.SeekBarView;

public class StickerSliderCell extends FrameLayout {
    private final SeekBarView sizeBar;
    private final TextPaint textPaint;
    private TGSLContract contract;
    private int startRadius;
    private int endRadius;

    private Theme.ResourcesProvider resourcesProvider;

    public StickerSliderCell(Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;

        setWillNotDraw(false);

        textPaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
        textPaint.setTextSize(AndroidUtilities.dp(16));

        sizeBar = new SeekBarView(context, resourcesProvider);
        sizeBar.setReportChanges(true);
        sizeBar.setDelegate(new SeekBarView.SeekBarViewDelegate() {
            @Override
            public void onSeekBarDrag(boolean stop, float progress) {
                contract.setValue(Math.round(startRadius + (endRadius - startRadius) * progress));
                requestLayout();
            }

            @Override
            public void onSeekBarPressed(boolean pressed) {

            }
        });
        addView(sizeBar, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, 38, Gravity.START | Gravity.TOP, 5, 5, 39, 0));
    }

    private static final int SLIDER_ROW_DP = 48;

    @Override
    public void addView(View child, int index, ViewGroup.LayoutParams params) {
        if (child != sizeBar && sizeBar != null) {
            final FrameLayout.LayoutParams below = params instanceof FrameLayout.LayoutParams
                    ? (FrameLayout.LayoutParams) params
                    : new FrameLayout.LayoutParams(params.width, params.height);
            below.gravity = Gravity.START | Gravity.TOP;
            below.topMargin = AndroidUtilities.dp(SLIDER_ROW_DP);
            super.addView(child, index, below);
            return;
        }
        super.addView(child, index, params);
    }

    public StickerSliderCell setContract(TGSLContract contract) {
        this.contract = contract;
        this.startRadius = contract.getMin();
        this.endRadius = contract.getMax();
        return this;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        textPaint.setColor(Theme.getColor(Theme.key_windowBackgroundWhiteValueText, resourcesProvider));
        canvas.drawText("" + contract.getPreferenceValue(), getMeasuredWidth() - AndroidUtilities.dp(39), AndroidUtilities.dp(28), textPaint);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(MeasureSpec.makeMeasureSpec(MeasureSpec.getSize(widthMeasureSpec), MeasureSpec.EXACTLY), heightMeasureSpec);
        sizeBar.setProgress((contract.getPreferenceValue() - startRadius) / (float) (endRadius - startRadius));
    }

    @Override
    public void invalidate() {
        super.invalidate();
        sizeBar.invalidate();
    }

    public interface TGSLContract {
        void setValue(int value);

        int getPreferenceValue();

        int getMin();

        int getMax();
    }

}
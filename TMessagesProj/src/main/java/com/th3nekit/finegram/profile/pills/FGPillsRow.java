/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.profile.pills;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.ui.ActionBar.Theme;

import java.util.ArrayList;
import java.util.List;

public class FGPillsRow extends ViewGroup {

    private static final int PILL_HEIGHT = 72;

    private static final int GAP = 8;
    private static final int SIDE = 12;

    private final Theme.ResourcesProvider resourcesProvider;

    private final List<FGPills.Pill> current = new ArrayList<>();

    public FGPillsRow(Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;
        setClipChildren(false);
        rebuild();
    }

    public void rebuild() {
        List<FGPills.Pill> pills = FGPills.visible();
        if (pills.equals(current)) {
            return;
        }
        removeAllViews();
        current.clear();
        current.addAll(pills);

        for (FGPills.Pill pill : pills) {
            FGPill view = null;
            try {
                view = pill.creator == null ? null : pill.creator.create(getContext(), resourcesProvider);
            } catch (Throwable e) {

                org.telegram.messenger.FileLog.e("карточка профиля " + pill.id + " не собралась", e);
            }
            if (view == null) {
                continue;
            }
            view.setPillId(pill.id);
            addView(view, new FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.WRAP_CONTENT));
        }
        requestLayout();
    }

    public boolean isEmpty() {
        return getChildCount() == 0;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = MeasureSpec.getSize(widthMeasureSpec);
        int inner = width - AndroidUtilities.dp(SIDE) * 2;
        int half = (inner - AndroidUtilities.dp(GAP)) / 2;
        int pillHeight = AndroidUtilities.dp(PILL_HEIGHT);

        int column = 0;
        int rows = 0;
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            int span = spanOf(i);
            if (span > 1 && column == 1) {

                column = 0;
                rows++;
            }
            int childWidth = span > 1 ? inner : half;
            child.measure(
                    MeasureSpec.makeMeasureSpec(childWidth, MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec(pillHeight, MeasureSpec.EXACTLY));
            column += span;
            if (column >= 2) {
                column = 0;
                rows++;
            }
        }
        if (column > 0) {
            rows++;
        }

        int height = rows == 0 ? 0
                : rows * pillHeight + (rows - 1) * AndroidUtilities.dp(GAP) + AndroidUtilities.dp(GAP) * 2;
        setMeasuredDimension(width, height);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        int inner = getMeasuredWidth() - AndroidUtilities.dp(SIDE) * 2;
        int half = (inner - AndroidUtilities.dp(GAP)) / 2;
        int pillHeight = AndroidUtilities.dp(PILL_HEIGHT);

        int x = AndroidUtilities.dp(SIDE);
        int y = AndroidUtilities.dp(GAP);
        int column = 0;

        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            int span = spanOf(i);
            if (span > 1 && column == 1) {
                column = 0;
                x = AndroidUtilities.dp(SIDE);
                y += pillHeight + AndroidUtilities.dp(GAP);
            }
            int childWidth = span > 1 ? inner : half;
            child.layout(x, y, x + childWidth, y + pillHeight);

            column += span;
            if (column >= 2) {
                column = 0;
                x = AndroidUtilities.dp(SIDE);
                y += pillHeight + AndroidUtilities.dp(GAP);
            } else {
                x += childWidth + AndroidUtilities.dp(GAP);
            }
        }
    }

    private int spanOf(int index) {
        return index < current.size() ? current.get(index).span : 1;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            if (child instanceof FGPill) {
                ((FGPill) child).onShown();
            }
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            if (child instanceof FGPill) {
                ((FGPill) child).onHidden();
            }
        }
    }
}

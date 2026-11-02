package com.th3nekit.finegram.chats.ui;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Path;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.ui.ActionBar.ActionBarMenuSubItem;
import org.telegram.ui.ActionBar.ActionBarPopupWindow;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.blur3.drawable.BlurredBackgroundDrawable;

import java.util.ArrayList;

public final class MessageMenuAppearance {
    private MessageMenuAppearance() {
    }

    public static void apply(ActionBarPopupWindow.ActionBarPopupWindowLayout popup,
                             boolean outgoing, Theme.ResourcesProvider provider) {
        if (popup == null) {
            return;
        }
        LinearLayout list = popup.getLinearLayout();
        MenuWidthLayout widthLayout;
        if (list.getParent() instanceof MenuWidthLayout) {
            widthLayout = (MenuWidthLayout) list.getParent();
        } else if (list.getParent() instanceof ViewGroup) {
            ViewGroup parent = (ViewGroup) list.getParent();
            int index = parent.indexOfChild(list);
            ViewGroup.LayoutParams params = list.getLayoutParams();
            params.width = ViewGroup.LayoutParams.WRAP_CONTENT;
            parent.removeView(list);
            widthLayout = new MenuWidthLayout(list.getContext(), list, popup);
            widthLayout.addView(list, new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            parent.addView(widthLayout, index, params);
        } else {
            return;
        }
        widthLayout.prepare(provider);
        list.requestLayout();
    }

    private static boolean isSimpleRow(ActionBarMenuSubItem row) {
        return row.getClass() == ActionBarMenuSubItem.class && row.getCheckView() == null
                && row.subtextView == null && row.getRightIcon() == null
                && row.openSwipeBackLayout == null && row.getChildCount() == 2
                && row.textView.getLayoutParams() instanceof FrameLayout.LayoutParams
                && row.imageView.getLayoutParams() instanceof FrameLayout.LayoutParams;
    }

    private static void styleRow(ActionBarMenuSubItem row) {
        boolean rtl = LocaleController.isRTL;
        boolean hasIcon = row.imageView.getVisibility() == View.VISIBLE;
        row.setMinimumWidth(0);
        row.textView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 17);
        row.textView.setTypeface(Typeface.DEFAULT, Typeface.NORMAL);
        row.textView.setIncludeFontPadding(false);
        row.textView.setSingleLine(false);
        row.textView.setMaxLines(Integer.MAX_VALUE);
        row.textView.setEllipsize(null);
        row.textView.setGravity((rtl ? Gravity.RIGHT : Gravity.LEFT) | Gravity.CENTER_VERTICAL);
        row.textView.setPadding(0, 0, 0, 0);

        FrameLayout.LayoutParams textParams = (FrameLayout.LayoutParams) row.textView.getLayoutParams();
        textParams.width = ViewGroup.LayoutParams.MATCH_PARENT;
        textParams.height = ViewGroup.LayoutParams.WRAP_CONTENT;
        textParams.gravity = (rtl ? Gravity.RIGHT : Gravity.LEFT) | Gravity.CENTER_VERTICAL;
        textParams.leftMargin = AndroidUtilities.dp(!rtl && hasIcon ? 60 : 18);
        textParams.rightMargin = AndroidUtilities.dp(rtl && hasIcon ? 60 : 18);
        row.textView.setLayoutParams(textParams);

        FrameLayout.LayoutParams iconParams = (FrameLayout.LayoutParams) row.imageView.getLayoutParams();
        iconParams.width = AndroidUtilities.dp(32);
        iconParams.height = AndroidUtilities.dp(28);
        iconParams.gravity = (rtl ? Gravity.RIGHT : Gravity.LEFT) | Gravity.CENTER_VERTICAL;
        iconParams.leftMargin = rtl ? 0 : AndroidUtilities.dp(20);
        iconParams.rightMargin = rtl ? AndroidUtilities.dp(20) : 0;
        row.imageView.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
        row.imageView.setLayoutParams(iconParams);
        row.updateSelectorBackground(true, true, 20);
        if (!(row.getBackground() instanceof RowSelector)) {
            row.setBackground(new RowSelector(row.getBackground()));
        }
        row.setPadding(0, 0, 0, 0);

        ViewGroup.LayoutParams rowParams = row.getLayoutParams();
        if (rowParams != null) {
            rowParams.width = ViewGroup.LayoutParams.MATCH_PARENT;
            row.setLayoutParams(rowParams);
        }
        row.requestLayout();
    }

    private static final class RowSelector extends InsetDrawable {
        private RowSelector(Drawable drawable) {
            super(drawable, AndroidUtilities.dp(10), 0, AndroidUtilities.dp(10), 0);
        }

        @Override
        public boolean getPadding(Rect padding) {
            padding.set(0, 0, 0, 0);
            return false;
        }
    }

    private static final class MenuWidthLayout extends FrameLayout {
        private final LinearLayout list;
        private final ActionBarPopupWindow.ActionBarPopupWindowLayout popup;
        private final ArrayList<ActionBarMenuSubItem> styledRows = new ArrayList<>();
        private final Matrix contentToPopup = new Matrix();
        private final Matrix popupToContent = new Matrix();
        private final Path contentClip = new Path();
        private Drawable lastBackground;
        private float lastRadius = -1;

        private MenuWidthLayout(Context context, LinearLayout list,
                                ActionBarPopupWindow.ActionBarPopupWindowLayout popup) {
            super(context);
            this.list = list;
            this.popup = popup;
            popup.addOnLayoutChangeListener((view, left, top, right, bottom,
                                             oldLeft, oldTop, oldRight, oldBottom) -> {
                Rect padding = popup.getPadding();
                updateRadius(Math.max(0, bottom - top - padding.top - padding.bottom));
            });
        }

        private void prepare(Theme.ResourcesProvider provider) {
            boolean hasActionRow = false;
            for (int i = 0; i < list.getChildCount(); i++) {
                View child = list.getChildAt(i);
                if (child instanceof ActionBarMenuSubItem) {
                    hasActionRow |= child.getVisibility() == View.VISIBLE;
                    ActionBarMenuSubItem row = (ActionBarMenuSubItem) child;
                    if (isSimpleRow(row) && (row.textView.getMaxLines() == 1
                            || styledRows.contains(row))) {
                        if (!styledRows.contains(row)) {
                            styledRows.add(row);
                        }
                        styleRow(row);
                    }
                } else if (child.getClass() == ActionBarPopupWindow.GapView.class) {
                    ActionBarPopupWindow.GapView gap = (ActionBarPopupWindow.GapView) child;
                    if (gap.getChildCount() == 0) {
                        gap.setBackgroundColor(0);
                        gap.setWillNotDraw(true);
                        gap.setMinimumHeight(0);
                        ViewGroup.LayoutParams params = gap.getLayoutParams();
                        int height = !hasActionRow && params.height > 0
                                ? params.height : AndroidUtilities.dp(20);
                        params.height = height;
                        gap.setLayoutParams(params);
                        View line = new View(getContext());
                        line.setBackgroundColor(Theme.getColor(Theme.key_divider, provider));
                        FrameLayout.LayoutParams lineParams = new FrameLayout.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT, AndroidUtilities.dp(1),
                                Gravity.LEFT | Gravity.TOP);
                        lineParams.leftMargin = lineParams.rightMargin = AndroidUtilities.dp(18);
                        lineParams.topMargin = (height - AndroidUtilities.dp(1)) / 2;
                        gap.addView(line, lineParams);
                    }
                }
            }
        }

        @Override
        protected void dispatchDraw(Canvas canvas) {
            Drawable background = popup.getBackgroundDrawable();
            if (!(background instanceof BlurredBackgroundDrawable)) {
                super.dispatchDraw(canvas);
                return;
            }
            contentToPopup.reset();
            View child = this;
            while (child != popup) {
                if (!(child.getParent() instanceof View)) {
                    super.dispatchDraw(canvas);
                    return;
                }
                View parent = (View) child.getParent();
                contentToPopup.postConcat(child.getMatrix());
                contentToPopup.postTranslate(child.getLeft() - parent.getScrollX(),
                        child.getTop() - parent.getScrollY());
                child = parent;
            }
            if (!contentToPopup.invert(popupToContent)) {
                return;
            }
            contentClip.set(((BlurredBackgroundDrawable) background).getPath());
            contentClip.transform(popupToContent);
            int save = canvas.save();
            canvas.clipPath(contentClip);
            super.dispatchDraw(canvas);
            canvas.restoreToCount(save);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int maxWidth = AndroidUtilities.dp(240);
            if (MeasureSpec.getMode(widthMeasureSpec) != MeasureSpec.UNSPECIFIED) {
                maxWidth = Math.min(maxWidth, MeasureSpec.getSize(widthMeasureSpec));
            }
            int width = Math.min(AndroidUtilities.dp(220), maxWidth);
            View first = null;
            View last = null;
            for (int i = 0; i < list.getChildCount(); i++) {
                View child = list.getChildAt(i);
                if (child.getVisibility() != View.VISIBLE) {
                    continue;
                }
                if (first == null) {
                    first = child;
                }
                last = child;
                if (child instanceof ActionBarMenuSubItem && styledRows.contains(child)
                        && isSimpleRow((ActionBarMenuSubItem) child)) {
                    ActionBarMenuSubItem row = (ActionBarMenuSubItem) child;
                    FrameLayout.LayoutParams params = (FrameLayout.LayoutParams) row.textView.getLayoutParams();
                    int insets = params.leftMargin + params.rightMargin;
                    row.textView.measure(MeasureSpec.makeMeasureSpec(Math.max(0, maxWidth - insets),
                            MeasureSpec.AT_MOST), MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
                    width = Math.max(width, Math.min(maxWidth, row.textView.getMeasuredWidth() + insets));
                } else {
                    child.measure(MeasureSpec.makeMeasureSpec(maxWidth, MeasureSpec.AT_MOST),
                            MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
                    width = Math.max(width, Math.min(maxWidth, child.getMeasuredWidth()));
                }
            }
            list.setPadding(0, first instanceof ActionBarMenuSubItem ? AndroidUtilities.dp(10) : 0,
                    0, last instanceof ActionBarMenuSubItem ? AndroidUtilities.dp(10) : 0);
            for (ActionBarMenuSubItem row : styledRows) {
                if (row.getParent() != list || row.getVisibility() != View.VISIBLE || !isSimpleRow(row)) {
                    continue;
                }
                FrameLayout.LayoutParams params = (FrameLayout.LayoutParams) row.textView.getLayoutParams();
                row.textView.measure(MeasureSpec.makeMeasureSpec(Math.max(0,
                                width - params.leftMargin - params.rightMargin), MeasureSpec.EXACTLY),
                        MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
                row.setItemHeight((int) Math.ceil(row.textView.getMeasuredHeight() / AndroidUtilities.density) + 22);
            }
            super.onMeasure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY), heightMeasureSpec);
            int height = list.getMeasuredHeight();
            if (popup.getMeasuredHeight() > 0) {
                Rect padding = popup.getPadding();
                height = Math.min(height, Math.max(0,
                        popup.getMeasuredHeight() - padding.top - padding.bottom));
            }
            updateRadius(height);
        }

        private void updateRadius(int height) {
            Drawable background = popup.getBackgroundDrawable();
            if (!(background instanceof BlurredBackgroundDrawable)) {
                return;
            }
            float radius = Math.min(AndroidUtilities.dp(30), height / 2f);
            if (background != lastBackground || radius != lastRadius) {
                ((BlurredBackgroundDrawable) background).setRadius(radius);
                lastBackground = background;
                lastRadius = radius;
            }
            for (int i = 0; i < list.getChildCount(); i++) {
                View child = list.getChildAt(i);
                if (child instanceof MessageMenuCompactView) {
                    ViewGroup.MarginLayoutParams params = (ViewGroup.MarginLayoutParams) child.getLayoutParams();
                    int inset = Math.min(params.leftMargin, Math.min(params.rightMargin, params.bottomMargin));
                    ((MessageMenuCompactView) child).setMenuCornerRadius(Math.max(0, Math.round(radius) - inset));
                }
            }
        }
    }
}

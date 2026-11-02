package org.telegram.ui.Components;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.content.Context;
import android.util.Log;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.ui.ActionBar.ActionBarPopupWindow;

public class ChatScrimPopupContainerLayout extends LinearLayout {

    private ReactionsContainerLayout reactionsLayout;
    private ActionBarPopupWindow.ActionBarPopupWindowLayout popupWindowLayout;
    private View bottomView;
    private int maxHeight;
    private boolean dynamicHeight;
    private float popupLayoutLeftOffset;
    private boolean messageAnchored;
    private float progressToSwipeBack;
    private float bottomViewYOffset;
    private float expandSize;
    private float bottomViewReactionsOffset;

    public ChatScrimPopupContainerLayout(Context context) {
        super(context);
        setOrientation(LinearLayout.VERTICAL);
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        super.onLayout(changed, l, t, r, b);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int availableWidth = MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.UNSPECIFIED
                ? Integer.MAX_VALUE : MeasureSpec.getSize(widthMeasureSpec);
        if (maxHeight > 0) {
            int availableHeight = MeasureSpec.getMode(heightMeasureSpec) == MeasureSpec.UNSPECIFIED
                    ? maxHeight : Math.min(maxHeight, MeasureSpec.getSize(heightMeasureSpec));
            heightMeasureSpec = MeasureSpec.makeMeasureSpec(availableHeight, MeasureSpec.AT_MOST);
        }
        if (reactionsLayout != null && popupWindowLayout != null) {
            reactionsLayout.getLayoutParams().width = LayoutHelper.WRAP_CONTENT;
            ((LayoutParams) reactionsLayout.getLayoutParams()).rightMargin = 0;
            popupLayoutLeftOffset = 0;
            super.onMeasure(widthMeasureSpec, dynamicHeight
                    ? MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED) : heightMeasureSpec);

            int maxWidth = reactionsLayout.getMeasuredWidth();
            if (popupWindowLayout.getSwipeBack() != null && popupWindowLayout.getSwipeBack().getMeasuredWidth() > maxWidth) {
                maxWidth = popupWindowLayout.getSwipeBack().getMeasuredWidth();
            }
            if (popupWindowLayout.getMeasuredWidth() > maxWidth) {
                maxWidth = popupWindowLayout.getMeasuredWidth();
            }
            maxWidth = Math.min(maxWidth, availableWidth);
            if (reactionsLayout.showCustomEmojiReaction()) {
                widthMeasureSpec = MeasureSpec.makeMeasureSpec(maxWidth, MeasureSpec.EXACTLY);
            }
            reactionsLayout.measureHint();

            int reactionsLayoutTotalWidth = reactionsLayout.getTotalWidth();
            View menuContainer = popupWindowLayout.getSwipeBack() != null ? popupWindowLayout.getSwipeBack().getChildAt(0) : popupWindowLayout.getChildAt(0);
            int maxReactionsLayoutWidth = menuContainer.getMeasuredWidth() + dp(16) + dp(16) + dp(36);
            int hintTextWidth = reactionsLayout.getHintTextWidth();
            if (hintTextWidth > maxReactionsLayoutWidth) {
                maxReactionsLayoutWidth = hintTextWidth;
            } else if (maxReactionsLayoutWidth > maxWidth) {
                maxReactionsLayoutWidth = maxWidth;
            }
            maxReactionsLayoutWidth = Math.min(maxReactionsLayoutWidth, maxWidth);
            reactionsLayout.bigCircleOffset = dp(36);
            if (reactionsLayout.showCustomEmojiReaction()) {
                reactionsLayout.getLayoutParams().width = Math.min(reactionsLayoutTotalWidth, maxWidth);
                reactionsLayout.bigCircleOffset = Math.max(reactionsLayoutTotalWidth - menuContainer.getMeasuredWidth() - dp(36), dp(36));
            } else if (reactionsLayoutTotalWidth > maxReactionsLayoutWidth) {
                int maxFullCount = Math.max(1, ((maxReactionsLayoutWidth - dp(16)) / dp(36)) + 1);
                int newWidth = maxFullCount * dp(36) + dp(8);
                if (hintTextWidth + dp(24) > newWidth) {
                    newWidth = hintTextWidth + dp(24);
                }
                if (newWidth > reactionsLayoutTotalWidth || maxFullCount == reactionsLayout.getItemsCount()) {
                    newWidth = reactionsLayoutTotalWidth;
                }
                reactionsLayout.getLayoutParams().width = Math.min(newWidth, maxWidth);
            } else {
                reactionsLayout.getLayoutParams().width = LayoutHelper.WRAP_CONTENT;
            }
            int widthDiff = 0;
            if (reactionsLayout.getMeasuredWidth() != maxWidth || !reactionsLayout.showCustomEmojiReaction()) {
                if (popupWindowLayout.getSwipeBack() != null) {
                    widthDiff = popupWindowLayout.getSwipeBack().getMeasuredWidth() - popupWindowLayout.getSwipeBack().getChildAt(0).getMeasuredWidth();
                }
                if (reactionsLayout.getLayoutParams().width != LayoutHelper.WRAP_CONTENT && reactionsLayout.getLayoutParams().width + widthDiff > maxWidth) {
                    widthDiff = maxWidth - reactionsLayout.getLayoutParams().width + dp(8);
                }
                if (widthDiff < 0) {
                    widthDiff = 0;
                }
                ((LayoutParams) reactionsLayout.getLayoutParams()).rightMargin = widthDiff;
                popupLayoutLeftOffset = 0;
                updatePopupTranslation();
            } else {
                popupLayoutLeftOffset = (maxWidth - menuContainer.getMeasuredWidth()) * 0.25f;
                reactionsLayout.bigCircleOffset -= popupLayoutLeftOffset;
                if (reactionsLayout.bigCircleOffset < dp(36)) {
                    popupLayoutLeftOffset = 0;
                    reactionsLayout.bigCircleOffset = dp(36);
                }
                updatePopupTranslation();
            }
            if (bottomView != null) {
                if (reactionsLayout.showCustomEmojiReaction()) {
                    bottomView.getLayoutParams().width = Math.min(menuContainer.getMeasuredWidth() + dp(16), maxWidth);
                    updatePopupTranslation();
                } else {
                    bottomView.getLayoutParams().width = LayoutHelper.MATCH_PARENT;
                }
                if (popupWindowLayout.getSwipeBack() != null) {
                    ((LayoutParams) bottomView.getLayoutParams()).rightMargin = widthDiff + dp(36);
                } else {
                    ((LayoutParams) bottomView.getLayoutParams()).rightMargin = dp(36);
                }
            }
            measureMenu(widthMeasureSpec, heightMeasureSpec);
        } else {
            if (dynamicHeight) super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
            measureMenu(widthMeasureSpec, heightMeasureSpec);
        }
        if (!dynamicHeight) {
            maxHeight = getMeasuredHeight();
        }
    }

    private void measureMenu(int widthSpec, int heightSpec) {
        if (!dynamicHeight || popupWindowLayout == null || MeasureSpec.getMode(heightSpec) == MeasureSpec.UNSPECIFIED) {
            super.onMeasure(widthSpec, heightSpec);
            return;
        }
        int remaining = MeasureSpec.getSize(heightSpec) - getPaddingTop() - getPaddingBottom();
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            if (child.getVisibility() == GONE) continue;
            LayoutParams params = (LayoutParams) child.getLayoutParams();
            remaining -= params.topMargin + params.bottomMargin;
            if (child != popupWindowLayout) {
                child.measure(getChildMeasureSpec(MeasureSpec.makeMeasureSpec(getMeasuredWidth(), MeasureSpec.EXACTLY),
                                getPaddingLeft() + getPaddingRight() + params.leftMargin + params.rightMargin, params.width),
                        getChildMeasureSpec(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                                params.topMargin + params.bottomMargin, params.height));
                remaining -= child.getMeasuredHeight();
            }
        }
        LayoutParams params = (LayoutParams) popupWindowLayout.getLayoutParams();
        int height = params.height;
        params.height = Math.max(1, Math.min(popupWindowLayout.getMeasuredHeight(), remaining));
        try {
            super.onMeasure(widthSpec, heightSpec);
        } finally {
            params.height = height;
        }
    }

    public int getMinimumMenuHeight() {
        int height = getPaddingTop() + getPaddingBottom();
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            if (child.getVisibility() == GONE) continue;
            LayoutParams params = (LayoutParams) child.getLayoutParams();
            height += params.topMargin + params.bottomMargin;
            height += child == popupWindowLayout ? Math.min(child.getMeasuredHeight(), dp(64)) : child.getMeasuredHeight();
        }
        return height;
    }

    private void updatePopupTranslation() {
        float x = messageAnchored ? 0 : (1f - progressToSwipeBack) * popupLayoutLeftOffset;
        popupWindowLayout.setTranslationX(x);
        if (bottomView != null) {
            bottomView.setTranslationX(x);
        }
    }

    public void applyViewBottom(FrameLayout bottomView) {
        this.bottomView = bottomView;
    }

    public void setReactionsLayout(ReactionsContainerLayout reactionsLayout) {
        if (this.reactionsLayout == reactionsLayout) {
            return;
        }
        if (this.reactionsLayout != null) {
            this.reactionsLayout.setChatScrimView(null);
        }
        this.reactionsLayout = reactionsLayout;
        if (popupWindowLayout != null && expandSize != 0f) {
            setExpandSize(0f);
        }
        if (reactionsLayout != null) {
            reactionsLayout.setChatScrimView(this);
        }
    }

    public void setPopupWindowLayout(ActionBarPopupWindow.ActionBarPopupWindowLayout popupWindowLayout) {
        this.popupWindowLayout = popupWindowLayout;
        popupWindowLayout.setOnSizeChangedListener(() -> {
            if (bottomView != null) {
                bottomViewYOffset = popupWindowLayout.getVisibleHeight() - popupWindowLayout.getMeasuredHeight();
                updateBottomViewPosition();
            }
        });
        if (popupWindowLayout.getSwipeBack() != null) {
            popupWindowLayout.getSwipeBack().addOnSwipeBackProgressListener((layout, toProgress, progress) -> {
                if (bottomView != null) {
                    bottomView.setAlpha(1f - progress);
                }
                progressToSwipeBack = progress;
                updatePopupTranslation();
            });
        }
    }

    private void updateBottomViewPosition() {
        if (bottomView != null) {
            bottomView.setTranslationY(bottomViewYOffset + expandSize + bottomViewReactionsOffset);
        }
    }

    public void setMessageAnchor(boolean outgoing) {
        messageAnchored = true;
        int gravity = outgoing ? android.view.Gravity.RIGHT : android.view.Gravity.LEFT;
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            LayoutParams params = (LayoutParams) child.getLayoutParams();
            params.gravity = gravity;
            if (child == reactionsLayout) {
                params.topMargin = 0;
            } else {
                params.leftMargin = 0;
                params.rightMargin = 0;
            }
            child.setLayoutParams(params);
        }
        updatePopupTranslation();
    }

    public void setMaxHeight(int maxHeight) {
        this.maxHeight = Math.max(0, maxHeight);
    }

    public void setDynamicHeight(boolean dynamicHeight) {
        this.dynamicHeight = dynamicHeight;
        if (dynamicHeight) {
            maxHeight = 0;
        }
    }

    public void setExpandSize(float expandSize) {
        popupWindowLayout.setTranslationY(expandSize);
        this.expandSize = expandSize;
        updateBottomViewPosition();
    }

    public void setPopupAlpha(float alpha) {
        popupWindowLayout.setAlpha(alpha);
        if (bottomView != null) {
            bottomView.setAlpha(alpha);
        }
    }

    public void setReactionsTransitionProgress(float v) {
        popupWindowLayout.setReactionsTransitionProgress(v);
        if (bottomView != null) {
            bottomView.setAlpha(v);
            float scale = 0.5f + v * 0.5f;
            bottomView.setPivotX(bottomView.getMeasuredWidth());
            bottomView.setPivotY(0);
            bottomViewReactionsOffset = -popupWindowLayout.getMeasuredHeight() * (1f - v);
            updateBottomViewPosition();
            bottomView.setScaleX(scale);
            bottomView.setScaleY(scale);
        }
    }
}

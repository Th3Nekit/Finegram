/*
 * This is the source code of Telegram for Android v. 5.x.x.
 * It is licensed under GNU GPL v. 2 or later.
 * You should have received a copy of the license in this archive (see LICENSE).
 *
 * Copyright Nikolai Kudashov, 2013-2018.
 */

package org.telegram.ui.ActionBar;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.BuildVars;
import org.telegram.messenger.FileLog;
import org.telegram.messenger.LocaleController;

public class DrawerLayoutContainer extends FrameLayout {

    private INavigationLayout parentActionBarLayout;
    private ActionBarLayout actionBarLayout;
    private boolean inLayout;

    public DrawerLayoutContainer(Context context) {
        super(context);

        ViewCompat.setOnApplyWindowInsetsListener(this, this::onApplyWindowInsets);
        setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    }

    public void setParentActionBarLayout(INavigationLayout layout) {
        parentActionBarLayout = layout;
    }

    public void setActionBarLayout(ActionBarLayout actionBarLayout) {
        this.actionBarLayout = actionBarLayout;
    }

    public boolean isDrawCurrentPreviewFragmentAbove() {
        return false;
    }

    public interface SwipeOpenDelegate {
        boolean canStartSwipe();

        void onSwipeStart();

        void onSwipe(float dx);

        void onSwipeEnd(float velocityX);
    }

    private SwipeOpenDelegate swipeOpenDelegate;
    private boolean swipeCandidate;
    private boolean swipeActive;
    private float swipeStartX;
    private float swipeStartY;
    private VelocityTracker swipeTracker;
    private int touchSlop;

    public void setSwipeOpenDelegate(SwipeOpenDelegate delegate) {
        swipeOpenDelegate = delegate;
        touchSlop = ViewConfiguration.get(getContext()).getScaledTouchSlop();
    }

    public boolean onTouchEvent(MotionEvent ev) {
        if (!swipeActive || swipeOpenDelegate == null) {
            return false;
        }
        if (swipeTracker != null) {
            swipeTracker.addMovement(ev);
        }
        final int action = ev.getActionMasked();
        if (action == MotionEvent.ACTION_MOVE) {
            swipeOpenDelegate.onSwipe(ev.getX() - swipeStartX);
        } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            float velocity = 0;
            if (swipeTracker != null) {
                swipeTracker.computeCurrentVelocity(1000);
                velocity = swipeTracker.getXVelocity();
            }
            swipeActive = false;
            releaseSwipeTracker();
            swipeOpenDelegate.onSwipeEnd(action == MotionEvent.ACTION_CANCEL ? 0 : velocity);
        }
        return true;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        if (parentActionBarLayout.checkTransitionAnimation()) {
            return true;
        }
        if (swipeOpenDelegate == null) {
            return false;
        }
        switch (ev.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                swipeStartX = ev.getX();
                swipeStartY = ev.getY();
                swipeActive = false;
                swipeCandidate = swipeOpenDelegate.canStartSwipe();
                releaseSwipeTracker();
                if (swipeCandidate) {
                    swipeTracker = VelocityTracker.obtain();
                    swipeTracker.addMovement(ev);
                }
                return false;
            case MotionEvent.ACTION_MOVE: {
                if (!swipeCandidate) {
                    return false;
                }
                if (swipeTracker != null) {
                    swipeTracker.addMovement(ev);
                }
                final float dx = ev.getX() - swipeStartX;
                final float dy = ev.getY() - swipeStartY;
                if (Math.abs(dy) > touchSlop && Math.abs(dy) >= Math.abs(dx)) {
                    swipeCandidate = false;
                    return false;
                }
                if (Math.abs(dx) > touchSlop && Math.abs(dx) > Math.abs(dy) * 1.5f) {
                    swipeCandidate = false;
                    final boolean towardsMenu = LocaleController.isRTL ? dx < 0 : dx > 0;
                    if (!towardsMenu) {
                        return false;
                    }
                    swipeActive = true;

                    swipeStartX = ev.getX();
                    swipeOpenDelegate.onSwipeStart();
                    return true;
                }
                return false;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                swipeCandidate = false;
                releaseSwipeTracker();
                return false;
        }
        return false;
    }

    private void releaseSwipeTracker() {
        if (swipeTracker != null) {
            swipeTracker.recycle();
            swipeTracker = null;
        }
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        inLayout = true;
        final int childCount = getChildCount();
        for (int i = 0; i < childCount; i++) {
            final View child = getChildAt(i);

            if (child.getVisibility() == GONE) {
                continue;
            }

            final LayoutParams lp = (LayoutParams) child.getLayoutParams();
            try {
                child.layout(lp.leftMargin, lp.topMargin + getPaddingTop(), lp.leftMargin + child.getMeasuredWidth(), lp.topMargin + child.getMeasuredHeight() + getPaddingTop());
            } catch (Exception e) {
                FileLog.e(e);
                if (BuildVars.DEBUG_VERSION) {
                    throw e;
                }
            }
        }
        inLayout = false;
    }

    @Override
    public void requestLayout() {
        if (!inLayout) {
            super.requestLayout();
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int widthSize = MeasureSpec.getSize(widthMeasureSpec);
        int heightSize = MeasureSpec.getSize(heightMeasureSpec);

        setMeasuredDimension(widthSize, heightSize);

        final int newDisplayWidth = widthSize
            - systemAndCutoutInsets.left
            - systemAndCutoutInsets.right;

        final int newDisplayHeight = heightSize
            - systemAndCutoutInsets.top
            - systemAndCutoutInsets.bottom;

        AndroidUtilities.displaySize.x = newDisplayWidth;
        AndroidUtilities.displaySize.y = newDisplayHeight;

        final int childCount = getChildCount();
        for (int i = 0; i < childCount; i++) {
            final View child = getChildAt(i);

            if (child.getVisibility() == GONE) {
                continue;
            }

            final LayoutParams lp = (LayoutParams) child.getLayoutParams();

            final int contentWidthSpec = MeasureSpec.makeMeasureSpec(widthSize - lp.leftMargin - lp.rightMargin, MeasureSpec.EXACTLY);
            final int contentHeightSpec;
            if (lp.height > 0) {
                contentHeightSpec = MeasureSpec.makeMeasureSpec(lp.height, MeasureSpec.EXACTLY);
            } else {
                contentHeightSpec = MeasureSpec.makeMeasureSpec(heightSize - lp.topMargin - lp.bottomMargin, MeasureSpec.EXACTLY);
            }
            if (child instanceof ActionBarLayout) {
                ActionBarLayout actionBarLayout = (ActionBarLayout) child;

                if (actionBarLayout.storyViewerAttached()) {
                    child.forceLayout();
                }
            }
            child.measure(contentWidthSpec, contentHeightSpec);
        }
    }

    @Override
    protected void dispatchDraw(@NonNull Canvas canvas) {
        if (actionBarLayout != null && actionBarLayout.getParent() == this) {
            actionBarLayout.parentDraw(this, canvas);
        }

        super.dispatchDraw(canvas);
    }

    @Override
    public boolean hasOverlappingRendering() {
        return false;
    }

    private final Paint internalNavbarPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    public Paint getInternalNavbarPaint() {
        return internalNavbarPaint;
    }

    public void setInternalNavigationBarColor(int color) {
        if (internalNavbarPaint.getColor() != color) {
            internalNavbarPaint.setColor(color);
            invalidate();

            for (int a = 0, N = getChildCount(); a < N; a++) {
                getChildAt(a).invalidate();
            }
        }
    }

    @Override
    public void addView(View child, int index, ViewGroup.LayoutParams params) {
        super.addView(child, index, params);
        if (lastWindowInsetsCompat != null) {
            dispatchApplyWindowInsetsInternal(child, lastWindowInsetsCompat);
        }
    }

    private @Nullable WindowInsetsCompat lastWindowInsetsCompat;
    private @NonNull Insets systemAndCutoutInsets = Insets.NONE;
    private @NonNull Insets systemAndCutoutAndImeInsets = Insets.NONE;

    private void dispatchApplyWindowInsetsInternal(View child, WindowInsetsCompat insets) {
        boolean canApplyInsets = child instanceof ActionBarLayout || child.getTag() == null;
        if (canApplyInsets) {
            ViewCompat.dispatchApplyWindowInsets(child, insets);
        }
    }

    @NonNull
    private WindowInsetsCompat onApplyWindowInsets(@NonNull View ignoredV, @NonNull WindowInsetsCompat insets) {
        lastWindowInsetsCompat = insets;

        final Insets systemInsets = AndroidUtilities.getDefaultWindowInsets(insets, false);
        final Insets systemAndImeInsets = AndroidUtilities.getDefaultWindowInsets(insets, true);

        if (!systemAndCutoutInsets.equals(systemInsets) || !systemAndCutoutAndImeInsets.equals(systemAndImeInsets)) {
            AndroidUtilities.statusBarHeight = systemInsets.top;
            AndroidUtilities.navigationBarHeight = systemInsets.bottom;

            systemAndCutoutInsets = systemInsets;
            systemAndCutoutAndImeInsets = systemAndImeInsets;
            requestLayout();
        }

        for (int a = 0, N = getChildCount(); a < N; a++) {
            final View child = getChildAt(a);
            dispatchApplyWindowInsetsInternal(child, insets);
        }

        invalidate();
        return WindowInsetsCompat.CONSUMED;
    }
}

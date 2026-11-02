package com.th3nekit.finegram.chats.ui;

import android.content.Context;
import android.graphics.Matrix;
import android.os.SystemClock;
import android.view.HapticFeedbackConstants;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.FrameLayout;
import android.widget.ScrollView;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.ui.Components.ReactionsContainerLayout;

public final class MessageMenuPreviewLayout extends FrameLayout {
    public interface Listener {
        boolean contains(float screenX, float screenY);
        void scroll(float delta);
        void dismiss();
    }

    private final View menu;
    private final ReactionsContainerLayout reactions;
    private final Listener listener;
    private final ScrollView scroller;
    private final FrameLayout content;
    private final int[] position = new int[2];
    private final int[] hitPosition = new int[2];
    private final Matrix hitMatrix = new Matrix();
    private final Matrix hitInverse = new Matrix();
    private final float[] hitPoint = new float[2];
    private int menuX, menuY, menuWidth, menuHeight;
    private int reactionsX, reactionsY, reactionsWidth, reactionsHeight;
    private int viewportTop, viewportBottom;
    private boolean initialScrollPending = true;
    private boolean ownsGesture, continuation, continuationMoved, closing;
    private float holdX, holdY;
    private boolean holdAnchored;
    private View highlighted;

    public MessageMenuPreviewLayout(Context context, View menu, ReactionsContainerLayout reactions, Listener listener) {
        super(context);
        this.menu = menu;
        this.reactions = reactions;
        this.listener = listener;
        content = new FrameLayout(context) {
            @Override
            protected void onMeasure(int widthSpec, int heightSpec) {
                menu.measure(MeasureSpec.makeMeasureSpec(menuWidth, MeasureSpec.EXACTLY),
                        MeasureSpec.makeMeasureSpec(menuHeight, MeasureSpec.EXACTLY));
                int height = Math.max(viewportBottom - viewportTop, menuY - viewportTop + menuHeight);
                setMeasuredDimension(MeasureSpec.getSize(widthSpec), Math.max(1, height));
            }

            @Override
            protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
                int x = menuX - position[0], y = menuY - viewportTop;
                menu.layout(x, y, x + menuWidth, y + menuHeight);
            }

            @Override
            public boolean onTouchEvent(MotionEvent event) {
                if (event.getActionMasked() == MotionEvent.ACTION_UP
                        && !listener.contains(event.getRawX(), event.getRawY())) listener.dismiss();
                return true;
            }
        };
        content.addView(menu);
        if (reactions != null) {
            reactions.setPadding(AndroidUtilities.dp(4), AndroidUtilities.dp(4),
                    AndroidUtilities.dp(4), AndroidUtilities.dp(18));
            reactions.playWhileFading = true;
        }
        scroller = new ScrollView(context) {
            @Override
            protected void onScrollChanged(int x, int y, int oldX, int oldY) {
                super.onScrollChanged(x, y, oldX, oldY);
                listener.scroll(-y);
            }
        };
        scroller.setFillViewport(true);
        scroller.setVerticalScrollBarEnabled(false);
        scroller.setOverScrollMode(OVER_SCROLL_NEVER);
        scroller.setClipChildren(false);
        scroller.addView(content, new ScrollView.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        addView(scroller);
        if (reactions != null) addView(reactions, new LayoutParams(LayoutParams.WRAP_CONTENT, AndroidUtilities.dp(70)));
        setFocusableInTouchMode(true);
        setClipChildren(true);
    }

    public void setViewport(int top, int bottom) {
        viewportTop = top;
        viewportBottom = Math.max(top + 1, bottom);
        requestLayout();
    }

    public int getInitialScroll() {
        return Math.max(0, menuY + menuHeight - viewportBottom);
    }

    public void setMenuPosition(int x, int y, int width, int height) {
        menuX = x;
        menuY = y;
        menuWidth = Math.max(1, width);
        menuHeight = Math.max(1, height);
        requestLayout();
    }

    public int measureReactions(int availableWidth) {
        if (reactions == null) return 0;
        reactionsWidth = Math.max(1, Math.min(availableWidth,
                Math.max(reactions.getTotalWidth(), AndroidUtilities.dp(48))));
        reactions.getLayoutParams().width = reactionsWidth;
        reactions.getLayoutParams().height = AndroidUtilities.dp(70) + Math.round(reactions.getTopOffset());
        reactions.measure(MeasureSpec.makeMeasureSpec(reactionsWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(reactions.getLayoutParams().height, MeasureSpec.EXACTLY));
        reactionsWidth = reactions.measurePreviewHint(availableWidth);
        reactions.getLayoutParams().width = reactionsWidth;
        reactionsHeight = reactions.getLayoutParams().height;
        reactions.measure(MeasureSpec.makeMeasureSpec(reactionsWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(reactionsHeight, MeasureSpec.EXACTLY));
        return reactionsHeight;
    }

    public void setReactionsPosition(int x, int y) {
        reactionsX = x;
        reactionsY = y;
        requestLayout();
    }

    public int getReactionsWidth() {
        return reactionsWidth;
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        setMeasuredDimension(MeasureSpec.getSize(widthSpec), MeasureSpec.getSize(heightSpec));
        scroller.measure(MeasureSpec.makeMeasureSpec(getMeasuredWidth(), MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(Math.max(1, viewportBottom - viewportTop), MeasureSpec.EXACTLY));
        if (reactions != null) {
            reactions.measure(MeasureSpec.makeMeasureSpec(reactionsWidth, MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec(reactionsHeight, MeasureSpec.EXACTLY));
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        getLocationOnScreen(position);
        int y = viewportTop - position[1];
        scroller.layout(0, y, getMeasuredWidth(), y + scroller.getMeasuredHeight());
        if (initialScrollPending) {
            initialScrollPending = false;
            scroller.scrollTo(0, getInitialScroll());
        }
        if (reactions != null) {
            int x = reactionsX - position[0];
            y = reactionsY - position[1];
            reactions.layout(x, y, x + reactionsWidth, y + reactionsHeight);
        }
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        if (closing) return true;
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            clearHighlight();
            continuation = false;
            ownsGesture = true;
        } else if (!ownsGesture && continuation) {
            return dispatchContinuation(event);
        }
        boolean handled = super.dispatchTouchEvent(event);
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) ownsGesture = false;
        return handled;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_UP) listener.dismiss();
        return true;
    }

    public void beginContinuation(float screenX, float screenY) {
        continuation = true;
        continuationMoved = false;
        holdX = screenX;
        holdY = screenY;
        holdAnchored = false;
    }

    public boolean dispatchContinuation(MotionEvent event) {
        if (closing) return false;
        if (!continuation) return false;
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_CANCEL || event.getPointerCount() != 1) {
            continuation = false;
            clearHighlight();
            return true;
        }
        float x = event.getRawX(), y = event.getRawY();
        if (!holdAnchored) {
            holdAnchored = true;
            holdX = x;
            holdY = y;
        }
        float slop = Math.max(ViewConfiguration.get(getContext()).getScaledTouchSlop(), AndroidUtilities.dp(10));
        if (!continuationMoved && Math.hypot(x - holdX, y - holdY) > slop) continuationMoved = true;
        View target = continuationMoved ? actionAt(menu, x, y, scroller) : null;
        if (target == null && continuationMoved && reactions != null) target = actionAt(reactions, x, y, this);
        if (target != highlighted) {
            clearHighlight();
            highlighted = target;
            if (target != null) {
                target.setPressed(true);
                target.performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP);
            }
        }
        if (action == MotionEvent.ACTION_UP) {
            View selected = highlighted;
            clearHighlight();
            continuation = false;
            if (selected instanceof ReactionsContainerLayout.ReactionHolderView) {
                long time = SystemClock.uptimeMillis();
                MotionEvent down = MotionEvent.obtain(time, time, MotionEvent.ACTION_DOWN,
                        selected.getWidth() * .5f, selected.getHeight() * .5f, 0);
                MotionEvent up = MotionEvent.obtain(time, time, MotionEvent.ACTION_UP,
                        selected.getWidth() * .5f, selected.getHeight() * .5f, 0);
                try {
                    if (selected.dispatchTouchEvent(down)) selected.dispatchTouchEvent(up);
                } finally {
                    down.recycle();
                    up.recycle();
                }
            } else if (selected != null) selected.performClick();
        }
        return true;
    }

    private View actionAt(View view, float x, float y, View viewport) {
        if (view.getVisibility() != VISIBLE || !view.isEnabled() || view.getAlpha() <= 0) return null;
        if (!containsScreenPoint(viewport, x, y) || !containsScreenPoint(view, x, y)) return null;
        if (view instanceof android.view.ViewGroup) {
            android.view.ViewGroup group = (android.view.ViewGroup) view;
            for (int i = group.getChildCount() - 1; i >= 0; i--) {
                View target = actionAt(group.getChildAt(i), x, y, viewport);
                if (target != null) return target;
            }
        }
        return view.isClickable() || view instanceof ReactionsContainerLayout.ReactionHolderView ? view : null;
    }

    private boolean containsScreenPoint(View view, float x, float y) {
        hitMatrix.reset();
        View current = view;
        while (true) {
            hitMatrix.postConcat(current.getMatrix());
            if (current.getParent() instanceof View) {
                View parent = (View) current.getParent();
                hitMatrix.postTranslate(current.getLeft() - parent.getScrollX(),
                        current.getTop() - parent.getScrollY());
                current = parent;
            } else {
                current.getLocationOnScreen(hitPosition);
                hitPoint[0] = hitPoint[1] = 0;
                current.getMatrix().mapPoints(hitPoint);
                hitMatrix.postTranslate(hitPosition[0] - hitPoint[0], hitPosition[1] - hitPoint[1]);
                break;
            }
        }
        if (!hitMatrix.invert(hitInverse)) return false;
        hitPoint[0] = x;
        hitPoint[1] = y;
        hitInverse.mapPoints(hitPoint);
        return hitPoint[0] >= 0 && hitPoint[0] < view.getWidth()
                && hitPoint[1] >= 0 && hitPoint[1] < view.getHeight();
    }

    private void clearHighlight() {
        if (highlighted != null) highlighted.setPressed(false);
        highlighted = null;
    }

    public void stopInteraction() {
        if (closing) return;
        closing = true;
        long time = SystemClock.uptimeMillis();
        MotionEvent cancel = MotionEvent.obtain(time, time, MotionEvent.ACTION_CANCEL, 0, 0, 0);
        try {
            super.dispatchTouchEvent(cancel);
        } finally {
            cancel.recycle();
        }
        continuation = false;
        ownsGesture = false;
        clearHighlight();
        scroller.fling(0);
    }

    @Override
    protected void onDetachedFromWindow() {
        stopInteraction();
        super.onDetachedFromWindow();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK && event.getAction() == KeyEvent.ACTION_UP) {
            listener.dismiss();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }
}

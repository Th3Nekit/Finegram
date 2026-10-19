package com.th3nekit.finegram.chats.ui;

import android.content.Context;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.FrameLayout;

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
    private final int touchSlop;
    private final int[] position = new int[2];
    private int menuX, menuY, menuWidth, menuHeight;
    private int reactionsX, reactionsY, reactionsWidth, reactionsHeight;
    private int pointerId = -1;
    private float downY, lastY;
    private boolean dragging;

    public MessageMenuPreviewLayout(Context context, View menu, ReactionsContainerLayout reactions, Listener listener) {
        super(context);
        this.menu = menu;
        this.reactions = reactions;
        this.listener = listener;
        touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        addView(menu);
        if (reactions != null) {
            reactions.setPadding(AndroidUtilities.dp(4), AndroidUtilities.dp(4),
                    AndroidUtilities.dp(4), AndroidUtilities.dp(22));
            addView(reactions, new LayoutParams(LayoutParams.WRAP_CONTENT, AndroidUtilities.dp(74)));
        }
        setFocusableInTouchMode(true);
        setClipChildren(false);
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
        reactions.getLayoutParams().height = AndroidUtilities.dp(74) + Math.round(reactions.getTopOffset());
        reactions.measure(MeasureSpec.makeMeasureSpec(reactionsWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(reactions.getLayoutParams().height, MeasureSpec.EXACTLY));
        reactions.measureHint();
        reactionsWidth = Math.max(1, Math.min(availableWidth,
                Math.max(reactionsWidth, reactions.getHintTextWidth() + AndroidUtilities.dp(24))));
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
        menu.measure(MeasureSpec.makeMeasureSpec(menuWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(menuHeight, MeasureSpec.AT_MOST));
        if (reactions != null) {
            reactions.measure(MeasureSpec.makeMeasureSpec(reactionsWidth, MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec(reactionsHeight, MeasureSpec.EXACTLY));
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        getLocationInWindow(position);
        int x = menuX - position[0];
        int y = menuY - position[1];
        menu.layout(x, y, x + menu.getMeasuredWidth(), y + menu.getMeasuredHeight());
        if (reactions != null) {
            x = reactionsX - position[0];
            y = reactionsY - position[1];
            reactions.layout(x, y, x + reactionsWidth, y + reactionsHeight);
        }
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            pointerId = -1;
            dragging = false;
            if (listener.contains(event.getRawX(), event.getRawY())) {
                pointerId = event.getPointerId(0);
                downY = lastY = event.getY();
                return true;
            }
        }
        return super.onInterceptTouchEvent(event);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();
        if (pointerId < 0) {
            if (action == MotionEvent.ACTION_DOWN) listener.dismiss();
            return true;
        }
        int index = event.findPointerIndex(pointerId);
        if (index < 0) {
            pointerId = -1;
            return true;
        }
        float y = event.getY(index);
        if (action == MotionEvent.ACTION_MOVE) {
            if (!dragging && Math.abs(y - downY) > touchSlop) dragging = true;
            if (dragging) listener.scroll(y - lastY);
            lastY = y;
        } else if (action == MotionEvent.ACTION_POINTER_UP && event.getPointerId(event.getActionIndex()) == pointerId) {
            int replacement = event.getActionIndex() == 0 ? 1 : 0;
            pointerId = event.getPointerId(replacement);
            downY = lastY = event.getY(replacement);
        } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            pointerId = -1;
            dragging = false;
        }
        return true;
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

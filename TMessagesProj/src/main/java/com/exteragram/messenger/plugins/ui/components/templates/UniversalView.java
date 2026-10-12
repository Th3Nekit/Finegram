/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.ui.components.templates;

import android.content.Context;
import android.graphics.Canvas;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;

import org.telegram.messenger.Utilities;

public class UniversalView extends View {

    public interface UniversalViewDelegate {
        void onDraw(Canvas canvas, Utilities.Callback<Canvas> whenDefault);

        void onMeasure(int widthSpec, int heightSpec, Utilities.Callback2<Integer, Integer> whenDefault);

        void onAttachedToWindow();

        void onDetachedFromWindow();

        void onTouchEvent(MotionEvent event, Utilities.Callback<MotionEvent> whenDefault);

        void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info,
                                               Utilities.Callback<AccessibilityNodeInfo> whenDefault);
    }

    private final UniversalViewDelegate delegate;

    public UniversalView(Context context) {
        this(context, null);
    }

    public UniversalView(Context context, UniversalViewDelegate delegate) {
        super(context);
        this.delegate = delegate;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (delegate == null) {
            super.onDraw(canvas);
            return;
        }
        delegate.onDraw(canvas, super::onDraw);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (delegate == null) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            return;
        }

        final boolean[] measured = {false};
        delegate.onMeasure(widthMeasureSpec, heightMeasureSpec, (width, height) -> {
            measured[0] = true;
            super.onMeasure(width, height);
        });
        if (!measured[0]) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (delegate != null) {
            delegate.onAttachedToWindow();
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (delegate != null) {
            delegate.onDetachedFromWindow();
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (delegate == null) {
            return super.onTouchEvent(event);
        }
        final boolean[] handled = {false};
        delegate.onTouchEvent(event, passed -> handled[0] = super.onTouchEvent(passed));
        return handled[0];
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        if (delegate == null) {
            super.onInitializeAccessibilityNodeInfo(info);
            return;
        }
        delegate.onInitializeAccessibilityNodeInfo(info, super::onInitializeAccessibilityNodeInfo);
    }
}

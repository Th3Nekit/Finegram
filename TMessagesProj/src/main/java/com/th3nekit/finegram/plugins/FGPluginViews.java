/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.plugins;

import android.content.Context;
import android.graphics.Canvas;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.chaquo.python.PyObject;

import org.telegram.messenger.FileLog;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

public final class FGPluginViews {

    private FGPluginViews() {
    }

    public interface Delegating {
        void attachOwner(PyObject owner, List<PyObject> overridden);

        PyObject owner();
    }

    private static final class Link {
        PyObject owner;
        final Set<String> overridden = new HashSet<>();

        void attach(PyObject owner, List<PyObject> names) {
            this.owner = owner;
            overridden.clear();
            if (names == null) {
                return;
            }
            for (PyObject name : names) {
                if (name != null) {
                    overridden.add(name.toString());
                }
            }
        }

        boolean has(String method) {
            return owner != null && overridden.contains(method);
        }

        void call(String method, Object... args) {
            try {
                owner.callAttr(method, args);
            } catch (Throwable t) {

                FileLog.e(t);
            }
        }

        boolean callBoolean(String method, boolean fallback, Object... args) {
            try {
                final PyObject result = owner.callAttr(method, args);
                return result == null ? fallback : result.toBoolean();
            } catch (Throwable t) {
                FileLog.e(t);
                return fallback;
            }
        }
    }

    public static class PluginFrameLayout extends FrameLayout implements Delegating {
        private final Link link = new Link();

        public PluginFrameLayout(Context context) {
            super(context);
        }

        @Override
        public void attachOwner(PyObject owner, List<PyObject> overridden) {
            link.attach(owner, overridden);
        }

        @Override
        public PyObject owner() {
            return link.owner;
        }

        public void superOnMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }

        public void superOnLayout(boolean changed, int left, int top, int right, int bottom) {
            super.onLayout(changed, left, top, right, bottom);
        }

        public void superOnDraw(Canvas canvas) {
            super.onDraw(canvas);
        }

        public void superDispatchDraw(Canvas canvas) {
            super.dispatchDraw(canvas);
        }

        public boolean superOnTouchEvent(MotionEvent event) {
            return super.onTouchEvent(event);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            if (link.has("onMeasure")) {
                link.call("onMeasure", widthMeasureSpec, heightMeasureSpec);
            } else {
                super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            }
        }

        @Override
        protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
            if (link.has("onLayout")) {
                link.call("onLayout", changed, left, top, right, bottom);
            } else {
                super.onLayout(changed, left, top, right, bottom);
            }
        }

        @Override
        protected void onDraw(Canvas canvas) {
            if (link.has("onDraw")) {
                link.call("onDraw", canvas);
            } else {
                super.onDraw(canvas);
            }
        }

        @Override
        protected void dispatchDraw(Canvas canvas) {
            if (link.has("dispatchDraw")) {
                link.call("dispatchDraw", canvas);
            } else {
                super.dispatchDraw(canvas);
            }
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            if (link.has("onTouchEvent")) {
                return link.callBoolean("onTouchEvent", false, event);
            }
            return super.onTouchEvent(event);
        }

        @Override
        protected void onAttachedToWindow() {
            super.onAttachedToWindow();
            if (link.has("onAttachedToWindow")) {
                link.call("onAttachedToWindow");
            }
        }

        @Override
        protected void onDetachedFromWindow() {
            super.onDetachedFromWindow();
            if (link.has("onDetachedFromWindow")) {
                link.call("onDetachedFromWindow");
            }
        }
    }

    public static class PluginLinearLayout extends LinearLayout implements Delegating {
        private final Link link = new Link();

        public PluginLinearLayout(Context context) {
            super(context);
        }

        @Override
        public void attachOwner(PyObject owner, List<PyObject> overridden) {
            link.attach(owner, overridden);
        }

        @Override
        public PyObject owner() {
            return link.owner;
        }

        public void superOnMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }

        public void superOnDraw(Canvas canvas) {
            super.onDraw(canvas);
        }

        public void superDispatchDraw(Canvas canvas) {
            super.dispatchDraw(canvas);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            if (link.has("onMeasure")) {
                link.call("onMeasure", widthMeasureSpec, heightMeasureSpec);
            } else {
                super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            }
        }

        @Override
        protected void onDraw(Canvas canvas) {
            if (link.has("onDraw")) {
                link.call("onDraw", canvas);
            } else {
                super.onDraw(canvas);
            }
        }

        @Override
        protected void dispatchDraw(Canvas canvas) {
            if (link.has("dispatchDraw")) {
                link.call("dispatchDraw", canvas);
            } else {
                super.dispatchDraw(canvas);
            }
        }
    }

    public static class PluginView extends View implements Delegating {
        private final Link link = new Link();

        public PluginView(Context context) {
            super(context);
        }

        @Override
        public void attachOwner(PyObject owner, List<PyObject> overridden) {
            link.attach(owner, overridden);
        }

        @Override
        public PyObject owner() {
            return link.owner;
        }

        public void superOnMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }

        public void superOnDraw(Canvas canvas) {
            super.onDraw(canvas);
        }

        public boolean superOnTouchEvent(MotionEvent event) {
            return super.onTouchEvent(event);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            if (link.has("onMeasure")) {
                link.call("onMeasure", widthMeasureSpec, heightMeasureSpec);
            } else {
                super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            }
        }

        @Override
        protected void onDraw(Canvas canvas) {
            if (link.has("onDraw")) {
                link.call("onDraw", canvas);
            } else {
                super.onDraw(canvas);
            }
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            if (link.has("onTouchEvent")) {
                return link.callBoolean("onTouchEvent", false, event);
            }
            return super.onTouchEvent(event);
        }
    }

    public static class PluginTextView extends TextView implements Delegating {
        private final Link link = new Link();

        public PluginTextView(Context context) {
            super(context);
        }

        @Override
        public void attachOwner(PyObject owner, List<PyObject> overridden) {
            link.attach(owner, overridden);
        }

        @Override
        public PyObject owner() {
            return link.owner;
        }

        public void superOnMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }

        public void superOnDraw(Canvas canvas) {
            super.onDraw(canvas);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            if (link.has("onMeasure")) {
                link.call("onMeasure", widthMeasureSpec, heightMeasureSpec);
            } else {
                super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            }
        }

        @Override
        protected void onDraw(Canvas canvas) {
            if (link.has("onDraw")) {
                link.call("onDraw", canvas);
            } else {
                super.onDraw(canvas);
            }
        }
    }

    public static View create(String superclassName, Context context) {
        if (superclassName == null || context == null) {
            return null;
        }
        switch (superclassName) {
            case "android.widget.FrameLayout":
                return new PluginFrameLayout(context);
            case "android.widget.LinearLayout":
                return new PluginLinearLayout(context);
            case "android.widget.TextView":
                return new PluginTextView(context);
            case "android.view.View":
                return new PluginView(context);
            default:
                return null;
        }
    }

    public static boolean supports(String superclassName) {
        return superclassName != null && (
                "android.widget.FrameLayout".equals(superclassName)
                        || "android.widget.LinearLayout".equals(superclassName)
                        || "android.widget.TextView".equals(superclassName)
                        || "android.view.View".equals(superclassName));
    }
}

package org.telegram.ui.Components.chat;

import android.graphics.PointF;
import android.graphics.RectF;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.ViewTreeObserver;

import androidx.annotation.NonNull;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.WeakHashMap;

public final class ViewPositionWatcher implements
        ViewTreeObserver.OnPreDrawListener,
        View.OnAttachStateChangeListener {

    public interface OnChangedListener {
        void onPositionChanged(@NonNull View view, @NonNull RectF rectInParent);
    }

    private final View anchorView;
    private ViewTreeObserver vto;
    private boolean listening;

    private static final class Tracked {
        final ViewGroup parent;
        final OnChangedListener listener;
        final RectF last = new RectF();
        boolean multiwindow;
        boolean hasLast;

        Tracked(@NonNull ViewGroup parent, @NonNull OnChangedListener listener) {
            this.parent = parent;
            this.listener = listener;
        }
    }

    private final WeakHashMap<View, List<Tracked>> tracked = new WeakHashMap<>();
    private final RectF tmpRect = new RectF();
    private static final int[] tmpCords = new int[2];

    public ViewPositionWatcher(@NonNull View anchorView) {
        this.anchorView = anchorView;
        anchorView.addOnAttachStateChangeListener(this);
        attachIfPossible();
    }

    public void subscribe(@NonNull View view,
                          @NonNull ViewGroup parentView,
                          @NonNull OnChangedListener listener) {
        subscribe(view, parentView, listener, false);
    }

    public void subscribe(@NonNull View view,
                          @NonNull ViewGroup parentView,
                          @NonNull OnChangedListener listener,
                          boolean multiwindow) {
        Tracked t = new Tracked(parentView, listener);
        t.multiwindow = multiwindow;
        List<Tracked> tList = tracked.get(view);
        if (tList == null) {
            tList = new ArrayList<>(1);
            tracked.put(view, tList);
        }
        tList.add(t);

        computeRectInParent(view, parentView, tmpRect);
        t.last.set(tmpRect);

        ensureListening();

        if (multiwindow) {
            new ViewOnPreDraw(view, this);
        }
    }

    public void unsubscribe(@NonNull View view) {
        tracked.remove(view);
    }

    public void clear() {
        tracked.clear();
    }

    public void shutdown() {
        detachIfListening();
        anchorView.removeOnAttachStateChangeListener(this);
        tracked.clear();
    }

    private void attachIfPossible() {
        if (!anchorView.isAttachedToWindow()) return;
        ViewTreeObserver newVto = anchorView.getViewTreeObserver();
        if (newVto != null && newVto.isAlive()) {
            vto = newVto;
            if (!listening) {
                vto.addOnPreDrawListener(this);
                listening = true;
            }
        }
    }

    private void ensureListening() {
        if (!listening) attachIfPossible();
    }

    private void detachIfListening() {
        if (listening && vto != null && vto.isAlive()) {
            vto.removeOnPreDrawListener(this);
        }
        listening = false;
        vto = null;
    }

    @Override
    public void onViewAttachedToWindow(@NonNull View v) {
        attachIfPossible();
    }

    @Override
    public void onViewDetachedFromWindow(@NonNull View v) {
        if (v == anchorView) {
            detachIfListening();
        }
    }

    @Override
    public boolean onPreDraw() {

        ViewTreeObserver current = anchorView.getViewTreeObserver();
        if (current != vto) {
            detachIfListening();
            attachIfPossible();
        }

        if (tracked.isEmpty()) return true;

        for (Map.Entry<View, List<Tracked>> e : tracked.entrySet()) {
            View view = e.getKey();
            List<Tracked> tList = e.getValue();
            if (view == null || tList == null) continue;

            for (Tracked t : tList) {
                if (t.multiwindow) {
                    view.getLocationOnScreen(tmpCords);
                    tmpRect.set(tmpCords[0], tmpCords[1], tmpCords[0] + view.getWidth(), tmpCords[1] + view.getHeight());

                    t.parent.getLocationOnScreen(tmpCords);
                    tmpRect.offset(-tmpCords[0], -tmpCords[1]);
                } else {
                    if (!computeRectInParent(view, t.parent, tmpRect)) continue;
                }

                if (!t.hasLast || !tmpRect.equals(t.last)) {
                    t.last.set(tmpRect);
                    t.hasLast = true;
                    try {
                        t.listener.onPositionChanged(view, new RectF(tmpRect));
                    } catch (Throwable ignored) {

                    }
                }
            }
        }
        return true;
    }

    public static float computeYCoordinateInParent(@NonNull View view, @NonNull ViewGroup parentView) {
        computeRectInParent(view, parentView, tmpRectF2);
        return tmpRectF2.top;
    }

    public static float computeXCoordinateInParent(@NonNull View view, @NonNull ViewGroup parentView) {
        computeRectInParent(view, parentView, tmpRectF2);
        return tmpRectF2.left;
    }

    private static RectF tmpRectF2 = new RectF();
    public static boolean computeCoordinatesInParent(@NonNull View view,
                                                   @NonNull ViewGroup parentView, PointF out) {
        final boolean result = computeRectInParent(view, parentView, tmpRectF2);
        if (result) {
            out.x = tmpRectF2.left;
            out.y = tmpRectF2.top;
        }

        return result;
    }

    public static boolean computeRectInParent(@NonNull View view,
                                               @NonNull View parentView,
                                               @NonNull RectF out) {
        float left = 0f;
        float top = 0f;

        View current = view;
        while (current != null && current != parentView) {
            left += current.getX();
            top  += current.getY();

            ViewParent vp = current.getParent();
            if (!(vp instanceof View)) {
                return false;
            }
            View parent = (View) vp;
            left -= parent.getScrollX();
            top  -= parent.getScrollY();

            current = parent;
        }

        if (current != parentView) {

            return false;
        }

        final float l = left;
        final float t = top;
        final float r = l + view.getWidth();
        final float b = t + view.getHeight();
        out.set(l, t, r, b);
        return true;
    }

    private static class ViewOnPreDraw {

        private final View view;
        private final ViewTreeObserver.OnPreDrawListener listener;

        private ViewTreeObserver viewTreeObserver;

        public ViewOnPreDraw(
                @NonNull View view,
                @NonNull ViewTreeObserver.OnPreDrawListener listener
        ) {
            this.view = view;
            this.listener = listener;

            view.addOnAttachStateChangeListener(attachStateChangeListener);

            if (view.isAttachedToWindow()) {
                attach();
            }
        }

        private final View.OnAttachStateChangeListener attachStateChangeListener =
                new View.OnAttachStateChangeListener() {
                    @Override
                    public void onViewAttachedToWindow(@NonNull View v) {
                        attach();
                    }

                    @Override
                    public void onViewDetachedFromWindow(@NonNull View v) {
                        detach();
                    }
                };

        private void attach() {
            ViewTreeObserver observer = view.getViewTreeObserver();
            if (viewTreeObserver == observer) {
                return;
            }

            detach();

            viewTreeObserver = observer;
            if (observer.isAlive()) {
                observer.addOnPreDrawListener(listener);
            }
        }

        private void detach() {
            if (viewTreeObserver == null) {
                return;
            }

            if (viewTreeObserver.isAlive()) {
                viewTreeObserver.removeOnPreDrawListener(listener);
            }
            viewTreeObserver = null;
        }

        public void destroy() {
            detach();
            view.removeOnAttachStateChangeListener(attachStateChangeListener);
        }
    }
}

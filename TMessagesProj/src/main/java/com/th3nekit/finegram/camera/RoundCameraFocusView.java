package com.th3nekit.finegram.camera;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.os.Build;
import android.os.SystemClock;
import android.provider.Settings;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.TextureView;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewParent;
import android.view.animation.DecelerateInterpolator;

import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.messenger.SharedConfig;

import java.util.ArrayList;

public final class RoundCameraFocusView extends View {
    public interface Listener {
        void onFocus(float x, float y, float displayX, float displayY, int request);
        void onSwitchCamera();
        default boolean onFocusLock(float x, float y, float displayX, float displayY, int request) { return false; }
        default boolean canAdjustExposure() { return false; }
        default void onExposure(float value) {}
    }

    private static final float NEUTRAL = 0.5f;
    private static final int LOCK_COLOR = 0xffffd23f;

    private final RoundCameraFocusGesture gesture;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint pillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path brackets = new Path();
    private final Path sun = new Path();
    private final RectF pill = new RectF();
    private final Matrix inverse = new Matrix();
    private final float density;
    private final float slop;
    private Listener listener;
    private Runnable pendingFocus;
    private Runnable pendingLock;
    private ValueAnimator animator;
    private float centerX, centerY, progress = 1f, opacity;
    private int requestVersion;
    private boolean locked;
    private float exposure = NEUTRAL;
    private boolean exposureDrag;
    private boolean lockHeld;
    private boolean exposureMoved;
    private float dragStartY, dragStartExposure;
    private long exposureSentAt;
    private float exposureSent = NEUTRAL;
    private final Runnable hide = () -> {
        if (locked) {
            fadeTo(0.6f);
        } else if (motionEnabled()) {
            fadeTo(0f);
        } else {
            dismiss();
        }
    };

    public RoundCameraFocusView(Context context) {
        super(context);
        density = getResources().getDisplayMetrics().density;
        ViewConfiguration configuration = ViewConfiguration.get(context);
        slop = Math.min(8 * density, configuration.getScaledTouchSlop());
        gesture = new RoundCameraFocusGesture(slop,
                configuration.getScaledDoubleTapSlop(), ViewConfiguration.getDoubleTapTimeout(),
                ViewConfiguration.getLongPressTimeout());
        setClickable(false);
        setFocusable(false);
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeCap(Paint.Cap.ROUND);
        paint.setStrokeJoin(Paint.Join.ROUND);
        textPaint.setTextSize(11 * density);
        textPaint.setTypeface(Typeface.create(Typeface.DEFAULT, Typeface.BOLD));
        textPaint.setTextAlign(Paint.Align.CENTER);
    }

    public void setListener(Listener listener) { this.listener = listener; }

    public boolean isTapFocusEnabled() {
        return getContext().getSharedPreferences("mainconfig", Context.MODE_PRIVATE)
                .getBoolean("FG_RoundTapFocus", true);
    }

    public boolean isDoubleTapSwitchEnabled() {
        return getContext().getSharedPreferences("mainconfig", Context.MODE_PRIVATE)
                .getBoolean("FG_RoundDoubleTapSwitch", true);
    }

    public boolean observeTouch(MotionEvent event, View source, TextureView preview, boolean enabled) {
        float[] display = {event.getX(), event.getY()};
        boolean mapped = toPreview(source, preview, display);
        int action = event.getActionMasked();
        if (exposureDrag) {
            if (action == MotionEvent.ACTION_MOVE && event.getPointerCount() == 1 && mapped) {
                dragExposure(display[1], false);
            } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL
                    || action == MotionEvent.ACTION_POINTER_DOWN) {
                if (mapped) dragExposure(display[1], true);
                exposureDrag = false;
                removeCallbacks(hide);
                postDelayed(hide, 1500L);
                invalidate();
            }
            return true;
        }
        if (lockHeld) {
            if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) lockHeld = false;
            if (action != MotionEvent.ACTION_DOWN) return false;
            lockHeld = false;
        }
        if (action == MotionEvent.ACTION_DOWN && enabled && mapped && onSunHandle(display[0], display[1])) {
            cancelPendingFocus();
            cancelPendingLock();
            gesture.reset();
            exposureDrag = true;
            exposureMoved = false;
            dragStartY = display[1];
            dragStartExposure = exposure;
            removeCallbacks(hide);
            stopAnimation();
            opacity = 1f;
            invalidate();
            return true;
        }

        boolean eligible = enabled && mapped && preview.getWidth() > 0 && preview.getHeight() > 0
                && RoundCameraFocusMath.insideCircle(display[0] / preview.getWidth(), display[1] / preview.getHeight());
        float[] content = display.clone();
        if (eligible) {
            preview.getTransform(inverse);
            eligible = inverse.invert(inverse);
            if (eligible) inverse.mapPoints(content);
            eligible &= content[0] >= 0 && content[0] <= preview.getWidth()
                    && content[1] >= 0 && content[1] <= preview.getHeight();
        }
        if (action == MotionEvent.ACTION_DOWN) {
            cancelPendingFocus();
            cancelPendingLock();
            ++requestVersion;
            gesture.down(display[0], display[1], event.getPointerId(0), event.getEventTime(), eligible);
            if (!eligible) {
                abandonGesture();
            } else if (isTapFocusEnabled()) {
                armLock(content[0] / preview.getWidth(), content[1] / preview.getHeight(), display[0], display[1]);
            }
        } else if (action == MotionEvent.ACTION_MOVE && event.getPointerCount() == 1) {
            gesture.move(display[0], display[1], event.getPointerId(0));
            if (!eligible || !gesture.isTracking()) abandonGesture();
        } else if (action == MotionEvent.ACTION_UP && event.getPointerCount() == 1) {
            cancelPendingLock();
            boolean doubleEnabled = isDoubleTapSwitchEnabled();
            int result = gesture.up(display[0], display[1], event.getPointerId(0), event.getEventTime(), eligible, doubleEnabled);
            if (result == RoundCameraFocusGesture.DOUBLE_TAP) {
                clear();
                if (listener != null) listener.onSwitchCamera();
            } else if (result == RoundCameraFocusGesture.TAP) {
                cancelPendingFocus();
                if (!isTapFocusEnabled()) {
                    dismiss();
                    return false;
                }
                int version = ++requestVersion;
                float x = content[0] / preview.getWidth(), y = content[1] / preview.getHeight();
                float dx = display[0], dy = display[1];
                pendingFocus = () -> {
                    pendingFocus = null;
                    if (!isCurrentRequest(version) || listener == null) return;
                    locked = false;
                    listener.onFocus(x, y, dx, dy, version);
                };
                if (doubleEnabled) postDelayed(pendingFocus, ViewConfiguration.getDoubleTapTimeout());
                else pendingFocus.run();
            } else {
                abandonGesture();
            }
        } else if (action == MotionEvent.ACTION_CANCEL || event.getPointerCount() > 1
                || action == MotionEvent.ACTION_POINTER_UP) {
            abandonGesture();
        }
        return false;
    }

    private void armLock(float x, float y, float displayX, float displayY) {
        final int version = requestVersion;
        pendingLock = () -> {
            pendingLock = null;
            if (version != requestVersion || !gesture.isTracking() || listener == null) return;
            gesture.reset();
            lockHeld = true;
            int request = ++requestVersion;
            if (listener.onFocusLock(x, y, displayX, displayY, request)) {
                performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
            }
        };
        postDelayed(pendingLock, ViewConfiguration.getLongPressTimeout());
    }

    private void cancelPendingLock() {
        if (pendingLock != null) removeCallbacks(pendingLock);
        pendingLock = null;
    }

    private void dragExposure(float y, boolean finished) {
        if (!exposureMoved && Math.abs(y - dragStartY) < slop && !finished) return;
        exposureMoved = true;
        float travel = trackHalf() * 2f * 1.6f;
        float value = Math.max(0f, Math.min(1f, dragStartExposure + (dragStartY - y) / travel));
        exposure = value;
        invalidate();
        long now = SystemClock.uptimeMillis();
        if (listener != null && (finished || now - exposureSentAt >= 50L) && Math.abs(value - exposureSent) > 0.001f) {
            exposureSentAt = now;
            exposureSent = value;
            listener.onExposure(value);
        }
    }

    private void resetExposure() {
        exposure = NEUTRAL;
        exposureSent = NEUTRAL;
    }

    private boolean onSunHandle(float x, float y) {
        if (opacity < 0.3f || listener == null || !listener.canAdjustExposure()) return false;
        float reach = 30 * density;
        return Math.abs(x - sunX()) <= reach && Math.abs(y - centerY) <= trackHalf() + reach;
    }

    private float bracketRadius() {
        return density * (23f + 4f * (1f - progress));
    }

    private float trackHalf() {
        return 46 * density;
    }

    private float sunX() {
        float x = centerX + bracketRadius() + 20 * density;
        if (x > getWidth() - 16 * density) x = centerX - bracketRadius() - 20 * density;
        return x;
    }

    private boolean toPreview(View source, View preview, float[] point) {
        ArrayList<View> chain = new ArrayList<>();
        View current = preview;
        while (current != source) {
            chain.add(current);
            ViewParent parent = current.getParent();
            if (!(parent instanceof View)) return false;
            current = (View) parent;
        }
        for (int i = chain.size() - 1; i >= 0; i--) {
            View child = chain.get(i);
            View parent = (View) child.getParent();
            point[0] += parent.getScrollX() - child.getLeft();
            point[1] += parent.getScrollY() - child.getTop();
            if (!child.getMatrix().invert(inverse)) return false;
            inverse.mapPoints(point);
        }
        return true;
    }

    public boolean isCurrentRequest(int request) {
        return request == requestVersion && isAttachedToWindow() && isTapFocusEnabled();
    }

    public void showRequest(int request, float x, float y) {
        showRequest(request, x, y, false);
    }

    public void showRequest(int request, float x, float y, boolean lock) {
        if (!isCurrentRequest(request)) return;
        stopAnimation();
        removeCallbacks(hide);
        locked = lock;
        centerX = x;
        centerY = y;
        opacity = 1f;
        progress = motionEnabled() ? 0f : 1f;
        invalidate();
        if (motionEnabled()) animateIn();
        boolean exposureReady = listener != null && listener.canAdjustExposure();
        postDelayed(hide, lock ? 2500L : exposureReady ? 2500L : 900L);
    }

    public void clear() {
        ++requestVersion;
        gesture.reset();
        cancelPendingFocus();
        cancelPendingLock();
        exposureDrag = false;
        lockHeld = false;
        locked = false;
        dismiss();
    }

    public void reset() {
        clear();
        resetExposure();
    }

    private void abandonGesture() {
        ++requestVersion;
        gesture.reset();
        cancelPendingFocus();
        cancelPendingLock();
        if (!locked) dismiss();
    }

    private void cancelPendingFocus() {
        if (pendingFocus != null) removeCallbacks(pendingFocus);
        pendingFocus = null;
    }

    private void dismiss() {
        removeCallbacks(hide);
        stopAnimation();
        opacity = 0f;
        invalidate();
    }

    private void stopAnimation() {
        if (animator == null) return;
        animator.removeAllUpdateListeners();
        animator.cancel();
        animator = null;
    }

    private void animateIn() {
        stopAnimation();
        animator = ValueAnimator.ofFloat(0f, 1f);
        animator.setDuration(180L);
        animator.setInterpolator(new DecelerateInterpolator());
        animator.addUpdateListener(value -> {
            progress = (float) value.getAnimatedValue();
            invalidate();
        });
        animator.start();
    }

    private void fadeTo(float target) {
        stopAnimation();
        if (!motionEnabled()) {
            opacity = target;
            invalidate();
            return;
        }
        final float from = opacity;
        animator = ValueAnimator.ofFloat(0f, 1f);
        animator.setDuration(180L);
        animator.setInterpolator(new DecelerateInterpolator());
        animator.addUpdateListener(value -> {
            float p = (float) value.getAnimatedValue();
            opacity = from + (target - from) * p;
            invalidate();
        });
        animator.start();
    }

    private boolean motionEnabled() {
        if (!SharedConfig.animationsEnabled()) return false;
        if (Build.VERSION.SDK_INT >= 26) return ValueAnimator.areAnimatorsEnabled();
        return Settings.Global.getFloat(getContext().getContentResolver(), Settings.Global.ANIMATOR_DURATION_SCALE, 1f) > 0f;
    }

    @Override protected void onDraw(Canvas canvas) {
        if (opacity <= 0f) return;
        int color = locked ? LOCK_COLOR : 0xfff5f5f0;
        float radius = bracketRadius();
        float arm = density * 9f;
        brackets.rewind();
        for (int x = -1; x <= 1; x += 2) for (int y = -1; y <= 1; y += 2) {
            float px = centerX + radius * x, py = centerY + radius * y;
            brackets.moveTo(px - arm * x, py);
            brackets.lineTo(px, py);
            brackets.lineTo(px, py - arm * y);
        }
        sun.rewind();
        boolean exposureShown = listener != null && listener.canAdjustExposure();
        float sx = sunX();
        float sy = centerY + (NEUTRAL - exposure) * 2f * trackHalf();
        if (exposureShown) {
            float core = 4.5f * density;
            sun.addCircle(sx, sy, core, Path.Direction.CW);
            for (int i = 0; i < 8; i++) {
                double angle = Math.PI / 4 * i;
                float cos = (float) Math.cos(angle), sin = (float) Math.sin(angle);
                sun.moveTo(sx + cos * (core + 2.5f * density), sy + sin * (core + 2.5f * density));
                sun.lineTo(sx + cos * (core + 5f * density), sy + sin * (core + 5f * density));
            }
            if (exposureDrag || Math.abs(exposure - NEUTRAL) > 0.01f) {
                float gap = core + 7f * density;
                float top = centerY - trackHalf(), bottom = centerY + trackHalf();
                if (sy - gap > top) {
                    sun.moveTo(sx, top);
                    sun.lineTo(sx, sy - gap);
                }
                if (sy + gap < bottom) {
                    sun.moveTo(sx, sy + gap);
                    sun.lineTo(sx, bottom);
                }
            }
        }
        paint.setColor(0x60000000);
        paint.setAlpha((int) (96 * opacity));
        paint.setStrokeWidth(density * 3f);
        canvas.drawPath(brackets, paint);
        if (exposureShown) canvas.drawPath(sun, paint);
        paint.setColor(color);
        paint.setAlpha((int) (230 * opacity));
        paint.setStrokeWidth(density * 1.4f);
        canvas.drawPath(brackets, paint);
        if (exposureShown) canvas.drawPath(sun, paint);

        if (locked) {
            String label = LocaleController.getString(R.string.FG_RoundFocusLocked);
            float width = textPaint.measureText(label) + 16 * density;
            float height = 20 * density;
            float cx = Math.max(width / 2 + 4 * density, Math.min(getWidth() - width / 2 - 4 * density, centerX));
            float bottom = centerY - radius - 8 * density;
            if (bottom - height < 4 * density) bottom = centerY + radius + 8 * density + height;
            pill.set(cx - width / 2, bottom - height, cx + width / 2, bottom);
            pillPaint.setColor(0x99000000);
            pillPaint.setAlpha((int) (153 * opacity));
            canvas.drawRoundRect(pill, height / 2, height / 2, pillPaint);
            textPaint.setColor(LOCK_COLOR);
            textPaint.setAlpha((int) (255 * opacity));
            canvas.drawText(label, cx, pill.centerY() - (textPaint.descent() + textPaint.ascent()) / 2, textPaint);
        }
    }

    @Override protected void onDetachedFromWindow() {
        reset();
        super.onDetachedFromWindow();
    }
}

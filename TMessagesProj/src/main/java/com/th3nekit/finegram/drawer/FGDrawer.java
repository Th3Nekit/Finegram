/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.drawer;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.FrameLayout;

import androidx.dynamicanimation.animation.FloatPropertyCompat;
import androidx.dynamicanimation.animation.SpringAnimation;
import androidx.dynamicanimation.animation.SpringForce;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.ui.ActionBar.Theme;

public class FGDrawer extends FrameLayout {

    private static final int MAX_WIDTH_DP = 300;
    private static final int EDGE_GAP_DP = 56;

    private static final int CORNER_DP = 20;

    private static final int SCRIM_ALPHA = 102;

    private final FrameLayout panel;
    private final Paint scrimPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path clipPath = new Path();
    private final RectF clipRect = new RectF();
    private final float[] corners = new float[8];

    private float progress;
    private SpringAnimation spring;

    private View contentBehind;

    private VelocityTracker tracker;
    private boolean dragging;
    private float startX;
    private float startY;
    private float startProgress;
    private final int touchSlop;

    public FGDrawer(Context context) {
        super(context);
        touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        setWillNotDraw(false);
        setVisibility(GONE);

        panel = new FrameLayout(context);
        addView(panel, new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT));
    }

    public FrameLayout getPanel() {
        return panel;
    }

    public void setContentBehind(View view) {
        contentBehind = view;
    }

    public boolean isOpened() {
        return progress > 0.5f;
    }

    private int panelWidth() {

        int width = getMeasuredWidth();
        if (width == 0 && getParent() instanceof View) {
            width = ((View) getParent()).getWidth();
        }
        return Math.min(AndroidUtilities.dp(MAX_WIDTH_DP),
                Math.max(AndroidUtilities.dp(MAX_WIDTH_DP) / 2,
                        width - AndroidUtilities.dp(EDGE_GAP_DP)));
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        panel.measure(
                MeasureSpec.makeMeasureSpec(panelWidth(), MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(getMeasuredHeight(), MeasureSpec.EXACTLY));
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        final int width = panelWidth();
        if (LocaleController.isRTL) {
            panel.layout(getMeasuredWidth() - width, 0, getMeasuredWidth(), getMeasuredHeight());
        } else {
            panel.layout(0, 0, width, getMeasuredHeight());
        }
        applyProgress();
    }

    private void applyProgress() {
        final int width = panelWidth();
        final boolean rtl = LocaleController.isRTL;
        final float hidden = (1f - progress) * width;
        panel.setTranslationX(rtl ? hidden : -hidden);
        if (contentBehind != null) {

            final float push = progress * width * 0.3f;
            contentBehind.setTranslationX(rtl ? -push : push);
        }
        setVisibility(progress <= 0.001f ? GONE : VISIBLE);
        invalidate();
    }

    private void setProgress(float value) {
        final boolean wasClosed = progress <= 0.001f;
        progress = Math.max(0f, Math.min(1f, value));
        if (wasClosed && progress > 0.001f && onOpening != null) {

            onOpening.run();
        }
        applyProgress();
    }

    public void setOnOpening(Runnable listener) {
        onOpening = listener;
    }

    private Runnable onOpening;

    private static final FloatPropertyCompat<FGDrawer> PROGRESS =
            new FloatPropertyCompat<FGDrawer>("drawerProgress") {
                @Override
                public float getValue(FGDrawer drawer) {
                    return drawer.progress * 100f;
                }

                @Override
                public void setValue(FGDrawer drawer, float value) {
                    drawer.setProgress(value / 100f);
                }
            };

    private void springTo(boolean open, float velocityPerSecond) {
        if (spring != null) {
            spring.cancel();
        }
        spring = new SpringAnimation(this, PROGRESS)
                .setSpring(new SpringForce(open ? 100f : 0f)
                        .setStiffness(900f)
                        .setDampingRatio(SpringForce.DAMPING_RATIO_NO_BOUNCY));
        if (velocityPerSecond != 0) {
            spring.setStartVelocity(velocityPerSecond / Math.max(1, panelWidth()) * 100f);
        }
        spring.start();
    }

    public void open() {
        springTo(true, 0);
    }

    public void close() {
        springTo(false, 0);
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        if (progress > 0.001f) {
            scrimPaint.setColor(Color.BLACK);
            scrimPaint.setAlpha((int) (SCRIM_ALPHA * progress));
            canvas.drawRect(0, 0, getMeasuredWidth(), getMeasuredHeight(), scrimPaint);
        }
        super.dispatchDraw(canvas);
    }

    @Override
    protected boolean drawChild(Canvas canvas, View child, long drawingTime) {
        if (child != panel) {
            return super.drawChild(canvas, child, drawingTime);
        }

        final float r = com.th3nekit.finegram.core.configs.FinegramAppearanceConfig.INSTANCE.getSideDrawerRounded()
                ? AndroidUtilities.dp(CORNER_DP) : 0f;
        final boolean rtl = LocaleController.isRTL;
        corners[0] = corners[1] = rtl ? r : 0f;
        corners[2] = corners[3] = rtl ? 0f : r;
        corners[4] = corners[5] = rtl ? 0f : r;
        corners[6] = corners[7] = rtl ? r : 0f;
        clipRect.set(panel.getLeft() + panel.getTranslationX(), panel.getTop(),
                panel.getRight() + panel.getTranslationX(), panel.getBottom());
        clipPath.rewind();
        clipPath.addRoundRect(clipRect, corners, Path.Direction.CW);
        canvas.save();
        canvas.clipPath(clipPath);
        final boolean result = super.drawChild(canvas, child, drawingTime);
        canvas.restore();

        edgePaint.setColor(androidx.core.graphics.ColorUtils.setAlphaComponent(
                Theme.getColor(Theme.key_windowBackgroundWhiteBlackText), 30));
        edgePaint.setStrokeWidth(Math.max(1, AndroidUtilities.dp(0.66f)));
        edgePath.rewind();
        final float edgeX = rtl ? clipRect.left : clipRect.right;
        edgePath.moveTo(edgeX, clipRect.top + r);
        edgePath.lineTo(edgeX, clipRect.bottom - r);
        canvas.drawPath(edgePath, edgePaint);
        return result;
    }

    private final Paint edgePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path edgePath = new Path();

    {
        edgePaint.setStyle(Paint.Style.STROKE);
    }

    public void beginSwipe() {
        if (spring != null) {
            spring.cancel();
        }
        swipeStartProgress = progress;
    }

    public void swipeBy(float dx) {
        final float width = Math.max(1, panelWidth());
        setProgress(swipeStartProgress + (LocaleController.isRTL ? -dx : dx) / width);
    }

    public void endSwipe(float velocityX) {
        final float velocity = LocaleController.isRTL ? -velocityX : velocityX;
        final boolean open = Math.abs(velocity) > AndroidUtilities.dp(400)
                ? velocity > 0
                : progress > 0.35f;
        springTo(open, velocity);
    }

    private float swipeStartProgress;

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            startX = event.getX();
            startY = event.getY();
            startProgress = progress;
            dragging = false;
            return false;
        }
        if (action != MotionEvent.ACTION_MOVE) {
            return false;
        }
        final float dx = event.getX() - startX;
        final float dy = event.getY() - startY;

        return Math.abs(dx) > touchSlop && Math.abs(dx) > Math.abs(dy);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        if (tracker == null) {
            tracker = VelocityTracker.obtain();
        }
        tracker.addMovement(event);

        final int width = Math.max(1, panelWidth());
        final boolean rtl = LocaleController.isRTL;

        switch (action) {
            case MotionEvent.ACTION_DOWN:
                startX = event.getX();
                startY = event.getY();
                startProgress = progress;
                dragging = false;
                return progress > 0.001f;
            case MotionEvent.ACTION_MOVE: {
                final float dx = event.getX() - startX;
                if (!dragging && Math.abs(dx) > touchSlop) {
                    dragging = true;
                    if (spring != null) {
                        spring.cancel();
                    }
                }
                if (dragging) {
                    setProgress(startProgress + (rtl ? -dx : dx) / width);
                }
                return true;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL: {
                tracker.computeCurrentVelocity(1000);
                final float velocity = rtl ? -tracker.getXVelocity() : tracker.getXVelocity();
                tracker.recycle();
                tracker = null;
                if (!dragging) {

                    final boolean onPanel = rtl
                            ? startX > getMeasuredWidth() - width
                            : startX < width;
                    if (action == MotionEvent.ACTION_UP && progress > 0.5f && !onPanel) {
                        close();
                    }
                    return true;
                }
                dragging = false;

                final boolean open = Math.abs(velocity) > AndroidUtilities.dp(400)
                        ? velocity > 0
                        : progress > 0.5f;
                springTo(open, velocity);
                return true;
            }
            default:
                return true;
        }
    }
}

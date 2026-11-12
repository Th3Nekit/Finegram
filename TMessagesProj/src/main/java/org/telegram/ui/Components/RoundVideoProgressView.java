package org.telegram.ui.Components;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.widget.FrameLayout;

import org.telegram.messenger.AndroidUtilities;

public final class RoundVideoProgressView extends FrameLayout {
    private final Paint backgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint progressPaint = new Paint(Paint.ANTI_ALIAS_FLAG) {
        @Override
        public void setAlpha(int alpha) {
            super.setAlpha(alpha);
            backgroundPaint.setAlpha(Math.round(alpha * 0.20f));
            invalidate();
        }
    };
    private final RectF arcBounds = new RectF();
    private final float strokeWidth;
    private final float ringInset;

    private float progress;

    public RoundVideoProgressView(Context context) {
        super(context);
        strokeWidth = AndroidUtilities.dp(3);
        ringInset = AndroidUtilities.dp(5) + strokeWidth * 0.5f;
        configureStroke(backgroundPaint, 0.20f);
        configureStroke(progressPaint, 1f);
        setWillNotDraw(false);
    }

    public Paint getPaint() {
        return progressPaint;
    }

    public void setProgress(float progress) {
        float value = clamp(progress, 0f, 1f);
        if (this.progress == value) return;
        this.progress = value;
        invalidate();
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        super.dispatchDraw(canvas);
        arcBounds.set(
                ringInset,
                ringInset,
                getWidth() - ringInset,
                getHeight() - ringInset
        );
        canvas.drawOval(arcBounds, backgroundPaint);
        canvas.drawArc(arcBounds, -90f, progress * 360f, false, progressPaint);
    }

    private static void configureStroke(Paint paint, float alpha) {
        paint.setColor(Color.WHITE);
        paint.setAlpha(Math.round(255f * alpha));
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(AndroidUtilities.dp(3));
        paint.setStrokeCap(Paint.Cap.ROUND);
    }

    private static float clamp(float value, float min, float max) {
        return Math.max(min, Math.min(max, value));
    }
}

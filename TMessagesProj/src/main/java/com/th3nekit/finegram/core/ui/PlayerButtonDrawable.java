/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.ui;

import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;

import androidx.annotation.NonNull;
import androidx.core.graphics.ColorUtils;

public final class PlayerButtonDrawable extends Drawable {

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF rect = new RectF();
    private int color;
    private int pressedColor;
    private int inset;
    private float corner = 1f;
    private float cornerTarget = 1f;
    private float fill = 1f;
    private float fillTarget = 1f;
    private float press;
    private boolean pressed;
    private long lastFrame;

    public PlayerButtonDrawable(int color) {
        this.color = color;
        pressedColor = ColorUtils.calculateLuminance(color) > .5 ? 0x14000000 : 0x1fffffff;
    }

    public void setColor(int color) {
        if (this.color != color) {
            this.color = color;
            pressedColor = ColorUtils.calculateLuminance(color) > .5 ? 0x14000000 : 0x1fffffff;
            invalidateSelf();
        }
    }

    public void setInset(int inset) {
        if (this.inset != inset) {
            this.inset = inset;
            invalidateSelf();
        }
    }

    public void setCorner(float fraction, boolean animated) {
        cornerTarget = Math.max(0f, Math.min(1f, fraction));
        if (!animated) corner = cornerTarget;
        invalidateSelf();
    }

    public void setFilled(boolean filled, boolean animated) {
        fillTarget = filled ? 1f : 0f;
        if (!animated) fill = fillTarget;
        invalidateSelf();
    }

    @Override
    public boolean isStateful() {
        return true;
    }

    @Override
    protected boolean onStateChange(@NonNull int[] state) {
        boolean now = false;
        for (int s : state) {
            if (s == android.R.attr.state_pressed) {
                now = true;
                break;
            }
        }
        if (now != pressed) {
            pressed = now;
            invalidateSelf();
            return true;
        }
        return false;
    }

    @Override
    public void draw(@NonNull Canvas canvas) {
        long now = SystemClock.uptimeMillis();
        float dt = lastFrame == 0 ? 16f : Math.min(48f, now - lastFrame);
        lastFrame = now;
        float k = 1f - (float) Math.exp(-dt / 70f);
        boolean animating = false;
        if (Math.abs(corner - cornerTarget) > .002f) {
            corner += (cornerTarget - corner) * k;
            animating = true;
        } else {
            corner = cornerTarget;
        }
        if (Math.abs(fill - fillTarget) > .004f) {
            fill += (fillTarget - fill) * k;
            animating = true;
        } else {
            fill = fillTarget;
        }
        float pressTarget = pressed ? 1f : 0f;
        if (Math.abs(press - pressTarget) > .01f) {
            press += (pressTarget - press) * Math.min(1f, k * 1.6f);
            animating = true;
        } else {
            press = pressTarget;
        }
        Rect b = getBounds();
        rect.set(b.left + inset, b.top + inset, b.right - inset, b.bottom - inset);
        float radius = Math.min(rect.width(), rect.height()) / 2f * (.36f + .64f * corner);
        if (fill > 0f) {
            paint.setColor(color);
            paint.setAlpha(Math.round(android.graphics.Color.alpha(color) * fill));
            canvas.drawRoundRect(rect, radius, radius, paint);
        }
        if (press > 0f) {
            paint.setColor(pressedColor);
            paint.setAlpha(Math.round(android.graphics.Color.alpha(pressedColor) * press));
            canvas.drawRoundRect(rect, radius, radius, paint);
        }
        if (animating) {
            invalidateSelf();
        } else {
            lastFrame = 0;
        }
    }

    @Override
    public void setAlpha(int alpha) {
    }

    @Override
    public void setColorFilter(ColorFilter colorFilter) {
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }
}

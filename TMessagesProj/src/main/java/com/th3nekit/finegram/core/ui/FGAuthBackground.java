/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.ui;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.RadialGradient;
import android.graphics.Shader;
import android.provider.Settings;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.core.graphics.ColorUtils;

import org.telegram.ui.ActionBar.Theme;

public class FGAuthBackground extends View {

    private static final long CYCLE_MS = 23000;
    private static final long FRAME_MS = 50;

    private static final float ALPHA_LIGHT = 0.10f;
    private static final float ALPHA_DARK = 0.17f;

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Matrix shift = new Matrix();
    private final Blob[] blobs = new Blob[3];
    private final boolean animated;

    private long startedAt;

    private static final class Blob {
        final int color;
        final float radiusFactor;
        final float centerX;
        final float centerY;
        final float driftX;
        final float driftY;
        final float speed;
        final float phase;

        RadialGradient shader;
        float shaderRadius;

        Blob(int color, float radiusFactor, float centerX, float centerY,
             float driftX, float driftY, float speed, float phase) {
            this.color = color;
            this.radiusFactor = radiusFactor;
            this.centerX = centerX;
            this.centerY = centerY;
            this.driftX = driftX;
            this.driftY = driftY;
            this.speed = speed;
            this.phase = phase;
        }
    }

    public FGAuthBackground(Context context) {
        super(context);
        setWillNotDraw(false);
        animated = systemAnimationsOn(context);

        final boolean dark = Theme.isCurrentThemeDark();
        final int alpha = Math.round(255 * (dark ? ALPHA_DARK : ALPHA_LIGHT));
        final int first = Theme.getColor(Theme.key_featuredStickers_addButton);
        final int second = Theme.getColor(Theme.key_chats_actionBackground);
        final int third = Theme.getColor(Theme.key_windowBackgroundWhiteBlueText);

        blobs[0] = new Blob(ColorUtils.setAlphaComponent(first, alpha),
                0.85f, 0.18f, 0.16f, 0.10f, 0.06f, 1.0f, 0f);
        blobs[1] = new Blob(ColorUtils.setAlphaComponent(second, alpha),
                0.70f, 0.86f, 0.30f, 0.08f, 0.09f, 0.73f, 2.1f);
        blobs[2] = new Blob(ColorUtils.setAlphaComponent(third, alpha),
                0.95f, 0.50f, 0.92f, 0.12f, 0.05f, 0.55f, 4.2f);
    }

    private static boolean systemAnimationsOn(Context context) {
        try {
            return Settings.Global.getFloat(context.getContentResolver(),
                    Settings.Global.ANIMATOR_DURATION_SCALE, 1f) > 0f;
        } catch (Throwable ignore) {
            return true;
        }
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        startedAt = System.currentTimeMillis();
        if (animated) {
            postInvalidateDelayed(FRAME_MS);
        }
    }

    @Override
    protected void onDraw(@NonNull Canvas canvas) {
        final int width = getMeasuredWidth();
        final int height = getMeasuredHeight();
        if (width == 0 || height == 0) {
            return;
        }

        final float time = animated
                ? ((System.currentTimeMillis() - startedAt) % CYCLE_MS) / (float) CYCLE_MS
                : 0.25f;
        final float full = (float) (Math.PI * 2);
        final float side = Math.max(width, height);

        for (Blob blob : blobs) {
            final float angle = full * time * blob.speed + blob.phase;
            final float cx = (blob.centerX + blob.driftX * (float) Math.cos(angle)) * width;
            final float cy = (blob.centerY + blob.driftY * (float) Math.sin(angle)) * height;
            final float radius = side * blob.radiusFactor;

            if (blob.shader == null || blob.shaderRadius != radius) {
                blob.shaderRadius = radius;
                blob.shader = new RadialGradient(0, 0, radius,
                        blob.color, Color.TRANSPARENT, Shader.TileMode.CLAMP);
            }
            shift.setTranslate(cx, cy);
            blob.shader.setLocalMatrix(shift);
            paint.setShader(blob.shader);
            canvas.drawCircle(cx, cy, radius, paint);
        }

        if (animated && isAttachedToWindow()) {
            postInvalidateDelayed(FRAME_MS);
        }
    }
}

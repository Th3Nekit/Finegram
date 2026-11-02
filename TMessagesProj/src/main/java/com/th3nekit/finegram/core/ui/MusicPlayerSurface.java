package com.th3nekit.finegram.core.ui;

import android.animation.ValueAnimator;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;
import android.os.Build;
import android.view.View;

import androidx.core.graphics.ColorUtils;

import org.telegram.messenger.ImageReceiver;
import org.telegram.messenger.LiteMode;
import org.telegram.messenger.SharedConfig;
import org.telegram.messenger.Utilities;
import org.telegram.ui.Components.CubicBezierInterpolator;

import java.lang.ref.WeakReference;

public final class MusicPlayerSurface {
    private final View owner;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final RectF bounds = new RectF();
    private Bitmap artwork;
    private Bitmap previousArtwork;
    private ValueAnimator transition;
    private float artworkProgress = 1f;
    private WeakReference<Bitmap> source;
    private int generation;
    private int orientation;
    private LinearGradient panel;
    private int panelBase;
    private int panelAccent;
    private float panelTop;
    private float panelBottom;
    private LinearGradient artworkFade;
    private int fadeBase;
    private float fadeHeight;

    public MusicPlayerSurface(View owner) {
        this.owner = owner;
    }

    public void update(ImageReceiver receiver) {
        ImageReceiver.BitmapHolder holder = receiver.getBitmapSafe();
        if (holder == null) return;
        Bitmap small = null;
        Bitmap readable = null;
        Bitmap blended = null;
        try {
            Bitmap bitmap = holder.bitmap;
            if (bitmap == null || bitmap.isRecycled()) return;
            if (source != null && source.get() == bitmap && generation == bitmap.getGenerationId()
                    && orientation == holder.orientation) return;
            Bitmap input = bitmap;
            if (Build.VERSION.SDK_INT >= 26 && bitmap.getConfig() == Bitmap.Config.HARDWARE) {
                readable = bitmap.copy(Bitmap.Config.ARGB_8888, false);
                if (readable == null) return;
                input = readable;
            }
            small = Bitmap.createBitmap(48, 48, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(small);
            float scale = Math.max(48f / input.getWidth(), 48f / input.getHeight());
            float width = input.getWidth() * scale;
            float height = input.getHeight() * scale;
            canvas.rotate(holder.orientation, 24, 24);
            bounds.set((48 - width) / 2, (48 - height) / 2, (48 + width) / 2, (48 + height) / 2);
            paint.setAlpha(255);
            paint.setShader(null);
            canvas.drawBitmap(input, null, bounds, paint);
            Utilities.stackBlurBitmap(small, 10);
            Bitmap start = artwork;
            if (previousArtwork != null && artworkProgress < 1f) {
                blended = Bitmap.createBitmap(48, 48, Bitmap.Config.ARGB_8888);
                Canvas blendCanvas = new Canvas(blended);
                bounds.set(0, 0, 48, 48);
                paint.setAlpha(255);
                blendCanvas.drawBitmap(previousArtwork, null, bounds, paint);
                paint.setAlpha(Math.round(255 * artworkProgress));
                blendCanvas.drawBitmap(artwork, null, bounds, paint);
                start = blended;
            }
            cancelTransition();
            if (start != artwork && artwork != null) artwork.recycle();
            previousArtwork = start;
            blended = null;
            artwork = small;
            small = null;
            source = new WeakReference<>(bitmap);
            generation = bitmap.getGenerationId();
            orientation = holder.orientation;
            if (owner.isAttachedToWindow() && SharedConfig.animationsEnabled() && !LiteMode.isPowerSaverApplied()
                    && (Build.VERSION.SDK_INT < 26 || ValueAnimator.areAnimatorsEnabled())) {
                artworkProgress = 0f;
                transition = ValueAnimator.ofFloat(0f, 1f);
                transition.setDuration(200);
                transition.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
                transition.addUpdateListener(animator -> {
                    artworkProgress = (float) animator.getAnimatedValue();
                    if (artworkProgress == 1f) releasePreviousArtwork();
                    owner.invalidate();
                });
                transition.start();
            } else {
                artworkProgress = 1f;
                releasePreviousArtwork();
            }
            owner.invalidate();
        } catch (RuntimeException | OutOfMemoryError ignored) {
            clear();
        } finally {
            if (small != null) small.recycle();
            if (readable != null) readable.recycle();
            if (blended != null) blended.recycle();
            holder.release();
        }
    }

    public void draw(Canvas canvas, MusicPlayerLayout layout, float density, int base, int accent,
                     int extraQueueHeight) {
        drawBackground(canvas, owner.getWidth(), owner.getHeight(), density, base, layout != null);
        drawPanel(canvas, layout, owner.getWidth(), density, base, accent, extraQueueHeight);
    }

    public void drawBackground(Canvas canvas, float width, float height, float density, int base, boolean ambient) {
        canvas.drawColor(base);
        if (ambient && artwork != null && !artwork.isRecycled()) {
            bounds.set(-width * .16f, -70 * density, width * 1.16f, height + 70 * density);
            paint.setShader(null);
            int alpha = ColorUtils.calculateLuminance(base) > .5 ? 72 : 108;
            if (previousArtwork != null && !previousArtwork.isRecycled()) {
                int layer = canvas.saveLayerAlpha(bounds, alpha);
                paint.setAlpha(255);
                canvas.drawBitmap(previousArtwork, null, bounds, paint);
                paint.setAlpha(Math.round(255 * artworkProgress));
                canvas.drawBitmap(artwork, null, bounds, paint);
                canvas.restoreToCount(layer);
            } else {
                paint.setAlpha(Math.round(alpha * artworkProgress));
                canvas.drawBitmap(artwork, null, bounds, paint);
            }
            if (artworkFade == null || fadeBase != base || fadeHeight != height) {
                artworkFade = new LinearGradient(0, 0, 0, Math.max(1, height),
                        new int[] {ColorUtils.setAlphaComponent(base, 80), ColorUtils.setAlphaComponent(base, 0), base},
                        new float[] {0f, .35f, 1f}, Shader.TileMode.CLAMP);
                fadeBase = base;
                fadeHeight = height;
            }
            bounds.set(0, 0, width, height);
            paint.setAlpha(255);
            paint.setShader(artworkFade);
            canvas.drawRect(bounds, paint);
            paint.setShader(null);
        }
    }

    public void drawPanel(Canvas canvas, MusicPlayerLayout layout, float width, float density, int base, int accent,
                          int extraQueueHeight) {
        if (layout == null) return;
        float top = layout.panelTop * density;
        float bottom = (layout.panelBottom + extraQueueHeight) * density;
        if (panel == null || panelBase != base || panelAccent != accent || panelTop != top || panelBottom != bottom) {
            int surface = ColorUtils.blendARGB(base, accent, .07f);
            panel = new LinearGradient(0, top, 0, bottom,
                    ColorUtils.setAlphaComponent(surface, 224), ColorUtils.setAlphaComponent(base, 244), Shader.TileMode.CLAMP);
            panelBase = base;
            panelAccent = accent;
            panelTop = top;
            panelBottom = bottom;
        }
        float side = MusicPlayerLayout.panelSideInset(Math.round(width / density)) * density;
        bounds.set(side, top, width - side, bottom);
        paint.setAlpha(255);
        paint.setShader(panel);
        canvas.drawRoundRect(bounds, 28 * density, 28 * density, paint);
        paint.setShader(null);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(density);
        paint.setColor(ColorUtils.setAlphaComponent(ColorUtils.calculateLuminance(base) > .5 ? Color.BLACK : Color.WHITE, 28));
        bounds.inset(density / 2, density / 2);
        canvas.drawRoundRect(bounds, 28 * density, 28 * density, paint);
        paint.setStyle(Paint.Style.FILL);
    }

    public void clear() {
        cancelTransition();
        if (artwork != null) {
            artwork.recycle();
            artwork = null;
            owner.invalidate();
        }
        source = null;
    }

    private void cancelTransition() {
        if (transition != null) {
            transition.cancel();
            transition = null;
        }
        releasePreviousArtwork();
    }

    private void releasePreviousArtwork() {
        if (previousArtwork != null) {
            previousArtwork.recycle();
            previousArtwork = null;
        }
    }
}

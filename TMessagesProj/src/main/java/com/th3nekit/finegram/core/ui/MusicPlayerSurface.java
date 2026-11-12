package com.th3nekit.finegram.core.ui;

import android.animation.ValueAnimator;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;
import android.os.Build;
import android.os.SystemClock;
import android.os.PowerManager;
import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import androidx.core.graphics.ColorUtils;

import org.telegram.messenger.ImageReceiver;
import org.telegram.messenger.AndroidUtilities;
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
    private Bitmap coverGlow;
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
    private final Matrix matrix = new Matrix();
    private final RectF glowRect = new RectF();
    private int accent;
    private boolean live;
    private boolean liveRunning;
    private long liveElapsed;
    private long liveFrame;
    private int artworkRequest;
    private Runnable onArtworkReady;
    private final Rect visibleRect = new Rect();
    private final PowerManager powerManager;

    public MusicPlayerSurface(View owner) {
        this.owner = owner;
        powerManager = (PowerManager) owner.getContext().getSystemService(Context.POWER_SERVICE);
    }

    public void setOnArtworkReady(Runnable listener) {
        onArtworkReady = listener;
    }

    public static final class Artwork {
        public final Bitmap bitmap;
        public final int accent;

        private Artwork(Bitmap bitmap, int accent) {
            this.bitmap = bitmap;
            this.accent = accent;
        }
    }

    public static Artwork prepareArtwork(Bitmap bitmap, int orientation) {
        Bitmap readable = null;
        Bitmap small = null;
        try {
            if (bitmap == null || bitmap.isRecycled()) return null;
            Bitmap input = bitmap;
            if (Build.VERSION.SDK_INT >= 26 && bitmap.getConfig() == Bitmap.Config.HARDWARE) {
                readable = bitmap.copy(Bitmap.Config.ARGB_8888, false);
                if (readable == null) return null;
                input = readable;
            }
            small = Bitmap.createBitmap(48, 48, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(small);
            Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
            float scale = Math.max(48f / input.getWidth(), 48f / input.getHeight());
            float width = input.getWidth() * scale;
            float height = input.getHeight() * scale;
            canvas.rotate(orientation, 24, 24);
            canvas.drawBitmap(input, null, new RectF((48 - width) / 2, (48 - height) / 2,
                    (48 + width) / 2, (48 + height) / 2), paint);
            int accent = pickAccent(small);
            Utilities.stackBlurBitmap(small, 10);
            Artwork result = new Artwork(small, accent);
            small = null;
            return result;
        } catch (RuntimeException | OutOfMemoryError ignored) {
            return null;
        } finally {
            if (small != null) small.recycle();
            if (readable != null) readable.recycle();
        }
    }

    private static Bitmap prepareCoverGlow(Bitmap bitmap) {
        Bitmap glow = null;
        try {
            glow = bitmap.copy(Bitmap.Config.ARGB_8888, true);
            if (glow == null) return null;
            for (int y = 0; y < 48; y++) {
                float fy = Math.min(1f, Math.min(y, 47 - y) / 8f);
                fy = fy * fy * (3f - 2f * fy);
                for (int x = 0; x < 48; x++) {
                    float fx = Math.min(1f, Math.min(x, 47 - x) / 8f);
                    fx = fx * fx * (3f - 2f * fx);
                    int color = glow.getPixel(x, y);
                    glow.setPixel(x, y, ColorUtils.setAlphaComponent(color, Math.round(Color.alpha(color) * fx * fy)));
                }
            }
            return glow;
        } catch (RuntimeException | OutOfMemoryError ignored) {
            if (glow != null) glow.recycle();
            return null;
        }
    }

    public void update(ImageReceiver receiver) {
        if (receiver.getAnimation() != null || receiver.getLottieAnimation() != null) return;
        ImageReceiver.BitmapHolder holder = receiver.getBitmapSafe();
        if (holder == null) return;
        Bitmap bitmap = holder.bitmap;
        if (bitmap == null || bitmap.isRecycled() || source != null && source.get() == bitmap
                && generation == bitmap.getGenerationId() && orientation == holder.orientation) {
            holder.release();
            return;
        }
        source = new WeakReference<>(bitmap);
        generation = bitmap.getGenerationId();
        orientation = holder.orientation;
        int request = ++artworkRequest;
        Utilities.globalQueue.postRunnable(() -> {
            Artwork result = prepareArtwork(bitmap, holder.orientation);
            Bitmap glow = result == null ? null : prepareCoverGlow(result.bitmap);
            AndroidUtilities.runOnUIThread(() -> {
                holder.release();
                if (request != artworkRequest) {
                    if (result != null) result.bitmap.recycle();
                    if (glow != null) glow.recycle();
                    return;
                }
                if (result == null) {
                    source = null;
                    return;
                }
                cancelTransition();
                previousArtwork = artwork;
                artwork = result.bitmap;
                if (coverGlow != null) coverGlow.recycle();
                coverGlow = glow;
                accent = result.accent;
                artworkProgress = canAnimate() && previousArtwork != null ? 0f : 1f;
                if (artworkProgress == 0f) {
                    transition = ValueAnimator.ofFloat(0f, 1f);
                    transition.setDuration(200);
                    transition.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
                    transition.addUpdateListener(animator -> {
                        artworkProgress = canAnimate() ? (float) animator.getAnimatedValue() : 1f;
                        if (artworkProgress == 1f) {
                            releasePreviousArtwork();
                            if (!canAnimate()) animator.cancel();
                        }
                        owner.invalidate();
                    });
                    transition.start();
                } else {
                    releasePreviousArtwork();
                }
                if (onArtworkReady != null) onArtworkReady.run();
                owner.invalidate();
            });
        });
    }

    public boolean canAnimate() {
        if (!owner.isAttachedToWindow() || !owner.isShown() || !owner.hasWindowFocus()
                || owner.getWindowVisibility() != View.VISIBLE || !SharedConfig.animationsEnabled()
                || LiteMode.isPowerSaverApplied() || powerManager != null && powerManager.isPowerSaveMode()
                || Build.VERSION.SDK_INT >= 26 && !ValueAnimator.areAnimatorsEnabled()) return false;
        float alpha = 1f;
        for (View view = owner; view != null; view = view.getParent() instanceof View ? (View) view.getParent() : null) {
            alpha *= view.getAlpha();
            if (alpha <= .05f) return false;
        }
        return owner.getGlobalVisibleRect(visibleRect) && !visibleRect.isEmpty();
    }

    public void draw(Canvas canvas, MusicPlayerLayout layout, float density, int base, int accent,
                     int extraQueueHeight) {
        drawBackground(canvas, owner.getWidth(), owner.getHeight(), density, base, layout != null);
        drawPanel(canvas, layout, owner.getWidth(), density, base, accent, extraQueueHeight);
    }

    public int getAccent() {
        return accent;
    }

    public void setLive(boolean live, boolean running) {
        if (this.live == live && liveRunning == running) return;
        this.live = live;
        liveRunning = running;
        liveFrame = 0;
        owner.invalidate();
    }

    private static int pickAccent(Bitmap bitmap) {
        float[] hsv = new float[3];
        float bestScore = -1f;
        int best = 0;
        int width = bitmap.getWidth(), height = bitmap.getHeight();
        long r = 0, g = 0, b = 0, weight = 0;
        for (int y = 0; y < height; y += 2) {
            for (int x = 0; x < width; x += 2) {
                int c = bitmap.getPixel(x, y);
                if (Color.alpha(c) < 200) continue;
                Color.colorToHSV(c, hsv);
                float score = hsv[1] * (1f - Math.abs(hsv[2] - .72f));
                if (score > bestScore) {
                    bestScore = score;
                    best = c;
                }
                if (hsv[1] > .25f && hsv[2] > .25f) {
                    int w = Math.round(hsv[1] * 100);
                    r += (long) Color.red(c) * w;
                    g += (long) Color.green(c) * w;
                    b += (long) Color.blue(c) * w;
                    weight += w;
                }
            }
        }
        int color = weight > 0 ? Color.rgb((int) (r / weight), (int) (g / weight), (int) (b / weight)) : best;
        if (color == 0) return 0;
        Color.colorToHSV(color, hsv);
        if (hsv[1] < .12f) return 0;
        hsv[1] = Math.max(hsv[1], .45f);
        hsv[2] = Math.max(.62f, Math.min(.92f, hsv[2] + .12f));
        return Color.HSVToColor(hsv);
    }

    public void drawCoverGlow(Canvas canvas, float left, float top, float right, float bottom, float density, float alpha) {
        if (coverGlow == null || coverGlow.isRecycled() || alpha <= 0f) return;
        float spread = 26 * density;
        glowRect.set(left - spread, top - spread * .4f, right + spread, bottom + spread * 1.6f);
        paint.setShader(null);
        paint.setAlpha(Math.round(150 * alpha * artworkProgress));
        canvas.drawBitmap(coverGlow, null, glowRect, paint);
        paint.setAlpha(255);
    }

    public void drawBackground(Canvas canvas, float width, float height, float density, int base, boolean ambient) {
        canvas.drawColor(base);
        if (ambient && live && artwork != null && !artwork.isRecycled()) {
            drawLive(canvas, width, height, base);
            return;
        }
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

    private void drawLive(Canvas canvas, float width, float height, int base) {
        long now = SystemClock.uptimeMillis();
        boolean moving = liveRunning && canAnimate();
        if (moving && liveFrame != 0) liveElapsed += Math.min(64, now - liveFrame);
        liveFrame = moving ? now : 0;
        float t = liveElapsed / 1000f;
        boolean light = ColorUtils.calculateLuminance(base) > .5;
        int alpha = light ? 96 : 150;
        float side = Math.max(width, height);
        paint.setShader(null);
        for (int i = 0; i < 2; i++) {
            float phase = i == 0 ? 0f : 2.1f;
            float angle = (float) Math.sin(t / (i == 0 ? 9f : 12f) + phase) * (i == 0 ? 14f : -18f);
            float scale = 1.25f + .12f * (float) Math.sin(t / (i == 0 ? 6.5f : 8.5f) + phase);
            float dx = width * .14f * (float) Math.sin(t / (i == 0 ? 7f : 10f) + phase);
            float dy = height * .08f * (float) Math.cos(t / (i == 0 ? 8f : 11f) + phase);
            matrix.reset();
            matrix.postScale(side * scale / artwork.getWidth(), side * scale / artwork.getHeight());
            matrix.postTranslate(-side * scale / 2f, -side * scale / 2f);
            matrix.postRotate(angle + (i == 0 ? 0 : 180));
            matrix.postTranslate(width / 2f + dx, height * (i == 0 ? .38f : .62f) + dy);
            paint.setAlpha(Math.round((i == 0 ? alpha : alpha * .55f) * artworkProgress));
            canvas.drawBitmap(artwork, matrix, paint);
        }
        if (artworkFade == null || fadeBase != base || fadeHeight != height) {
            artworkFade = new LinearGradient(0, 0, 0, Math.max(1, height),
                    new int[] {ColorUtils.setAlphaComponent(base, 80), ColorUtils.setAlphaComponent(base, 0), base},
                    new float[] {0f, .35f, 1f}, Shader.TileMode.CLAMP);
            fadeBase = base;
            fadeHeight = height;
        }
        bounds.set(0, 0, width, height);
        paint.setAlpha(200);
        paint.setShader(artworkFade);
        canvas.drawRect(bounds, paint);
        paint.setShader(null);
        paint.setAlpha(255);
        if (moving) {
            owner.postInvalidateDelayed(33);
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
        artworkRequest++;
        liveFrame = 0;
        accent = 0;
        cancelTransition();
        if (artwork != null) {
            artwork.recycle();
            artwork = null;
            owner.invalidate();
        }
        if (coverGlow != null) {
            coverGlow.recycle();
            coverGlow = null;
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

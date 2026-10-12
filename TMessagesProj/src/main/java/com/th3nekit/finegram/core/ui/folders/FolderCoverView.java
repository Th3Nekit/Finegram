package com.th3nekit.finegram.core.ui.folders;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.LinearGradient;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Shader;
import android.graphics.Path;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.view.View;

import androidx.core.graphics.ColorUtils;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.SharedConfig;
import org.telegram.messenger.Utilities;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.CubicBezierInterpolator;

public final class FolderCoverView extends View {
    private final Theme.ResourcesProvider resourcesProvider;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final Paint fadePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF bounds = new RectF();
    private final Rect source = new Rect();
    private final Path path = new Path();
    private FolderCoverStore.Cover current;
    private FolderCoverStore.Cover previous;
    private Bitmap image;
    private Bitmap previousImage;
    private float progress = 1;
    private ValueAnimator animator;
    private int generation;
    private boolean attached;

    public FolderCoverView(Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;
        fadePaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.DST_IN));
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
    }

    public void setFolder(int account, int folder, boolean animated) {
        FolderCoverStore.Cover cover = FolderCoverStore.get(account, folder);
        if (cover.sameAs(current)) return;
        if (animator != null) animator.cancel();
        previous = current;
        previousImage = image;
        current = cover;
        image = null;
        generation++;
        loadImage();
        if (animated && attached && previous != null && SharedConfig.animationsEnabled()
                && (Build.VERSION.SDK_INT < 26 || ValueAnimator.areAnimatorsEnabled())) {
            progress = 0;
            animator = ValueAnimator.ofFloat(0, 1);
            animator.setDuration(250);
            animator.setInterpolator(CubicBezierInterpolator.EASE_OUT);
            animator.addUpdateListener(a -> {
                progress = (float) a.getAnimatedValue();
                if (progress == 1) {
                    previous = null;
                    previousImage = null;
                }
                invalidate();
            });
            animator.start();
        } else {
            progress = 1;
            previous = null;
            previousImage = null;
        }
        invalidate();
    }

    private void loadImage() {
        if (!attached || current == null || current.style != FolderCoverStore.IMAGE || current.image.isEmpty()) return;
        int token = generation;
        String file = current.image;
        Utilities.globalQueue.postRunnable(() -> {
            Bitmap bitmap = null;
            try {
                bitmap = BitmapFactory.decodeFile(file);
            } catch (Exception | OutOfMemoryError ignored) {}
            Bitmap result = bitmap;
            AndroidUtilities.runOnUIThread(() -> {
                if (attached && token == generation) {
                    image = result;
                    invalidate();
                } else if (result != null) {
                    result.recycle();
                }
            });
        });
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        attached = true;
        if (image == null) loadImage();
    }

    @Override
    protected void onDetachedFromWindow() {
        attached = false;
        generation++;
        if (animator != null) animator.cancel();
        progress = 1;
        previous = null;
        previousImage = null;
        image = null;
        super.onDetachedFromWindow();
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        bounds.set(0, 0, width, height);
        if (height > 0) {
            fadePaint.setShader(new LinearGradient(0, 0, 0, height,
                    new int[]{0xCCFFFFFF, 0xB3FFFFFF, 0x00FFFFFF},
                    new float[]{0f, .45f, 1f}, Shader.TileMode.CLAMP));
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (bounds.width() <= 0 || bounds.height() <= 0) return;
        int layer = canvas.saveLayer(bounds, null);
        drawCover(canvas, current, image, 1);
        if (previous != null && progress < 1) drawCover(canvas, previous, previousImage, 1 - progress);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(ColorUtils.setAlphaComponent(
                Theme.getColor(Theme.key_actionBarDefault, resourcesProvider), 96));
        canvas.drawRect(bounds, paint);
        canvas.drawRect(bounds, fadePaint);
        canvas.restoreToCount(layer);
    }

    private void drawCover(Canvas canvas, FolderCoverStore.Cover cover, Bitmap bitmap, float alpha) {
        if (cover == null || alpha <= 0) return;
        int layer = canvas.saveLayerAlpha(bounds, Math.round(alpha * 255));
        int base = Theme.getColor(Theme.key_windowBackgroundWhite, resourcesProvider);
        int accent = Theme.getColor(Theme.key_windowBackgroundWhiteBlueText, resourcesProvider);
        int hue = Color.HSVToColor(new float[]{Math.floorMod(cover.seed * 47 + 198, 360), .46f, .85f});
        int ink = ColorUtils.blendARGB(accent, hue, .55f);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(ColorUtils.blendARGB(base, ink, .16f));
        canvas.drawRect(bounds, paint);
        if (cover.style == FolderCoverStore.IMAGE && bitmap != null && !bitmap.isRecycled()) {
            float scale = Math.max(bounds.width() / bitmap.getWidth(), bounds.height() / bitmap.getHeight());
            int w = Math.max(1, Math.min(bitmap.getWidth(), Math.round(bounds.width() / scale)));
            int h = Math.max(1, Math.min(bitmap.getHeight(), Math.round(bounds.height() / scale)));
            source.set((bitmap.getWidth() - w) / 2, (bitmap.getHeight() - h) / 2, (bitmap.getWidth() + w) / 2, (bitmap.getHeight() + h) / 2);
            canvas.drawBitmap(bitmap, source, bounds, paint);
        } else {
            canvas.save();
            canvas.translate(bounds.left, bounds.top);
            canvas.scale(bounds.width() / 320f, bounds.height() / 64f);
            int style = cover.style == FolderCoverStore.AUTO || cover.style == FolderCoverStore.IMAGE
                    ? (Math.floorMod(cover.seed, 2) == 0 ? FolderCoverStore.GEOMETRY : FolderCoverStore.LANDSCAPE) : cover.style;
            if (style == FolderCoverStore.LANDSCAPE) {
                paint.setColor(ColorUtils.blendARGB(base, ink, .38f));
                canvas.drawCircle(251, 19, 13, paint);
                path.rewind();
                path.moveTo(-8, 67);
                path.cubicTo(30, 14, 73, 31, 118, 53);
                path.cubicTo(156, 8, 205, 23, 247, 48);
                path.cubicTo(278, 25, 304, 28, 332, 38);
                path.lineTo(332, 68);
                path.close();
                paint.setColor(ColorUtils.blendARGB(base, ink, .36f));
                canvas.drawPath(path, paint);
                path.rewind();
                path.moveTo(-8, 67);
                path.cubicTo(55, 29, 88, 73, 142, 52);
                path.cubicTo(206, 26, 260, 76, 332, 46);
                path.lineTo(332, 68);
                path.close();
                paint.setColor(ColorUtils.blendARGB(base, ink, .65f));
                canvas.drawPath(path, paint);
            } else {
                paint.setColor(ColorUtils.blendARGB(base, ink, .45f));
                canvas.drawCircle(52, 45, 35, paint);
                paint.setColor(ColorUtils.blendARGB(base, ink, .28f));
                canvas.drawRoundRect(102, -23, 174, 48, 18, 18, paint);
                canvas.save();
                canvas.rotate(-24, 227, 42);
                paint.setColor(ColorUtils.blendARGB(base, ink, .65f));
                canvas.drawRoundRect(203, 10, 253, 80, 11, 11, paint);
                canvas.restore();
                paint.setStyle(Paint.Style.STROKE);
                paint.setStrokeWidth(1.5f);
                paint.setColor(ColorUtils.blendARGB(base, ink, .64f));
                canvas.drawCircle(289, 5, 33, paint);
                paint.setStyle(Paint.Style.FILL);
            }
            canvas.restore();
        }
        canvas.restoreToCount(layer);
    }
}

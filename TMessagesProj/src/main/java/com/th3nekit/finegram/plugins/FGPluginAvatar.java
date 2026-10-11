/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.plugins;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.text.TextPaint;

import androidx.annotation.NonNull;
import androidx.core.graphics.ColorUtils;

import org.telegram.messenger.R;

public final class FGPluginAvatar extends Drawable {

    private static final float[] HUES = {212, 238, 262, 286, 318, 346, 12, 28, 168, 188, 146, 200};

    private static final float BACKGROUND_SATURATION = 0.30f;
    private static final float BACKGROUND_LIGHTNESS = 0.24f;
    private static final float ICON_SATURATION = 0.46f;
    private static final float ICON_LIGHTNESS = 0.72f;

    private static final float ICON_FRACTION = 0.52f;

    private final Paint backgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final TextPaint emojiPaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
    private final RectF rect = new RectF();
    private final Drawable puzzle;
    private final String emoji;
    private final float cornerFraction;
    private int alpha = 255;

    public FGPluginAvatar(Context context, String id, String icon, float cornerFraction) {
        this.cornerFraction = cornerFraction;
        backgroundPaint.setColor(background(id));
        emoji = FGPluginIcon.isEmoji(icon) ? icon.trim() : null;
        if (emoji == null) {
            puzzle = context.getResources().getDrawable(R.drawable.fg_plugin_puzzle, null).mutate();
            puzzle.setColorFilter(new PorterDuffColorFilter(foreground(id), PorterDuff.Mode.SRC_IN));
        } else {
            puzzle = null;
            emojiPaint.setTextAlign(Paint.Align.CENTER);
        }
    }

    private static float hueOf(String id) {
        final int hash = id == null ? 0 : id.hashCode();
        return HUES[Math.abs(hash % HUES.length)];
    }

    public static int background(String id) {
        return ColorUtils.HSLToColor(new float[]{hueOf(id), BACKGROUND_SATURATION, BACKGROUND_LIGHTNESS});
    }

    public static int foreground(String id) {
        return ColorUtils.HSLToColor(new float[]{hueOf(id), ICON_SATURATION, ICON_LIGHTNESS});
    }

    @Override
    public void draw(@NonNull Canvas canvas) {
        final Rect bounds = getBounds();
        if (bounds.isEmpty()) {
            return;
        }
        rect.set(bounds);
        final float radius = Math.min(rect.width(), rect.height()) * cornerFraction;
        backgroundPaint.setAlpha(alpha);
        canvas.drawRoundRect(rect, radius, radius, backgroundPaint);

        final int side = Math.min(bounds.width(), bounds.height());
        if (puzzle != null) {
            final int size = Math.round(side * ICON_FRACTION);
            final int left = bounds.centerX() - size / 2;
            final int top = bounds.centerY() - size / 2;
            puzzle.setBounds(left, top, left + size, top + size);
            puzzle.setAlpha(alpha);
            puzzle.draw(canvas);
        } else {
            emojiPaint.setTextSize(side * 0.5f);
            emojiPaint.setAlpha(alpha);
            final Paint.FontMetrics metrics = emojiPaint.getFontMetrics();
            final float baseline = bounds.exactCenterY() - (metrics.ascent + metrics.descent) / 2f;
            canvas.drawText(emoji, bounds.exactCenterX(), baseline, emojiPaint);
        }
    }

    @Override
    public void setAlpha(int alpha) {
        this.alpha = alpha;
        invalidateSelf();
    }

    @Override
    public int getAlpha() {
        return alpha;
    }

    @Override
    public void setColorFilter(ColorFilter colorFilter) {
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }
}

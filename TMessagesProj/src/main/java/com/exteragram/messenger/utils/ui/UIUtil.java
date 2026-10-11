/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.utils.ui;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.Drawable;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.CombinedDrawable;

public final class UIUtil {

    public static final UIUtil INSTANCE = new UIUtil();

    private UIUtil() {
    }

    public static Bitmap drawableToBitmap(Drawable drawable, int width, int height) {
        if (drawable == null || width <= 0 || height <= 0) return null;
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, width, height);
        drawable.draw(canvas);
        return bitmap;
    }

    public static CombinedDrawable createCircleDrawableWithIcon(Context context, int iconRes, int size) {
        Drawable icon = iconRes == 0 || context == null ? null : context.getResources().getDrawable(iconRes).mutate();
        Drawable circle = Theme.createCircleDrawable(AndroidUtilities.dp(size), 0xffffffff);
        CombinedDrawable combined = new CombinedDrawable(circle, icon);
        combined.setCustomSize(AndroidUtilities.dp(size), AndroidUtilities.dp(size));
        return combined;
    }

    public static int adjustHsl(int color, float luminance) {
        return adjustHsl(color, luminance, 1f);
    }

    public static int adjustHsl(int color, float luminance, float saturation) {
        float[] hsl = new float[3];
        Color.colorToHSV(color, hsl);
        hsl[1] = Math.max(0f, Math.min(1f, hsl[1] * saturation));
        hsl[2] = Math.max(0f, Math.min(1f, hsl[2] * luminance));
        return Color.HSVToColor(Color.alpha(color), hsl);
    }

    public static void drawNowPlayingPattern(Canvas canvas, Drawable pattern, float w, float h, float alpha) {
        if (canvas == null || pattern == null || w <= 0 || h <= 0) return;
        pattern.setAlpha((int) (255 * Math.max(0f, Math.min(1f, alpha))));
        pattern.setBounds(0, 0, (int) w, (int) h);
        pattern.draw(canvas);
    }
}

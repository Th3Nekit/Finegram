package org.telegram.ui.Components;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Rect;
import android.graphics.RectF;

import org.telegram.messenger.AndroidUtilities;

public final class TornEdge {

    private TornEdge() {}

    public static final class Params {
        public int seed = 1337;

        public float stepDp = 2.5f;
        public float jitterDp = 1.9f;

        public float maxDeviationDp() {
            return jitterDp;
        }

        public int paddingPx() {
            return (int) Math.ceil(px(maxDeviationDp()));
        }
    }

    private static float px(float dp) {
        return dp * AndroidUtilities.density;
    }

    private static int hash(int x, int seed) {
        int h = x * 374761393 + seed * 668265263;
        h = (h ^ (h >>> 13)) * 1274126177;
        return h ^ (h >>> 16);
    }

    private static double rand01(int x, int seed) {
        return (hash(x, seed) >>> 8) / 16777216.0;
    }

    public static float[] profile(Params p, int widthPx, int seed) {
        final float step = Math.max(1f, px(p.stepDp));
        final float jitter = px(p.jitterDp);
        final int n = (int) Math.ceil(widthPx / step) + 1;
        final float[] out = new float[n];
        for (int k = 0; k < n; k++) {
            out[k] = (float) ((rand01(k, seed ^ 0x9E37) - 0.5) * 2.0 * jitter);
        }
        return out;
    }

    public static Path buildSlabPath(Params p, float width,
                                     float topBaseline, float bottomBaseline,
                                     float[] topProfile, float[] bottomProfile) {
        final float step = Math.max(1f, px(p.stepDp));
        final Path path = new Path();

        if (topProfile != null) {
            for (int k = 0; k < topProfile.length; k++) {
                float x = Math.min(k * step, width);
                float y = topBaseline + topProfile[k];
                if (k == 0) {
                    path.moveTo(x, y);
                } else {
                    path.lineTo(x, y);
                }
            }
        } else {
            path.moveTo(0, topBaseline);
            path.lineTo(width, topBaseline);
        }

        if (bottomProfile != null) {
            for (int k = bottomProfile.length - 1; k >= 0; k--) {
                path.lineTo(Math.min(k * step, width), bottomBaseline + bottomProfile[k]);
            }
        } else {
            path.lineTo(width, bottomBaseline);
            path.lineTo(0, bottomBaseline);
        }

        path.close();
        return path;
    }

    public static void draw(Canvas canvas, Params p, float width,
                            float topBaseline, float bottomBaseline,
                            int color, float cornerRadius,
                            float[] topProfile, float[] bottomProfile) {
        Path body = buildSlabPath(p, width, topBaseline, bottomBaseline, topProfile, bottomProfile);

        if (cornerRadius > 0) {
            Path clip = new Path();
            RectF bounds = new RectF();
            body.computeBounds(bounds, true);
            clip.addRoundRect(new RectF(0, bounds.top, width, bounds.bottom),
                    cornerRadius, cornerRadius, Path.Direction.CW);
            body.op(clip, Path.Op.INTERSECT);
        }

        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setColor(color);
        canvas.drawPath(body, paint);
    }

    public static Bitmap createTearBitmap(Params p, int width, int seed) {
        final int pad = p.paddingPx();
        final int height = bitmapHeight(p);
        final float[] prof = profile(p, width, seed);
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ALPHA_8);
        Canvas canvas = new Canvas(bitmap);
        draw(canvas, p, width, pad, height - pad, Color.BLACK, 0, prof, prof);
        return bitmap;
    }

    public static int fragmentHeight(Params p) {
        return p.paddingPx() * 2 + solidGuardPx();
    }

    public static int bitmapHeight(Params p) {
        return fragmentHeight(p) * 2;
    }

    private static int solidGuardPx() {
        return Math.max(1, (int) Math.ceil(px(0.5f)));
    }

    public static void drawTopEdge(Canvas canvas, Bitmap tear, Params p, int width,
                                   float left, float baselineY, Paint paint) {
        final int band = fragmentHeight(p);
        stamp(canvas, tear, width, left, baselineY, band, band, paint);
    }

    public static void drawBottomEdge(Canvas canvas, Bitmap tear, Params p, int width,
                                      float left, float baselineY, Paint paint) {
        final int band = fragmentHeight(p);
        stamp(canvas, tear, width, left, baselineY - band, 0, band, paint);
    }

    private static void stamp(Canvas canvas, Bitmap tear, int width, float left, float top,
                              int srcTop, int srcHeight, Paint paint) {
        final int w = Math.min(width, tear.getWidth());
        final Rect src = new Rect(0, srcTop, w, srcTop + srcHeight);
        final Rect dst = new Rect((int) left, (int) top, (int) left + w, (int) top + srcHeight);
        canvas.drawBitmap(tear, src, dst, paint);
    }
}
package org.telegram.ui.iv;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.text.Spanned;
import android.text.style.ReplacementSpan;

import org.telegram.messenger.FileLog;

public class MathSpan extends ReplacementSpan {

    public final String source;

    private final Bitmap bitmap;
    private final int width;
    private final int height;
    private final int depth;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);

    private MathSpan(String source, Bitmap bitmap, int w, int h, int color, int depth) {
        this.source = source;
        this.bitmap = bitmap;
        this.width = w;
        this.height = h;
        this.depth = depth;
        paint.setColor(color);
    }

    public static MathSpan create(String source, int color, float textSizePx) {
        if (source == null || source.isEmpty()) return null;
        final Latex r = Latex.render(source, textSizePx, true);
        if (r == null) return null;
        return new MathSpan(source, r.bitmap, r.width, r.height, color, r.depth);
    }

    @Override
    public int getSize(Paint p, CharSequence text, int start, int end, Paint.FontMetricsInt fm) {
        if (fm != null) {

            fm.top = fm.ascent = -(height - depth);
            fm.bottom = fm.descent = depth;
        }
        return width;
    }

    @Override
    public void draw(Canvas canvas, CharSequence text, int start, int end, float x, int top, int y, int bottom, Paint p) {
        if (bitmap == null) return;

        paint.setColor(p.getColor());
        canvas.drawBitmap(bitmap, x, y - (height - depth), paint);
    }

    public static String sourceAt(CharSequence cs, int from, int to) {
        if (!(cs instanceof Spanned)) return null;
        final MathSpan[] spans = ((Spanned) cs).getSpans(from, to, MathSpan.class);
        return spans.length > 0 ? spans[0].source : null;
    }
}

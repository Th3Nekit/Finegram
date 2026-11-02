package org.telegram.utils.recorder.round;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;

public final class RoundVideoWatermark {

    private RoundVideoWatermark() {
    }

    public static Bitmap createBitmap(int outputSize) {
        if (outputSize <= 0) {
            throw new IllegalArgumentException("outputSize must be positive");
        }
        Bitmap bitmap = Bitmap.createBitmap(outputSize, outputSize, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
        paint.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
        paint.setTextSize(outputSize * 0.04f);

        float textSize = paint.getTextSize();
        float initialWidth = textSize * 0.54f;
        float gap = textSize * 0.08f;
        float wordWidth = initialWidth + gap + paint.measureText("inegram");
        float height = outputSize * 0.068f;
        float centerY = outputSize * 0.84f;
        float padding = outputSize * 0.018f;
        RectF plate = new RectF(
                (outputSize - wordWidth) * 0.5f - padding,
                centerY - height * 0.5f,
                (outputSize + wordWidth) * 0.5f + padding,
                centerY + height * 0.5f
        );
        float radius = height * 0.5f;
        paint.setShader(new LinearGradient(
                0f, plate.top, 0f, plate.bottom,
                Color.argb(166, 23, 31, 40), Color.argb(190, 9, 15, 23),
                Shader.TileMode.CLAMP
        ));
        canvas.drawRoundRect(plate, radius, radius, paint);
        paint.setShader(null);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(outputSize / 720f);
        paint.setColor(Color.argb(42, 244, 248, 255));
        canvas.drawRoundRect(plate, radius, radius, paint);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(0xfff4f8fc);

        Paint.FontMetrics metrics = paint.getFontMetrics();
        float baseline = centerY - (metrics.ascent + metrics.descent) * 0.5f;
        float left = (outputSize - wordWidth) * 0.5f;
        Rect capitals = new Rect();
        paint.getTextBounds("H", 0, 1, capitals);
        float top = baseline + capitals.top;
        float stem = textSize * 0.1f;
        float middle = top + capitals.height() * 0.44f;
        Path initial = new Path();
        initial.moveTo(left, baseline);
        initial.lineTo(left, top);
        initial.lineTo(left + initialWidth, top);
        initial.lineTo(left + initialWidth - stem, top + stem);
        initial.lineTo(left + stem, top + stem);
        initial.lineTo(left + stem, middle);
        initial.lineTo(left + initialWidth * 0.76f, middle);
        initial.lineTo(left + initialWidth * 0.76f - stem, middle + stem);
        initial.lineTo(left + stem, middle + stem);
        initial.lineTo(left + stem, baseline);
        initial.close();
        canvas.drawPath(initial, paint);
        canvas.drawText("inegram", left + initialWidth + gap, baseline, paint);
        return bitmap;
    }
}

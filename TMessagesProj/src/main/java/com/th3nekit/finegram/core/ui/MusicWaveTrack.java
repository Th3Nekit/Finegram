package com.th3nekit.finegram.core.ui;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;

public final class MusicWaveTrack {
    private final Path path = new Path();
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private long lastFrame;
    private float phase;
    private float amplitude;

    public boolean beginFrame(long now, boolean playing, boolean motionAllowed) {
        float target = playing ? 1f : 0f;
        long elapsed = lastFrame == 0 ? 0 : Math.max(0, Math.min(48, now - lastFrame));
        lastFrame = now;
        if (!motionAllowed) {
            amplitude = target;
            return false;
        }
        if (playing) phase = (phase + elapsed / 1000f) % 1f;
        float change = elapsed / 150f;
        amplitude = amplitude < target ? Math.min(target, amplitude + change) : Math.max(target, amplitude - change);
        return playing || amplitude != target;
    }

    public void resetClock() {
        lastFrame = 0;
    }

    public void draw(Canvas canvas, float start, float end, float y, float origin, float activeEnd,
                     float density, float thickness, Paint color) {
        if (end <= start || density <= 0 || !Float.isFinite(end) || !Float.isFinite(start)) return;
        float wavelength = 24f * density;
        float height = 3f * density * amplitude;
        float step = Math.max(1f, 2f * density);
        path.rewind();
        for (float x = start; ; x = Math.min(end, x + step)) {
            float taper = Math.max(0f, Math.min(1f, Math.min(x - origin, activeEnd - x) / (wavelength / 4f)));
            float offset = height * taper * (float) Math.sin((x - origin) / wavelength * Math.PI * 2 - phase * Math.PI * 2);
            if (x == start) path.moveTo(x, y + offset);
            else path.lineTo(x, y + offset);
            if (x >= end) break;
        }
        paint.setColor(color.getColor());
        paint.setAlpha(color.getAlpha());
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(thickness);
        paint.setStrokeCap(Paint.Cap.ROUND);
        paint.setStrokeJoin(Paint.Join.ROUND);
        canvas.drawPath(path, paint);
    }
}

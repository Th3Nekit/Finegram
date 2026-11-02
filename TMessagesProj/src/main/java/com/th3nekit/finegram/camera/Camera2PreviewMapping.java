package com.th3nekit.finegram.camera;

import java.util.WeakHashMap;

public final class Camera2PreviewMapping {
    private static final WeakHashMap<Object, Frame> frames = new WeakHashMap<>();

    private static final class Frame {
        final int width, height, crop;
        final float[] matrix;
        final long time;
        Frame(int width, int height, int crop, float[] matrix, long time) {
            this.width = width;
            this.height = height;
            this.crop = crop;
            this.matrix = matrix.clone();
            this.time = time;
        }
    }

    public static synchronized void publish(Object owner, int width, int height, int crop,
                                            float[] matrix, long time) {
        if (owner == null || width <= 0 || height <= 0 || crop <= 0
                || matrix == null || matrix.length < 16) return;
        frames.put(owner, new Frame(width, height, Math.min(crop, Math.min(width, height)), matrix, time));
    }

    public static synchronized void invalidate(Object owner) {
        frames.remove(owner);
    }

    public static synchronized boolean map(Object owner, float x, float y, float[] out,
                                            long notBefore, long now) {
        Frame frame = frames.get(owner);
        if (out == null || out.length < 2 || !Float.isFinite(x) || !Float.isFinite(y)
                || x < 0 || x > 1 || y < 0 || y > 1 || frame == null
                || frame.time < notBefore || now < frame.time || now - frame.time > 500_000_000L) return false;
        float[] m = frame.matrix;
        boolean swapped = Math.abs(m[1]) > Math.abs(m[0]);
        float u = .5f + (x - .5f) * frame.crop / (swapped ? frame.height : frame.width);
        float v = .5f + (.5f - y) * frame.crop / (swapped ? frame.width : frame.height);
        return transform(m, u, v, out);
    }

    public static boolean mapTexture(float[] matrix, float x, float y, float[] out) {
        if (matrix == null || matrix.length < 16 || out == null || out.length < 2
                || !Float.isFinite(x) || !Float.isFinite(y)
                || x < 0 || x > 1 || y < 0 || y > 1) return false;
        return transform(matrix, x, 1f - y, out);
    }

    private static boolean transform(float[] m, float u, float v, float[] out) {
        float w = m[3] * u + m[7] * v + m[15];
        float bx = (m[0] * u + m[4] * v + m[12]) / w;
        float by = (m[1] * u + m[5] * v + m[13]) / w;
        if (!Float.isFinite(bx) || !Float.isFinite(by) || bx < 0 || bx > 1 || by < 0 || by > 1) return false;
        out[0] = bx;
        out[1] = by;
        return true;
    }
}

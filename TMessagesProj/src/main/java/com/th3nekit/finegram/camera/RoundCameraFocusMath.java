package com.th3nekit.finegram.camera;

public final class RoundCameraFocusMath {
    private RoundCameraFocusMath() {}

    public static boolean insideCircle(float x, float y) {
        return Float.isFinite(x) && Float.isFinite(y)
                && (x - .5f) * (x - .5f) + (y - .5f) * (y - .5f) <= .25f;
    }

    public static boolean map(float x, float y, float[] mvp, float[] texture,
                              float[] corners, float[] out) {
        if (out == null || out.length < 2 || mvp == null || mvp.length < 16
                || texture == null || texture.length < 16 || corners == null || corners.length < 8
                || !unit(x) || !unit(y)) return false;
        for (float v : mvp) if (!Float.isFinite(v)) return false;
        for (float v : texture) if (!Float.isFinite(v)) return false;
        for (float v : corners) if (!Float.isFinite(v)) return false;
        double a = mvp[0], b = mvp[4], c = mvp[12];
        double d = mvp[1], e = mvp[5], f = mvp[13];
        double g = mvp[3], h = mvp[7], i = mvp[15];
        double determinant = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
        if (Math.abs(determinant) < 1e-8) return false;
        double px = x * 2 - 1, py = 1 - y * 2;
        double w = (d * h - e * g) * px + (b * g - a * h) * py + a * e - b * d;
        if (Math.abs(w) < 1e-8) return false;
        double u = (((e * i - f * h) * px + (c * h - b * i) * py + b * f - c * e) / w + 1) * .5;
        double v = (((f * g - d * i) * px + (a * i - c * g) * py + c * d - a * f) / w + 1) * .5;
        if (u < -.00001 || u > 1.00001 || v < -.00001 || v > 1.00001) return false;
        double tx = mix(corners[0], corners[2], corners[4], corners[6], u, v);
        double ty = mix(corners[1], corners[3], corners[5], corners[7], u, v);
        double tw = texture[3] * tx + texture[7] * ty + texture[15];
        if (Math.abs(tw) < 1e-8) return false;
        double rawX = (texture[0] * tx + texture[4] * ty + texture[12]) / tw;
        double rawY = (texture[1] * tx + texture[5] * ty + texture[13]) / tw;
        if (!Double.isFinite(rawX) || !Double.isFinite(rawY)
                || rawX < -.00001 || rawX > 1.00001 || rawY < -.00001 || rawY > 1.00001) return false;
        out[0] = (float) Math.max(0, Math.min(1, rawX));
        out[1] = (float) Math.max(0, Math.min(1, rawY));
        return true;
    }

    private static boolean unit(float x) {
        return Float.isFinite(x) && x >= 0 && x <= 1;
    }

    private static double mix(float a, float b, float c, float d, double u, double v) {
        return (a + (b - a) * u) * (1 - v) + (c + (d - c) * u) * v;
    }
}

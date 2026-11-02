package com.th3nekit.finegram.camera;

public final class Camera2MeteringGeometry {
    private Camera2MeteringGeometry() {}

    public static int[] region(float x, float y, int left, int top, int right, int bottom,
                               int bufferWidth, int bufferHeight) {
        if (!Float.isFinite(x) || !Float.isFinite(y) || x < 0f || x > 1f || y < 0f || y > 1f
                || right <= left || bottom <= top || bufferWidth <= 0 || bufferHeight <= 0) return null;
        float width = right - left;
        float height = bottom - top;
        float aspect = bufferWidth / (float) bufferHeight;
        if (width / height > aspect) {
            float cropped = height * aspect;
            left += Math.round((width - cropped) * .5f);
            width = cropped;
        } else {
            float cropped = width / aspect;
            top += Math.round((height - cropped) * .5f);
            height = cropped;
        }
        int w = Math.max(1, Math.round(width * .12f));
        int h = Math.max(1, Math.round(height * .12f));
        int l = Math.max(left, Math.min(Math.round(left + x * width) - w / 2,
                Math.min(right, Math.round(left + width)) - w));
        int t = Math.max(top, Math.min(Math.round(top + y * height) - h / 2,
                Math.min(bottom, Math.round(top + height)) - h));
        return new int[]{l, t, Math.min(right, l + w), Math.min(bottom, t + h)};
    }
}

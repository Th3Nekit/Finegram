package com.th3nekit.finegram.camera;

public final class RoundVideoZoom {
    private RoundVideoZoom() {}

    public static float ratio(float progress, float min, float max) {
        if (!Float.isFinite(min) || !Float.isFinite(max) || min <= 0f || max < min) return 1f;
        if (!Float.isFinite(progress)) return Math.max(min, Math.min(max, 1f));
        return min + Math.max(0f, Math.min(1f, progress)) * (max - min);
    }

    public static float progress(float ratio, float min, float max) {
        if (!Float.isFinite(min) || !Float.isFinite(max) || min <= 0f || max <= min) return 0f;
        if (!Float.isFinite(ratio)) ratio = 1f;
        return Math.max(0f, Math.min(1f, (ratio - min) / (max - min)));
    }
}

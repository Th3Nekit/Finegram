package com.th3nekit.finegram.core.ui;

public final class RoundMediaGeometry {
    public static final int SIDE = 512;
    public static final long MAX_DURATION = 60000;
    public final int width;
    public final int height;
    public final float fractionX;
    public final float fractionY;

    public RoundMediaGeometry(int width, int height, int rotation) {
        if (width <= 0 || height <= 0 || rotation % 90 != 0) {
            throw new IllegalArgumentException();
        }
        boolean swapped = Math.floorMod(rotation, 180) == 90;
        this.width = swapped ? height : width;
        this.height = swapped ? width : height;
        int side = Math.min(this.width, this.height);
        fractionX = (float) side / this.width;
        fractionY = (float) side / this.height;
    }

    public float clampX(float offset) {
        return clamp(offset, (1 - fractionX) / 2);
    }

    public float clampY(float offset) {
        return clamp(offset, (1 - fractionY) / 2);
    }

    private static float clamp(float value, float limit) {
        return Float.isFinite(value) ? Math.max(-limit, Math.min(limit, value)) : 0;
    }

    public static long start(long requested, long duration) {
        return Math.max(0, Math.min(requested, Math.max(0, duration - 1)));
    }

    public static long length(long requested, long start, long duration) {
        return Math.max(1, Math.min(MAX_DURATION, Math.min(requested, Math.max(1, duration - start(start, duration)))));
    }
}

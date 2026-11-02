package com.th3nekit.finegram.camera;

public interface CameraZoomTarget {
    boolean isZoomReady();
    boolean isFrontface();
    float getMinZoomRatio();
    float getMaxZoomRatio();
    float getZoomRatio();
    float[] getZoomShortcuts();
    void setZoomRatio(float ratio);

    default float normalizedForRatio(float ratio) {
        float min = getMinZoomRatio();
        float max = getMaxZoomRatio();
        if (!Float.isFinite(ratio) || !Float.isFinite(min) || !Float.isFinite(max) || max <= min) {
            return 0f;
        }
        return Math.max(0f, Math.min(1f, (ratio - min) / (max - min)));
    }
}

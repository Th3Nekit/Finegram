package com.th3nekit.finegram.camera;

public interface CameraZoomTarget {
    boolean isZoomReady();
    boolean isFrontface();
    float getMinZoomRatio();
    float getMaxZoomRatio();
    float getZoomRatio();
    float[] getZoomShortcuts();
    void setZoomRatio(float ratio);
}

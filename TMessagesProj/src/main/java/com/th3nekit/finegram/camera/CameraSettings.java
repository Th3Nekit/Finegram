package com.th3nekit.finegram.camera;

import com.th3nekit.finegram.core.configs.FinegramCameraConfig;

public final class CameraSettings {
    private CameraSettings() {}

    public static boolean takeFrontFacing() {
        Boolean rear = FinegramCameraConfig.INSTANCE.takePendingRoundRear();
        return !(rear != null ? rear : FinegramCameraConfig.INSTANCE.startWithRear());
    }

    public static int roundResolution(int fallback) {
        int resolution = FinegramCameraConfig.INSTANCE.getVideoMessagesResolution();
        return resolution > 0 ? resolution : fallback;
    }

    public static void advanceFlashHint() {
        int count = FinegramCameraConfig.INSTANCE.getVideoMessagesHintCount();
        FinegramCameraConfig.INSTANCE.setVideoMessagesHintCount(Math.min(3, count + 1));
    }
}

package com.th3nekit.finegram.camera;

import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;

import com.th3nekit.finegram.core.configs.FinegramCameraConfig;

import org.telegram.messenger.FileLog;

public final class CameraIds {
    private CameraIds() {}

    public static boolean hasFacing(CameraManager manager, boolean front) {
        if (manager == null) return false;
        int facing = front ? CameraCharacteristics.LENS_FACING_FRONT : CameraCharacteristics.LENS_FACING_BACK;
        try {
            for (String id : manager.getCameraIdList()) {
                try {
                    Integer side = manager.getCameraCharacteristics(id).get(CameraCharacteristics.LENS_FACING);
                    if (side != null && side == facing) return true;
                } catch (Exception error) {
                    FileLog.e(error);
                }
            }
        } catch (Exception error) {
            FileLog.e(error);
        }
        return false;
    }

    public static String validated(CameraManager manager, boolean front) {
        String requested = FinegramCameraConfig.manualIdFor(front).trim();
        if (requested.isEmpty() || manager == null) return "";
        int facing = front ? CameraCharacteristics.LENS_FACING_FRONT : CameraCharacteristics.LENS_FACING_BACK;
        try {
            Integer side = manager.getCameraCharacteristics(requested).get(CameraCharacteristics.LENS_FACING);
            if (side != null && side == facing) return requested;
            FileLog.d("Camera ID " + requested + " unavailable for " + (front ? "front" : "rear"));
        } catch (Exception error) {
            FileLog.e(error);
        }
        return "";
    }
}

package com.th3nekit.finegram.camera;

import android.graphics.Rect;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.os.Build;
import android.util.Size;
import android.util.SizeF;
import java.util.ArrayList;

public final class Camera2ZoomShortcuts {
    private Camera2ZoomShortcuts() {}

    public static float[] load(CameraManager manager, CameraCharacteristics characteristics,
                               boolean front, float min, float max) {
        float[] candidates = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P && characteristics != null) {
            try {
                java.util.Set<String> ids = characteristics.getPhysicalCameraIds();
                ArrayList<Float> scales = new ArrayList<>();
                for (String id : ids) {
                    try {
                        CameraCharacteristics child = manager.getCameraCharacteristics(id);
                        Integer facing = child.get(CameraCharacteristics.LENS_FACING);
                        if (facing == null || facing != (front
                                ? CameraCharacteristics.LENS_FACING_FRONT : CameraCharacteristics.LENS_FACING_BACK)) continue;
                        float scale = zoomOpticalScale(child);
                        if (Float.isFinite(scale) && scale > 0f) scales.add(scale);
                    } catch (Exception ignored) {}
                }
                float[] physical = new float[scales.size()];
                for (int i = 0; i < physical.length; i++) physical[i] = scales.get(i);
                candidates = com.th3nekit.finegram.camera.CameraXZoomShortcuts.fromOpticalScales(
                        zoomOpticalScale(characteristics), physical);
            } catch (Exception ignored) {}
        }
        return com.th3nekit.finegram.camera.CameraXZoomShortcuts.select(min, max, candidates);
    }

    private static float zoomOpticalScale(CameraCharacteristics characteristics) {
        SizeF sensor = characteristics.get(CameraCharacteristics.SENSOR_INFO_PHYSICAL_SIZE);
        Size pixels = characteristics.get(CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE);
        Rect active = characteristics.get(CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE);
        if (sensor == null || pixels == null || active == null
                || pixels.getWidth() <= 0 || pixels.getHeight() <= 0
                || active.left < 0 || active.top < 0 || active.width() <= 0 || active.height() <= 0
                || active.right > pixels.getWidth() || active.bottom > pixels.getHeight()) return Float.NaN;
        return com.th3nekit.finegram.camera.CameraXZoomShortcuts.opticalScale(
                characteristics.get(CameraCharacteristics.LENS_INFO_AVAILABLE_FOCAL_LENGTHS),
                sensor.getWidth() * ((float) active.width() / pixels.getWidth()),
                sensor.getHeight() * ((float) active.height() / pixels.getHeight()));
    }
}

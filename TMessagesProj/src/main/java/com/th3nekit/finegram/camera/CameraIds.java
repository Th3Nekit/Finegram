package com.th3nekit.finegram.camera;

import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.os.Build;
import android.util.Range;

import com.th3nekit.finegram.core.configs.FinegramCameraConfig;

import org.telegram.messenger.FileLog;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

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
        return validated(manager, front, requested);
    }

    public static String validatedUltraWide(CameraManager manager) {
        return validated(manager, false, FinegramCameraConfig.manualUltraWideId().trim());
    }

    public static List<String> compatibleLogicalRearIds(CameraManager manager, String mainId, String ultraWideId) {
        List<String> compatible = new ArrayList<>();
        if (manager == null || Build.VERSION.SDK_INT < Build.VERSION_CODES.R
                || mainId == null || ultraWideId == null) return compatible;
        mainId = mainId.trim();
        ultraWideId = ultraWideId.trim();
        if (mainId.isEmpty() || ultraWideId.isEmpty() || mainId.equals(ultraWideId)) return compatible;
        try {
            CameraCharacteristics main = manager.getCameraCharacteristics(mainId);
            CameraCharacteristics wide = manager.getCameraCharacteristics(ultraWideId);
            if (!Integer.valueOf(CameraCharacteristics.LENS_FACING_BACK).equals(main.get(CameraCharacteristics.LENS_FACING))
                    || !Integer.valueOf(CameraCharacteristics.LENS_FACING_BACK).equals(wide.get(CameraCharacteristics.LENS_FACING))) return compatible;
            float mainScale = Camera2ZoomShortcuts.zoomOpticalScale(main);
            float wideScale = Camera2ZoomShortcuts.zoomOpticalScale(wide);
            if (!Float.isFinite(mainScale) || mainScale <= 0f || !Float.isFinite(wideScale)
                    || wideScale <= 0f || wideScale / mainScale >= .95f) return compatible;
            float mainMaximum;
            Range<Float> mainRange = main.get(CameraCharacteristics.CONTROL_ZOOM_RATIO_RANGE);
            if (mainRange != null && Float.isFinite(mainRange.getLower()) && mainRange.getLower() > 0f
                    && Float.isFinite(mainRange.getUpper()) && mainRange.getUpper() >= mainRange.getLower()) {
                mainMaximum = mainRange.getUpper();
            } else {
                Float digital = main.get(CameraCharacteristics.SCALER_AVAILABLE_MAX_DIGITAL_ZOOM);
                if (digital == null || !Float.isFinite(digital) || digital < 1f) return compatible;
                mainMaximum = digital;
            }
            for (String id : manager.getCameraIdList()) {
                try {
                    CameraCharacteristics logical = manager.getCameraCharacteristics(id);
                    if (!Integer.valueOf(CameraCharacteristics.LENS_FACING_BACK).equals(logical.get(CameraCharacteristics.LENS_FACING))) continue;
                    int[] capabilities = logical.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
                    boolean multiCamera = false;
                    if (capabilities != null) {
                        for (int capability : capabilities) {
                            if (capability == CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA) {
                                multiCamera = true;
                                break;
                            }
                        }
                    }
                    if (!multiCamera) continue;
                    Set<String> children = logical.getPhysicalCameraIds();
                    if (!children.contains(mainId) || !children.contains(ultraWideId)) continue;
                    float logicalScale = Camera2ZoomShortcuts.zoomOpticalScale(logical);
                    if (!Float.isFinite(logicalScale) || logicalScale <= 0f
                            || logicalScale != mainScale) continue;
                    Range<Float> range = logical.get(CameraCharacteristics.CONTROL_ZOOM_RATIO_RANGE);
                    if (range == null || !Float.isFinite(range.getLower()) || !Float.isFinite(range.getUpper())
                            || range.getLower() <= 0f || range.getLower() > Math.min(1f, wideScale / logicalScale)
                            || range.getUpper() < Math.max(1f, mainMaximum)) continue;
                    compatible.add(id);
                } catch (Exception error) {
                    FileLog.e(error);
                }
            }
        } catch (Exception error) {
            FileLog.e(error);
        }
        return compatible;
    }

    private static String validated(CameraManager manager, boolean front, String requested) {
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

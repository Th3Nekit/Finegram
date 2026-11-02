package com.th3nekit.finegram.camera;

import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.os.Build;
import android.util.Range;

import org.telegram.messenger.FileLog;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.Map;

public final class Camera2LensCatalog {
    public static final class Route {
        public final String cameraId;
        public final String physicalId;
        public final CameraCharacteristics characteristics;
        public final CameraCharacteristics controlCharacteristics;
        public final float scale;
        public final float minimum;
        public final float maximum;
        private final float controlScale;

        private Route(String cameraId, String physicalId, CameraCharacteristics characteristics,
                      float scale, float minimum, float maximum,
                      CameraCharacteristics controlCharacteristics, float controlScale) {
            this.cameraId = cameraId;
            this.physicalId = physicalId;
            this.characteristics = characteristics;
            this.scale = scale;
            this.minimum = minimum;
            this.maximum = maximum;
            this.controlCharacteristics = controlCharacteristics;
            this.controlScale = controlScale;
        }

        public float localZoom(float ratio) {
            return Math.max(minimum, Math.min(maximum, ratio / controlScale));
        }

        public boolean sameLens(Route other) {
            return other != null && cameraId.equals(other.cameraId)
                    && java.util.Objects.equals(physicalId, other.physicalId);
        }
    }

    public final Route main;
    public final float minimum;
    public final float maximum;
    private final ArrayList<Route> lenses = new ArrayList<>();
    private final String manualWide;
    private final boolean preferLogicalZoom;

    public Camera2LensCatalog(CameraManager manager, String mainId,
                              CameraCharacteristics selected, boolean front) {
        this(manager, mainId, selected, front, false);
    }

    public Camera2LensCatalog(CameraManager manager, String mainId,
                              CameraCharacteristics selected, boolean front, boolean preferLogicalZoom) {
        main = route(mainId, null, selected, 1f);
        manualWide = front ? "" : CameraIds.validatedUltraWide(manager);
        this.preferLogicalZoom = preferLogicalZoom && !front
                && CameraIds.compatibleLogicalRearIds(manager, CameraIds.validated(manager, false), manualWide).contains(mainId);
        float low = main.minimum;
        float high = main.maximum;
        lenses.add(main);
        float reference = Camera2ZoomShortcuts.zoomOpticalScale(selected);
        if (!front && manager != null && Float.isFinite(reference) && reference > 0f) {
            try {
                Map<String, Route> found = new LinkedHashMap<>();
                String[] ids = manager.getCameraIdList();
                for (String id : ids) {
                    try {
                        CameraCharacteristics cc = manager.getCameraCharacteristics(id);
                        add(found, id, null, cc, cc, reference);
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                            for (String child : cc.getPhysicalCameraIds()) {
                                if (java.util.Arrays.asList(ids).contains(child)) continue;
                                try {
                                    add(found, id, child, manager.getCameraCharacteristics(child), cc, reference);
                                } catch (Exception error) {
                                    FileLog.d("Camera2 physical lens unavailable: " + child);
                                }
                            }
                        }
                    } catch (Exception error) {
                        FileLog.d("Camera2 catalog camera unavailable: " + id);
                    }
                }
                for (Route lens : found.values()) {
                    if (lens.sameLens(main)) continue;
                    if (lens.scale < .95f || lens.scale > 1.05f) {
                        lenses.add(lens);
                        low = Math.min(low, lens.scale);
                        high = Math.max(high, lens.scale);
                    }
                }
            } catch (Exception error) {
                FileLog.e(error);
            }
        } else if (!front) {
            FileLog.d("Camera2 extra lens routes unavailable: main optical reference is ambiguous");
        }
        minimum = low;
        maximum = high;
        FileLog.d("Camera2 lens catalog: main=" + mainId + ", manualUW=" + manualWide
                + ", range=" + minimum + ".." + maximum + ", routes=" + lenses.size());
    }

    private void add(Map<String, Route> found, String id, String physical,
                     CameraCharacteristics cc, CameraCharacteristics parent, float reference) {
        Integer facing = cc.get(CameraCharacteristics.LENS_FACING);
        if (facing == null || facing != CameraCharacteristics.LENS_FACING_BACK) return;
        float scale = Camera2ZoomShortcuts.zoomOpticalScale(cc) / reference;
        if (!Float.isFinite(scale) || scale <= 0f) return;
        float nominal = Math.round(scale);
        if (nominal >= 2f && Math.abs(scale - nominal) / nominal <= .050001f) scale = nominal;
        String key = physical == null ? id : physical;
        Route candidate = route(id, physical, cc, scale);
        if (physical != null) {
            float parentScale = Camera2ZoomShortcuts.zoomOpticalScale(parent) / reference;
            Range<Float> range = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                    ? parent.get(CameraCharacteristics.CONTROL_ZOOM_RATIO_RANGE) : null;
            float local = scale / parentScale;
            if (range == null || !Float.isFinite(parentScale) || parentScale <= 0f
                    || !Float.isFinite(local) || !Float.isFinite(range.getLower())
                    || !Float.isFinite(range.getUpper()) || range.getLower() <= 0f
                    || local < range.getLower() - .015f || local > range.getUpper() + .015f) {
                FileLog.d("Camera2 physical route omitted: full lens FOV cannot be requested: " + id + "/" + physical);
                return;
            }
            candidate = new Route(id, physical, cc, scale, range.getLower(), range.getUpper(), parent, parentScale);
        }
        if (!found.containsKey(key) || physical == null) found.put(key, candidate);
    }

    private static Route route(String id, String physical, CameraCharacteristics cc, float scale) {
        float min = 1f;
        Float digital = cc.get(CameraCharacteristics.SCALER_AVAILABLE_MAX_DIGITAL_ZOOM);
        float max = digital != null && Float.isFinite(digital) ? Math.max(1f, digital) : 1f;
        if (physical == null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Range<Float> range = cc.get(CameraCharacteristics.CONTROL_ZOOM_RATIO_RANGE);
            if (range != null && Float.isFinite(range.getLower()) && range.getLower() > 0f
                    && Float.isFinite(range.getUpper()) && range.getUpper() >= range.getLower()) {
                min = range.getLower();
                max = range.getUpper();
            }
        }
        return new Route(id, physical, cc, scale, min, max, cc, scale);
    }

    public Route resolve(float ratio) {
        if (!Float.isFinite(ratio) || ratio <= 0f || Math.abs(ratio - 1f) < .001f) return main;
        if (preferLogicalZoom && ratio >= main.minimum && ratio <= main.maximum) return main;
        Route best = null;
        for (Route lens : lenses) {
            if (lens == main || lens.scale > ratio + .015f) continue;
            if (ratio < 1f && lens.scale >= .95f) continue;
            if (ratio >= 1f && lens.scale <= 1.05f) continue;
            String id = lens.physicalId == null ? lens.cameraId : lens.physicalId;
            if (ratio < 1f && !manualWide.isEmpty() && manualWide.equals(id)) return lens;
            if (best == null || lens.scale > best.scale) best = lens;
        }
        if (ratio < 1f && !manualWide.isEmpty()) {
            for (Route lens : lenses) {
                String id = lens.physicalId == null ? lens.cameraId : lens.physicalId;
                if (manualWide.equals(id) && lens.scale < .95f) return lens;
            }
        }
        return best == null ? main : best;
    }

    public float[] shortcuts(float max) {
        float[] ratios = new float[lenses.size()];
        for (int i = 0; i < ratios.length; i++) ratios[i] = lenses.get(i).scale;
        return CameraXZoomShortcuts.select(minimum, Math.min(maximum, max), ratios);
    }
}

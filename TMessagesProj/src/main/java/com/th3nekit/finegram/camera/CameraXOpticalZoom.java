package com.th3nekit.finegram.camera;

import java.util.List;

final class CameraXOpticalZoom {
    static class Lens {
        final String key;
        final float opticalRatio;
        final float minimum;
        final float maximum;

        Lens(String key, float opticalRatio, float minimum, float maximum) {
            this.key = key;
            this.opticalRatio = opticalRatio;
            this.minimum = minimum;
            this.maximum = maximum;
        }

        boolean isValid() {
            return valid(opticalRatio) && valid(minimum) && valid(maximum) && maximum >= minimum;
        }

        float toNative(float global) {
            return Math.max(minimum, Math.min(maximum, valid(global) ? global / opticalRatio : minimum));
        }

        float toGlobal(float nativeRatio) {
            return opticalRatio * nativeRatio;
        }
    }

    private CameraXOpticalZoom() {}

    static <T extends Lens> T choose(List<T> lenses, float ratio) {
        if (!valid(ratio)) return null;
        T best = null;
        T fallback = null;
        for (T lens : lenses) {
            if (!lens.isValid()) continue;
            float lower = lens.toGlobal(lens.minimum);
            float upper = lens.toGlobal(lens.maximum);
            if (ratio < lower - .002f || ratio > upper + .002f) continue;
            if (fallback == null || lens.opticalRatio < fallback.opticalRatio) fallback = lens;
            if (lens.opticalRatio <= ratio + .002f
                    && (best == null || lens.opticalRatio > best.opticalRatio)) best = lens;
        }
        return best != null ? best : fallback;
    }

    static <T extends Lens> T choose(List<T> lenses, float ratio, boolean preferNativeWide) {
        T selected = choose(lenses, ratio);
        if (!preferNativeWide || selected == null || selected.opticalRatio > 1.05f) return selected;
        for (T lens : lenses) {
            if (lens.isValid() && Math.abs(lens.opticalRatio - 1f) < .002f
                    && lens.minimum < .95f && ratio >= lens.minimum - .002f
                    && ratio <= lens.maximum + .002f) return lens;
        }
        return selected;
    }

    static float minimum(List<? extends Lens> lenses) {
        float result = Float.POSITIVE_INFINITY;
        for (Lens lens : lenses) if (lens.isValid()) result = Math.min(result, lens.toGlobal(lens.minimum));
        return Float.isFinite(result) ? result : 1f;
    }

    static float maximum(List<? extends Lens> lenses) {
        float result = 1f;
        for (Lens lens : lenses) if (lens.isValid()) result = Math.max(result, lens.toGlobal(lens.maximum));
        return result;
    }

    private static boolean valid(float value) {
        return Float.isFinite(value) && value > 0f;
    }
}

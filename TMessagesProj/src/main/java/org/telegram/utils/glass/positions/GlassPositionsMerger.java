package org.telegram.utils.glass.positions;

import android.graphics.RectF;

public final class GlassPositionsMerger {

    private static final float EPS = 1e-4f;

    public static final GlassPositionsMerger DEFAULT =
            new GlassPositionsMerger(
                    Float.POSITIVE_INFINITY,
                    Float.POSITIVE_INFINITY
            );

    private final float epsX;
    private final float epsY;

    public GlassPositionsMerger() {
        this(EPS, EPS);
    }

    public GlassPositionsMerger(float eps) {
        this(eps, eps);
    }

    public GlassPositionsMerger(float epsX, float epsY) {
        if (Float.isNaN(epsX) || epsX < 0f) {
            throw new IllegalArgumentException("epsX must be >= 0");
        }
        if (Float.isNaN(epsY) || epsY < 0f) {
            throw new IllegalArgumentException("epsY must be >= 0");
        }

        this.epsX = epsX;
        this.epsY = epsY;
    }

    public void merge(
            GlassPositionsArray positions,
            GlassPositionsArray output
    ) {
        if (positions == output) {
            throw new IllegalArgumentException(
                    "positions and output must be different arrays"
            );
        }

        output.clear();

        final int count = positions.size();

        for (int i = 0; i < count; i++) {
            final RectF src = positions.get(i);

            float left = src.left;
            float top = src.top;
            float right = src.right;
            float bottom = src.bottom;

            int j = 0;

            while (j < output.size()) {
                final RectF other = output.get(j);

                if (intersectsOrNear(
                        left,
                        top,
                        right,
                        bottom,
                        other
                )) {
                    if (other.left < left) {
                        left = other.left;
                    }
                    if (other.top < top) {
                        top = other.top;
                    }
                    if (other.right > right) {
                        right = other.right;
                    }
                    if (other.bottom > bottom) {
                        bottom = other.bottom;
                    }

                    output.removeUnordered(j);
                    j = 0;
                } else {
                    j++;
                }
            }

            output.add(left, top, right, bottom);
        }

        sort(output);
    }

    private boolean intersectsOrNear(
            float left,
            float top,
            float right,
            float bottom,
            RectF other
    ) {
        return intersectsOrNear1D(
                left,
                right,
                other.left,
                other.right,
                epsX
        ) && intersectsOrNear1D(
                top,
                bottom,
                other.top,
                other.bottom,
                epsY
        );
    }

    private static boolean intersectsOrNear1D(
            float minA,
            float maxA,
            float minB,
            float maxB,
            float eps
    ) {
        if (maxA < minB) {
            return minB - maxA <= eps;
        }

        if (maxB < minA) {
            return minA - maxB <= eps;
        }

        return true;
    }

    private static void sort(GlassPositionsArray array) {
        final int count = array.size();

        for (int i = 1; i < count; i++) {
            final RectF current = array.get(i);

            final float left = current.left;
            final float top = current.top;
            final float right = current.right;
            final float bottom = current.bottom;

            int j = i - 1;

            while (j >= 0) {
                final RectF previous = array.get(j);

                if (compare(previous.top, previous.left, top, left) <= 0) {
                    break;
                }

                array.get(j + 1).set(previous);
                j--;
            }

            array.get(j + 1).set(left, top, right, bottom);
        }
    }

    private static int compare(
            float topA,
            float leftA,
            float topB,
            float leftB
    ) {
        int result = Float.compare(topA, topB);

        if (result == 0) {
            result = Float.compare(leftA, leftB);
        }

        return result;
    }
}
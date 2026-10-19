package org.telegram.messenger.utils;

import android.graphics.RectF;

import java.util.Collections;
import java.util.Comparator;
import java.util.List;

@Deprecated
public class RectFMergeBounding {

    private static final float EPS = 1e-4f;

    public static int mergeOverlapping(
            List<RectF> positions,
            int count,
            List<RectF> output
    ) {
        if (positions == null || count <= 0) {
            return 0;
        }
        if (count > positions.size()) {
            count = positions.size();
        }

        for (int i = output.size(); i < count; i++) {
            output.add(new RectF());
        }

        for (int i = 0; i < count; i++) {
            RectF src = positions.get(i);
            RectF dst = output.get(i);
            if (src != null) {
                dst.set(src);
            } else {
                dst.set(0f, 0f, 0f, 0f);
            }
        }

        int activeCount = count;

        boolean merged;
        do {
            merged = false;

            outer:
            for (int i = 0; i < activeCount; i++) {
                RectF a = output.get(i);

                for (int j = i + 1; j < activeCount; j++) {
                    RectF b = output.get(j);

                    if (intersectsOrTouches(a, b)) {

                        if (b.left   < a.left)   a.left   = b.left;
                        if (b.top    < a.top)    a.top    = b.top;
                        if (b.right  > a.right)  a.right  = b.right;
                        if (b.bottom > a.bottom) a.bottom = b.bottom;

                        int last = activeCount - 1;
                        if (j != last) {
                            output.get(j).set(output.get(last));
                        }
                        activeCount--;

                        merged = true;

                        break outer;
                    }
                }
            }
        } while (merged);

        for (int i = activeCount; i < output.size(); i++) {
            RectF r = output.get(i);
            r.top = Float.MAX_VALUE;
            r.left = Float.MAX_VALUE;

        }

        Collections.sort(output, RECT_COMPARATOR);

        return activeCount;
    }

    private static boolean intersectsOrTouches(RectF a, RectF b) {
        return a.left   <= b.right  + EPS &&
                a.right  >= b.left   - EPS &&
                a.top    <= b.bottom + EPS &&
                a.bottom >= b.top    - EPS;
    }

    private static final Comparator<RectF> RECT_COMPARATOR = (a, b) -> {

        if (Math.abs(a.top - b.top) > EPS) {
            return (a.top < b.top) ? -1 : 1;
        }

        if (Math.abs(a.left - b.left) > EPS) {
            return (a.left < b.left) ? -1 : 1;
        }
        return 0;
    };
}

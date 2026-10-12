package org.telegram.ui.Components.blur3.utils;

import android.graphics.Bitmap;

import java.lang.ref.WeakReference;

public class BitmapChangeTracker {

    private WeakReference<Bitmap> ref;
    private long generationId;

    private boolean invalidated = true;

    public void set(Bitmap bitmap) {
        ref = bitmap != null ? new WeakReference<>(bitmap) : null;
        generationId = generationOf(bitmap);
        invalidated = false;
    }

    public boolean isInvalidated(Bitmap bitmap) {
        if (invalidated) {
            return true;
        }

        final Bitmap recorded = ref != null ? ref.get() : null;
        if (recorded != bitmap) {

            return true;
        }

        return generationOf(bitmap) != generationId;
    }

    public void invalidate() {
        ref = null;
        generationId = 0;
        invalidated = true;
    }

    private static long generationOf(Bitmap bitmap) {
        return bitmap != null && !bitmap.isRecycled() ? bitmap.getGenerationId() : 0;
    }
}
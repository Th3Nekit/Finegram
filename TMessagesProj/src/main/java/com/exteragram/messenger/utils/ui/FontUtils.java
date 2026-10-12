/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.utils.ui;

import android.graphics.Typeface;

import com.th3nekit.finegram.helpers.ui.FontHelper;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;

public final class FontUtils {

    public static Typeface getSystemTypeface(String name) {
        if (name == null) {
            return null;
        }
        switch (name) {
            case AndroidUtilities.TYPEFACE_ROBOTO_MEDIUM:
                return isMediumWeightSupported()
                        ? FontHelper.createTypeface(500, false)
                        : Typeface.create("sans-serif", Typeface.BOLD);
            case "fonts/ritalic.ttf":
                return FontHelper.createTypeface(400, true);
            case AndroidUtilities.TYPEFACE_ROBOTO_MEDIUM_ITALIC:
                return isMediumWeightSupported()
                        ? FontHelper.createTypeface(500, true)
                        : Typeface.create("sans-serif", Typeface.BOLD_ITALIC);
            case AndroidUtilities.TYPEFACE_ROBOTO_MONO:
                return Typeface.MONOSPACE;
            case "fonts/rcondensedbold.ttf":
                return Typeface.create("sans-serif-condensed", Typeface.BOLD);
            case AndroidUtilities.TYPEFACE_ROBOTO_EXTRA_BOLD:
                return isMediumWeightSupported()
                        ? FontHelper.createTypeface(800, false)
                        : Typeface.create("sans-serif", Typeface.BOLD);
            default:
                return null;
        }
    }

    public static Typeface getFontFromAssets(String path) {
        try {
            return Typeface.createFromAsset(ApplicationLoader.applicationContext.getAssets(), path);
        } catch (Throwable e) {
            return null;
        }
    }

    public static boolean isMediumWeightSupported() {
        return FontHelper.isMediumWeightSupported();
    }

    public static boolean isItalicSupported() {
        return true;
    }

    private FontUtils() {
    }
}

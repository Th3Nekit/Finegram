/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.utils.ui;

import android.graphics.Typeface;

import com.th3nekit.finegram.helpers.ui.FontHelper;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;

/**
 * Какой системный шрифт стоит за каждым начертанием клиента.
 *
 * Клиент просит шрифты по старым именам файлов («fonts/rmedium.ttf» и т. п.), а берёт
 * системные. Соответствие живёт здесь, в одном методе: плагины со своими шрифтами
 * перехватывают его и подменяют начертание сразу во всём клиенте.
 */
public final class FontUtils {

    /**
     * Системный шрифт для имени начертания; null — это не системное начертание, файл
     * надо брать из ассетов как есть.
     */
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

    /** Шрифт, который лежит в ассетах клиента. */
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

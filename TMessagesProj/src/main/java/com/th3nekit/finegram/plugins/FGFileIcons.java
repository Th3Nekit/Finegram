/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.plugins;

import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;

import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Значки файлов, которые дали плагины.
 *
 * Плагин говорит: файлы с таким расширением рисуй вот этим. Так Custom Profile показывает
 * свои рамки вместо серого прямоугольника с надписью «FRAME». Храним по расширению в нижнем
 * регистре: в чате оно приходит и так, и так, а разбираться в этом плагину незачем.
 *
 * Рисунок общий на все ячейки, поэтому перед отрисовкой ему задают границы — как и любому
 * другому Drawable в списке сообщений.
 */
public final class FGFileIcons {

    private static final Map<String, Drawable> icons = new ConcurrentHashMap<>();

    private FGFileIcons() {
    }

    /** Возвращает сам рисунок: плагин считает null отказом. */
    @Nullable
    public static Object register(String extension, Object drawable) {
        if (extension == null || extension.isEmpty() || !(drawable instanceof Drawable)) {
            return null;
        }
        icons.put(normalize(extension), (Drawable) drawable);
        return drawable;
    }

    public static void unregister(String extension) {
        if (extension == null) return;
        icons.remove(normalize(extension));
    }

    /** Есть ли чужой значок для такого файла. */
    @Nullable
    public static Drawable iconFor(@Nullable String fileName) {
        if (fileName == null || icons.isEmpty()) return null;
        int dot = fileName.lastIndexOf('.');
        if (dot < 0 || dot == fileName.length() - 1) return null;
        return icons.get(normalize(fileName.substring(dot + 1)));
    }

    public static boolean isEmpty() {
        return icons.isEmpty();
    }

    private static String normalize(String extension) {
        return extension.trim().toLowerCase(Locale.ROOT);
    }
}

/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.plugins;

import java.io.File;
import java.io.IOException;
import java.util.function.Predicate;

public final class FGPluginFiles {
    private FGPluginFiles() {}

    public static boolean validId(String id) {
        return id != null && id.matches("[A-Za-z0-9_-][A-Za-z0-9._-]{0,99}")
                && !id.equals(".") && !id.equals("..");
    }

    public static File staging(File cache) throws IOException {
        File dir = new File(cache, "plugin-review");
        if (!dir.isDirectory() && !dir.mkdirs()) throw new IOException("Cannot create plugin staging");
        return dir;
    }

    public static boolean isLegacyPlaceholder(File file, String id, String name, String version,
            String author, String description, String icon) {
        String filename = file.getName();
        return filename.matches("(?:-?plugin|finegram-preview|finegram-plugin)\\d+\\.plugin")
                && filename.equals(id + ".plugin") && (blank(name) || id.equals(name))
                && blank(version) && blank(author) && blank(description) && blank(icon);
    }

    private static boolean blank(String value) {
        return value == null || value.trim().isEmpty();
    }

    public static void quarantineLegacy(File root, File quarantine, long now,
                                        Predicate<File> invalidAndInactive) {
        File[] files = root.listFiles();
        if (files == null) return;
        for (File file : files) {
            if (!file.getName().matches("(?:-?plugin|finegram-preview|finegram-plugin)\\d+\\.plugin")
                    || !file.isFile() || file.length() > 2 * 1024 * 1024
                    || file.lastModified() <= 0 || now - file.lastModified() < 86400000L) continue;
            try {
                if (!file.getCanonicalFile().equals(file.getAbsoluteFile())
                        || !root.getCanonicalFile().equals(file.getCanonicalFile().getParentFile())
                        || !invalidAndInactive.test(file)) continue;
                if (!quarantine.isDirectory() && !quarantine.mkdirs()) continue;
                File target = new File(quarantine, file.getName());
                if (!target.exists()) file.renameTo(target);
            } catch (IOException | SecurityException ignored) {
            }
        }
    }
}

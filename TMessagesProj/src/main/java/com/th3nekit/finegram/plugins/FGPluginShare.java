/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.plugins;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.Components.BulletinFactory;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import com.th3nekit.finegram.core.FinegramLogger;
import com.th3nekit.finegram.helpers.FGFileSender;

public final class FGPluginShare {

    private FGPluginShare() {
    }

    public static void share(BaseFragment fragment, String pluginId) {
        if (fragment == null || pluginId == null) {
            return;
        }
        FGPluginsController.INSTANCE.getQueue().postRunnable(() -> {
            final File file = prepare(pluginId);
            AndroidUtilities.runOnUIThread(() -> {
                if (fragment.getParentActivity() == null) {
                    return;
                }
                if (file == null) {
                    BulletinFactory.of(fragment).createErrorBulletin(
                            LocaleController.getString(R.string.FG_Plugins_ShareFailed)).show();
                    return;
                }
                FGFileSender.pickChatAndSend(fragment, file, null, dialogId ->
                        BulletinFactory.of(fragment).createSimpleBulletin(R.raw.forward,
                                LocaleController.getString(R.string.FG_Plugins_Shared)).show());
            });
        });
    }

    private static File prepare(String pluginId) {
        try {
            final File dir = new File(ApplicationLoader.applicationContext.getCacheDir(), "fg-share");
            dir.mkdirs();
            for (FGPluginsController.Plugin plugin : FGPluginsController.INSTANCE.installed()) {
                if (!plugin.getId().equals(pluginId)) {
                    continue;
                }
                final File source = plugin.getFile();
                if (source.isDirectory()) {
                    final File target = new File(dir, fileName(plugin.getName(), pluginId) + "." + FGPluginBundle.EXTENSION);
                    zip(source, target);
                    return target;
                }
                final File target = new File(dir, fileName(plugin.getName(), pluginId) + extensionOf(source));
                copy(source, target);
                return target;
            }
            for (FGDexPlugins.Plugin plugin : FGDexPlugins.INSTANCE.installed()) {
                if (!plugin.getId().equals(pluginId)) {
                    continue;
                }
                final File source = plugin.getFile();
                final File target = new File(dir, fileName(plugin.getName(), pluginId) + extensionOf(source));
                copy(source, target);
                return target;
            }
        } catch (Throwable e) {
            FinegramLogger.e("FGPlugins", () -> "плагин " + pluginId + " не собрался для отправки", e);
        }
        return null;
    }

    private static String fileName(String name, String fallback) {
        String clean = name == null ? "" : name.replaceAll("[\\\\/:*?\"<>|\\n\\r\\t]", " ").trim();
        if (clean.length() > 60) {
            clean = clean.substring(0, 60).trim();
        }
        return clean.isEmpty() ? fallback : clean;
    }

    private static String extensionOf(File file) {
        final String name = file.getName();
        final int dot = name.lastIndexOf('.');
        return dot > 0 ? name.substring(dot) : ".plugin";
    }

    private static void copy(File source, File target) throws Exception {
        try (FileInputStream in = new FileInputStream(source); FileOutputStream out = new FileOutputStream(target)) {
            final byte[] buffer = new byte[64 * 1024];
            int read;
            while ((read = in.read(buffer)) > 0) {
                out.write(buffer, 0, read);
            }
        }
    }

    private static void zip(File root, File target) throws Exception {
        try (ZipOutputStream out = new ZipOutputStream(new FileOutputStream(target))) {
            addToZip(out, root, "");
        }
    }

    private static void addToZip(ZipOutputStream out, File file, String path) throws Exception {
        final File[] children = file.listFiles();
        if (children == null) {
            return;
        }
        for (File child : children) {
            final String entry = path.isEmpty() ? child.getName() : path + "/" + child.getName();
            if (child.isDirectory()) {
                addToZip(out, child, entry);
                continue;
            }

            if (entry.contains("__pycache__") || entry.endsWith(".pyc")) {
                continue;
            }
            out.putNextEntry(new ZipEntry(entry));
            try (FileInputStream in = new FileInputStream(child)) {
                final byte[] buffer = new byte[64 * 1024];
                int read;
                while ((read = in.read(buffer)) > 0) {
                    out.write(buffer, 0, read);
                }
            }
            out.closeEntry();
        }
    }
}

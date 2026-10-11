/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins;

import static org.telegram.messenger.LocaleController.formatString;
import static org.telegram.messenger.LocaleController.getString;

import com.exteragram.messenger.plugins.ui.components.InstallPluginBottomSheet;

import org.telegram.messenger.FileLoader;
import org.telegram.messenger.FileLog;
import org.telegram.messenger.MessageObject;
import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.Components.BulletinFactory;

import java.io.File;

public final class FGPluginsHook {

    private FGPluginsHook() {
    }

    public static boolean handleMessage(MessageObject message, String fileName) {
        try {
            File file = resolveFile(message);
            if (file == null) {
                android.util.Log.w("FGOpen", "plugin: file is not on disk: " + fileName);
                return false;
            }

            BaseFragment fragment = com.th3nekit.finegram.core.ui.FGHostScreen.find();
            if (fragment == null) {
                android.util.Log.w("FGOpen", "plugin: no live screen to show the sheet over");
                return false;
            }

            boolean isDex = FGDexPlugins.looksLikePlugin(fileName);
            return isDex ? showDexSheet(fragment, file) : showPythonSheet(fragment, file, message);
        } catch (Throwable e) {
            android.util.Log.w("FGOpen", "plugin: sheet failed", e);
            FileLog.e(e);
            return false;
        }
    }

    public static boolean handleFile(MessageObject message, String fileName) {
        try {
            File file = resolveFile(message);
            if (file == null) {
                return false;
            }
            return FGPluginsDispatcher.onFileOpened(fileName, file.getAbsolutePath(), message);
        } catch (Throwable e) {
            FileLog.e(e);
            return false;
        }
    }

    private static File resolveFile(MessageObject message) {
        File file = null;
        if (message.messageOwner.attachPath != null && message.messageOwner.attachPath.length() != 0) {
            file = new File(message.messageOwner.attachPath);
        }
        if (file == null || !file.exists()) {
            file = FileLoader.getInstance(message.currentAccount).getPathToMessage(message.messageOwner);
        }
        return file != null && file.exists() ? file : null;
    }

    public static boolean promptInstall(BaseFragment fragment, File file) {
        if (fragment == null || file == null || !file.exists()) {
            return false;
        }
        if (FGDexPlugins.looksLikePlugin(file.getName())) {
            return showDexSheet(fragment, file);
        }
        return showPythonSheet(fragment, file, null);
    }

    private static boolean showPythonSheet(BaseFragment fragment, File file, MessageObject message) {
        FGPluginsController.Plugin preview = FGPluginsController.INSTANCE.readPreview(file);
        if (preview == null) {
            android.util.Log.w("FGOpen", "plugin: header not readable: " + file.getName());
            return false;
        }
        return showSheet(fragment, preview.getId(), preview.getName(), preview.getVersion(),
                preview.getAuthor(), preview.getDescription(), preview.getIcon(), file,
                () -> FGPluginsController.INSTANCE.getQueue().postRunnable(() -> {

                    FGPluginsController.Plugin installed = FGPluginsController.INSTANCE.install(file);
                    if (installed == null) {
                        AndroidUtilities.runOnUIThread(FGPluginsHook::reportBadFile);
                        return;
                    }
                    FGPluginUpdates.rememberFromMessage(installed.getId(), message);
                    boolean started = FGPluginsController.INSTANCE.enable(installed);
                    AndroidUtilities.runOnUIThread(() ->
                            report(installed.getId(), installed.getName(), started));
                }));
    }

    private static boolean showDexSheet(BaseFragment fragment, File file) {
        FGDexPlugins.Plugin preview = FGDexPlugins.INSTANCE.readPreview(file);
        if (preview == null) {
            android.util.Log.w("FGOpen", "dex plugin: header not readable: " + file.getName());
            return false;
        }
        return showSheet(fragment, preview.getId(), preview.getName(), preview.getVersion(),
                preview.getAuthor(), preview.getDescription(), null, file,
                () -> FGPluginsController.INSTANCE.getQueue().postRunnable(() -> {
                    FGDexPlugins.Plugin installed = FGDexPlugins.INSTANCE.install(file);
                    if (installed == null) {
                        AndroidUtilities.runOnUIThread(FGPluginsHook::reportBadFile);
                        return;
                    }
                    boolean started = FGDexPlugins.INSTANCE.enable(installed);
                    AndroidUtilities.runOnUIThread(() ->
                            report(installed.getId(), installed.getName(), started));
                }));
    }

    private static boolean showSheet(BaseFragment fragment, String id, String name, String version,
                                     String author, String description, String icon, File file,
                                     Runnable onInstall) {
        if (fragment == null || fragment.getParentActivity() == null) {
            return false;
        }
        InstallPluginBottomSheet.InstallParams params =
                new InstallPluginBottomSheet.InstallParams(file.getAbsolutePath());
        params.id = id;
        params.name = name;
        params.version = version;
        params.author = author;
        params.description = description;
        params.icon = icon;
        fragment.showDialog(new InstallPluginBottomSheet(fragment.getParentActivity(),
                fragment.getResourceProvider(), params, ignored -> onInstall.run()));
        return true;
    }

    private static void reportBadFile() {
        BaseFragment current = com.th3nekit.finegram.core.ui.FGHostScreen.find();
        if (current != null) {
            BulletinFactory.of(current).createErrorBulletin(getString(R.string.FG_Plugins_BadFile)).show();
        }
    }

    private static void report(String id, String name, boolean started) {
        BaseFragment current = com.th3nekit.finegram.core.ui.FGHostScreen.find();
        if (current == null) {
            return;
        }
        if (!started && FGPluginsController.blockedByBypass(id)) {

            BulletinFactory.of(current).createSimpleBulletin(R.raw.info,
                    getString(R.string.FG_Plugins_BypassClash)).show();
            return;
        }
        if (started) {
            BulletinFactory.of(current).createSimpleBulletin(
                    R.raw.done,
                    formatString(R.string.FG_Plugins_Installed_Toast, name)
            ).show();
        } else {
            BulletinFactory.of(current).createErrorBulletin(
                    formatString(R.string.FG_Plugins_Failed, name)
            ).show();
        }
    }
}

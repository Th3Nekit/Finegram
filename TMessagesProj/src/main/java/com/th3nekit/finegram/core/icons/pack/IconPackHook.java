/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.icons.pack;

import org.telegram.messenger.FileLoader;
import org.telegram.messenger.FileLog;
import org.telegram.messenger.MessageObject;
import org.telegram.ui.ActionBar.BaseFragment;

import java.io.File;

public final class IconPackHook {

    private IconPackHook() {
    }

    public static boolean handleMessage(MessageObject message) {
        try {
            if (message == null || message.messageOwner == null) {
                return false;
            }
            String fileName = message.getDocumentName();
            if (fileName == null) {
                return false;
            }
            String lower = fileName.toLowerCase();

            if (lower.endsWith(".plugin") || lower.endsWith(".dex") || lower.endsWith(".jar")
                    || lower.endsWith(".eaf") || lower.endsWith(".elyx")) {
                return com.th3nekit.finegram.plugins.FGPluginsHook.handleMessage(message, fileName);
            }

            if (com.th3nekit.finegram.plugins.FGPluginsHook.handleFile(message, fileName)) {
                return true;
            }
            if (!lower.endsWith(".icons")) {
                return false;
            }

            File file = null;
            if (message.messageOwner.attachPath != null && message.messageOwner.attachPath.length() != 0) {
                file = new File(message.messageOwner.attachPath);
            }
            if (file == null || !file.exists()) {
                file = FileLoader.getInstance(message.currentAccount).getPathToMessage(message.messageOwner);
            }
            if (file == null || !file.exists()) {
                return false;
            }

            BaseFragment fragment = com.th3nekit.finegram.core.ui.FGHostScreen.find();
            return IconPackPreviewSheet.showIfIconPack(file, fileName, fragment);
        } catch (Throwable e) {
            FileLog.e(e);
            return false;
        }
    }
}

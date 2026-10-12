/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.privacy;

import org.telegram.messenger.ChatObject;
import org.telegram.messenger.DialogObject;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.UserConfig;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ChatActivity;

import java.util.ArrayList;

import com.th3nekit.finegram.core.configs.FinegramPrivacyConfig;

public final class FGDeletedKeeper {

    private FGDeletedKeeper() {
    }

    public static ArrayList<Integer> keep(int account, long dialogId, ArrayList<Integer> messages, int mode) {
        if (messages == null || messages.isEmpty()) return messages;
        if (!enabledFor(account, dialogId, mode)) return messages;

        final ArrayList<Integer> temporary = new ArrayList<>();
        final ArrayList<Integer> real = new ArrayList<>();
        for (Integer id : messages) {
            if (id == null) continue;
            if (id > 0) {
                real.add(id);
            } else {
                temporary.add(id);
            }
        }
        if (real.isEmpty()) return messages;

        FGDeletedStore.remember(account, dialogId, real);
        return temporary;
    }

    public static boolean enabledFor(int account, long dialogId, int mode) {
        if (!FinegramPrivacyConfig.INSTANCE.getSaveDeleted()) return false;
        if (FGDeletedExceptions.excluded(account, dialogId)) return false;

        if (mode == ChatActivity.MODE_SCHEDULED || mode == ChatActivity.MODE_QUICK_REPLIES
                || mode == ChatActivity.MODE_WELCOME_MESSAGES) {
            return false;
        }

        if (clearingHistory()) return false;
        return !skipDialog(account, dialogId);
    }

    public static ArrayList<Integer> holdInOpenChat(int account, long dialogId, java.util.List<Integer> messages, int mode) {
        final ArrayList<Integer> held = new ArrayList<>();
        if (messages == null || messages.isEmpty()) return held;
        if (!enabledFor(account, dialogId, mode)) return held;
        for (Integer id : messages) {

            if (id != null && id > 0) held.add(id);
        }
        if (!held.isEmpty()) {
            FGDeletedStore.markNow(account, dialogId, held);
        }
        return held;
    }

    private static boolean clearingHistory() {
        try {
            for (StackTraceElement element : Thread.currentThread().getStackTrace()) {
                final String method = element.getMethodName();
                if ("deleteMessagesRange".equals(method)
                        || "deleteDialog".equals(method)
                        || "clearUserPhotos".equals(method)) {
                    return true;
                }
            }
        } catch (Throwable ignored) {
        }
        return false;
    }

    private static boolean skipDialog(int account, long dialogId) {
        try {

            if (dialogId == 0) return true;
            if (DialogObject.isEncryptedDialog(dialogId)) return true;

            if (dialogId == UserConfig.getInstance(account).getClientUserId()) return true;

            if (dialogId > 0) {
                final TLRPC.User user = MessagesController.getInstance(account).getUser(dialogId);
                if (user != null && user.bot && !FinegramPrivacyConfig.INSTANCE.getSaveDeletedFromBots()) {
                    return true;
                }
            } else if (!FinegramPrivacyConfig.INSTANCE.getSaveDeletedFromChannels()) {
                final TLRPC.Chat chat = MessagesController.getInstance(account).getChat(-dialogId);
                if (chat != null && ChatObject.isChannelAndNotMegaGroup(chat)) {
                    return true;
                }
            }
        } catch (Throwable ignored) {
        }
        return false;
    }
}

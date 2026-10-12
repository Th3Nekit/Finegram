/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.helpers;

import android.os.Bundle;

import org.telegram.messenger.AccountInstance;
import org.telegram.messenger.FileLoader;
import org.telegram.messenger.MessageObject;
import org.telegram.messenger.MessagesStorage;
import org.telegram.messenger.SendMessagesHelper;
import org.telegram.messenger.UserConfig;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.Components.AlertsCreator;
import org.telegram.ui.DialogsActivity;

import java.io.File;

public final class FGFileSender {

    private FGFileSender() {
    }

    public interface Callback {
        void onSent(long dialogId);
    }

    public static void pickChatAndSend(BaseFragment fragment, File file, String caption) {
        pickChatAndSend(fragment, file, caption, null);
    }

    public static void pickChatAndSend(BaseFragment fragment, File file, String caption, Callback callback) {
        if (fragment == null || fragment.getParentActivity() == null || file == null
                || !file.isFile() || !file.canRead() || file.length() == 0) {
            return;
        }
        final int account = fragment.getCurrentAccount();
        Bundle args = new Bundle();
        args.putBoolean("onlySelect", true);
        args.putInt("dialogsType", DialogsActivity.DIALOGS_TYPE_FORWARD);
        args.putBoolean("canSelectTopics", true);

        DialogsActivity chooser = new DialogsActivity(args);
        chooser.setCurrentAccount(account);
        chooser.setDelegate((chooserFragment, dialogs, message, param, notify, scheduleDate, scheduleRepeatPeriod, topicsFragment) -> {
            if (dialogs == null || dialogs.isEmpty()) {
                return false;
            }
            for (MessagesStorage.TopicKey key : dialogs) {
                if (AlertsCreator.checkSlowMode(chooserFragment.getParentActivity(), account, key.dialogId, false)
                        || (key.topicId != 0 && topicMessage(account, key) == null)) {
                    return false;
                }
            }
            boolean queued = false;
            for (MessagesStorage.TopicKey key : dialogs) {
                queued |= send(account, file, caption, key, notify, scheduleDate, scheduleRepeatPeriod);
            }
            if (!queued) return false;
            if (topicsFragment != null) topicsFragment.finishFragment();
            chooserFragment.finishFragment();
            if (callback != null) {
                callback.onSent(dialogs.get(0).dialogId);
            }
            return true;
        });
        fragment.presentFragment(chooser);
    }

    public static void send(int account, File file, String caption, long dialogId) {
        send(account, file, caption, MessagesStorage.TopicKey.of(dialogId, 0), true, 0, 0);
    }

    private static MessageObject topicMessage(int account, MessagesStorage.TopicKey key) {
        TLRPC.TL_forumTopic topic = AccountInstance.getInstance(account).getMessagesController()
                .getTopicsController().findTopic(-key.dialogId, key.topicId);
        if (topic == null || topic.topicStartMessage == null) return null;
        MessageObject message = new MessageObject(account, topic.topicStartMessage, false, false);
        message.isTopicMainMessage = true;
        return message;
    }

    private static boolean send(int account, File file, String caption, MessagesStorage.TopicKey key,
                                boolean notify, int scheduleDate, int scheduleRepeatPeriod) {
        if (file == null || !file.isFile() || !file.canRead() || file.length() == 0
                || !FileLoader.checkUploadFileSize(account, file.length())) return false;
        AccountInstance instance = AccountInstance.getInstance(account);
        MessageObject topic = key.topicId == 0 ? null : topicMessage(account, key);
        if (key.topicId != 0 && topic == null) return false;
        TLRPC.TL_document document = new TLRPC.TL_document();
        document.file_reference = new byte[0];
        document.date = instance.getConnectionsManager().getCurrentTime();
        document.size = file.length();
        document.mime_type = file.getName().endsWith(".log") ? "text/plain" : "application/octet-stream";
        TLRPC.TL_documentAttributeFilename name = new TLRPC.TL_documentAttributeFilename();
        name.file_name = file.getName();
        document.attributes.add(name);
        instance.getSendMessagesHelper().sendMessage(SendMessagesHelper.SendMessageParams.of(
                document, null, file.getAbsolutePath(), key.dialogId, topic, topic, caption,
                null, null, null, notify, scheduleDate, scheduleRepeatPeriod, 0, null, null, false));
        return true;
    }

    public static void sendToSaved(int account, File file, String caption) {
        send(account, file, caption, UserConfig.getInstance(account).getClientUserId());
    }
}

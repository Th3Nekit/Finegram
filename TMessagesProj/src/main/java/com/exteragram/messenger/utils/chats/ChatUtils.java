/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.utils.chats;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.MessageObject;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.MessagesStorage;
import org.telegram.messenger.UserConfig;
import org.telegram.messenger.Utilities;
import org.telegram.tgnet.ConnectionsManager;
import org.telegram.tgnet.TLRPC;

public final class ChatUtils {

    private final int account;

    private ChatUtils(int account) {
        this.account = account;
    }

    public static ChatUtils getInstance() {
        return getInstance(UserConfig.selectedAccount);
    }

    public static ChatUtils getInstance(int account) {
        return new ChatUtils(account);
    }

    public void resolveChannel(String username, Utilities.Callback<TLRPC.Chat> callback) {
        if (callback == null) {
            return;
        }
        if (username == null || username.isEmpty()) {
            callback.run(null);
            return;
        }
        String name = username.startsWith("@") ? username.substring(1) : username;

        TLRPC.TL_contacts_resolveUsername request = new TLRPC.TL_contacts_resolveUsername();
        request.username = name;
        ConnectionsManager.getInstance(account).sendRequest(request, (response, error) -> {
            TLRPC.Chat chat = null;
            if (response instanceof TLRPC.TL_contacts_resolvedPeer) {
                TLRPC.TL_contacts_resolvedPeer resolved = (TLRPC.TL_contacts_resolvedPeer) response;
                MessagesController.getInstance(account).putChats(resolved.chats, false);
                MessagesController.getInstance(account).putUsers(resolved.users, false);
                MessagesStorage.getInstance(account).putUsersAndChats(resolved.users, resolved.chats, false, true);
                if (!resolved.chats.isEmpty()) {
                    chat = resolved.chats.get(0);
                }
            }
            final TLRPC.Chat found = chat;
            AndroidUtilities.runOnUIThread(() -> callback.run(found));
        });
    }

    public static final org.telegram.messenger.DispatchQueue utilsQueue =
            new org.telegram.messenger.DispatchQueue("chatUtilsQueue");

    public MessagesController getMessagesController() {
        return MessagesController.getInstance(account);
    }

    public MessagesStorage getMessageStorage() {
        return MessagesStorage.getInstance(account);
    }

    public UserConfig getUserConfig() {
        return UserConfig.getInstance(account);
    }

    public ConnectionsManager getConnectionsManager() {
        return ConnectionsManager.getInstance(account);
    }

    public org.telegram.messenger.FileLoader getFileLoader() {
        return org.telegram.messenger.FileLoader.getInstance(account);
    }

    public CharSequence getMessageText(MessageObject message, MessageObject.GroupedMessages group) {
        if (group != null && group.messages != null) {
            for (MessageObject part : group.messages) {
                if (part != null && !android.text.TextUtils.isEmpty(part.caption)) {
                    return part.caption;
                }
            }
        }
        if (message == null) {
            return null;
        }
        if (!android.text.TextUtils.isEmpty(message.caption)) {
            return message.caption;
        }
        return message.messageText;
    }

    public String getName(long dialogId) {
        if (dialogId > 0) {
            final TLRPC.User user = getMessagesController().getUser(dialogId);
            return user == null ? null : org.telegram.messenger.UserObject.getUserName(user);
        }
        final TLRPC.Chat chat = getMessagesController().getChat(-dialogId);
        return chat == null ? null : chat.title;
    }

    public void saveStickerToGallery(android.app.Activity activity, MessageObject message,
                                     Utilities.Callback<Object> callback) {
        saveStickerToGallery(activity, message == null ? null : message.getDocument(), callback);
    }

    public void saveStickerToGallery(android.app.Activity activity, TLRPC.Document document,
                                     Utilities.Callback<Object> callback) {
        if (document == null) {
            finish(callback, false);
            return;
        }
        final java.io.File file = getFileLoader().getPathToAttach(document, true);
        saveStickerToGallery(activity, file == null ? null : file.getAbsolutePath(),
                MessageObject.isVideoStickerDocument(document), callback);
    }

    public void saveStickerToGallery(android.app.Activity activity, String path, boolean video,
                                     Utilities.Callback<Object> callback) {
        if (activity == null || path == null || !new java.io.File(path).exists()) {
            finish(callback, false);
            return;
        }
        if (video) {
            org.telegram.messenger.MediaController.saveFile(path, activity, 1, null, "video/webm",
                    uri -> finish(callback, uri != null));
            return;
        }
        utilsQueue.postRunnable(() -> {
            boolean ok = false;
            try {
                final android.graphics.Bitmap bitmap = android.graphics.BitmapFactory.decodeFile(path);
                if (bitmap != null) {
                    final android.content.ContentValues values = new android.content.ContentValues();
                    values.put(android.provider.MediaStore.Images.Media.DISPLAY_NAME,
                            "sticker_" + System.currentTimeMillis() + ".png");
                    values.put(android.provider.MediaStore.Images.Media.MIME_TYPE, "image/png");
                    final android.net.Uri uri = activity.getContentResolver().insert(
                            android.provider.MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values);
                    if (uri != null) {
                        try (java.io.OutputStream out = activity.getContentResolver().openOutputStream(uri)) {
                            ok = out != null && bitmap.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, out);
                        }
                    }
                    bitmap.recycle();
                }
            } catch (Throwable ignored) {
            }
            finish(callback, ok);
        });
    }

    private static void finish(Utilities.Callback<Object> callback, boolean ok) {
        if (callback != null) {
            AndroidUtilities.runOnUIThread(() -> callback.run(ok));
        }
    }

    public String getPathToMessage(MessageObject messageObject) {
        if (messageObject == null) {
            return null;
        }
        try {
            return org.telegram.messenger.FileLoader.getInstance(account)
                    .getPathToMessage(messageObject.messageOwner).toString();
        } catch (Throwable e) {
            return null;
        }
    }
}

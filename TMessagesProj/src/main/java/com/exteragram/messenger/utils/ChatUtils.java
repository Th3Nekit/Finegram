/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.utils;

import org.telegram.messenger.MessageObject;
import org.telegram.messenger.UserConfig;
import org.telegram.tgnet.TLRPC;
import org.telegram.messenger.Utilities;

public final class ChatUtils {

    private final com.exteragram.messenger.utils.chats.ChatUtils real;

    private ChatUtils(com.exteragram.messenger.utils.chats.ChatUtils real) {
        this.real = real;
    }

    public static ChatUtils getInstance() {
        return getInstance(UserConfig.selectedAccount);
    }

    public static ChatUtils getInstance(int account) {
        return new ChatUtils(com.exteragram.messenger.utils.chats.ChatUtils.getInstance(account));
    }

    public String getPathToMessage(MessageObject messageObject) {
        return real.getPathToMessage(messageObject);
    }

    public void resolveChannel(String username, Utilities.Callback<TLRPC.Chat> callback) {
        real.resolveChannel(username, callback);
    }
}

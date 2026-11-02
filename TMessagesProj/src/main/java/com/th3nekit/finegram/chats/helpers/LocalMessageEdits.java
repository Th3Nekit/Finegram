package com.th3nekit.finegram.chats.helpers;

import java.util.IdentityHashMap;
import org.telegram.messenger.MessageObject;

public final class LocalMessageEdits {
    private final IdentityHashMap<MessageObject, Boolean> messages = new IdentityHashMap<>();

    public void apply(MessageObject message, String text) {
        if (message == null || text == null || text.trim().isEmpty()) return;
        messages.put(message, Boolean.TRUE);
        message.setLocalText(text);
    }

    public void clear() {
        for (MessageObject message : messages.keySet()) message.setLocalText(null);
        messages.clear();
    }
}

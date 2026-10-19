package com.th3nekit.finegram.core;

import android.content.SharedPreferences;
import android.util.Base64;

import org.telegram.messenger.DialogObject;
import org.telegram.messenger.FileLog;
import org.telegram.tgnet.SerializedData;
import org.telegram.tgnet.TLRPC;

import java.util.ArrayList;
import java.util.WeakHashMap;

public final class LocalPremiumEmojiStatus {
    private static final String STATUS_KEY = "finegramLocalEmojiStatus";
    private static final String USER_KEY = "finegramLocalEmojiStatusUserId";

    private final SharedPreferences preferences;
    private final WeakHashMap<TLRPC.User, TLRPC.EmojiStatus> serverStatuses = new WeakHashMap<>();
    private TLRPC.EmojiStatus localStatus;
    private long userId;

    public LocalPremiumEmojiStatus(SharedPreferences preferences) {
        this.preferences = preferences;
        try {
            String encoded = preferences.getString(STATUS_KEY, null);
            if (encoded != null) {
                localStatus = deserialize(encoded);
                userId = preferences.getLong(USER_KEY, 0);
            }
        } catch (Exception e) {
            FileLog.e(e);
        }
    }

    private static TLRPC.EmojiStatus deserialize(String encoded) {
        SerializedData data = new SerializedData(Base64.decode(encoded, Base64.DEFAULT));
        try {
            TLRPC.EmojiStatus status = TLRPC.EmojiStatus.TLdeserialize(data, data.readInt32(true), true);
            if (status instanceof TLRPC.TL_inputEmojiStatusCollectible || data.remaining() != 0) {
                throw new IllegalArgumentException("Invalid local emoji status");
            }
            return status;
        } finally {
            data.cleanup();
        }
    }

    public synchronized boolean set(long userId, TLRPC.EmojiStatus status) {
        if (userId == 0 || status instanceof TLRPC.TL_inputEmojiStatusCollectible) {
            return false;
        }
        SerializedData data = new SerializedData();
        try {
            if (status == null) {
                status = new TLRPC.TL_emojiStatusEmpty();
            }
            status.serializeToStream(data);
            String encoded = Base64.encodeToString(data.toByteArray(), Base64.NO_WRAP);
            TLRPC.EmojiStatus savedStatus = deserialize(encoded);
            preferences.edit().putString(STATUS_KEY, encoded).putLong(USER_KEY, userId).apply();
            localStatus = savedStatus;
            this.userId = userId;
            return true;
        } catch (Exception e) {
            FileLog.e(e);
            return false;
        } finally {
            data.cleanup();
        }
    }

    public synchronized void apply(TLRPC.User user, long accountUserId, boolean enabled) {
        if (user == null || user.id != accountUserId) {
            return;
        }
        if (serverStatuses.containsKey(user)) {
            user.emoji_status = serverStatuses.remove(user);
        }
        int until = DialogObject.getEmojiStatusUntil(localStatus);
        if (enabled && !user.premium && localStatus != null && userId == user.id
                && (until == 0 || until > System.currentTimeMillis() / 1000L)) {
            serverStatuses.put(user, user.emoji_status);
            user.emoji_status = localStatus;
        }
    }

    public synchronized void updateFromServer(TLRPC.User user, TLRPC.EmojiStatus status, long accountUserId, boolean enabled) {
        if (user == null) {
            return;
        }
        serverStatuses.remove(user);
        user.emoji_status = status;
        apply(user, accountUserId, enabled);
    }

    public synchronized void refresh(long accountUserId, boolean enabled) {
        for (TLRPC.User user : new ArrayList<>(serverStatuses.keySet())) {
            apply(user, accountUserId, enabled);
        }
    }

    public synchronized TLRPC.EmojiStatus getServerStatus(TLRPC.User user) {
        return serverStatuses.containsKey(user) ? serverStatuses.get(user) : user.emoji_status;
    }

    public synchronized void clear() {
        for (TLRPC.User user : new ArrayList<>(serverStatuses.keySet())) {
            user.emoji_status = serverStatuses.get(user);
        }
        serverStatuses.clear();
        localStatus = null;
        userId = 0;
        preferences.edit().remove(STATUS_KEY).remove(USER_KEY).apply();
    }
}

/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core;

import android.content.SharedPreferences;
import android.util.Base64;

import org.telegram.messenger.FileLog;
import org.telegram.tgnet.SerializedData;
import org.telegram.tgnet.TLRPC;

import java.util.ArrayList;
import java.util.WeakHashMap;

public final class LocalPremiumPeerColors {
    private static final String USER_KEY = "finegramLocalPeerColorsUserId";
    private static final String NAME_KEY = "finegramLocalNameColor";
    private static final String PROFILE_KEY = "finegramLocalProfileColor";
    private static final int COLOR_FLAGS = 256 | 512;

    private final SharedPreferences preferences;
    private final WeakHashMap<TLRPC.User, ServerColors> serverColors = new WeakHashMap<>();
    private String name;
    private String profile;
    private long userId;
    private int revision;

    public LocalPremiumPeerColors(SharedPreferences preferences) {
        this.preferences = preferences;
        try {
            String name = preferences.getString(NAME_KEY, null);
            String profile = preferences.getString(PROFILE_KEY, null);
            decode(name);
            decode(profile);
            userId = preferences.getLong(USER_KEY, 0);
            this.name = name;
            this.profile = profile;
        } catch (Exception e) {
            FileLog.e(e);
        }
    }

    private static String encode(TLRPC.PeerColor color) {
        if (color == null) return "";
        if (!(color instanceof TLRPC.TL_peerColor) && !(color instanceof TLRPC.TL_peerColorCollectible)) {
            throw new IllegalArgumentException("Invalid local peer color");
        }
        SerializedData data = new SerializedData();
        try {
            color.serializeToStream(data);
            return Base64.encodeToString(data.toByteArray(), Base64.NO_WRAP);
        } finally {
            data.cleanup();
        }
    }

    private static TLRPC.PeerColor decode(String encoded) {
        if (encoded == null || encoded.isEmpty()) return null;
        SerializedData data = new SerializedData(Base64.decode(encoded, Base64.DEFAULT));
        try {
            TLRPC.PeerColor color = TLRPC.PeerColor.TLdeserialize(data, data.readInt32(true), true);
            if (color instanceof TLRPC.TL_inputPeerColorCollectible || data.remaining() != 0) {
                throw new IllegalArgumentException("Invalid local peer color");
            }
            return color;
        } finally {
            data.cleanup();
        }
    }

    private static TLRPC.PeerColor copy(TLRPC.PeerColor color) {
        return decode(encode(color));
    }

    public synchronized boolean set(long userId, boolean nameChanged, TLRPC.PeerColor name,
                                    boolean profileChanged, TLRPC.PeerColor profile) {
        if (userId == 0) return false;
        if (!nameChanged && !profileChanged) return true;
        try {
            String nextName = nameChanged ? encode(name) : this.userId == userId ? this.name : null;
            String nextProfile = profileChanged ? encode(profile) : this.userId == userId ? this.profile : null;
            decode(nextName);
            decode(nextProfile);
            SharedPreferences.Editor editor = preferences.edit().putLong(USER_KEY, userId);
            if (nextName == null) editor.remove(NAME_KEY); else editor.putString(NAME_KEY, nextName);
            if (nextProfile == null) editor.remove(PROFILE_KEY); else editor.putString(PROFILE_KEY, nextProfile);
            editor.apply();
            this.name = nextName;
            this.profile = nextProfile;
            this.userId = userId;
            revision++;
            return true;
        } catch (Exception e) {
            FileLog.e(e);
            return false;
        }
    }

    public synchronized void apply(TLRPC.User user, long accountUserId, boolean enabled) {
        if (user == null) return;
        ServerColors previous = serverColors.get(user);
        boolean active = enabled && !user.premium && user.id == accountUserId && user.id == userId
                && (name != null || profile != null);
        if (active && previous != null && previous.revision == revision
                && user.color == previous.appliedName && user.profile_color == previous.appliedProfile) return;
        if (previous != null) {
            previous.restore(user);
            serverColors.remove(user);
        }
        if (!active) return;
        try {
            ServerColors saved = new ServerColors(user);
            TLRPC.PeerColor localName = name == null ? user.color : decode(name);
            TLRPC.PeerColor localProfile = profile == null ? user.profile_color : decode(profile);
            user.color = localName;
            user.profile_color = localProfile;
            if (name != null) user.flags2 = TLRPC.User.setFlag(user.flags2, 256, localName != null);
            if (profile != null) user.flags2 = TLRPC.User.setFlag(user.flags2, 512, localProfile != null);
            saved.nameOverridden = name != null;
            saved.profileOverridden = profile != null;
            saved.appliedName = localName;
            saved.appliedProfile = localProfile;
            saved.revision = revision;
            serverColors.put(user, saved);
        } catch (Exception e) {
            FileLog.e(e);
        }
    }

    public synchronized void refresh(long accountUserId, boolean enabled) {
        for (TLRPC.User user : new ArrayList<>(serverColors.keySet())) {
            apply(user, accountUserId, enabled);
        }
    }

    public synchronized void serializeUser(TLRPC.User user, SerializedData data) {
        ServerColors saved = serverColors.get(user);
        if (saved == null) {
            user.serializeToStream(data);
            return;
        }
        SerializedData serialized = new SerializedData();
        SerializedData input = null;
        try {
            user.serializeToStream(serialized);
            input = new SerializedData(serialized.toByteArray());
            TLRPC.User copy = TLRPC.User.TLdeserialize(input, input.readInt32(true), true);
            saved.restore(copy, user);
            copy.serializeToStream(data);
        } finally {
            if (input != null) input.cleanup();
            serialized.cleanup();
        }
    }

    public synchronized void clear() {
        for (TLRPC.User user : new ArrayList<>(serverColors.keySet())) {
            serverColors.get(user).restore(user);
        }
        serverColors.clear();
        name = profile = null;
        userId = 0;
        revision++;
        preferences.edit().remove(NAME_KEY).remove(PROFILE_KEY).remove(USER_KEY).apply();
    }

    private static final class ServerColors {
        private final TLRPC.PeerColor name;
        private final TLRPC.PeerColor profile;
        private final int flags;
        private TLRPC.PeerColor appliedName;
        private TLRPC.PeerColor appliedProfile;
        private int revision;
        private boolean nameOverridden;
        private boolean profileOverridden;

        private ServerColors(TLRPC.User user) {
            name = copy(user.color);
            profile = copy(user.profile_color);
            flags = user.flags2 & COLOR_FLAGS;
        }

        private void restore(TLRPC.User user) {
            restore(user, user);
        }

        private void restore(TLRPC.User target, TLRPC.User displayed) {
            if (nameOverridden && displayed.color == appliedName) {
                target.color = name;
                target.flags2 = target.flags2 & ~256 | flags & 256;
            }
            if (profileOverridden && displayed.profile_color == appliedProfile) {
                target.profile_color = profile;
                target.flags2 = target.flags2 & ~512 | flags & 512;
            }
        }
    }
}

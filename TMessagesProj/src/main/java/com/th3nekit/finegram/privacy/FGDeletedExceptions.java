package com.th3nekit.finegram.privacy;

import android.content.SharedPreferences;

import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.UserConfig;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

public final class FGDeletedExceptions {

    private FGDeletedExceptions() {
    }

    private static String key(int account) {
        if (account < 0 || account >= UserConfig.MAX_ACCOUNT_COUNT) return null;
        final long ownerId = UserConfig.getInstance(account).getClientUserId();
        return ownerId > 0 ? "dialogs_" + ownerId : null;
    }

    private static SharedPreferences preferences() {
        return ApplicationLoader.applicationContext.getSharedPreferences("fg_deleted_exceptions", 0);
    }

    public static boolean excluded(int account, long dialogId) {
        if (dialogId == 0) return false;
        final String key = key(account);
        return key != null && preferences().getStringSet(key, Collections.emptySet())
                .contains(String.valueOf(dialogId));
    }

    public static ArrayList<Long> dialogs(int account) {
        final ArrayList<Long> dialogs = new ArrayList<>();
        final String key = key(account);
        if (key == null) return dialogs;
        for (String value : preferences().getStringSet(key, Collections.emptySet())) {
            try {
                final long dialogId = Long.parseLong(value);
                if (dialogId != 0) dialogs.add(dialogId);
            } catch (NumberFormatException ignored) {
            }
        }
        Collections.sort(dialogs);
        return dialogs;
    }

    public static synchronized void set(int account, long dialogId, boolean excluded) {
        if (dialogId == 0) return;
        final String key = key(account);
        if (key == null) return;
        final SharedPreferences preferences = preferences();
        final Set<String> dialogs = new HashSet<>(preferences.getStringSet(key, Collections.emptySet()));
        final boolean changed = excluded ? dialogs.add(String.valueOf(dialogId))
                : dialogs.remove(String.valueOf(dialogId));
        if (changed) preferences.edit().putStringSet(key, dialogs).apply();
    }
}

package com.th3nekit.finegram.core.updater;

import android.content.SharedPreferences;

import org.telegram.messenger.ApplicationLoader;
import org.telegram.tgnet.TLRPC;

public final class FGUpdatePrompts {
    private static final long REMINDER_INTERVAL_MS = 24L * 60 * 60 * 1000;

    private FGUpdatePrompts() {
    }

    private static SharedPreferences preferences() {
        return ApplicationLoader.applicationContext.getSharedPreferences("finegram_update_prompts", 0);
    }

    public static boolean shouldPrompt(TLRPC.TL_help_appUpdate update, boolean manual,
                                       boolean requiredByMinimum, long now) {
        if (update == null) return false;
        if (manual || requiredByMinimum || update.can_not_skip) return true;
        String release = releaseKey(update, FGClientUpdates.getPending());
        SharedPreferences prefs = preferences();
        return !isRecent(prefs.getLong("shown_at_" + release, 0), now)
                && !isRecent(prefs.getLong("deferred_at_" + release, 0), now);
    }

    public static void recordShown(TLRPC.TL_help_appUpdate update, FGClientUpdates.Update direct, long now) {
        if (update == null) return;
        record("shown_at_", update, direct, now);
    }

    public static void defer(TLRPC.TL_help_appUpdate update, FGClientUpdates.Update direct, long now) {
        if (update == null || update.can_not_skip) return;
        record("deferred_at_", update, direct, now);
    }

    private static void record(String prefix, TLRPC.TL_help_appUpdate update, FGClientUpdates.Update direct, long now) {
        SharedPreferences prefs = preferences();
        SharedPreferences.Editor editor = prefs.edit();
        for (String key : prefs.getAll().keySet()) {
            if ((key.startsWith("shown_at_") || key.startsWith("deferred_at_"))
                    && !isRecent(prefs.getLong(key, 0), now)) {
                editor.remove(key);
            }
        }
        editor.putLong(prefix + releaseKey(update, direct), now).apply();
    }

    private static boolean isRecent(long timestamp, long now) {
        return timestamp > 0 && now >= timestamp && now - timestamp < REMINDER_INTERVAL_MS;
    }

    private static String releaseKey(TLRPC.TL_help_appUpdate update, FGClientUpdates.Update direct) {
        String flavor = update.build_flavor == null ? "" : update.build_flavor;
        String release = update.version + "|" + flavor;
        if (update.document == null && direct != null && direct.version.equals(update.version)
                && flavor.equals(direct.flavor == null ? "" : direct.flavor)) {
            return release + "|code:" + direct.versionCode;
        }
        return release + "|document:" + (update.document == null ? 0 : update.document.id);
    }
}

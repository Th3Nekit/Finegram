/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.bypass;

import android.content.Context;
import android.content.SharedPreferences;

import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.UserConfig;

import java.util.UUID;

public final class FGBypassStore {

    private static final String PREFS_NAME = "finegram_bypass_store";

    private static volatile SharedPreferences cached;

    private static SharedPreferences prefs() {
        SharedPreferences p = cached;
        if (p == null) {
            synchronized (FGBypassStore.class) {
                p = cached;
                if (p == null) {
                    p = ApplicationLoader.applicationContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
                    cached = p;
                }
            }
        }
        return p;
    }

    private static SharedPreferences.Editor editor() {
        return prefs().edit();
    }

    public static String getWsRelayTokenForUid(long uid) {
        return prefs().getString("wsRelayToken_" + uid, "");
    }

    public static void setWsRelayTokenForUid(long uid, String token) {
        editor().putString("wsRelayToken_" + uid, token == null ? "" : token).apply();
    }

    public static String getVoipRelayTokenForUid(long uid) {
        return prefs().getString("voipRelayToken_" + uid, "");
    }

    public static void setVoipRelayTokenForUid(long uid, String token) {
        editor().putString("voipRelayToken_" + uid, token == null ? "" : token).apply();
    }

    private static volatile String installId = "";

    public static synchronized String ensureWsInstallId() {
        if (installId == null || installId.isEmpty()) {
            installId = prefs().getString("installId", "");
        }
        if (installId == null || installId.isEmpty()) {
            installId = UUID.randomUUID().toString().replace("-", "");
            editor().putString("installId", installId).apply();
        }
        return installId;
    }

    public static long currentUid() {
        try {
            return UserConfig.getInstance(UserConfig.selectedAccount).getClientUserId();
        } catch (Throwable e) {
            return 0L;
        }
    }

    private FGBypassStore() {
    }
}

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

/**
 * Что обход помнит между запусками: пропуска на релеи и номер установки.
 *
 * Пропуск выдаётся на аккаунт, поэтому и хранится у каждого свой: чужой аккаунт с чужим
 * пропуском на сервер не пустят. Номер установки один на клиент — по нему сервер отличает
 * устройства, не зная, кто за ними.
 */
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

    // --- пропуск на релей сообщений ---

    public static String getWsRelayTokenForUid(long uid) {
        return prefs().getString("wsRelayToken_" + uid, "");
    }

    public static void setWsRelayTokenForUid(long uid, String token) {
        editor().putString("wsRelayToken_" + uid, token == null ? "" : token).apply();
    }

    // --- пропуск на релей звонков ---

    public static String getVoipRelayTokenForUid(long uid) {
        return prefs().getString("voipRelayToken_" + uid, "");
    }

    public static void setVoipRelayTokenForUid(long uid, String token) {
        editor().putString("voipRelayToken_" + uid, token == null ? "" : token).apply();
    }

    // --- номер установки ---

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

    /** Кому сейчас выдан пропуск: номер выбранного аккаунта. */
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

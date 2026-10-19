/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.plugins;

import android.content.Context;
import android.content.SharedPreferences;
import org.telegram.messenger.ApplicationLoader;

public final class FGPluginSecuritySettings {
    private FGPluginSecuritySettings() {}

    private static SharedPreferences preferences() {
        return ApplicationLoader.applicationContext.getSharedPreferences("finegram_plugins", Context.MODE_PRIVATE);
    }

    public static boolean checkOnInstall() {
        return preferences().getBoolean("check_on_install", true);
    }

    public static void setCheckOnInstall(boolean value) {
        preferences().edit().putBoolean("check_on_install", value).apply();
    }

}

/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.outbound;

import android.content.SharedPreferences;
import java.security.SecureRandom;
import java.util.Locale;

final class FGSubscriptionDevice {
    static String id(SharedPreferences preferences, String androidId) throws FGSubscriptionError {
        String candidate = androidId == null ? "" : androidId.trim().toUpperCase(Locale.ROOT);
        if (candidate.matches("[0-9A-F]{16}") && !candidate.equals("9774D56D682E549C")
                && !candidate.equals("0000000000000000")) return candidate;
        String stored = preferences.getString("device_fallback", "");
        if (stored.matches("[0-9A-F]{32}")) return stored;
        byte[] bytes = new byte[16];
        new SecureRandom().nextBytes(bytes);
        StringBuilder result = new StringBuilder();
        for (byte value : bytes) result.append(String.format(Locale.ROOT, "%02X", value & 255));
        if (!preferences.edit().putString("device_fallback", result.toString()).commit()) {
            throw new FGSubscriptionError(FGSubscriptionError.Code.STORAGE);
        }
        return result.toString();
    }
}

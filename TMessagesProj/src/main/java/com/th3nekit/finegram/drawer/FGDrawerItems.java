/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.drawer;

import android.content.Context;
import android.content.SharedPreferences;

import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.R;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;

public final class FGDrawerItems {

    private FGDrawerItems() {
    }

    public static final int ITEM_ARCHIVE = 1;
    public static final int ITEM_CONTACTS = 2;
    public static final int ITEM_CALLS = 3;
    public static final int ITEM_SAVED = 4;
    public static final int ITEM_NEW_GROUP = 5;
    public static final int ITEM_NEW_CHANNEL = 6;
    public static final int ITEM_SETTINGS = 7;
    public static final int ITEM_PLUGINS = 8;
    public static final int ITEM_QR = 9;
    public static final int ITEM_PROFILE = 10;
    public static final int ITEM_FINEGRAM = 11;

    private static final int[] DEFAULT_ORDER = {
            ITEM_PROFILE, ITEM_CONTACTS, ITEM_CALLS, ITEM_SAVED, ITEM_ARCHIVE,
            ITEM_NEW_GROUP, ITEM_NEW_CHANNEL,
            ITEM_SETTINGS, ITEM_FINEGRAM, ITEM_PLUGINS, ITEM_QR
    };

    public static boolean startsGroup(int item) {
        return item == ITEM_NEW_GROUP || item == ITEM_SETTINGS;
    }

    private static final String PREFS_NAME = "finegram_drawer";
    private static final String KEY_ORDER = "order";
    private static final String KEY_HIDDEN = "hidden";

    private static SharedPreferences prefs() {
        return ApplicationLoader.applicationContext
                .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
    }

    public static int titleOf(int item) {
        switch (item) {
            case ITEM_ARCHIVE: return R.string.ArchivedChats;
            case ITEM_CONTACTS: return R.string.Contacts;
            case ITEM_CALLS: return R.string.Calls;
            case ITEM_SAVED: return R.string.SavedMessages;
            case ITEM_NEW_GROUP: return R.string.NewGroup;
            case ITEM_NEW_CHANNEL: return R.string.NewChannel;
            case ITEM_SETTINGS: return R.string.Settings;
            case ITEM_PLUGINS: return R.string.FG_Plugins;
            case ITEM_QR: return R.string.AuthAnotherClient;
            case ITEM_PROFILE: return R.string.MyProfile;
            case ITEM_FINEGRAM: return R.string.FGP_AdvancedSettings;
            default: return 0;
        }
    }

    public static int iconOf(int item) {
        switch (item) {
            case ITEM_ARCHIVE: return R.drawable.msg_archive;
            case ITEM_CONTACTS: return R.drawable.msg_contacts;
            case ITEM_CALLS: return R.drawable.msg_calls;
            case ITEM_SAVED: return R.drawable.msg_saved;
            case ITEM_NEW_GROUP: return R.drawable.msg_groups;
            case ITEM_NEW_CHANNEL: return R.drawable.msg_channel;
            case ITEM_SETTINGS: return R.drawable.msg_settings;
            case ITEM_PLUGINS: return R.drawable.msg_customize;
            case ITEM_QR: return R.drawable.msg_qrcode;
            case ITEM_PROFILE: return R.drawable.left_status_profile;
            case ITEM_FINEGRAM: return R.drawable.msg_settings_old;
            default: return 0;
        }
    }

    public static List<Integer> visible() {
        final LinkedHashSet<Integer> hidden = parse(prefs().getString(KEY_HIDDEN, ""));
        final List<Integer> out = new ArrayList<>();
        for (int item : order()) {
            if (!hidden.contains(item)) {
                out.add(item);
            }
        }
        return out;
    }

    public static List<Integer> order() {
        final LinkedHashSet<Integer> saved = parse(prefs().getString(KEY_ORDER, ""));
        final List<Integer> out = new ArrayList<>();
        for (int item : saved) {
            if (titleOf(item) != 0) {
                out.add(item);
            }
        }
        for (int item : DEFAULT_ORDER) {
            if (!out.contains(item)) {
                out.add(item);
            }
        }
        return out;
    }

    public static boolean isHidden(int item) {
        return parse(prefs().getString(KEY_HIDDEN, "")).contains(item);
    }

    public static void setHidden(int item, boolean hidden) {
        final LinkedHashSet<Integer> all = parse(prefs().getString(KEY_HIDDEN, ""));
        if (hidden) {
            all.add(item);
        } else {
            all.remove(item);
        }
        prefs().edit().putString(KEY_HIDDEN, join(all)).apply();
    }

    public static void saveOrder(List<Integer> items) {
        prefs().edit().putString(KEY_ORDER, join(items)).apply();
    }

    public static void reset() {
        prefs().edit().remove(KEY_ORDER).remove(KEY_HIDDEN).apply();
    }

    private static LinkedHashSet<Integer> parse(String raw) {
        final LinkedHashSet<Integer> out = new LinkedHashSet<>();
        if (raw == null || raw.isEmpty()) {
            return out;
        }
        for (String part : raw.split(",")) {
            try {
                out.add(Integer.parseInt(part.trim()));
            } catch (NumberFormatException ignore) {

            }
        }
        return out;
    }

    private static String join(Iterable<Integer> items) {
        final StringBuilder sb = new StringBuilder();
        for (Integer item : items) {
            if (sb.length() > 0) {
                sb.append(',');
            }
            sb.append(item);
        }
        return sb.toString();
    }
}

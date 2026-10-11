/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Мысль — разрешить призраку не действовать на выбранных собеседников — взята из re:extera
 * (GPL-3.0, Copyright the re:extera authors, https://github.com/fossSquad/re-extera).
 * Реализация здесь своя.
 */

package com.th3nekit.finegram.privacy;

import android.content.SharedPreferences;

import org.telegram.messenger.ApplicationLoader;

import java.util.Collections;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;

public final class FGGhostExceptions {

    private static final String PREFS = "fg_ghost_exceptions";
    private static final String KEY = "dialogs";

    private static volatile Set<Long> cached;

    private FGGhostExceptions() {
    }

    private static SharedPreferences prefs() {
        return ApplicationLoader.applicationContext.getSharedPreferences(PREFS, 0);
    }

    private static Set<Long> load() {
        Set<Long> local = cached;
        if (local != null) {
            return local;
        }
        synchronized (FGGhostExceptions.class) {
            if (cached == null) {
                final Set<Long> out = new HashSet<>();
                try {
                    final Set<String> raw = prefs().getStringSet(KEY, Collections.emptySet());
                    for (String value : raw) {
                        try {
                            out.add(Long.parseLong(value));
                        } catch (NumberFormatException ignored) {
                        }
                    }
                } catch (Throwable ignored) {
                }
                cached = out;
            }
            return cached;
        }
    }

    public static boolean excluded(long dialogId) {
        return dialogId != 0 && load().contains(dialogId);
    }

    public static boolean empty() {
        return load().isEmpty();
    }

    public static void set(long dialogId, boolean excluded) {
        if (dialogId == 0) {
            return;
        }
        synchronized (FGGhostExceptions.class) {
            final Set<Long> out = new HashSet<>(load());
            if (excluded) {
                out.add(dialogId);
            } else {
                out.remove(dialogId);
            }
            save(out);
        }
    }

    public static ArrayList<Long> snapshot() {
        ArrayList<Long> ids = new ArrayList<>(load());
        Collections.sort(ids);
        return ids;
    }

    public static void replace(Iterable<Long> ids) {
        synchronized (FGGhostExceptions.class) {
            final Set<Long> out = new HashSet<>();
            if (ids != null) {
                for (Long id : ids) {
                    if (id != null && id != 0) out.add(id);
                }
            }
            save(out);
        }
    }

    private static void save(Set<Long> ids) {
        final Set<String> raw = new HashSet<>();
        for (Long id : ids) raw.add(String.valueOf(id));
        cached = ids;
        prefs().edit().putStringSet(KEY, raw).apply();
    }

    public static void toggle(long dialogId) {
        set(dialogId, !excluded(dialogId));
    }

    public static int count() {
        return load().size();
    }
}

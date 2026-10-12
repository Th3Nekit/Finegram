/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Идея — прятать человека у себя, не трогая его самого — взята из re:extera
 * (GPL-3.0, Copyright the re:extera authors, https://github.com/fossSquad/re-extera).
 * Реализация здесь своя.
 */

package com.th3nekit.finegram.privacy;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;

import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.Utilities;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;

import com.th3nekit.finegram.core.FinegramLogger;

public final class FGShadowbanStore {

    private static final String NAME = "fg_shadowban.db";
    private static final int VERSION = 1;
    private static final String TABLE = "hidden";

    private static final ConcurrentHashMap<Long, Entry> cached = new ConcurrentHashMap<>();

    private static volatile boolean loaded;
    private static Helper helper;

    private FGShadowbanStore() {
    }

    public static final class Entry {
        public final long userId;
        public final boolean hideDialog;
        public final boolean hideInGroups;
        public final long addedAt;

        Entry(long userId, boolean hideDialog, boolean hideInGroups, long addedAt) {
            this.userId = userId;
            this.hideDialog = hideDialog;
            this.hideInGroups = hideInGroups;
            this.addedAt = addedAt;
        }
    }

    private static final class Helper extends SQLiteOpenHelper {
        Helper(Context context) {
            super(context, NAME, null, VERSION);
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            db.execSQL("CREATE TABLE IF NOT EXISTS " + TABLE + " ("
                    + "user_id INTEGER PRIMARY KEY,"
                    + "hide_dialog INTEGER NOT NULL,"
                    + "hide_in_groups INTEGER NOT NULL,"
                    + "added_at INTEGER NOT NULL)");
        }

        @Override
        public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
            db.execSQL("DROP TABLE IF EXISTS " + TABLE);
            onCreate(db);
        }
    }

    private static synchronized SQLiteDatabase db() {
        if (helper == null) {
            helper = new Helper(ApplicationLoader.applicationContext);
        }
        return helper.getWritableDatabase();
    }

    public static void preload() {
        if (loaded) {
            return;
        }
        loaded = true;
        Utilities.globalQueue.postRunnable(() -> {
            final ConcurrentHashMap<Long, Entry> fresh = new ConcurrentHashMap<>();
            try (Cursor c = db().query(TABLE,
                    new String[]{"user_id", "hide_dialog", "hide_in_groups", "added_at"},
                    null, null, null, null, null)) {
                while (c.moveToNext()) {
                    final long id = c.getLong(0);
                    fresh.put(id, new Entry(id, c.getInt(1) != 0, c.getInt(2) != 0, c.getLong(3)));
                }
            } catch (Throwable e) {
                FinegramLogger.e("FGShadowban", () -> "не смог прочитать список", e);
                return;
            }
            cached.clear();
            cached.putAll(fresh);
        });
    }

    public static boolean hidesDialog(long userId) {
        final Entry entry = cached.get(userId);
        return entry != null && entry.hideDialog;
    }

    public static boolean hidesInGroups(long userId) {
        final Entry entry = cached.get(userId);
        return entry != null && entry.hideInGroups;
    }

    public static boolean hidden(long userId) {
        return cached.containsKey(userId);
    }

    public static Entry get(long userId) {
        return cached.get(userId);
    }

    public static boolean empty() {
        return cached.isEmpty();
    }

    public static void put(long userId, boolean hideDialog, boolean hideInGroups) {
        if (!hideDialog && !hideInGroups) {
            remove(userId);
            return;
        }
        final Entry existing = cached.get(userId);
        final long addedAt = existing != null ? existing.addedAt : System.currentTimeMillis();
        cached.put(userId, new Entry(userId, hideDialog, hideInGroups, addedAt));
        Utilities.globalQueue.postRunnable(() -> {
            try {
                final ContentValues values = new ContentValues();
                values.put("user_id", userId);
                values.put("hide_dialog", hideDialog ? 1 : 0);
                values.put("hide_in_groups", hideInGroups ? 1 : 0);
                values.put("added_at", addedAt);
                db().insertWithOnConflict(TABLE, null, values, SQLiteDatabase.CONFLICT_REPLACE);
            } catch (Throwable e) {
                FinegramLogger.e("FGShadowban", () -> "не смог запомнить " + userId, e);
            }
        });
    }

    public static void remove(long userId) {
        cached.remove(userId);
        Utilities.globalQueue.postRunnable(() -> {
            try {
                db().delete(TABLE, "user_id = ?", new String[]{String.valueOf(userId)});
            } catch (Throwable e) {
                FinegramLogger.e("FGShadowban", () -> "не смог забыть " + userId, e);
            }
        });
    }

    public static void clear() {
        cached.clear();
        Utilities.globalQueue.postRunnable(() -> {
            try {
                db().delete(TABLE, null, null);
            } catch (Throwable e) {
                FinegramLogger.e("FGShadowban", () -> "не смог очистить список", e);
            }
        });
    }

    public static List<Entry> all() {
        final List<Entry> out = new ArrayList<>(cached.values());
        out.sort((a, b) -> Long.compare(b.addedAt, a.addedAt));
        return out;
    }

    public static int count() {
        return cached.size();
    }
}

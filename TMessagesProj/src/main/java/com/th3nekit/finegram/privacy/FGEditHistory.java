/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Приём — запоминать, каким сообщение было до правки — взят из re:extera
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

import com.th3nekit.finegram.core.FinegramLogger;
import com.th3nekit.finegram.core.configs.FinegramPrivacyConfig;

public final class FGEditHistory {

    private static final String NAME = "fg_edits.db";
    private static final int VERSION = 1;
    private static final String TABLE = "edits";

    private static final int PER_MESSAGE = 20;

    private static Helper helper;

    private FGEditHistory() {
    }

    public static final class Version {
        public final String text;
        public final long at;

        Version(String text, long at) {
            this.text = text;
            this.at = at;
        }
    }

    private static final class Helper extends SQLiteOpenHelper {
        Helper(Context context) {
            super(context, NAME, null, VERSION);
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            db.execSQL("CREATE TABLE IF NOT EXISTS " + TABLE + " ("
                    + "dialog_id INTEGER NOT NULL,"
                    + "message_id INTEGER NOT NULL,"
                    + "at INTEGER NOT NULL,"
                    + "text TEXT NOT NULL)");
            db.execSQL("CREATE INDEX IF NOT EXISTS edits_msg ON " + TABLE + " (dialog_id, message_id)");
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

    public static void remember(long dialogId, int messageId, CharSequence previous, int date) {
        if (!FinegramPrivacyConfig.INSTANCE.getKeepEdits()) {
            return;
        }
        if (previous == null || previous.length() == 0) {
            return;
        }
        final String text = previous.toString();
        final long at = date > 0 ? date * 1000L : System.currentTimeMillis();
        Utilities.globalQueue.postRunnable(() -> {
            try {
                final SQLiteDatabase db = db();

                try (Cursor c = db.rawQuery("SELECT text FROM " + TABLE
                        + " WHERE dialog_id = ? AND message_id = ? ORDER BY at DESC LIMIT 1",
                        new String[]{String.valueOf(dialogId), String.valueOf(messageId)})) {
                    if (c.moveToFirst() && text.equals(c.getString(0))) {
                        return;
                    }
                }
                final ContentValues values = new ContentValues();
                values.put("dialog_id", dialogId);
                values.put("message_id", messageId);
                values.put("at", at);
                values.put("text", text);
                db.insert(TABLE, null, values);
                db.execSQL("DELETE FROM " + TABLE + " WHERE dialog_id = ? AND message_id = ?"
                                + " AND rowid NOT IN (SELECT rowid FROM " + TABLE
                                + " WHERE dialog_id = ? AND message_id = ? ORDER BY at DESC LIMIT ?)",
                        new Object[]{dialogId, messageId, dialogId, messageId, PER_MESSAGE});
            } catch (Throwable e) {
                FinegramLogger.e("FGEdits", () -> "не смог запомнить правку " + messageId, e);
            }
        });
    }

    public static List<Version> of(long dialogId, int messageId) {
        final List<Version> out = new ArrayList<>();
        try (Cursor c = db().query(TABLE, new String[]{"text", "at"},
                "dialog_id = ? AND message_id = ?",
                new String[]{String.valueOf(dialogId), String.valueOf(messageId)},
                null, null, "at ASC")) {
            while (c.moveToNext()) {
                out.add(new Version(c.getString(0), c.getLong(1)));
            }
        } catch (Throwable e) {
            FinegramLogger.e("FGEdits", () -> "не смог прочитать историю " + messageId, e);
        }
        return out;
    }

    public static void clear() {
        Utilities.globalQueue.postRunnable(() -> {
            try {
                db().delete(TABLE, null, null);
            } catch (Throwable e) {
                FinegramLogger.e("FGEdits", () -> "не смог очистить историю", e);
            }
        });
    }
}

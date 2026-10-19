/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Приём — не дать клиенту удалить сообщение из своей же базы и запомнить его как
 * удалённое — взят из re:extera (GPL-3.0, Copyright the re:extera authors,
 * https://github.com/fossSquad/re-extera). Реализация здесь своя.
 */

package com.th3nekit.finegram.privacy;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;

import org.telegram.messenger.ApplicationLoader;

import java.util.ArrayList;
import java.util.List;

import com.th3nekit.finegram.core.FinegramLogger;
import com.th3nekit.finegram.core.configs.FinegramPrivacyConfig;

public final class FGDeletedStore {

    private static final String NAME = "fg_deleted.db";
    private static final int VERSION = 3;
    private static final String TABLE = "deleted";

    private static final int LIMIT = 20000;

    private static volatile Helper helper;

    private FGDeletedStore() {
    }

    private static Helper helper() {
        Helper local = helper;
        if (local == null) {
            synchronized (FGDeletedStore.class) {
                local = helper;
                if (local == null) {
                    local = new Helper(ApplicationLoader.applicationContext);
                    helper = local;
                }
            }
        }
        return local;
    }

    private static final java.util.concurrent.ConcurrentHashMap<Long, Store> stores =
            new java.util.concurrent.ConcurrentHashMap<>();

    private static Store store(int account) {
        final long ownerId = org.telegram.messenger.UserConfig.getInstance(account).getClientUserId();
        return storeForOwner(ownerId > 0 ? ownerId : -1);
    }

    private static Store storeForOwner(long ownerId) {
        return stores.computeIfAbsent(ownerId, Store::new);
    }

    public static void remember(long dialogId, List<Integer> messageIds) {
        remember(org.telegram.messenger.UserConfig.selectedAccount, dialogId, messageIds);
    }

    public static void remember(int account, long dialogId, List<Integer> messageIds) {
        store(account).remember(dialogId, messageIds);
    }

    public static void markNow(long dialogId, List<Integer> messageIds) {
        markNow(org.telegram.messenger.UserConfig.selectedAccount, dialogId, messageIds);
    }

    public static void markNow(int account, long dialogId, List<Integer> messageIds) {
        store(account).markNow(dialogId, messageIds);
    }

    public static boolean isDeleted(long dialogId, int messageId) {
        return isDeleted(org.telegram.messenger.UserConfig.selectedAccount, dialogId, messageId);
    }

    public static boolean isDeleted(int account, long dialogId, int messageId) {
        return store(account).isDeleted(dialogId, messageId);
    }

    public static void preload(long dialogId) {
        preload(org.telegram.messenger.UserConfig.selectedAccount, dialogId);
    }

    public static void preload(int account, long dialogId) {
        store(account).preload(dialogId);
    }

    public static ArrayList<Integer> inDialog(long dialogId, int limit) {
        return inDialog(org.telegram.messenger.UserConfig.selectedAccount, dialogId, limit);
    }

    public static ArrayList<Integer> inDialog(int account, long dialogId, int limit) {
        return store(account).inDialog(dialogId, limit);
    }

    public static ArrayList<long[]> dialogs() {
        return dialogs(org.telegram.messenger.UserConfig.selectedAccount);
    }

    public static ArrayList<long[]> dialogs(int account) {
        return store(account).dialogs();
    }

    public static int count() {
        return count(org.telegram.messenger.UserConfig.selectedAccount);
    }

    public static int count(int account) {
        return store(account).count();
    }

    public static void forget(long dialogId) {
        forget(org.telegram.messenger.UserConfig.selectedAccount, dialogId);
    }

    public static void forget(int account, long dialogId) {
        store(account).forget(dialogId);
    }

    public static boolean forgetMessage(int account, long expectedOwnerId, long dialogId, int messageId) {
        return forgetMessage(account, expectedOwnerId, dialogId, messageId, () -> true);
    }

    public static boolean forgetMessage(int account, long expectedOwnerId, long dialogId, int messageId,
                                        java.util.function.BooleanSupplier deleteMessage) {
        if (expectedOwnerId <= 0 || dialogId == 0 || messageId <= 0 || deleteMessage == null
                || org.telegram.messenger.UserConfig.getInstance(account).getClientUserId() != expectedOwnerId) {
            return false;
        }
        return storeForOwner(expectedOwnerId).forgetMessage(dialogId, messageId, deleteMessage);
    }

    public static boolean forgetMessages(int account, long expectedOwnerId, long dialogId, List<Integer> messageIds,
                                         java.util.function.BooleanSupplier deleteMessages) {
        if (expectedOwnerId <= 0 || dialogId == 0 || messageIds == null || messageIds.isEmpty() || deleteMessages == null
                || org.telegram.messenger.UserConfig.getInstance(account).getClientUserId() != expectedOwnerId) return false;
        return storeForOwner(expectedOwnerId).forgetMessages(dialogId, messageIds, deleteMessages);
    }

    public static void forgetAll() {
        forgetAll(org.telegram.messenger.UserConfig.selectedAccount);
    }

    public static void forgetAll(int account) {
        store(account).forgetAll();
    }

    public static ArrayList<long[]> previousDialogs() {
        return storeForOwner(0).dialogs();
    }

    public static int previousCount() {
        return storeForOwner(0).count();
    }

    public static void forgetPrevious(long dialogId) {
        storeForOwner(0).forget(dialogId);
    }

    public static void forgetAllPrevious() {
        storeForOwner(0).forgetAll();
    }

    public static boolean transferPrevious(int account, long dialogId) {
        final Store target = store(account);
        if (target.ownerId <= 0 || dialogId == 0) return false;
        final Store previous = storeForOwner(0);
        synchronized (previous.writeLock) {
            synchronized (target.writeLock) {
                try {
                    final SQLiteDatabase db = helper().getWritableDatabase();
                    db.beginTransaction();
                    try {
                        db.execSQL("INSERT OR IGNORE INTO " + TABLE
                                + " (owner_id, dialog_id, message_id, at)"
                                + " SELECT ?, dialog_id, message_id, at FROM " + TABLE
                                + " WHERE owner_id = 0 AND dialog_id = ?",
                                new Object[]{target.ownerId, dialogId});
                        db.delete(TABLE, "owner_id = 0 AND dialog_id = ?",
                                new String[]{String.valueOf(dialogId)});
                        db.setTransactionSuccessful();
                    } finally {
                        db.endTransaction();
                    }
                    previous.invalidate(dialogId);
                    target.invalidate(dialogId);
                    target.preload(dialogId);
                    previous.notifyChanged(dialogId);
                    target.notifyChanged(dialogId);
                    return true;
                } catch (Throwable t) {
                    FinegramLogger.e("FGDeleted", () -> "журнал не перенесён", t);
                    return false;
                }
            }
        }
    }

    private static final class Store {
        private final long ownerId;

        Store(long ownerId) {
            this.ownerId = ownerId;
        }

        public void remember(long dialogId, List<Integer> messageIds) {
            synchronized (writeLock) {
                ArrayList<Integer> copy = markPending(dialogId, messageIds);
                if (copy != null && persist(dialogId, copy)) {
                    invalidate(dialogId);
                    preload(dialogId);
                }
            }
        }

        private boolean persist(long dialogId, List<Integer> messageIds) {
            if (ownerId <= 0 || messageIds == null || messageIds.isEmpty()) return false;

            if (dialogId == 0) return false;
            try {
                final SQLiteDatabase db = helper().getWritableDatabase();
                db.beginTransaction();
                try {
                    final long now = System.currentTimeMillis();
                    for (Integer id : messageIds) {
                        if (id == null || id <= 0) continue;
                        final ContentValues values = new ContentValues();
                        values.put("owner_id", ownerId);
                        values.put("dialog_id", dialogId);
                        values.put("message_id", id);
                        values.put("at", now);
                        db.insertWithOnConflict(TABLE, null, values, SQLiteDatabase.CONFLICT_REPLACE);
                    }
                    db.setTransactionSuccessful();
                } finally {
                    db.endTransaction();
                }
                trim(db);
                return true;
            } catch (Throwable t) {
                FinegramLogger.e("FGDeleted", () -> "не записалось", t);
                return false;
            }
        }

        private final int CACHE_LIMIT = 64;
        private final java.util.LinkedHashMap<Long, java.util.Set<Integer>> cached =
                new java.util.LinkedHashMap<>(16, 0.75f, true);
        private final java.util.HashMap<Long, Object> loading = new java.util.HashMap<>();
        private final java.util.concurrent.ConcurrentHashMap<Long, java.util.HashSet<Integer>>
                pendingMarks = new java.util.concurrent.ConcurrentHashMap<>();
        private final Object writeLock = new Object();
        private final java.util.HashMap<Long, PendingWrite> pendingWrites = new java.util.HashMap<>();

        private final class PendingWrite {
            int count;
            final java.util.HashSet<Integer> removed = new java.util.HashSet<>();
        }

        private ArrayList<Integer> markPending(long dialogId, List<Integer> messageIds) {
            if (ownerId <= 0 || dialogId == 0 || messageIds == null || messageIds.isEmpty()) return null;
            final ArrayList<Integer> copy = new ArrayList<>();
            for (Integer id : messageIds) if (id != null && id > 0) copy.add(id);
            if (copy.isEmpty()) return null;
            pendingMarks.compute(dialogId, (key, old) -> {
                final java.util.HashSet<Integer> next = old == null
                        ? new java.util.HashSet<>() : new java.util.HashSet<>(old);
                next.addAll(copy);
                return next;
            });
            return copy;
        }

        public void markNow(long dialogId, List<Integer> messageIds) {
            synchronized (writeLock) {
                final ArrayList<Integer> copy = markPending(dialogId, messageIds);
                if (copy == null) return;
                final PendingWrite write = pendingWrites.computeIfAbsent(dialogId, key -> new PendingWrite());
                write.count++;
                org.telegram.messenger.Utilities.globalQueue.postRunnable(() -> {
                    synchronized (writeLock) {
                        if (pendingWrites.get(dialogId) != write) return;
                        try {
                            copy.removeAll(write.removed);
                            remember(dialogId, copy);
                        } finally {
                            if (--write.count == 0) pendingWrites.remove(dialogId);
                        }
                    }
                });
            }
        }

        public boolean isDeleted(long dialogId, int messageId) {
            if (ownerId <= 0 || !FinegramPrivacyConfig.INSTANCE.getSaveDeleted() || dialogId == 0 || messageId <= 0) return false;
            java.util.Set<Integer> pending = pendingMarks.get(dialogId);
            if (pending != null && pending.contains(messageId)) {
                preload(dialogId);
                return true;
            }
            synchronized (this) {
                java.util.Set<Integer> ids = cached.get(dialogId);
                if (ids != null) return ids.contains(messageId);
            }
            preload(dialogId);
            return false;
        }

        public void preload(long dialogId) {
            if (ownerId <= 0 || dialogId == 0 || !FinegramPrivacyConfig.INSTANCE.getSaveDeleted()) return;
            final Object ticket = new Object();
            synchronized (this) {
                if (cached.containsKey(dialogId) || loading.containsKey(dialogId) || loading.size() >= CACHE_LIMIT) return;
                loading.put(dialogId, ticket);
            }
            org.telegram.messenger.Utilities.globalQueue.postRunnable(() -> {
                final java.util.HashSet<Integer> ids = new java.util.HashSet<>();
                boolean success = false;
                try (Cursor cursor = helper().getReadableDatabase().rawQuery(
                        "SELECT message_id FROM " + TABLE + " WHERE owner_id = ? AND dialog_id = ?",
                        new String[]{String.valueOf(ownerId), String.valueOf(dialogId)})) {
                    while (cursor.moveToNext()) ids.add(cursor.getInt(0));
                    success = true;
                } catch (Throwable t) {
                    FinegramLogger.e("FGDeleted", () -> "пометки чата не прочитались", t);
                }
                synchronized (this) {
                    if (loading.get(dialogId) != ticket) return;
                    loading.remove(dialogId);
                    if (!success) return;
                    cached.put(dialogId, java.util.Collections.unmodifiableSet(ids));
                    while (cached.size() > CACHE_LIMIT) {
                        cached.remove(cached.keySet().iterator().next());
                    }
                    pendingMarks.computeIfPresent(dialogId, (key, old) -> {
                        java.util.HashSet<Integer> next = new java.util.HashSet<>(old);
                        next.removeAll(ids);
                        return next.isEmpty() ? null : next;
                    });
                }
                notifyChanged(dialogId);
            });
        }

        private synchronized void invalidate(long dialogId) {
            cached.remove(dialogId);
            loading.remove(dialogId);
        }

        private void notifyChanged(long dialogId) {
            org.telegram.messenger.AndroidUtilities.runOnUIThread(() ->
                    org.telegram.messenger.NotificationCenter.getGlobalInstance().postNotificationName(
                            org.telegram.messenger.NotificationCenter.finegramDeletedMarksLoaded, dialogId, ownerId));
        }

        public ArrayList<Integer> inDialog(long dialogId, int limit) {
            final ArrayList<Integer> out = new ArrayList<>();
            try (Cursor cursor = helper().getReadableDatabase().rawQuery(
                    "SELECT message_id FROM " + TABLE + " WHERE owner_id = ? AND dialog_id = ? ORDER BY at DESC LIMIT ?",
                    new String[]{String.valueOf(ownerId), String.valueOf(dialogId), String.valueOf(limit)})) {
                while (cursor.moveToNext()) {
                    out.add(cursor.getInt(0));
                }
            } catch (Throwable t) {
                FinegramLogger.e("FGDeleted", () -> "не прочиталось", t);
            }
            return out;
        }

        public ArrayList<long[]> dialogs() {
            final ArrayList<long[]> out = new ArrayList<>();
            try (Cursor cursor = helper().getReadableDatabase().rawQuery(
                    "SELECT dialog_id, COUNT(*), MAX(at) FROM " + TABLE
                            + " WHERE owner_id = ? GROUP BY dialog_id ORDER BY MAX(at) DESC",
                    new String[]{String.valueOf(ownerId)})) {
                while (cursor.moveToNext()) {
                    out.add(new long[]{cursor.getLong(0), cursor.getLong(1), cursor.getLong(2)});
                }
            } catch (Throwable t) {
                FinegramLogger.e("FGDeleted", () -> "список чатов не прочитался", t);
            }
            return out;
        }

        public int count() {
            try (Cursor cursor = helper().getReadableDatabase().rawQuery(
                    "SELECT COUNT(*) FROM " + TABLE + " WHERE owner_id = ?",
                    new String[]{String.valueOf(ownerId)})) {
                return cursor.moveToFirst() ? cursor.getInt(0) : 0;
            } catch (Throwable t) {
                return 0;
            }
        }

        public boolean forgetMessage(long dialogId, int messageId, java.util.function.BooleanSupplier deleteMessage) {
            synchronized (writeLock) {
                try {
                    final SQLiteDatabase db = helper().getWritableDatabase();
                    db.beginTransaction();
                    try {
                        db.delete(TABLE, "owner_id = ? AND dialog_id = ? AND message_id = ?",
                                new String[]{String.valueOf(ownerId), String.valueOf(dialogId), String.valueOf(messageId)});
                        if (!deleteMessage.getAsBoolean()) return false;
                        db.setTransactionSuccessful();
                    } finally {
                        db.endTransaction();
                    }
                    final PendingWrite write = pendingWrites.get(dialogId);
                    if (write != null) write.removed.add(messageId);
                    pendingMarks.computeIfPresent(dialogId, (key, old) -> {
                        final java.util.HashSet<Integer> next = new java.util.HashSet<>(old);
                        next.remove(messageId);
                        return next.isEmpty() ? null : next;
                    });
                    invalidate(dialogId);
                    preload(dialogId);
                    notifyChanged(dialogId);
                    return true;
                } catch (Throwable t) {
                    FinegramLogger.e("FGDeleted", () -> "сообщение не удалено", t);
                    return false;
                }
            }
        }

        public boolean forgetMessages(long dialogId, List<Integer> messageIds, java.util.function.BooleanSupplier deleteMessages) {
            final java.util.HashSet<Integer> ids = new java.util.HashSet<>();
            for (Integer id : messageIds) if (id != null && id > 0) ids.add(id);
            synchronized (writeLock) {
                try {
                    final SQLiteDatabase db = helper().getWritableDatabase();
                    db.beginTransaction();
                    try {
                        for (Integer id : ids) {
                            db.delete(TABLE, "owner_id = ? AND dialog_id = ? AND message_id = ?",
                                    new String[]{String.valueOf(ownerId), String.valueOf(dialogId), String.valueOf(id)});
                        }
                        if (!deleteMessages.getAsBoolean()) return false;
                        db.setTransactionSuccessful();
                    } finally {
                        db.endTransaction();
                    }
                    final PendingWrite write = pendingWrites.get(dialogId);
                    if (write != null) write.removed.addAll(ids);
                    pendingMarks.computeIfPresent(dialogId, (key, old) -> {
                        final java.util.HashSet<Integer> next = new java.util.HashSet<>(old);
                        next.removeAll(ids);
                        return next.isEmpty() ? null : next;
                    });
                    invalidate(dialogId);
                    preload(dialogId);
                    notifyChanged(dialogId);
                    return true;
                } catch (Throwable t) {
                    FinegramLogger.e("FGDeleted", () -> "сообщения не удалены", t);
                    return false;
                }
            }
        }

        public void forget(long dialogId) {
            try {
                synchronized (writeLock) {
                    helper().getWritableDatabase().delete(TABLE, "owner_id = ? AND dialog_id = ?",
                            new String[]{String.valueOf(ownerId), String.valueOf(dialogId)});
                    pendingWrites.remove(dialogId);
                    pendingMarks.remove(dialogId);
                    invalidate(dialogId);
                }
                notifyChanged(dialogId);
            } catch (Throwable t) {
                FinegramLogger.e("FGDeleted", () -> "не очистилось", t);
            }
        }

        public void forgetAll() {
            try {
                synchronized (writeLock) {
                    helper().getWritableDatabase().delete(TABLE, "owner_id = ?", new String[]{String.valueOf(ownerId)});
                    pendingWrites.clear();
                    pendingMarks.clear();
                    synchronized (this) {
                        cached.clear();
                        loading.clear();
                    }
                }
                notifyChanged(0);
            } catch (Throwable t) {
                FinegramLogger.e("FGDeleted", () -> "не очистилось", t);
            }
        }

        private void trim(SQLiteDatabase db) {
            try {
                db.execSQL("DELETE FROM " + TABLE + " WHERE owner_id = ? AND rowid NOT IN ("
                        + "SELECT rowid FROM " + TABLE + " WHERE owner_id = ? ORDER BY at DESC LIMIT " + LIMIT + ")",
                        new Object[]{ownerId, ownerId});
            } catch (Throwable ignored) {
            }
        }

    }

    private static final class Helper extends SQLiteOpenHelper {

        Helper(Context context) {
            super(context, NAME, null, VERSION);
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            db.execSQL("CREATE TABLE " + TABLE + " ("
                    + "owner_id INTEGER NOT NULL,"
                    + "dialog_id INTEGER NOT NULL,"
                    + "message_id INTEGER NOT NULL,"
                    + "at INTEGER NOT NULL,"
                    + "PRIMARY KEY (owner_id, dialog_id, message_id))");
            db.execSQL("CREATE INDEX idx_owner_at ON " + TABLE + " (owner_id, at)");
        }

        @Override
        public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
            if (oldVersion < 3) {
                db.execSQL("ALTER TABLE " + TABLE + " RENAME TO deleted_previous");
                onCreate(db);
                db.execSQL("INSERT INTO " + TABLE + " (owner_id, dialog_id, message_id, at)"
                        + " SELECT 0, dialog_id, message_id, at FROM deleted_previous");
                db.execSQL("DROP TABLE deleted_previous");
            }
        }
    }
}

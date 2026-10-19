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
import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.DialogObject;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.UserConfig;
import org.telegram.messenger.Utilities;
import org.telegram.tgnet.TLRPC;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicLong;

import com.th3nekit.finegram.core.FinegramLogger;

/**
 * Кого прятать.
 *
 * Отличие от блокировки в том, что собеседник ничего не замечает: его сообщения приходят
 * и лежат в переписке, просто не всплывают. Можно спрятать переписку целиком, а можно
 * только его реплики в общих группах — это разные желания, поэтому и признака два.
 *
 * Спрашивается это на каждой отрисовке списка диалогов и на каждом уведомлении, поэтому
 * ответ живёт в памяти, а база нужна только чтобы пережить перезапуск.
 */
public final class FGShadowbanStore {

    private static final String NAME = "fg_shadowban.db";
    private static final int VERSION = 1;
    private static final String TABLE = "hidden";

    /** Кого прячем: id → что именно прятать. Спрашивается из потока отрисовки. */
    private static volatile ConcurrentHashMap<Long, Entry> cached = new ConcurrentHashMap<>();
    private static final AtomicLong revision = new AtomicLong();
    private static final HashMap<Long, Entry> pendingChanges = new HashMap<>();

    private static volatile boolean loaded;
    private static boolean loading;
    private static boolean clearBeforeLoad;
    private static Helper helper;

    private FGShadowbanStore() {
    }

    /** Что прятать у одного человека. */
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

    /**
     * Поднимает список в память. Пока он не поднят, никто не спрятан — это честнее, чем
     * лезть в базу из потока отрисовки и ронять список чатов ради одной проверки.
     */
    public static synchronized void preload() {
        if (loaded || loading) {
            return;
        }
        loading = true;
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
                synchronized (FGShadowbanStore.class) {
                    loading = false;
                }
                FinegramLogger.e("FGShadowban", () -> "не смог прочитать список", e);
                return;
            }
            synchronized (FGShadowbanStore.class) {
                if (clearBeforeLoad) fresh.clear();
                for (Map.Entry<Long, Entry> change : pendingChanges.entrySet()) {
                    if (change.getValue() == null) fresh.remove(change.getKey());
                    else fresh.put(change.getKey(), change.getValue());
                }
                cached = fresh;
                loaded = true;
                loading = false;
                pendingChanges.clear();
                clearBeforeLoad = false;
                revision.incrementAndGet();
            }
            AndroidUtilities.runOnUIThread(() -> {
                for (int account = 0; account < UserConfig.MAX_ACCOUNT_COUNT; account++) {
                    NotificationCenter.getInstance(account).postNotificationName(NotificationCenter.dialogsNeedReload);
                }
            });
        });
    }

    /** Прятать ли переписку с этим человеком из списка чатов. */
    public static boolean hidesDialog(long userId) {
        final Entry entry = cached.get(userId);
        return entry != null && entry.hideDialog;
    }

    public static boolean hidesDialog(int account, long dialogId) {
        if (cached.isEmpty()) {
            return false;
        }
        if (DialogObject.isEncryptedDialog(dialogId)) {
            TLRPC.EncryptedChat chat = MessagesController.getInstance(account)
                    .getEncryptedChat(DialogObject.getEncryptedChatId(dialogId));
            return chat != null && hidesDialog(chat.user_id);
        }
        return DialogObject.isUserDialog(dialogId) && hidesDialog(dialogId);
    }

    public static boolean hidesSearchResult(Object object) {
        if (object instanceof TLRPC.User) {
            return hidesDialog(((TLRPC.User) object).id);
        }
        if (object instanceof TLRPC.EncryptedChat) {
            return hidesDialog(((TLRPC.EncryptedChat) object).user_id);
        }
        if (object instanceof TLRPC.Peer) {
            return hidesDialog(((TLRPC.Peer) object).user_id);
        }
        return false;
    }

    public static ArrayList<TLRPC.Dialog> visibleDialogs(int account, ArrayList<TLRPC.Dialog> dialogs) {
        if (dialogs == null) {
            return new ArrayList<>();
        }
        ArrayList<TLRPC.Dialog> visible = null;
        if (!cached.isEmpty()) {
            for (int i = 0; i < dialogs.size(); i++) {
                TLRPC.Dialog dialog = dialogs.get(i);
                if (dialog != null && hidesDialog(account, dialog.id)) {
                    if (visible == null) {
                        visible = new ArrayList<>(dialogs.subList(0, i));
                    }
                } else if (visible != null) {
                    visible.add(dialog);
                }
            }
        }
        return visible != null ? visible : dialogs;
    }

    public static ArrayList<TLRPC.TL_topPeer> visibleHints(ArrayList<TLRPC.TL_topPeer> hints) {
        if (cached.isEmpty()) {
            return hints;
        }
        ArrayList<TLRPC.TL_topPeer> visible = new ArrayList<>(hints.size());
        for (TLRPC.TL_topPeer hint : hints) {
            if (hint != null && !hidesSearchResult(hint.peer)) {
                visible.add(hint);
            }
        }
        return visible;
    }

    public static long revision() {
        return revision.get();
    }

    /** Прятать ли его реплики в общих группах. */
    public static boolean hidesInGroups(long userId) {
        final Entry entry = cached.get(userId);
        return entry != null && entry.hideInGroups;
    }

    /** Спрятан ли вообще: для галочки в меню профиля. */
    public static boolean hidden(long userId) {
        return cached.containsKey(userId);
    }

    public static Entry get(long userId) {
        return cached.get(userId);
    }

    /** Есть ли вообще кто-то спрятанный: одна проверка вместо похода в таблицу. */
    public static boolean empty() {
        return cached.isEmpty();
    }

    public static synchronized void put(long userId, boolean hideDialog, boolean hideInGroups) {
        if (!hideDialog && !hideInGroups) {
            remove(userId);
            return;
        }
        final Entry existing = cached.get(userId);
        final long addedAt = existing != null ? existing.addedAt : System.currentTimeMillis();
        final Entry entry = new Entry(userId, hideDialog, hideInGroups, addedAt);
        cached.put(userId, entry);
        if (!loaded) pendingChanges.put(userId, entry);
        revision.incrementAndGet();
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

    public static synchronized void remove(long userId) {
        cached.remove(userId);
        if (!loaded) pendingChanges.put(userId, null);
        revision.incrementAndGet();
        Utilities.globalQueue.postRunnable(() -> {
            try {
                db().delete(TABLE, "user_id = ?", new String[]{String.valueOf(userId)});
            } catch (Throwable e) {
                FinegramLogger.e("FGShadowban", () -> "не смог забыть " + userId, e);
            }
        });
    }

    public static synchronized void clear() {
        cached.clear();
        if (!loaded) {
            clearBeforeLoad = true;
            pendingChanges.clear();
        }
        revision.incrementAndGet();
        Utilities.globalQueue.postRunnable(() -> {
            try {
                db().delete(TABLE, null, null);
            } catch (Throwable e) {
                FinegramLogger.e("FGShadowban", () -> "не смог очистить список", e);
            }
        });
    }

    /** Весь список для экрана настроек, свежие сверху. */
    public static List<Entry> all() {
        final List<Entry> out = new ArrayList<>(cached.values());
        out.sort((a, b) -> Long.compare(b.addedAt, a.addedAt));
        return out;
    }

    public static int count() {
        return cached.size();
    }
}

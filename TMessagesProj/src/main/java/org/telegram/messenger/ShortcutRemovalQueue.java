/*
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package org.telegram.messenger;

import android.content.pm.ShortcutManager;
import android.os.Build;

import androidx.core.content.pm.ShortcutManagerCompat;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

final class ShortcutRemovalQueue {
    private static final int BATCH_SIZE = 100;
    private static final LinkedHashSet<String> pendingIds = new LinkedHashSet<>();
    private static final ScheduledExecutorService queue = Executors.newSingleThreadScheduledExecutor(
            runnable -> new Thread(runnable, "shortcutRemovalQueue"));
    private static boolean scheduled;

    private ShortcutRemovalQueue() {
    }

    static void remove(ArrayList<String> ids) {
        if (ids.isEmpty()) {
            return;
        }
        synchronized (pendingIds) {
            pendingIds.addAll(ids);
            if (!scheduled) {
                scheduled = true;
                queue.schedule(ShortcutRemovalQueue::flush, 100, TimeUnit.MILLISECONDS);
            }
        }
    }

    private static void flush() {
        final ArrayList<String> ids;
        synchronized (pendingIds) {
            ids = new ArrayList<>(pendingIds);
            pendingIds.clear();
            scheduled = false;
        }
        for (int start = 0; start < ids.size(); start += BATCH_SIZE) {
            ArrayList<String> batch = new ArrayList<>(ids.subList(start, Math.min(start + BATCH_SIZE, ids.size())));
            try {
                ShortcutManagerCompat.removeDynamicShortcuts(ApplicationLoader.applicationContext, batch);
            } catch (Exception e) {
                FileLog.e(e);
            }
            if (Build.VERSION.SDK_INT >= 30) {
                try {
                    ShortcutManager manager = ApplicationLoader.applicationContext.getSystemService(ShortcutManager.class);
                    if (manager != null) {
                        manager.removeLongLivedShortcuts(batch);
                    }
                } catch (Exception e) {
                    FileLog.e(e);
                }
            }
        }
    }
}

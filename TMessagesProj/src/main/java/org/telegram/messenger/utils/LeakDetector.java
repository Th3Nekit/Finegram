package org.telegram.messenger.utils;

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.NotificationCenter;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

import me.vkryl.core.reference.ReferenceMap;

public final class LeakDetector {

    private static final int LEAK_THRESHOLD = 5;

    private static final long CHECK_INTERVAL_MS = 1_000L;

    private static final long GC_RECHECK_DELAY_MS = 2_000L;

    private static volatile LeakDetector instance;

    @NonNull
    public static LeakDetector getInstance() {
        if (instance == null) {
            synchronized (LeakDetector.class) {
                if (instance == null) {
                    instance = new LeakDetector();
                }
            }
        }
        return instance;
    }

    private final ReferenceMap<Class<?>, Object> registry = new ReferenceMap<>(false);

    private final Set<Class<?>> reportedLeaks = new HashSet<>();

    private final Map<Class<?>, Integer> pendingRecheck = new HashMap<>();

    private boolean running;

    private final Runnable checkRunnable = new Runnable() {
        @Override
        public void run() {
            check();
            if (running) {
                AndroidUtilities.runOnUIThread(this, CHECK_INTERVAL_MS);
            }
        }
    };

    private LeakDetector() {}

    @MainThread
    public void start() {
        if (running) return;
        running = true;
        AndroidUtilities.runOnUIThread(checkRunnable, CHECK_INTERVAL_MS);
    }

    @MainThread
    public void stop() {
        if (!running) return;
        running = false;
        AndroidUtilities.cancelRunOnUIThread(checkRunnable);
    }

    @MainThread
    public <T> void add(@NonNull T object) {
        registry.add(object.getClass(), object);
    }

    @MainThread
    private void check() {
        final Set<Class<?>> keys = registry.keySetUnchecked();
        if (keys == null) return;

        for (Class<?> clazz : new ArrayList<>(keys)) {
            if (reportedLeaks.contains(clazz)) continue;

            final int count = countLiveInstances(clazz);

            if (count >= LEAK_THRESHOLD) {
                if (!pendingRecheck.containsKey(clazz)) {

                    pendingRecheck.put(clazz, count);
                    System.gc();
                    AndroidUtilities.runOnUIThread(() -> confirmLeak(clazz), GC_RECHECK_DELAY_MS);
                }

            } else {

                pendingRecheck.remove(clazz);
            }
        }
    }

    @MainThread
    private void confirmLeak(final Class<?> clazz) {
        pendingRecheck.remove(clazz);

        if (reportedLeaks.contains(clazz)) return;

        final int count = countLiveInstances(clazz);
        if (count >= LEAK_THRESHOLD && reportedLeaks.add(clazz)) {

        }
    }

    @MainThread
    private int countLiveInstances(final Class<?> clazz) {
        final Iterator<Object> it = registry.iterator(clazz);
        if (it == null) return 0;
        int count = 0;
        while (it.hasNext()) {
            it.next();
            count++;
        }
        return count;
    }
}
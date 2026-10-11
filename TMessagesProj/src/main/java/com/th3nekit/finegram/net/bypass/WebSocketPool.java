/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Обход блокировок: MTProto поверх WebSocket. Своя реализация на
 * наши серверы.
 */

package com.th3nekit.finegram.net.bypass;

import org.telegram.messenger.FileLog;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

public final class WebSocketPool {
    private static final long MAX_AGE_MS = 60_000L;
    private static final int HEALTH_TIMEOUT_MS = 1200;
    private static final int MAX_PER_KEY = 4;
    private static final int MAX_TOTAL = 8;

    private static final class Entry {
        final RawWebSocket ws;
        final long acquiredAtMs;
        boolean healthCheckInProgress;

        Entry(RawWebSocket ws, long acquiredAtMs) {
            this.ws = ws;
            this.acquiredAtMs = acquiredAtMs;
        }
    }

    private final Object lock = new Object();
    private final HashMap<Long, ArrayDeque<Entry>> idle = new HashMap<>();

    private static long keyOf(int dc, boolean isMedia) {
        return ((long) dc << 1) | (isMedia ? 1L : 0L);
    }

    public RawWebSocket get(int dc, boolean isMedia) {
        ArrayList<RawWebSocket> toClose = new ArrayList<>();
        RawWebSocket candidate = null;
        synchronized (lock) {
            pruneLocked(System.currentTimeMillis(), toClose);
            long key = keyOf(dc, isMedia);
            ArrayDeque<Entry> q = idle.get(key);
            if (q != null) {
                Iterator<Entry> it = q.iterator();
                while (it.hasNext()) {
                    Entry entry = it.next();
                    if (!entry.healthCheckInProgress) {
                        candidate = entry.ws;
                        it.remove();
                        break;
                    }
                }
                if (q.isEmpty()) idle.remove(key);
            }
        }
        closeAll(toClose);
        if (candidate == null) return null;
        if (!candidate.alive(HEALTH_TIMEOUT_MS)) {
            safeClose(candidate);
            return null;
        }
        return candidate;
    }

    public void put(int dc, boolean isMedia, RawWebSocket ws) {
        if (ws == null || isClosed(ws)) return;
        ArrayList<RawWebSocket> toClose = new ArrayList<>();
        synchronized (lock) {
            long now = System.currentTimeMillis();
            pruneLocked(now, toClose);
            long key = keyOf(dc, isMedia);
            ArrayDeque<Entry> q = idle.get(key);
            boolean accepted = true;
            if (q != null && q.size() >= MAX_PER_KEY) {
                accepted = evictOldestLocked(key, toClose);
            }
            if (accepted && countLocked() >= MAX_TOTAL) {
                accepted = evictOldestLocked(null, toClose);
            }
            if (accepted) {
                idle.computeIfAbsent(key, ignored -> new ArrayDeque<>(MAX_PER_KEY))
                        .addLast(new Entry(ws, now));
            } else {
                toClose.add(ws);
            }
        }
        closeAll(toClose);
    }

    public int size(int dc, boolean isMedia) {
        ArrayList<RawWebSocket> toClose = new ArrayList<>();
        int size;
        synchronized (lock) {
            pruneLocked(System.currentTimeMillis(), toClose);
            ArrayDeque<Entry> q = idle.get(keyOf(dc, isMedia));
            size = q == null ? 0 : q.size();
        }
        closeAll(toClose);
        return size;
    }

    public boolean hasRoom(int dc, boolean isMedia, int target) {
        ArrayList<RawWebSocket> toClose = new ArrayList<>();
        boolean room;
        synchronized (lock) {
            pruneLocked(System.currentTimeMillis(), toClose);
            ArrayDeque<Entry> q = idle.get(keyOf(dc, isMedia));
            room = countLocked() < MAX_TOTAL
                    && (q == null || q.size() < Math.min(target, MAX_PER_KEY));
        }
        closeAll(toClose);
        return room;
    }

    public void clear() {
        ArrayList<RawWebSocket> all = new ArrayList<>();
        synchronized (lock) {
            for (ArrayDeque<Entry> q : idle.values()) {
                for (Entry entry : q) all.add(entry.ws);
            }
            idle.clear();
        }
        closeAll(all);
    }

    public void healthScan() {
        ArrayList<RawWebSocket> toClose = new ArrayList<>();
        ArrayList<Entry> snapshot = new ArrayList<>();
        synchronized (lock) {
            pruneLocked(System.currentTimeMillis(), toClose);
            for (ArrayDeque<Entry> q : idle.values()) {
                for (Entry entry : q) {
                    if (!entry.healthCheckInProgress) {
                        entry.healthCheckInProgress = true;
                        snapshot.add(entry);
                    }
                }
            }
        }
        closeAll(toClose);
        toClose.clear();
        Set<Entry> failed = java.util.Collections.newSetFromMap(new IdentityHashMap<>());
        for (Entry entry : snapshot) {
            try {
                if (isClosed(entry.ws) || !entry.ws.ping(new byte[0], HEALTH_TIMEOUT_MS)) failed.add(entry);
            } catch (Throwable t) {
                failed.add(entry);
            }
        }
        synchronized (lock) {
            Iterator<Map.Entry<Long, ArrayDeque<Entry>>> it = idle.entrySet().iterator();
            while (it.hasNext()) {
                ArrayDeque<Entry> q = it.next().getValue();
                Iterator<Entry> entries = q.iterator();
                while (entries.hasNext()) {
                    Entry entry = entries.next();
                    if (failed.contains(entry)) {
                        entries.remove();
                        toClose.add(entry.ws);
                    }
                }
                if (q.isEmpty()) it.remove();
            }
            for (Entry entry : snapshot) entry.healthCheckInProgress = false;
            pruneLocked(System.currentTimeMillis(), toClose);
        }
        closeAll(toClose);
        if (!toClose.isEmpty()) FileLog.d("WebSocketPool healthScan dropped=" + toClose.size());
    }

    private void pruneLocked(long now, ArrayList<RawWebSocket> toClose) {
        Iterator<Map.Entry<Long, ArrayDeque<Entry>>> it = idle.entrySet().iterator();
        while (it.hasNext()) {
            ArrayDeque<Entry> q = it.next().getValue();
            Iterator<Entry> entries = q.iterator();
            while (entries.hasNext()) {
                Entry entry = entries.next();
                if (isClosed(entry.ws) || now - entry.acquiredAtMs > MAX_AGE_MS) {
                    entries.remove();
                    toClose.add(entry.ws);
                }
            }
            if (q.isEmpty()) it.remove();
        }
    }

    private int countLocked() {
        int count = 0;
        for (ArrayDeque<Entry> q : idle.values()) count += q.size();
        return count;
    }

    private boolean evictOldestLocked(Long onlyKey, ArrayList<RawWebSocket> toClose) {
        Entry oldest = null;
        Long oldestKey = null;
        for (Map.Entry<Long, ArrayDeque<Entry>> item : idle.entrySet()) {
            if (onlyKey != null && !onlyKey.equals(item.getKey())) continue;
            for (Entry entry : item.getValue()) {
                if (!entry.healthCheckInProgress
                        && (oldest == null || entry.acquiredAtMs < oldest.acquiredAtMs)) {
                    oldest = entry;
                    oldestKey = item.getKey();
                }
            }
        }
        if (oldest == null) return false;
        ArrayDeque<Entry> q = idle.get(oldestKey);
        q.remove(oldest);
        if (q.isEmpty()) idle.remove(oldestKey);
        toClose.add(oldest.ws);
        return true;
    }

    private static boolean isClosed(RawWebSocket ws) {
        try {
            return ws.isClosed();
        } catch (Throwable t) {
            return true;
        }
    }

    private static void closeAll(ArrayList<RawWebSocket> sockets) {
        for (RawWebSocket ws : sockets) safeClose(ws);
    }

    private static void safeClose(RawWebSocket ws) {
        try {
            ws.close();
        } catch (Throwable ignored) {
        }
    }
}

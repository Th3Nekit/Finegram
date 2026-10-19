/*
 * Finegram navigation diagnostics.
 * Licensed under GNU GPL v. 2 or later; see LICENSE.
 */

package com.th3nekit.finegram.core.crash;

import android.os.SystemClock;

import java.util.ArrayDeque;

public final class NavigationDiagnostics {
    private static final int LIMIT = 48;
    private static final int MAX_ENTRY = 1536;
    private static final ArrayDeque<String> entries = new ArrayDeque<>();

    private NavigationDiagnostics() {
    }

    public static synchronized void record(String event, String state) {
        if (entries.size() == LIMIT) {
            entries.removeFirst();
        }
        String entry = SystemClock.uptimeMillis() + " " + event + " " + state;
        entries.addLast(entry.length() > MAX_ENTRY ? entry.substring(0, MAX_ENTRY) : entry);
    }

    public static synchronized String snapshot() {
        if (entries.isEmpty()) {
            return "";
        }
        StringBuilder result = new StringBuilder("Proxy navigation (view state only):\n");
        for (String entry : entries) {
            result.append(entry).append('\n');
        }
        return result.toString();
    }

    public static synchronized String latest() {
        return entries.isEmpty() ? "" : "\n\nProxy navigation: " + entries.peekLast();
    }
}

package com.th3nekit.finegram.core.crash;

import java.util.ArrayDeque;

public final class CameraDiagnostics {
    private static final int MAX_ENTRIES = 768;
    private static final int MAX_ENTRY = 512;
    private static final ArrayDeque<String> EVENTS = new ArrayDeque<>();
    private static final long START = System.nanoTime();

    public static void record(String event, String state) {
        String entry = ((System.nanoTime() - START) / 1_000_000L) + "ms "
                + DiagnosticText.clean(event) + " " + DiagnosticText.clean(state);
        if (entry.length() > MAX_ENTRY) entry = entry.substring(0, MAX_ENTRY) + " [truncated]";
        synchronized (EVENTS) {
            while (EVENTS.size() >= MAX_ENTRIES) EVENTS.removeFirst();
            EVENTS.addLast(entry);
        }
    }

    public static void record(String event, Throwable error) {
        if (error == null) {
            record(event, "error=unknown");
            return;
        }
        StringBuilder state = new StringBuilder(error.getClass().getSimpleName()).append(": ").append(error.getMessage());
        StackTraceElement[] stack = error.getStackTrace();
        for (int i = 0; i < Math.min(4, stack.length); i++) state.append("\n at ").append(stack[i]);
        record(event, state.toString());
    }

    public static String snapshot() {
        synchronized (EVENTS) {
            if (EVENTS.isEmpty()) return "No camera events recorded in this process.\n";
            StringBuilder out = new StringBuilder();
            for (String event : EVENTS) out.append(event).append('\n');
            return out.toString();
        }
    }

    private CameraDiagnostics() {}
}

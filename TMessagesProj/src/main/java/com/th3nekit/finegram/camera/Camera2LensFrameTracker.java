package com.th3nekit.finegram.camera;

import java.util.Arrays;

public final class Camera2LensFrameTracker {
    private static final int CAPACITY = 16;

    private static final class Frame {
        final int generation;
        final long timestampNanos;

        Frame(int generation, long timestampNanos) {
            this.generation = generation;
            this.timestampNanos = timestampNanos;
        }
    }

    private final Frame[] frames = new Frame[CAPACITY];
    private int currentGeneration;
    private Object currentSession;
    private int next;
    private boolean firstFrame;

    public synchronized void invalidate(int generation) {
        currentGeneration = generation;
        currentSession = null;
        Arrays.fill(frames, null);
        next = 0;
        firstFrame = false;
    }

    public synchronized void beginSession(Object session, int generation) {
        if (session == null || generation != currentGeneration || currentSession == session) return;
        if (currentSession != null) return;
        currentSession = session;
    }

    public synchronized boolean record(Object session, int generation, long timestampNanos) {
        if (session == null || session != currentSession || generation != currentGeneration
                || timestampNanos <= 0) return false;
        for (Frame frame : frames) {
            if (frame != null && frame.timestampNanos == timestampNanos) return false;
        }
        frames[next] = new Frame(generation, timestampNanos);
        next = (next + 1) % CAPACITY;
        boolean first = !firstFrame;
        firstFrame = true;
        return first;
    }

    public synchronized int getFrameGeneration(Object session, int generation, long timestampNanos) {
        if (session == null || session != currentSession || generation != currentGeneration
                || timestampNanos <= 0) return -1;
        for (Frame frame : frames) {
            if (frame != null && frame.generation == generation && frame.timestampNanos == timestampNanos) {
                return generation;
            }
        }
        return -1;
    }
}

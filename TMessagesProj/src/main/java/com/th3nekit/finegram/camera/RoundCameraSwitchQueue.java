package com.th3nekit.finegram.camera;

public final class RoundCameraSwitchQueue {
    private boolean pending;

    public void toggle() { pending = !pending; }
    public boolean isPending() { return pending; }
    public void reset() { pending = false; }

    public boolean take(boolean ready) {
        if (!ready || !pending) return false;
        pending = false;
        return true;
    }
}

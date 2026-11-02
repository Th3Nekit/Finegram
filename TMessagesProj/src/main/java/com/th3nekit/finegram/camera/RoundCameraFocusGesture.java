package com.th3nekit.finegram.camera;

public final class RoundCameraFocusGesture {
    public static final int NONE = 0, TAP = 1, DOUBLE_TAP = 2;
    private final float slopSquared, doubleSlopSquared;
    private final long doubleTimeout, holdTimeout;
    private float downX, downY, lastX, lastY;
    private long downTime, lastUpTime = -1;
    private int pointerId;
    private boolean tracking;

    public RoundCameraFocusGesture(float slop, float doubleSlop, long doubleTimeout, long holdTimeout) {
        slopSquared = slop * slop;
        doubleSlopSquared = doubleSlop * doubleSlop;
        this.doubleTimeout = doubleTimeout;
        this.holdTimeout = holdTimeout;
    }

    public void down(float x, float y, int id, long time, boolean eligible) {
        tracking = eligible && Float.isFinite(x) && Float.isFinite(y);
        downX = x;
        downY = y;
        downTime = time;
        pointerId = id;
        if (!tracking) lastUpTime = -1;
    }

    public void move(float x, float y, int id) {
        if (tracking && (id != pointerId || !Float.isFinite(x) || !Float.isFinite(y)
                || distance(x, y, downX, downY) > slopSquared)) cancel();
    }

    public int up(float x, float y, int id, long time, boolean eligible, boolean doubleEnabled) {
        move(x, y, id);
        if (!tracking || !eligible || time < downTime || time - downTime > holdTimeout) {
            cancel();
            return NONE;
        }
        tracking = false;
        if (doubleEnabled && lastUpTime >= 0 && downTime >= lastUpTime
                && downTime - lastUpTime <= doubleTimeout
                && distance(x, y, lastX, lastY) <= doubleSlopSquared) {
            lastUpTime = -1;
            return DOUBLE_TAP;
        }
        lastUpTime = doubleEnabled ? time : -1;
        lastX = x;
        lastY = y;
        return TAP;
    }

    public void cancel() {
        tracking = false;
        lastUpTime = -1;
    }

    public boolean isTracking() { return tracking; }

    public void reset() { cancel(); }

    private static float distance(float x, float y, float x2, float y2) {
        return (x - x2) * (x - x2) + (y - y2) * (y - y2);
    }
}

package com.th3nekit.finegram.camera;

public final class RoundVideoFrameClock {
    private static final long MAX_DRIFT_NANOS = 100_000_000L;
    private static final long FROZEN_INTERVAL_NANOS = 28_000_000L;

    private boolean initialized;
    private long lastRawTimestamp;
    private int lastCameraId;
    private long lastRawArrival;
    private long lastRawMappedTimestamp;
    private long lastArrival;
    private long arrivalTimestamp;
    private long mappedTimestamp;

    public long accept(long rawTimestamp, int cameraId, long nowNano) {
        if (rawTimestamp <= 0) return -1;
        if (initialized && rawTimestamp == lastRawTimestamp && cameraId == lastCameraId) {
            return -1;
        }
        if (!initialized) {
            initialized = true;
            lastRawTimestamp = rawTimestamp;
            lastCameraId = cameraId;
            lastRawArrival = lastArrival = nowNano;
            lastRawMappedTimestamp = arrivalTimestamp = mappedTimestamp = rawTimestamp;
            return mappedTimestamp;
        }

        long elapsed = elapsed(nowNano, lastArrival);
        long expectedTimestamp = add(arrivalTimestamp, elapsed);
        long nextTimestamp = add(mappedTimestamp, elapsed);
        if (rawTimestamp > lastRawTimestamp) {
            long rawDelta = rawTimestamp - lastRawTimestamp;
            long rawElapsed = elapsed(nowNano, lastRawArrival);
            if (withinDrift(rawDelta, rawElapsed)) {
                long candidate = add(lastRawMappedTimestamp, rawDelta);
                if (candidate > mappedTimestamp && withinDrift(candidate, expectedTimestamp)) {
                    nextTimestamp = candidate;
                }
            }
        }

        if (nextTimestamp <= mappedTimestamp) return -1;
        mappedTimestamp = nextTimestamp;
        arrivalTimestamp = expectedTimestamp;
        if (elapsed > 0) lastArrival = nowNano;
        lastRawTimestamp = rawTimestamp;
        lastCameraId = cameraId;
        lastRawArrival = lastArrival;
        lastRawMappedTimestamp = mappedTimestamp;
        return mappedTimestamp;
    }

    public long advanceFrozen(long nowNano) {
        if (!initialized) return -1;
        long elapsed = elapsed(nowNano, lastArrival);
        if (elapsed < FROZEN_INTERVAL_NANOS) return -1;
        long nextTimestamp = add(mappedTimestamp, elapsed);
        if (nextTimestamp <= mappedTimestamp) return -1;
        mappedTimestamp = nextTimestamp;
        arrivalTimestamp = add(arrivalTimestamp, elapsed);
        lastArrival = nowNano;
        return mappedTimestamp;
    }

    public void reset() {
        initialized = false;
    }

    private static long elapsed(long now, long previous) {
        long difference = now - previous;
        return difference > 0 ? difference : 0;
    }

    private static boolean withinDrift(long first, long second) {
        return (first >= second ? first - second : second - first) <= MAX_DRIFT_NANOS;
    }

    private static long add(long timestamp, long delta) {
        return delta > Long.MAX_VALUE - timestamp ? Long.MAX_VALUE : timestamp + delta;
    }
}

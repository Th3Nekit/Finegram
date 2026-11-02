package com.th3nekit.finegram.core.crash;

final class CameraReportPolicy {
    static final int DEFAULT_UNTIL = 7350;

    static boolean byDefault(int versionCode) {
        return versionCode > 0 && versionCode <= DEFAULT_UNTIL;
    }

    static boolean enabled(int versionCode, Boolean choice) {
        return choice != null ? choice : byDefault(versionCode);
    }

    static boolean collect(int versionCode, Boolean choice, boolean informed) {
        return informed && enabled(versionCode, choice);
    }

    private CameraReportPolicy() {}
}

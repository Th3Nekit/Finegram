/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.updater;

import java.util.Arrays;
import java.util.HashSet;

final class FGUpdatePolicy {
    private FGUpdatePolicy() {
    }

    static boolean isNewRelease(String installed, int installedCode, String offered, int offeredCode) {
        if (offered == null || offered.trim().isEmpty()) return false;
        int comparison = compareVersions(offered, installed);
        return comparison > 0 || comparison == 0 && offeredCode > installedCode;
    }

    static int compareVersions(String left, String right) {
        String[] a = left.split("\\.");
        String[] b = right.split("\\.");
        for (int i = 0; i < Math.max(a.length, b.length); i++) {
            long x = component(i < a.length ? a[i] : "0");
            long y = component(i < b.length ? b[i] : "0");
            if (x != y) return Long.compare(x, y);
        }
        return 0;
    }

    private static long component(String value) {
        long result = 0;
        for (int i = 0; i < value.length(); i++) {
            char c = value.charAt(i);
            if (c < '0' || c > '9') break;
            result = Math.min(Integer.MAX_VALUE, result * 10 + c - '0');
        }
        return result;
    }

    static boolean acceptsPackage(String installedPackage, long installedCode, String archivePackage,
                                  long archiveCode, String[] installedSigners, String[] archiveSigners) {
        return installedPackage.equals(archivePackage) && archiveCode > installedCode
                && installedSigners != null && archiveSigners != null && installedSigners.length > 0
                && archiveSigners.length > 0
                && new HashSet<>(Arrays.asList(installedSigners)).equals(new HashSet<>(Arrays.asList(archiveSigners)));
    }
}

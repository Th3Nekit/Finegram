/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2. or later.
 */

package com.th3nekit.finegram.core.updater;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.os.Build;

import java.util.Arrays;
import java.util.HashSet;

final class FGUpdateSignatures {
    enum Result { MATCH, MISMATCH, UNREADABLE }

    private FGUpdateSignatures() {
    }

    static Result compare(PackageManager manager, String name, String path,
                          PackageInfo installed, PackageInfo archive) {
        String[] currentInstalled;
        String[] currentArchive;
        try {
            currentInstalled = signers(installed, Build.VERSION.SDK_INT >= 28);
            currentArchive = signers(archive, Build.VERSION.SDK_INT >= 28);
        } catch (RuntimeException error) {
            return Result.UNREADABLE;
        }
        if (currentInstalled != null && currentArchive != null) {
            return same(currentInstalled, currentArchive) ? Result.MATCH : Result.MISMATCH;
        }
        return Result.UNREADABLE;
    }

    private static String[] signers(PackageInfo info, boolean modern) {
        if (info == null) return null;
        Signature[] signatures;
        try {
            signatures = modern
                    ? info.signingInfo == null ? null : info.signingInfo.getApkContentsSigners()
                    : info.signatures;
        } catch (RuntimeException error) {
            return null;
        }
        if (signatures == null || signatures.length == 0) return null;
        String[] result = new String[signatures.length];
        for (int i = 0; i < signatures.length; i++) {
            if (signatures[i] == null) throw new IllegalArgumentException("Incomplete signer set");
            byte[] bytes = signatures[i].toByteArray();
            if (bytes == null || bytes.length == 0) throw new IllegalArgumentException("Empty signing certificate");
            result[i] = FGUpdateTransfer.hex(bytes);
        }
        return result;
    }

    private static boolean same(String[] left, String[] right) {
        return new HashSet<>(Arrays.asList(left)).equals(new HashSet<>(Arrays.asList(right)));
    }
}

/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.utils.system;

import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Build;

import com.th3nekit.finegram.core.configs.FinegramCameraConfig;

import org.telegram.messenger.ApplicationLoader;

public final class SystemUtils {

    public static int getRoundVideoResolution() {
        return FinegramCameraConfig.INSTANCE.getVideoMessagesResolution();
    }

    public static int getRoundVideoBitrate() {
        return FinegramCameraConfig.INSTANCE.getVideoMessagesBitrateKbps() * 1024;
    }

    public static int getRoundAudioBitrate() {
        return FinegramCameraConfig.INSTANCE.getVideoMessagesAudioBitrateKbps() * 1024;
    }

    public static long getRoundVideoMaxDurationMs() {
        return 60_000L;
    }

    public static boolean isPermissionGranted(String permission) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return true;
        }
        return ApplicationLoader.applicationContext.checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED;
    }

    public static void requestPermissions(Activity activity, int requestCode, String[] permissions) {
        if (activity != null && permissions != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            activity.requestPermissions(permissions, requestCode);
        }
    }

    public static boolean isAppInstalled(String packageName) {
        try {
            ApplicationLoader.applicationContext.getPackageManager().getPackageInfo(packageName, 0);
            return true;
        } catch (Throwable e) {
            return false;
        }
    }

    private SystemUtils() {
    }
}

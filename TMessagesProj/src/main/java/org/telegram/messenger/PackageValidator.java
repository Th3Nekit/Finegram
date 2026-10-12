package org.telegram.messenger;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.os.Build;
import android.os.Process;
import android.util.Base64;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.HashSet;
import java.util.Set;

public final class PackageValidator {

    private PackageValidator() {}

    private static final Set<String> KNOWN_PACKAGES = new HashSet<>();

    static {

        KNOWN_PACKAGES.add("com.google.android.projection.gearhead");

        KNOWN_PACKAGES.add("com.google.android.mediasimulator");

        KNOWN_PACKAGES.add("com.android.car.media");
        KNOWN_PACKAGES.add("com.android.car.carlauncher");
        KNOWN_PACKAGES.add("com.google.android.car.kitchensink");

        KNOWN_PACKAGES.add("com.google.android.wearable.app");
        KNOWN_PACKAGES.add("com.google.android.wearable.media.sessions");

        KNOWN_PACKAGES.add("com.google.android.googlequicksearchbox");
        KNOWN_PACKAGES.add("com.google.android.apps.gsa.staticplugins");

        KNOWN_PACKAGES.add("com.google.android.bluetooth");
    }

    public static boolean isKnownCaller(Context context, String callerPackageName, int callerUid) {
        if (callerPackageName == null) return false;
        if (callerUid == Process.SYSTEM_UID || callerUid == Process.myUid()) return true;
        if (KNOWN_PACKAGES.contains(callerPackageName)) return true;
        return hasPermission(context, callerPackageName, callerUid);
    }

    private static boolean hasPermission(Context context, String packageName, int uid) {
        PackageManager pm = context.getPackageManager();
        try {
            int contentControl = pm.checkPermission(
                    "android.permission.MEDIA_CONTENT_CONTROL", packageName);
            if (contentControl == PackageManager.PERMISSION_GRANTED) return true;
            int notifListener = pm.checkPermission(
                    "android.permission.BIND_NOTIFICATION_LISTENER_SERVICE", packageName);
            if (notifListener == PackageManager.PERMISSION_GRANTED) return true;
        } catch (Throwable ignore) {
        }
        return false;
    }
}

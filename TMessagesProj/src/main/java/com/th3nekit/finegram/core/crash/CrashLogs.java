/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.crash;

import android.app.Activity;
import android.os.Build;

import androidx.annotation.NonNull;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.BuildVars;
import org.telegram.messenger.FileLoader;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.SharedConfig;
import com.th3nekit.finegram.helpers.FGFileSender;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.LaunchActivity;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;

import com.th3nekit.finegram.core.FinegramLogger;
import com.th3nekit.finegram.core.configs.FinegramCameraConfig;
import com.th3nekit.finegram.core.helpers.FGResourcesHelper;
import com.th3nekit.finegram.preferences.CameraPreferencesEntry;

public class CrashLogs implements Thread.UncaughtExceptionHandler {

    @SuppressWarnings("unused")
    private static volatile byte[] reservedMemory;

    private final Thread.UncaughtExceptionHandler defaultUEH;

    public CrashLogs() {
        this.defaultUEH = Thread.getDefaultUncaughtExceptionHandler();
    }

    public static void updateOOMReserve() {
        if (reservedMemory == null) {
            reservedMemory = new byte[5 * 1024 * 1024];
        }
    }

    @Override
    public void uncaughtException(@NonNull Thread t, @NonNull Throwable e) {
        if (e instanceof OutOfMemoryError && reservedMemory != null) {
            reservedMemory = null;

            try {
                org.telegram.messenger.FileLog.e(e);
            } catch (Throwable ignored) {}
        }

        try {
            saveCrashLogs(e);
        } catch (Throwable ignored) {}

        try {
            com.th3nekit.finegram.plugins.FGSafeMode.onCrash();
        } catch (Throwable ignored) {}

        try {
            java.io.StringWriter trace = new java.io.StringWriter();
            e.printStackTrace(new PrintWriter(trace));
            com.th3nekit.finegram.plugins.FGPluginsController.rememberCrashFromStack(trace.toString());
        } catch (Throwable ignored) {}

        if (defaultUEH != null) {
            defaultUEH.uncaughtException(t, e);
        } else {
            System.exit(10);
        }
    }

    private static File getLogFile() {
        return new File(ApplicationLoader.getFilesDirFixed(), "last_crash.log");
    }

    private static File getShareLogFile() {
        return new File(FileLoader.getDirectory(FileLoader.MEDIA_DIR_CACHE), "Logcat-" + System.currentTimeMillis() + ".log");
    }

    private static void saveCrashLogs(Throwable throwable) throws IOException {
        File file = getLogFile();

        try (PrintWriter writer = new PrintWriter(new FileOutputStream(file, false))) {
            throwable.printStackTrace(writer);
        }
    }

    public static boolean isCrashed() {

        com.th3nekit.finegram.plugins.FGSafeMode.checkPreviousExit();
        return getLogFile().exists();
    }

    public static void saveExitInfo(String description, String trace) {
        final File file = getLogFile();
        if (file.exists()) {
            return;
        }
        try (PrintWriter writer = new PrintWriter(new FileOutputStream(file, false))) {
            writer.println(description);
            if (trace != null) {
                writer.println();
                writer.println(trace);
            }
        } catch (IOException e) {
            FinegramLogger.e(e);
        }
    }

    private static File shareLogs() throws IOException {
        File file = getShareLogFile();
        try (BufferedReader reader = new BufferedReader(new FileReader(getLogFile()));
             BufferedWriter writer = new BufferedWriter(new FileWriter(file))) {

            writer.write("Plugins: " + com.th3nekit.finegram.plugins.FGPluginsController.enabledPluginsForReport());
            writer.newLine();
            writer.write("Safe mode: " + com.th3nekit.finegram.plugins.FGSafeMode.isActive());
            writer.newLine();
            writer.newLine();
            char[] buffer = new char[8192];
            int count;
            while ((count = reader.read(buffer)) != -1) writer.write(buffer, 0, count);
        }
        return file;
    }

    private static String getPerformanceClassString() {
        return switch (SharedConfig.getDevicePerformanceClass()) {
            case SharedConfig.PERFORMANCE_CLASS_LOW -> "LOW";
            case SharedConfig.PERFORMANCE_CLASS_AVERAGE -> "AVERAGE";
            case SharedConfig.PERFORMANCE_CLASS_HIGH -> "HIGH";
            default -> "UNKNOWN";
        };
    }

    private static long crashTime() {
        final long modified = getLogFile().lastModified();
        return modified > 0 ? modified : System.currentTimeMillis();
    }

    private static String getCrashReportMessage() {
        return getReportMessage() + "\n\n" +
                "Crash Date: " + LocaleController.getInstance().getFormatterStats().format(crashTime()) +
                "\n\n#crash";
    }

    public static String getReportMessage() {
        return  "Steps to reproduce:\n" +
                "Write here the steps to reproduce\n\n" +
                "Details:\n"+
                "• Finegram Version: " + FGResourcesHelper.getFinegramVersion() + " (" + FGResourcesHelper.getAbiCode() + ")\n" +
                "• Telegram Version: " + BuildVars.BUILD_VERSION_STRING + " (" + FGResourcesHelper.getSourceCodeVersion() + ")\n" +
                "• Build Type: " + FGResourcesHelper.getBuildType() + "\n" +
                "• Build Date: " + FGResourcesHelper.getBuildDate() + "\n" +
                "• Device: " + FGResourcesHelper.INSTANCE.capitalize(Build.MANUFACTURER) + " " + Build.MODEL + "\n" +
                "• OS Version: " + Build.VERSION.RELEASE + " • SDK: " + Build.VERSION.SDK_INT + "\n" +
                "• Screen: " + AndroidUtilities.displaySize.x + "x" + AndroidUtilities.displaySize.y + " • DPI: " + AndroidUtilities.densityDpi + "\n" +
                "• Camera: " + CameraPreferencesEntry.getCameraName() + " • Dual: " + FinegramCameraConfig.INSTANCE.getUseDualCamera() + "\n" +
                "• Performance Class: " + getPerformanceClassString() + "\n" +
                "• Google Play Services: " + ApplicationLoader.hasPlayServices + "\n" +
                "• Locale: " + LocaleController.getSystemLocaleStringIso639();
    }

    @SuppressWarnings("ResultOfMethodCallIgnored")
    public static void deleteCrashLogs() {
        File file = getLogFile();
        if (file.exists()) {
            file.delete();
        }
    }

    public static void sendCrashLogs(Activity activity, CrashReportBottomSheet bottomSheet) {
        try {
            BaseFragment fragment = LaunchActivity.getLastFragment();
            if (fragment == null || fragment.getParentActivity() != activity) {
                return;
            }
            File logFile = CrashLogs.shareLogs();
            bottomSheet.dismiss();
            FGFileSender.pickChatAndSend(fragment, logFile, CrashLogs.getCrashReportMessage());
        } catch (IOException e) {
            FinegramLogger.e(e);
        }
    }

}

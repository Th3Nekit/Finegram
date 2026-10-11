/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.utils;

import android.util.Log;

import com.google.gson.Gson;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.BuildVars;
import org.telegram.messenger.FileLog;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Field;
import java.util.Calendar;

public final class AppUtils {

    private static final String TAG = "FGPlugins";

    private static volatile Gson gson;

    public static Gson getGson() {
        Gson local = gson;
        if (local == null) {
            synchronized (AppUtils.class) {
                local = gson;
                if (local == null) {
                    local = new Gson();
                    gson = local;
                }
            }
        }
        return local;
    }

    public static void ensureRunningOnUi(Runnable action) {
        if (action == null) {
            return;
        }
        if (android.os.Looper.myLooper() == android.os.Looper.getMainLooper()) {
            action.run();
        } else {
            AndroidUtilities.runOnUIThread(action);
        }
    }

    public static String getVersionText() {
        return BuildVars.BUILD_VERSION_STRING;
    }

    public static boolean isAppModified() {
        return false;
    }

    public static void log(String message) {
        logInternal(message, null, Log.DEBUG);
    }

    public static void log(String message, Throwable error) {
        logInternal(message, error, Log.ERROR);
    }

    public static void log(Throwable error) {
        logInternal(null, error, Log.ERROR);
    }

    private static void logInternal(String message, Throwable error, int level) {
        final String text = message == null ? "" : message;
        if (level >= Log.ERROR) {
            if (error != null) {
                FileLog.e(text, error);
            } else {
                FileLog.e(text);
            }
            Log.println(level, TAG, error == null ? text : text + "\n" + stackTraceToString(error));
        } else {
            FileLog.d(text);
            if (BuildVars.LOGS_ENABLED) {
                Log.println(level, TAG, text);
            }
        }
    }

    public static String stackTraceToString(Throwable error) {
        if (error == null) {
            return "";
        }
        final StringWriter out = new StringWriter();
        error.printStackTrace(new PrintWriter(out));
        return out.toString();
    }

    public static void printObjectDetails(Object object) {
        if (object == null) {
            log("null");
            return;
        }
        final StringBuilder out = new StringBuilder(object.getClass().getName()).append(" {");
        for (Class<?> type = object.getClass(); type != null && type != Object.class; type = type.getSuperclass()) {
            for (Field field : type.getDeclaredFields()) {
                try {
                    field.setAccessible(true);
                    out.append("\n  ").append(field.getName()).append(" = ").append(field.get(object));
                } catch (Throwable ignored) {
                }
            }
        }
        log(out.append("\n}").toString());
    }

    private static Field findField(Class<?> type, String name) throws NoSuchFieldException {
        for (Class<?> current = type; current != null; current = current.getSuperclass()) {
            try {
                final Field field = current.getDeclaredField(name);
                field.setAccessible(true);
                return field;
            } catch (NoSuchFieldException ignored) {
            }
        }
        throw new NoSuchFieldException(name);
    }

    public static Object getPrivateField(Object target, String name) {
        try {
            return findField(target.getClass(), name).get(target);
        } catch (Throwable e) {
            return null;
        }
    }

    public static void setPrivateField(Object target, String name, Object value) {
        try {
            findField(target.getClass(), name).set(target, value);
        } catch (Throwable ignored) {
        }
    }

    public static Object getPrivateStaticField(Class<?> type, String name) {
        try {
            return findField(type, name).get(null);
        } catch (Throwable e) {
            return null;
        }
    }

    public static void setPrivateStaticField(Class<?> type, String name, Object value) {
        try {
            findField(type, name).set(null, value);
        } catch (Throwable ignored) {
        }
    }

    public static int compareVersionValues(String left, String right) {
        final String[] a = (left == null ? "" : left).split("[^0-9]+");
        final String[] b = (right == null ? "" : right).split("[^0-9]+");
        for (int i = 0; i < Math.max(a.length, b.length); i++) {
            final int x = i < a.length && !a[i].isEmpty() ? Integer.parseInt(a[i]) : 0;
            final int y = i < b.length && !b[i].isEmpty() ? Integer.parseInt(b[i]) : 0;
            if (x != y) {
                return x < y ? -1 : 1;
            }
        }
        return 0;
    }

    public static boolean compareVersions(String current, String required, String ignored) {
        return compareVersionValues(current, required) >= 0;
    }

    public static boolean compareVersions(String current, int major, int minor) {
        return compareVersionValues(current, major + "." + minor) >= 0;
    }

    public static boolean isWinter() {
        final int month = Calendar.getInstance().get(Calendar.MONTH);
        return month == Calendar.DECEMBER || month == Calendar.JANUARY;
    }

    private AppUtils() {
    }
}

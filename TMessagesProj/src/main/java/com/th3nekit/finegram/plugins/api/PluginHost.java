/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins.api;

import android.content.Context;

import java.lang.reflect.Member;

public interface PluginHost {

    String id();

    Context context();

    void log(String message);

    void showBulletin(String text, String kind);

    void runOnUiThread(Runnable action);

    void runInBackground(Runnable action);

    boolean getBoolean(String key, boolean fallback);

    void setBoolean(String key, boolean value);

    int getInt(String key, int fallback);

    void setInt(String key, int value);

    String getString(String key, String fallback);

    void setString(String key, String value);

    Object hook(Member method, PluginMethodHook hook);

    void unhook(Object token);

    Class<?> findClass(String className);

    Object getField(Object target, String fieldName);

    void setField(Object target, String fieldName, Object value);

    void subscribe(PluginEvents events);

    void unsubscribe();
}

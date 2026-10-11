/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.plugins

import android.app.ActivityManager
import android.app.ApplicationExitInfo
import android.content.Context
import android.content.SharedPreferences
import android.os.Build
import com.th3nekit.finegram.core.FinegramLogger
import com.th3nekit.finegram.core.crash.CrashLogs
import com.th3nekit.finegram.core.crash.TombstoneText
import org.telegram.messenger.ApplicationLoader

object FGSafeMode {

    private const val KEY_ACTIVE = "active"
    private const val KEY_LAST_EXIT = "last_exit"
    private const val KEY_SUSPENDED_PYTHON = "suspended_python"
    private const val KEY_SUSPENDED_DEX = "suspended_dex"

    private const val ENABLED_PREFIX = "enabled_"

    @Volatile
    private var exitChecked = false

    private fun preferences() = ApplicationLoader.applicationContext
        .getSharedPreferences("finegram_safe_mode", Context.MODE_PRIVATE)

    private fun pythonPreferences() = ApplicationLoader.applicationContext
        .getSharedPreferences("finegram_plugins", Context.MODE_PRIVATE)

    private fun dexPreferences() = ApplicationLoader.applicationContext
        .getSharedPreferences("finegram_plugins_dex", Context.MODE_PRIVATE)

    @JvmStatic
    fun isActive(): Boolean = preferences().getBoolean(KEY_ACTIVE, false)

    @JvmStatic
    fun suspendedPlugins(): List<String> {
        val prefs = preferences()
        return (suspended(prefs, KEY_SUSPENDED_PYTHON) + suspended(prefs, KEY_SUSPENDED_DEX)).sorted()
    }

    private fun suspended(prefs: SharedPreferences, key: String): Set<String> =

        prefs.getStringSet(key, null)?.toSet() ?: emptySet()

    @JvmStatic
    fun onCrash() {
        activate()
    }

    private fun activate(): Boolean {
        val prefs = preferences()
        if (prefs.getBoolean(KEY_ACTIVE, false)) return false

        val python = enabledIds(pythonPreferences())
        val dex = enabledIds(dexPreferences())
        if (python.isEmpty() && dex.isEmpty()) return false

        prefs.edit()
            .putBoolean(KEY_ACTIVE, true)
            .putStringSet(KEY_SUSPENDED_PYTHON, python)
            .putStringSet(KEY_SUSPENDED_DEX, dex)
            .commit()

        switchAll(pythonPreferences(), python, false)
        switchAll(dexPreferences(), dex, false)
        FGPluginsController.forgetInstalled()
        FinegramLogger.d("FGPlugins") { "безопасный режим: погашено ${python.size + dex.size}" }
        return true
    }

    private fun enabledIds(prefs: SharedPreferences): Set<String> = try {
        prefs.all
            .filter { (key, value) -> key.startsWith(ENABLED_PREFIX) && value == true }
            .keys
            .map { it.removePrefix(ENABLED_PREFIX) }
            .toSet()
    } catch (e: Throwable) {
        FinegramLogger.e("FGPlugins", { "не удалось прочитать состояние плагинов" }, e)
        emptySet()
    }

    private fun switchAll(prefs: SharedPreferences, ids: Set<String>, value: Boolean) {
        if (ids.isEmpty()) return
        val editor = prefs.edit()
        ids.forEach { editor.putBoolean(ENABLED_PREFIX + it, value) }
        editor.commit()
    }

    @JvmStatic
    fun checkPreviousExit() {
        if (exitChecked) return
        exitChecked = true
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return
        try {
            val context = ApplicationLoader.applicationContext
            val manager = context.getSystemService(ActivityManager::class.java) ?: return
            val info = manager.getHistoricalProcessExitReasons(null, 0, 1).firstOrNull() ?: return
            val prefs = preferences()
            val firstRun = !prefs.contains(KEY_LAST_EXIT)
            if (info.timestamp <= prefs.getLong(KEY_LAST_EXIT, 0)) return
            prefs.edit().putLong(KEY_LAST_EXIT, info.timestamp).apply()
            if (firstRun) return
            val crashed = info.reason == ApplicationExitInfo.REASON_CRASH
                    || info.reason == ApplicationExitInfo.REASON_CRASH_NATIVE
                    || info.reason == ApplicationExitInfo.REASON_ANR
            if (!crashed) return

            if (info.reason != ApplicationExitInfo.REASON_CRASH) {
                CrashLogs.saveExitInfo(describe(info), exitTrace(info))
            }
            activate()
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "не удалось узнать, как завершился прошлый запуск" }, e)
        }
    }

    private fun describe(info: ApplicationExitInfo): String {
        val kind = when (info.reason) {
            ApplicationExitInfo.REASON_CRASH_NATIVE -> "Native crash"
            ApplicationExitInfo.REASON_ANR -> "App not responding"
            else -> "Crash"
        }
        return buildString {
            append(kind)
            if (info.status != 0) append(" (signal ").append(info.status).append(')')
            append(" at ").append(java.util.Date(info.timestamp))
            if (!info.description.isNullOrEmpty()) append('\n').append(info.description)
        }
    }

    private fun exitTrace(info: ApplicationExitInfo): String? {
        return try {
            val bytes = info.traceInputStream?.use { it.readBytes() } ?: return null
            when (info.reason) {
                ApplicationExitInfo.REASON_ANR -> String(bytes, 0, minOf(bytes.size, 256 * 1024))
                ApplicationExitInfo.REASON_CRASH_NATIVE -> TombstoneText.format(bytes)
                else -> null
            }
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "не удалось прочитать отчёт системы о вылете" }, e)
            null
        }
    }

    @JvmStatic
    fun turnOff() {
        val prefs = preferences()
        switchAll(pythonPreferences(), suspended(prefs, KEY_SUSPENDED_PYTHON), true)
        switchAll(dexPreferences(), suspended(prefs, KEY_SUSPENDED_DEX), true)
        prefs.edit()
            .putBoolean(KEY_ACTIVE, false)
            .remove(KEY_SUSPENDED_PYTHON)
            .remove(KEY_SUSPENDED_DEX)
            .apply()
        FGPluginsController.forgetInstalled()
        FGPluginsController.startAfterSafeMode()
    }
}

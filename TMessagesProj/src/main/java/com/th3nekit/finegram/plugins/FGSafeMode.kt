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

/**
 * Безопасный режим: после вылета клиент поднимается без плагинов.
 *
 * Плагины вешают перехваты на отрисовку и сеть, и упавший перехват роняет клиент при каждом
 * запуске. Раньше выключался только тот плагин, чей код виден в стеке, а это ловит не всё:
 * нативный вылет внутри перехвата стека Java не оставляет, а плагин, который сам подгрузил
 * dex, в стеке назван чужим именем. Поэтому после любого вылета при включённых плагинах
 * следующий запуск идёт без них.
 *
 * Плагины при этом выключаются по-настоящему, а не просто не запускаются. Так было не
 * всегда: сперва переключатели оставляли нетронутыми, чтобы выход из режима вернул всё как
 * было. Выглядело это так, будто плагины работают, хотя они не работали, а включить один
 * вручную было нельзя — он уже «включён». Теперь список выключенных запоминается отдельно,
 * и кнопка возврата поднимает ровно тех, кого режим погасил.
 *
 * Вылет Java замечает обработчик падений. Нативный вылет и зависание обработчик не видит,
 * но причину прошлого завершения процесса система хранит сама — сверяемся с ней при старте.
 */
object FGSafeMode {

    private const val KEY_ACTIVE = "active"
    private const val KEY_LAST_EXIT = "last_exit"
    private const val KEY_SUSPENDED_PYTHON = "suspended_python"
    private const val KEY_SUSPENDED_DEX = "suspended_dex"

    /** Ключ в настройках плагинов: у обоих видов он одинаковый, файлы разные. */
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

    /**
     * Кого погасил режим. Нужно журналу сбоя: в настройках их уже нет, а знать, при каких
     * плагинах всё сломалось, — половина разбора.
     */
    @JvmStatic
    fun suspendedPlugins(): List<String> {
        val prefs = preferences()
        return (suspended(prefs, KEY_SUSPENDED_PYTHON) + suspended(prefs, KEY_SUSPENDED_DEX)).sorted()
    }

    private fun suspended(prefs: SharedPreferences, key: String): Set<String> =
        // getStringSet отдаёт то же множество, что лежит внутри; копируем, иначе правка
        // испортит хранимое
        prefs.getStringSet(key, null)?.toSet() ?: emptySet()

    /**
     * Вылет Java. Зовётся из обработчика падений, когда процессу остаются доли секунды,
     * поэтому пишем сразу на диск.
     */
    @JvmStatic
    fun onCrash() {
        activate()
    }

    /**
     * Включает режим и гасит плагины. Возвращает false, если гасить было нечего или режим
     * уже включён — второй заход затёр бы список тех, кого погасил первый.
     */
    private fun activate(): Boolean {
        val prefs = preferences()
        if (prefs.getBoolean(KEY_ACTIVE, false)) return false

        val python = enabledIds(pythonPreferences())
        val dex = enabledIds(dexPreferences())
        if (python.isEmpty() && dex.isEmpty()) return false

        // Список пишем до того, как гасим: оборвись процесс между двумя записями, плагины
        // останутся включёнными — это лучше, чем выключенные без возможности вернуть.
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

    /**
     * Сверяется с тем, как система завершила прошлый процесс.
     *
     * Одну и ту же запись разбираем один раз: её время запоминаем. При самом первом запуске
     * с этой проверкой старые записи только отмечаем — вылет недельной давности не повод
     * выключать плагины сейчас.
     */
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
            // Вылет Java уже записан обработчиком. Нативный и зависание журнала не оставляют —
            // пишем то, что знает о них система, чтобы шторка после сбоя показалась и без плагинов.
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

    /**
     * Подробности от системы. У зависания это стеки всех потоков текстом. У нативного вылета —
     * двоичный отчёт: сигнал, стек упавшего потока и хвост журнала процесса; его разбираем в
     * текст. Без него в журнал попадал только номер сигнала.
     */
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

    /**
     * Выход из режима: возвращаем ровно тех, кого он погасил, и запускаем сразу.
     *
     * Кого человек успел включить руками, пока режим был включён, второй раз не трогаем:
     * {@code enable} видит, что плагин уже работает, и ничего не перезагружает.
     */
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

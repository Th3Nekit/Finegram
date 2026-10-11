/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins

import android.net.Uri
import com.chaquo.python.PyObject
import com.chaquo.python.Python
import com.chaquo.python.android.AndroidPlatform
import org.json.JSONArray
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.ApplicationLoader
import org.telegram.messenger.NotificationCenter
import org.telegram.messenger.DispatchQueue
import com.th3nekit.finegram.core.FinegramLogger
import java.io.File

object FGPluginsController {

    private const val PLUGINS_DIR = "plugins"
    private const val LIBS_DIR = "libs"

    private const val SETTINGS_FILE = "plugin-settings.json"

    @JvmStatic
    val queue: DispatchQueue by lazy {
        DispatchQueue("fgPluginsQueue").also { created ->

            created.postRunnable {
                try {
                    android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_BACKGROUND)
                } catch (e: Throwable) {
                    FinegramLogger.d("FGPlugins") { "приоритет потока плагинов не понижен" }
                }
            }
        }
    }

    private const val START_DELAY = 2500L

    private var runtime: PyObject? = null

    data class Plugin(
        val id: String,
        val name: String,
        val version: String,
        val author: String,
        val description: String,

        val icon: String,
        val file: File
    ) {
        val enabled: Boolean get() = isEnabled(id)
    }

    private fun pluginsRoot(): File {
        val root = File(ApplicationLoader.getFilesDirFixed(), PLUGINS_DIR)
        if (!root.exists()) root.mkdirs()
        return root
    }

    private fun runtime(): PyObject? {
        runtime?.let { return it }
        return try {
            if (!Python.isStarted()) {
                Python.start(AndroidPlatform(ApplicationLoader.applicationContext))
            }
            val module = Python.getInstance().getModule("finegram.runtime")
            module.callAttr(
                "configure",
                File(pluginsRoot(), SETTINGS_FILE).absolutePath,
                FGPluginLogSink,

                File(pluginsRoot(), LIBS_DIR).apply { mkdirs() }.absolutePath,
                FGLibraryProgress
            )

            module.callAttr("set_bridge", FGHookBridge)
            runtime = module
            module
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "не удалось запустить Python" }, e)
            null
        }
    }

    @JvmStatic
    fun runtimeOrNull(): PyObject? = runtime

    @JvmStatic
    fun bootRuntime(): PyObject? = runtime()

    @JvmStatic
    fun note(message: String) {
        val module = runtimeOrNull() ?: return
        try {
            module.callAttr("log", message)
        } catch (e: Throwable) {
            FinegramLogger.d("FGPlugins") { "строка в журнал не записалась" }
        }
    }

    @JvmStatic
    fun recentLog(): String {
        val module = runtimeOrNull() ?: return ""
        return try {
            module.callAttr("recent_log").toString()
        } catch (e: Throwable) {
            ""
        }
    }

    @JvmStatic
    fun clearLog() {
        val module = runtimeOrNull() ?: return
        try {
            module.callAttr("clear_log")
        } catch (e: Throwable) {
            FinegramLogger.d("FGPlugins") { "журнал не очистился" }
        }
    }

    @JvmStatic
    fun setSetting(pluginId: String, key: String, value: Any?) {
        val module = runtimeOrNull() ?: return
        try {
            module.callAttr("set_setting", pluginId, key, value)
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "настройка $key у $pluginId не записалась" }, e)
        }
    }

    @JvmStatic
    fun pluginInstance(pluginId: String): PyObject? {
        val module = runtimeOrNull() ?: return null
        return try {
            module.callAttr("plugin_instance", pluginId)
        } catch (e: Throwable) {
            null
        }
    }

    private val BYPASS_CLASH_IDS = setOf("wsbypass", "ws_bypass", "greenpass")

    @JvmStatic
    fun clashesWithBypass(id: String): Boolean =
        BYPASS_CLASH_IDS.contains(id.lowercase())

    @JvmStatic
    fun blockedByBypass(id: String): Boolean =
        clashesWithBypass(id) && com.th3nekit.finegram.net.bypass.FGBypassConfig.enabled

    fun installed(): List<Plugin> {
        val root = pluginsRoot()
        val stamp = root.lastModified()
        val cached = installedCache
        if (cached != null && installedStamp == stamp) {
            return cached
        }
        val list = root.listFiles()
            ?.mapNotNull { file ->
                when {
                    file.isFile && file.name.endsWith(".plugin") -> readHeader(file)
                    file.isDirectory && FGPluginBundle.isUnpacked(file) -> FGPluginBundle.readMeta(file)
                    else -> null
                }
            }
            ?.sortedBy { it.name.lowercase() }
            ?: emptyList()
        installedCache = list
        installedStamp = stamp
        return list
    }

    @Volatile
    private var installedCache: List<Plugin>? = null

    @Volatile
    private var installedStamp: Long = -1

    internal fun forgetInstalled() {
        installedCache = null
        installedStamp = -1

        AndroidUtilities.runOnUIThread {
            NotificationCenter.getGlobalInstance()
                .postNotificationName(NotificationCenter.pluginsUpdated)
        }
    }

    private val HEADER = Regex("""^__(\w+)__\s*=\s*["'](.*?)["']""", RegexOption.MULTILINE)

    private fun unescape(raw: String): String {
        if (raw.indexOf(SLASH) < 0) return raw
        val out = StringBuilder(raw.length)
        var i = 0
        while (i < raw.length) {
            val c = raw[i]
            if (c != SLASH || i == raw.length - 1) {
                out.append(c)
                i++
                continue
            }
            when (val next = raw[i + 1]) {
                'n' -> out.append('\n')
                't' -> out.append('\t')
                'r' -> out.append('\r')
                SLASH, '"', '\'' -> out.append(next)
                else -> {

                    out.append(c)
                    out.append(next)
                }
            }
            i += 2
        }
        return out.toString()
    }

    private const val SLASH = '\\'

    private fun readHeader(file: File): Plugin? {
        val head = try {
            file.inputStream().use { input ->
                val buffer = ByteArray(8192)
                val read = input.read(buffer)
                if (read <= 0) return null
                String(buffer, 0, read, Charsets.UTF_8)
            }
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "не прочитать шапку ${file.name}" }, e)
            return null
        }

        val meta = HashMap<String, String>()
        for (match in HEADER.findAll(head)) {
            meta[match.groupValues[1]] = unescape(match.groupValues[2])
        }
        val id = meta["id"] ?: file.nameWithoutExtension
        if (id.isEmpty()) return null
        return Plugin(
            id = id,
            name = meta["name"] ?: id,
            version = meta["version"] ?: "",
            author = meta["author"] ?: "",
            description = meta["description"] ?: "",
            icon = meta["icon"] ?: "",
            file = file
        )
    }

    fun readPreview(file: File): Plugin? =
        if (FGPluginBundle.looksLikeBundle(file)) FGPluginBundle.readMeta(file) else readHeader(file)

    fun readPreview(uri: Uri): Plugin? {
        val context = ApplicationLoader.applicationContext ?: return null
        return try {
            val bytes = context.contentResolver.openInputStream(uri)?.use { input ->
                val head = ByteArray(8192)
                val read = input.read(head)
                if (read <= 0) return null
                head.copyOfRange(0, read)
            } ?: return null
            val temp = File.createTempFile("finegram-preview", ".plugin", context.cacheDir)
            temp.writeBytes(bytes)
            val plugin = readHeader(temp)
            temp.delete()
            plugin
        } catch (e: Throwable) {
            null
        }
    }

    fun install(uri: Uri, suggestedName: String?): Plugin? {
        val context = ApplicationLoader.applicationContext ?: return null
        return try {
            val bytes = context.contentResolver.openInputStream(uri)?.use { it.readBytes() } ?: return null
            installBytes(bytes, suggestedName)
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "не удалось прочитать плагин" }, e)
            null
        }
    }

    fun install(file: File): Plugin? = try {
        installBytes(file.readBytes(), file.name)
    } catch (e: Throwable) {
        FinegramLogger.e("FGPlugins", { "не удалось прочитать плагин" }, e)
        null
    }

    private fun installBytes(bytes: ByteArray, suggestedName: String?): Plugin? {
        if (FGPluginBundle.looksLikeBundle(bytes)) {
            return installBundle(bytes)
        }
        val temp = File.createTempFile("finegram-plugin", ".plugin", ApplicationLoader.applicationContext.cacheDir)
        temp.writeBytes(bytes)

        val meta = readHeader(temp)
        if (meta == null || meta.id.isEmpty()) {
            temp.delete()
            return null
        }
        val target = File(pluginsRoot(), sanitize(meta.id) + ".plugin")
        val wasEnabled = target.exists() && isEnabled(meta.id)
        temp.copyTo(target, overwrite = true)
        temp.delete()

        val installed = readHeader(target) ?: return null
        forgetInstalled()

        if (wasEnabled && isRunning(installed.id)) {
            disable(installed)
            enable(installed)
        }

        FinegramLogger.d("FGPlugins") { "установлен плагин ${installed.id}" }
        return installed
    }

    private fun installBundle(bytes: ByteArray): Plugin? {
        val cache = ApplicationLoader.applicationContext.cacheDir
        val archive = File.createTempFile("finegram-bundle", ".${FGPluginBundle.EXTENSION}", cache)
        return try {
            archive.writeBytes(bytes)
            val meta = FGPluginBundle.readMeta(archive) ?: return null
            val staging = File(cache, "finegram-bundle-" + sanitize(meta.id))
            if (!FGPluginBundle.unpack(archive, staging)) {
                return null
            }
            val target = File(pluginsRoot(), sanitize(meta.id))
            val wasEnabled = target.exists() && isEnabled(meta.id)
            if (wasEnabled) {
                installed().firstOrNull { it.id == meta.id }?.let { disable(it) }
            }
            if (target.exists() && !target.deleteRecursively()) {
                staging.deleteRecursively()
                return null
            }
            if (!staging.renameTo(target)) {
                staging.copyRecursively(target, overwrite = true)
                staging.deleteRecursively()
            }
            val installed = FGPluginBundle.readMeta(target) ?: return null
            forgetInstalled()
            if (wasEnabled) {
                enable(installed)
            }
            FinegramLogger.d("FGPlugins") { "установлена сборка ${installed.id}" }
            installed
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "сборка не установилась" }, e)
            null
        } finally {
            archive.delete()
        }
    }

    fun delete(plugin: Plugin) {
        disable(plugin)
        if (FGBundledPlugins.isBundled(plugin.id)) {

            return
        }
        if (plugin.file.isDirectory) {
            plugin.file.deleteRecursively()
        } else {
            plugin.file.delete()
        }
        forgetInstalled()
    }

    private const val LOADING_KEY = "loading_plugin"

    fun slowPlugins(): Map<String, Int> {
        val module = runtimeOrNull() ?: return emptyMap()
        return try {
            val result = LinkedHashMap<String, Int>()
            module.callAttr("slow_plugins").asList().forEach { entry ->
                val parts = entry.toString().split(":")
                if (parts.size >= 3) {
                    parts[2].toIntOrNull()?.let { longest -> result[parts[0]] = longest }
                }
            }
            result
        } catch (e: Throwable) {
            emptyMap()
        }
    }

    @JvmStatic
    fun rememberCrashFromStack(stack: String) {
        val marker = "finegram_plugin_"
        var from = stack.indexOf(marker)
        val blamed = LinkedHashSet<String>()
        while (from >= 0) {
            val start = from + marker.length
            var end = start
            while (end < stack.length && (stack[end].isLetterOrDigit() || stack[end] == '_')) {
                end++
            }
            if (end > start) {
                blamed.add(stack.substring(start, end))
            }
            from = stack.indexOf(marker, end)
        }
        if (blamed.isEmpty()) return

        val guilty = installed().map { it.id }.filter { id -> blamed.contains(_module_suffix(id)) }
        if (guilty.isEmpty()) return
        val editor = preferences().edit()
        val all = suspectedOfCrash().toMutableSet()
        guilty.forEach { id ->
            all.add(id)
            editor.putBoolean("enabled_$id", false)
        }
        editor.putStringSet("crashed_plugins", all)
        editor.commit()
    }

    private fun _module_suffix(pluginId: String): String =
        pluginId.replace(Regex("[^A-Za-z0-9_]"), "_")

    fun suspectedOfCrash(): Set<String> =
        preferences().getStringSet("crashed_plugins", emptySet()) ?: emptySet()

    fun forgetCrashSuspicion(pluginId: String) {
        setCrashStreak(pluginId, 0)
        val rest = suspectedOfCrash().toMutableSet()
        if (rest.remove(pluginId)) {
            preferences().edit().putStringSet("crashed_plugins", rest).apply()
        }
    }

    private fun rememberCrashSuspect(pluginId: String) {
        val all = suspectedOfCrash().toMutableSet()
        if (all.add(pluginId)) {
            preferences().edit().putStringSet("crashed_plugins", all).apply()
        }
    }

    private fun crashStreak(pluginId: String): Int =
        preferences().getInt("crash_streak_$pluginId", 0)

    private fun setCrashStreak(pluginId: String, value: Int) {
        val editor = preferences().edit()
        if (value <= 0) editor.remove("crash_streak_$pluginId") else editor.putInt("crash_streak_$pluginId", value)
        editor.apply()
    }

    private fun inspectPreviousRun() {
        val unfinished = preferences().getString(LOADING_KEY, null) ?: return
        preferences().edit().remove(LOADING_KEY).apply()
        val streak = crashStreak(unfinished) + 1
        setCrashStreak(unfinished, streak)
        if (streak < CRASH_STREAK_LIMIT) {
            FinegramLogger.d("FGPlugins") {
                "клиент оборвался на плагине $unfinished — даём ему ещё одну попытку"
            }
            return
        }
        rememberCrashSuspect(unfinished)
        setEnabled(unfinished, false)
        FinegramLogger.d("FGPlugins") {
            "плагин $unfinished уронил клиент $streak раза подряд — оставляем его выключенным"
        }
    }

    private const val CRASH_STREAK_LIMIT = 2

    fun enable(plugin: Plugin): Boolean {

        if (blockedByBypass(plugin.id)) {
            FinegramLogger.d("FGPlugins") { "плагин ${plugin.id} не включён: занят встроенный обход" }
            return false
        }

        if (isRunning(plugin.id)) {
            setEnabled(plugin.id, true)
            return true
        }
        val module = runtime() ?: return false
        preferences().edit().putString(LOADING_KEY, plugin.id).commit()
        return try {
            val id = module.callAttr("load", plugin.file.absolutePath).toString()
            val ok = id.isNotEmpty()
            if (ok) {
                setEnabled(plugin.id, true)

                setCrashStreak(plugin.id, 0)
                FGPluginsMenu.invalidate()

                FGPluginPanel.warmUp(plugin)
            }
            ok
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "плагин ${plugin.id} не включился" }, e)
            false
        } finally {

            preferences().edit().remove(LOADING_KEY).apply()
        }
    }

    fun disable(plugin: Plugin) {
        val module = runtime() ?: return
        try {
            module.callAttr("unload", plugin.id)
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "плагин ${plugin.id} не выключился" }, e)
        }
        setEnabled(plugin.id, false)
        FGPluginsMenu.invalidate()
        FGPluginPanel.forget()
    }

    fun lastError(pluginId: String): String {
        val module = runtime() ?: return ""
        return try {
            module.callAttr("last_error", pluginId).toString()
        } catch (e: Throwable) {
            ""
        }
    }

    fun isRunning(pluginId: String): Boolean {
        val module = runtimeOrNull() ?: return false
        return try {
            module.callAttr("has_plugin", pluginId).toBoolean()
        } catch (e: Throwable) {
            false
        }
    }

    fun callForError(pluginId: String, method: String): String {
        val module = runtimeOrNull() ?: return "плагины ещё не запущены"
        return try {
            module.callAttr("plugin_error", pluginId, method).toString()
        } catch (e: Throwable) {
            e.message ?: "вызов не удался"
        }
    }

    fun callString(pluginId: String, method: String): String {
        val module = runtimeOrNull() ?: return ""
        return try {
            module.callAttr("call_plugin", pluginId, method)?.toString() ?: ""
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "плагин $pluginId не ответил на $method" }, e)
            ""
        }
    }

    @Suppress("UNCHECKED_CAST")
    fun callMap(pluginId: String, method: String): Map<String, Any?> {
        val module = runtimeOrNull() ?: return emptyMap()
        return try {
            val result = module.callAttr("call_plugin", pluginId, method) ?: return emptyMap()
            result.toJava(Map::class.java) as? Map<String, Any?> ?: emptyMap()
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "плагин $pluginId не отдал таблицу из $method" }, e)
            emptyMap()
        }
    }

    data class MenuItem(
        val pluginId: String,
        val itemId: String,
        val menuType: String,
        val text: String,
        val subtext: String?,
        val icon: String?
    )

    @JvmOverloads
    fun menuItems(kind: String? = null, context: Map<String, Any?>? = null, includeHidden: Boolean = false): List<MenuItem> {
        val module = runtimeOrNull() ?: return emptyList()
        return try {
            val array = JSONArray(module.callAttr("menu_items_json", kind, context, includeHidden).toString())
            (0 until array.length()).mapNotNull { index ->
                val item = array.optJSONObject(index) ?: return@mapNotNull null
                MenuItem(
                    pluginId = item.optString("plugin"),
                    itemId = item.optString("id"),
                    menuType = item.optString("menu_type"),
                    text = item.optString("text"),
                    subtext = if (item.isNull("subtext")) null else item.optString("subtext"),
                    icon = if (item.isNull("icon")) null else item.optString("icon")
                )
            }
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "пункты меню не прочитались" }, e)
            emptyList()
        }
    }

    @JvmOverloads
    fun menuClick(item: MenuItem, context: Map<String, Any?>? = null) {
        val module = runtimeOrNull() ?: return
        try {
            module.callAttr("menu_click", item.pluginId, item.itemId, context)
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "пункт плагина ${item.pluginId} упал" }, e)
        }
    }

    @JvmStatic
    fun forgetSettings(pluginId: String) {
        val module = runtimeOrNull() ?: return
        try {
            module.callAttr("forget_settings", pluginId)
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "не сбросить настройки $pluginId" }, e)
        }
    }

    fun settingsJson(pluginId: String): String {
        val module = runtime() ?: return "[]"
        return try {
            module.callAttr("settings_json", pluginId).toString()
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "настройки плагина $pluginId не собрались" }, e)
            "[]"
        }
    }

    fun subSettingsJson(pluginId: String, screen: Int, index: Int): String {
        val module = runtime() ?: return "[]"
        return try {
            module.callAttr("sub_settings_json", pluginId, screen, index).toString()
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "вложенный экран плагина $pluginId не собрался" }, e)
            "[]"
        }
    }

    @JvmOverloads
    fun settingsClick(pluginId: String, index: Int, screen: Int = 0, view: Any? = null,
                      title: String? = null) {
        val module = runtime() ?: return
        try {
            module.callAttr("settings_click", pluginId, index, screen, view, title)
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "плагин $pluginId не принял нажатие" }, e)
        }
    }

    @JvmOverloads
    fun settingsLongClick(pluginId: String, index: Int, screen: Int = 0, view: Any? = null): Boolean {
        val module = runtime() ?: return false
        return try {
            module.callAttr("settings_long_click", pluginId, index, screen, view).toBoolean()
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "плагин $pluginId не принял долгое нажатие" }, e)
            false
        }
    }

    fun settingsView(pluginId: String, screen: Int, index: Int, context: android.content.Context): android.view.View? {
        val module = runtime() ?: return null
        return try {
            module.callAttr("settings_view", pluginId, screen, index, context)
                ?.toJava(android.view.View::class.java)
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "своя строка плагина $pluginId не нарисовалась" }, e)
            null
        }
    }

    @JvmOverloads
    fun settingsChanged(pluginId: String, index: Int, value: Any?, screen: Int = 0,
                        title: String? = null) {
        val module = runtime() ?: return
        try {
            module.callAttr("settings_changed", pluginId, index, value, screen, title)
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "плагин $pluginId не принял настройку" }, e)
        }
    }

    @JvmStatic
    fun restoreEnabled() {

        if (!restored.compareAndSet(false, true)) {
            return
        }
        inspectPreviousRun()

        FGSafeMode.checkPreviousExit()
        startEnabled()
    }

    @JvmStatic
    fun startAfterSafeMode() {
        startEnabled()
    }

    @JvmStatic
    fun hasEnabledPlugins(): Boolean {
        val python = try {
            preferences().all.any { (key, value) -> key.startsWith("enabled_") && value == true }
        } catch (e: Throwable) {
            false
        }
        return python || FGDexPlugins.hasEnabled()
    }

    @JvmStatic
    fun enabledPluginsForReport(): String {

        val suspended = try {
            FGSafeMode.suspendedPlugins()
        } catch (e: Throwable) {
            emptyList()
        }
        if (suspended.isNotEmpty()) {
            return suspended.joinToString(", ") + " (погашены безопасным режимом)"
        }
        val names = ArrayList<String>()
        try {
            preferences().all
                .filter { (key, value) -> key.startsWith("enabled_") && value == true }
                .keys.sorted()
                .forEach { names.add(it.removePrefix("enabled_")) }
        } catch (e: Throwable) {
            names.add("?")
        }
        try {
            FGDexPlugins.installed().filter { it.enabled }.forEach { names.add(it.id + " " + it.version + " (dex)") }
        } catch (e: Throwable) {
            names.add("dex ?")
        }
        return if (names.isEmpty()) "нет" else names.joinToString(", ")
    }

    private fun startEnabled() {

        queue.postRunnable({ FGDexPlugins.restoreEnabled() }, START_DELAY)

        queue.postRunnable({

            FGBundledPlugins.unpack()

            val hasEnabled = try {
                preferences().all.any { (key, value) -> key.startsWith("enabled_") && value == true }
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "не удалось прочитать состояние плагинов" }, e)
                false
            }
            if (!hasEnabled) {
                return@postRunnable
            }
            try {

                installed().filter { it.enabled }
                    .sortedByDescending { FGBundledPlugins.isBundled(it.id) }
                    .forEach { enable(it) }

                FGPluginsDispatcher.onAppStart()
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "не удалось восстановить плагины" }, e)
            }
        }, START_DELAY)
    }

    private val restored = java.util.concurrent.atomic.AtomicBoolean(false)

    private fun preferences() = ApplicationLoader.applicationContext
        .getSharedPreferences("finegram_plugins", android.content.Context.MODE_PRIVATE)

    private fun isEnabled(id: String): Boolean = preferences().getBoolean("enabled_$id", false)

    private fun setEnabled(id: String, value: Boolean) {
        preferences().edit().putBoolean("enabled_$id", value).apply()

        forgetInstalled()
    }

    private fun sanitize(raw: String): String =
        raw.trim().replace(Regex("[^A-Za-z0-9._-]"), "_").take(100).trim('.', '_')
}

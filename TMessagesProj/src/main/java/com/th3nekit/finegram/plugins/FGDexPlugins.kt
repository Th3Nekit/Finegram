/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins

import android.content.Context
import android.net.Uri
import com.th3nekit.finegram.core.FinegramLogger
import com.th3nekit.finegram.plugins.api.FinegramDexPlugin
import dalvik.system.DexClassLoader
import org.telegram.messenger.ApplicationLoader
import org.json.JSONObject
import java.io.File

object FGDexPlugins {

    private const val PLUGINS_DIR = "plugins-dex"
    private const val OPTIMIZED_DIR = "dex-plugins"
    private val EXTENSIONS = listOf(".dex", ".jar", ".zip")

    data class Plugin(
        val id: String,
        val name: String,
        val version: String,
        val author: String,
        val description: String,
        val entryClass: String,
        val file: File
    ) {
        val enabled: Boolean get() = isEnabled(id)
    }

    private class Running(
        val instance: FinegramDexPlugin,
        val host: FGDexPluginHost,
        val snapshot: File
    )

    private val running = HashMap<String, Running>()

    private fun pluginsRoot(): File {
        val root = File(ApplicationLoader.getFilesDirFixed(), PLUGINS_DIR)
        if (!root.exists()) root.mkdirs()
        return root
    }

    @JvmStatic
    fun looksLikePlugin(fileName: String?): Boolean {
        val name = fileName?.lowercase() ?: return false
        return EXTENSIONS.any { name.endsWith(it) }
    }

    fun installed(): List<Plugin> =
        pluginsRoot().listFiles { file -> file.isFile && looksLikePlugin(file.name) }
            ?.mapNotNull { readPlugin(it) }
            ?.sortedBy { it.name.lowercase() }
            ?: emptyList()

    fun readPreview(file: File): Plugin? = readMetadata(file, false)

    private fun readMetadata(file: File, allowCode: Boolean): Plugin? {
        val snapshot = snapshot(file) ?: return null
        try {
            val report = JSONObject(FGPluginsController.securityReport(snapshot))
            if (!allowCode || report.optBoolean("requires_review", true)) {
                return if (report.optBoolean("reviewable")) Plugin(
                    sanitize(file.nameWithoutExtension), file.nameWithoutExtension,
                    "", "", "", "", file
                ) else null
            }
            val entry = findEntryClass(snapshot) ?: return null
            val instance = entry.getDeclaredConstructor().newInstance() as FinegramDexPlugin
            return Plugin(
                id = sanitize(instance.id ?: ""),
                name = instance.name?.ifEmpty { file.nameWithoutExtension } ?: file.nameWithoutExtension,
                version = instance.version ?: "",
                author = instance.author ?: "",
                description = instance.description ?: "",
                entryClass = entry.name,
                file = file
            ).takeIf { it.id.isNotEmpty() }
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "не прочитать dex-плагин ${file.name}" }, e)
            return null
        } finally {
            snapshot.delete()
        }
    }

    private fun snapshot(file: File): File? {
        var target: File? = null
        return try {
            if (!file.isFile || file.length() > 32 * 1024 * 1024) return null
            val extension = EXTENSIONS.firstOrNull { file.name.lowercase().endsWith(it) } ?: ".dex"
            val copy = File.createTempFile("finegram-review", extension, ApplicationLoader.applicationContext.cacheDir)
            target = copy
            file.inputStream().use { input ->
                copy.outputStream().use { output ->
                    val buffer = ByteArray(8192)
                    var total = 0
                    while (true) {
                        val count = input.read(buffer)
                        if (count < 0) break
                        total += count
                        if (total > 32 * 1024 * 1024) throw java.io.IOException("Plugin is too large")
                        output.write(buffer, 0, count)
                    }
                }
            }
            if (!copy.setReadOnly()) throw java.io.IOException("Cannot protect plugin snapshot")
            copy
        } catch (e: Throwable) {
            target?.delete()
            null
        }
    }

    fun stage(uri: Uri): File? {
        var target: File? = null
        return try {
            val copy = File.createTempFile("finegram-install", ".dex", ApplicationLoader.applicationContext.cacheDir)
            target = copy
            ApplicationLoader.applicationContext.contentResolver.openInputStream(uri)?.use { input ->
                copy.outputStream().use { output ->
                    val buffer = ByteArray(8192)
                    var total = 0
                    while (true) {
                        val count = input.read(buffer)
                        if (count < 0) break
                        total += count
                        if (total > 32 * 1024 * 1024) throw java.io.IOException("Plugin is too large")
                        output.write(buffer, 0, count)
                    }
                }
            } ?: throw java.io.IOException("Cannot read plugin")
            if (!copy.setReadOnly()) throw java.io.IOException("Cannot protect plugin snapshot")
            copy
        } catch (e: Throwable) {
            target?.delete()
            null
        }
    }

    private fun readPlugin(file: File): Plugin? = readMetadata(file, true)

    private fun findEntryClass(file: File): Class<*>? {
        val names = FGDexScanner.classNames(file)
        if (names.isEmpty()) {
            return null
        }
        val loader = createLoader(file) ?: return null
        for (name in names) {
            val candidate = try {

                Class.forName(name, false, loader)
            } catch (e: Throwable) {
                continue
            }
            if (FinegramDexPlugin::class.java.isAssignableFrom(candidate) &&
                !candidate.isInterface &&
                !java.lang.reflect.Modifier.isAbstract(candidate.modifiers)
            ) {
                return candidate
            }
        }
        return null
    }

    private fun createLoader(file: File): DexClassLoader? = try {

        if (file.canWrite()) {
            file.setReadOnly()
        }
        val optimized = File(ApplicationLoader.applicationContext.codeCacheDir, OPTIMIZED_DIR)
        if (!optimized.exists()) optimized.mkdirs()
        DexClassLoader(file.absolutePath, optimized.absolutePath, null, FGDexPlugins::class.java.classLoader)
    } catch (e: Throwable) {
        FinegramLogger.e("FGPlugins", { "не подготовить загрузчик для ${file.name}" }, e)
        null
    }

    fun install(uri: Uri, suggestedName: String?): Plugin? {
        val staged = stage(uri) ?: return null
        return try {
            installBytes(staged.readBytes(), suggestedName ?: "plugin.dex")
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "не прочитать dex-плагин" }, e)
            null
        } finally {
            staged.delete()
        }
    }

    fun install(file: File): Plugin? {
        val snapshot = snapshot(file) ?: return null
        return try {
            installBytes(snapshot.readBytes(), file.name)
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "не прочитать dex-плагин" }, e)
            null
        } finally {
            snapshot.delete()
        }
    }

    fun installReviewed(file: File, fingerprint: String, acceptsRisk: Boolean): Plugin? {
        return try {
            val bytes = FGPluginSecurity.reviewedBytes(file, fingerprint, acceptsRisk) ?: return null
            installBytes(bytes, file.name)
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "проверенный dex-плагин не установился" }, e)
            null
        }
    }

    private fun installBytes(bytes: ByteArray, sourceName: String): Plugin? {
        val extension = EXTENSIONS.firstOrNull { sourceName.lowercase().endsWith(it) } ?: ".dex"
        val temp = File.createTempFile("finegram-dex", extension, ApplicationLoader.applicationContext.cacheDir)
        temp.writeBytes(bytes)

        val preview = readPlugin(temp)
        if (preview == null || preview.entryClass.isEmpty()) {
            temp.delete()
            return null
        }

        val target = File(pluginsRoot(), preview.id + extension)
        if (target.exists()) {
            disable(preview.id)
            target.setWritable(true)
            target.delete()
        }
        temp.copyTo(target, overwrite = true)
        temp.delete()

        val installed = readPlugin(target)
        if (installed != null) {
            FinegramLogger.d("FGPlugins") { "установлен dex-плагин ${installed.id}" }
        }
        return installed
    }

    fun delete(plugin: Plugin) {
        disable(plugin.id)
        plugin.file.setWritable(true)
        plugin.file.delete()
        setEnabled(plugin.id, false)
    }

    fun enable(plugin: Plugin): Boolean {
        if (running.containsKey(plugin.id)) {
            return true
        }
        val snapshot = snapshot(plugin.file) ?: return false
        var instance: FinegramDexPlugin? = null
        return try {
            if (JSONObject(FGPluginsController.securityReport(snapshot)).optBoolean("requires_review", true)) {
                setEnabled(plugin.id, false)
                lastErrors[plugin.id] = "Plugin code requires review"
                snapshot.delete()
                return false
            }
            val entry = findEntryClass(snapshot) ?: throw IllegalArgumentException("No plugin entry class")
            val loaded = entry.getDeclaredConstructor().newInstance() as FinegramDexPlugin
            instance = loaded
            val host = FGDexPluginHost(plugin.id)
            loaded.onLoad(host)
            running[plugin.id] = Running(loaded, host, snapshot)
            setEnabled(plugin.id, true)
            FinegramLogger.d("FGPlugins") { "dex-плагин ${plugin.id} включён" }
            true
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "dex-плагин ${plugin.id} не запустился" }, e)
            lastErrors[plugin.id] = describe(e)
            try {
                instance?.onUnload()
            } catch (_: Throwable) {
            }
            disable(plugin.id)
            snapshot.delete()
            false
        }
    }

    fun disable(pluginId: String) {
        val active = running.remove(pluginId)
        if (active != null) {
            try {
                active.instance.onUnload()
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "dex-плагин $pluginId упал при выключении" }, e)
            }
            active.snapshot.delete()
        }

        FGHookBridge.unhookAll(pluginId)
        FGPluginsDispatcher.unsubscribeDex(pluginId)
        setEnabled(pluginId, false)
    }

    fun disable(plugin: Plugin) = disable(plugin.id)

    private val restored = java.util.concurrent.atomic.AtomicBoolean(false)

    fun hasEnabled(): Boolean = try {
        installed().any { it.enabled }
    } catch (e: Throwable) {
        false
    }

    @JvmStatic
    fun restoreEnabled() {
        if (!restored.compareAndSet(false, true)) {
            return
        }
        try {
            installed().filter { it.enabled }.forEach { enable(it) }
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "не удалось восстановить dex-плагины" }, e)
        }
    }

    private val lastErrors = HashMap<String, String>()

    fun lastError(pluginId: String): String = lastErrors[pluginId] ?: ""

    private fun describe(error: Throwable): String {
        val text = StringBuilder(error.toString())
        error.stackTrace.take(6).forEach { text.append(System.lineSeparator()).append("    at ").append(it) }
        return text.toString()
    }

    private fun preferences() = ApplicationLoader.applicationContext
        .getSharedPreferences("finegram_plugins_dex", Context.MODE_PRIVATE)

    private fun isEnabled(id: String): Boolean = preferences().getBoolean("enabled_$id", false)

    private fun setEnabled(id: String, value: Boolean) {
        preferences().edit().putBoolean("enabled_$id", value).apply()
    }

    private fun sanitize(raw: String): String =
        raw.trim().replace(Regex("[^A-Za-z0-9._-]"), "_").take(100).trim('.', '_')
}

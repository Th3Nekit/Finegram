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
        val host: FGDexPluginHost
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

    fun readPreview(file: File): Plugin? {
        val entry = findEntryClass(file) ?: return null
        return try {
            val instance = entry.getDeclaredConstructor().newInstance() as FinegramDexPlugin
            Plugin(
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
            null
        }
    }

    private fun readPlugin(file: File): Plugin? = readPreview(file)

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
        val context = ApplicationLoader.applicationContext ?: return null
        return try {
            val bytes = context.contentResolver.openInputStream(uri)?.use { it.readBytes() } ?: return null
            installBytes(bytes, suggestedName ?: "plugin.dex")
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "не прочитать dex-плагин" }, e)
            null
        }
    }

    fun install(file: File): Plugin? = try {
        installBytes(file.readBytes(), file.name)
    } catch (e: Throwable) {
        FinegramLogger.e("FGPlugins", { "не прочитать dex-плагин" }, e)
        null
    }

    private fun installBytes(bytes: ByteArray, sourceName: String): Plugin? {
        val extension = EXTENSIONS.firstOrNull { sourceName.lowercase().endsWith(it) } ?: ".dex"
        val temp = File.createTempFile("finegram-dex", extension, ApplicationLoader.applicationContext.cacheDir)
        temp.writeBytes(bytes)

        val preview = readPreview(temp)
        if (preview == null) {
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
        return try {
            val loader = createLoader(plugin.file) ?: return false
            val entry = Class.forName(plugin.entryClass, true, loader)
            val instance = entry.getDeclaredConstructor().newInstance() as FinegramDexPlugin
            val host = FGDexPluginHost(plugin.id)
            instance.onLoad(host)
            running[plugin.id] = Running(instance, host)
            setEnabled(plugin.id, true)
            FinegramLogger.d("FGPlugins") { "dex-плагин ${plugin.id} включён" }
            true
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "dex-плагин ${plugin.id} не запустился" }, e)
            lastErrors[plugin.id] = describe(e)
            disable(plugin.id)
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

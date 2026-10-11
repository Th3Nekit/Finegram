/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins

import android.util.Base64
import dalvik.system.InMemoryDexClassLoader
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.ApplicationLoader
import org.telegram.messenger.LocaleController
import org.telegram.messenger.R
import org.telegram.ui.ActionBar.AlertDialog
import org.telegram.ui.ActionBar.BaseFragment
import org.telegram.ui.LaunchActivity
import com.th3nekit.finegram.core.FinegramLogger
import java.io.ByteArrayOutputStream
import java.io.File
import java.nio.ByteBuffer
import java.security.MessageDigest
import java.util.zip.Inflater

object FGPluginPanel {

    private val MARKERS = listOf(
        "# __WSDASH_BEGIN__" to "# __WSDASH_END__",
        "# __DEX_BEGIN__" to "# __DEX_END__",
        "# __PAYLOAD_BEGIN__" to "# __PAYLOAD_END__"
    )

    private const val KNOWN_PANEL_CLASS = "com.th3web.wsdash.Panel"

    private const val REFRESH_DELAY = 1000L

    private var loader: ClassLoader? = null
    private var loadedFrom: String = ""
    private var loadedStamp: String = ""
    private var loadedClass: String = ""

    @Volatile
    @JvmStatic
    var lastError: String = ""
        private set

    @JvmStatic
    fun forget() {
        watching = null
        watchingClass = null
        lastState = ""
    }

    @JvmStatic
    fun isAvailable(plugin: FGPluginsController.Plugin?): Boolean {
        if (plugin == null) return false
        return try {
            extract(plugin.file) != null
        } catch (e: Throwable) {
            false
        }
    }

    @JvmStatic
    fun warmUp(plugin: FGPluginsController.Plugin?) {
        if (plugin == null) return
        try {
            panelClass(plugin)
        } catch (e: Throwable) {
            FinegramLogger.d("FGPlugins") { "окно плагина ${plugin.id} не подготовилось" }
        }
    }

    private class Prepared(
        val panelClass: Class<*>,
        val state: String,
        val actions: Map<String, Any?>
    )

    @JvmStatic
    fun openWithProgress(fragment: BaseFragment?, plugin: FGPluginsController.Plugin?) {
        if (fragment == null || plugin == null) return

        val activity = fragment.parentActivity
        val progress = if (activity == null) null else AlertDialog(activity, AlertDialog.ALERT_TYPE_SPINNER)
        progress?.showDelayed(250)

        FGPluginsController.queue.postRunnable {
            var failure = ""
            var prepared: Prepared? = null

            if (!FGPluginsController.isRunning(plugin.id) && !FGPluginsController.enable(plugin)) {
                val reason = FGPluginsController.lastError(plugin.id)
                failure = if (reason.isEmpty()) "плагин не запустился" else reason.trim().lines().last()
            } else {
                prepared = prepare(plugin)
                if (prepared == null) {
                    failure = lastError.ifEmpty { "у плагина нет своего окна" }
                }
            }

            val ready = prepared
            val message = failure
            AndroidUtilities.runOnUIThread {
                try {
                    progress?.dismiss()
                } catch (e: Throwable) {

                }
                if (ready == null) {
                    showFailure(fragment, message)
                    return@runOnUIThread
                }
                val reason = show(fragment, plugin, ready)
                if (reason != null) showFailure(fragment, reason)
            }
        }
    }

    @JvmStatic
    fun open(fragment: BaseFragment?, plugin: FGPluginsController.Plugin?): String? {
        if (fragment == null || plugin == null) {
            return "экран недоступен"
        }
        val prepared = prepare(plugin) ?: return lastError.ifEmpty { "у плагина нет своего окна" }
        return show(fragment, plugin, prepared)
    }

    private fun prepare(plugin: FGPluginsController.Plugin): Prepared? {
        lastError = ""
        val panelClass = panelClass(plugin) ?: return null
        val state = FGPluginsController.callString(plugin.id, "_dash_state").ifEmpty { "{}" }
        val actions = FGPluginsController.callMap(plugin.id, "_dash_actions")
        return Prepared(panelClass, state, actions)
    }

    private fun show(fragment: BaseFragment, plugin: FGPluginsController.Plugin, prepared: Prepared): String? {
        return try {

            if (liveWindow(prepared.panelClass) != null) {
                return null
            }
            val create = prepared.panelClass.getMethod("create", String::class.java, Map::class.java)
            val panel = create.invoke(null, prepared.state, prepared.actions)
                ?: return "окно плагина не собралось"
            if (panel !is BaseFragment) {
                return "окно плагина не похоже на экран"
            }
            try {
                panel.currentAccount = fragment.currentAccount
            } catch (e: Throwable) {

            }
            if (fragment.presentFragment(panel)) {
                FinegramLogger.d("FGPlugins") { "окно плагина ${plugin.id} открыто" }
                watch(plugin, prepared.panelClass)
                return null
            }

            val host = LaunchActivity.getLastFragment()
            if (host != null && host !== fragment && host.presentFragment(panel)) {
                FinegramLogger.d("FGPlugins") { "окно плагина ${plugin.id} открыто с текущего экрана" }
                watch(plugin, prepared.panelClass)
                return null
            }
            "экран отказался открыться"
        } catch (e: Throwable) {
            val reason = describe(e)
            lastError = reason
            FinegramLogger.e("FGPlugins", { "окно плагина ${plugin.id} не открылось" }, e)
            reason
        }
    }

    private var watching: FGPluginsController.Plugin? = null
    private var watchingClass: Class<*>? = null
    private var lastState: String = ""
    private var refreshScheduled = false

    private fun watch(plugin: FGPluginsController.Plugin, panelClass: Class<*>) {
        watching = plugin
        watchingClass = panelClass
        lastState = ""

        FGPluginsController.queue.postRunnable {
            try {
                FGPluginsController.callString(plugin.id, "_dash_watch")
            } catch (e: Throwable) {
                FinegramLogger.d("FGPlugins") { "плагин ${plugin.id} не следит за окном" }
            }
        }
        scheduleRefresh()
    }

    private fun scheduleRefresh() {
        if (refreshScheduled) return
        refreshScheduled = true
        AndroidUtilities.runOnUIThread({
            refreshScheduled = false
            refreshOnce()
        }, REFRESH_DELAY)
    }

    private fun refreshOnce() {
        val plugin = watching ?: return
        val panelClass = watchingClass ?: return
        val live = liveWindow(panelClass)
        if (live == null) {

            watching = null
            watchingClass = null
            lastState = ""
            return
        }
        FGPluginsController.queue.postRunnable {
            val state = try {
                FGPluginsController.callString(plugin.id, "_dash_state")
            } catch (e: Throwable) {
                ""
            }
            AndroidUtilities.runOnUIThread {
                if (state.isNotEmpty() && state != lastState) {
                    lastState = state
                    apply(panelClass, live, state)
                }
                if (watching != null) {
                    scheduleRefresh()
                }
            }
        }
    }

    private fun liveWindow(panelClass: Class<*>): Any? = try {
        panelClass.getMethod("active").invoke(null)
    } catch (e: Throwable) {
        null
    }

    private fun apply(panelClass: Class<*>, live: Any, state: String) {
        try {
            panelClass.getMethod("update", String::class.java).invoke(live, state)
        } catch (e: Throwable) {
            FinegramLogger.d("FGPlugins") { "окно не приняло новое состояние" }
        }
    }

    private fun panelClass(plugin: FGPluginsController.Plugin): Class<*>? {

        val path = plugin.file.absolutePath + ":" + plugin.file.lastModified() + ":" + plugin.file.length()
        val cached = loader
        if (cached != null && loadedFrom == path && loadedClass.isNotEmpty()) {
            shareWithPlugin(loadedStamp, cached)
            return try {
                val panel = cached.loadClass(loadedClass)
                primePlugin(plugin, panel)
                panel
            } catch (e: Throwable) {
                lastError = describe(e)
                null
            }
        }

        val dex = try {
            extract(plugin.file)
        } catch (e: Throwable) {
            lastError = describe(e)
            null
        }
        if (dex == null) {
            if (lastError.isEmpty()) lastError = "в файле плагина нет своего окна"
            return null
        }

        return try {
            val parent = ApplicationLoader.applicationContext.classLoader
            val created = InMemoryDexClassLoader(ByteBuffer.wrap(dex), parent)
            val panel = findPanel(created, dex)
            if (panel == null) {
                lastError = "в окне плагина нет точки входа"
                return null
            }
            loader = created
            loadedFrom = path
            loadedClass = panel.name
            loadedStamp = stampOf(dex)
            shareWithPlugin(loadedStamp, created)
            primePlugin(plugin, panel)
            FinegramLogger.d("FGPlugins") { "окно плагина ${plugin.id} загружено: ${panel.name}, ${dex.size} байт" }
            panel
        } catch (e: Throwable) {
            lastError = describe(e)
            FinegramLogger.e("FGPlugins", { "окно плагина ${plugin.id} не загрузилось" }, e)
            null
        }
    }

    private fun findPanel(classLoader: ClassLoader, dex: ByteArray): Class<*>? {
        try {
            val known = classLoader.loadClass(KNOWN_PANEL_CLASS)
            if (hasEntryPoint(known)) return known
        } catch (e: Throwable) {

        }
        for (name in FGDexScanner.classNames(dex)) {
            if (name.contains('$')) continue
            val candidate = try {
                classLoader.loadClass(name)
            } catch (e: Throwable) {
                continue
            }
            if (hasEntryPoint(candidate)) return candidate
        }
        return null
    }

    private fun hasEntryPoint(candidate: Class<*>): Boolean = try {
        val method = candidate.getMethod("create", String::class.java, Map::class.java)
        java.lang.reflect.Modifier.isStatic(method.modifiers)
    } catch (e: Throwable) {
        false
    }

    private fun shareWithPlugin(stamp: String, classLoader: ClassLoader) {
        val module = FGPluginsController.runtimeOrNull() ?: return
        try {
            module.callAttr("share_dex_loader", stamp, classLoader)
        } catch (e: Throwable) {
            FinegramLogger.d("FGPlugins") { "загрузчик окна не отдан плагину" }
        }
    }

    private fun primePlugin(plugin: FGPluginsController.Plugin, panelClass: Class<*>) {
        val module = FGPluginsController.runtimeOrNull() ?: return
        val methods = HashMap<String, Any?>()
        methods["cls"] = panelClass
        fun put(key: String, name: String, vararg types: Class<*>) {
            try {
                methods[key] = panelClass.getMethod(name, *types)
            } catch (e: Throwable) {

            }
        }
        put("create", "create", String::class.java, Map::class.java)
        put("active", "active")
        put("update", "update", String::class.java)
        put("toast", "toast", String::class.java, String::class.java)
        put("isopen", "isOpen")
        put("close", "close")
        try {
            module.callAttr("prime_panel", plugin.id, methods)
        } catch (e: Throwable) {
            FinegramLogger.d("FGPlugins") { "плагин ${plugin.id} не принял готовое окно" }
        }
    }

    private fun stampOf(dex: ByteArray): String {
        return try {
            val digest = MessageDigest.getInstance("SHA-256").digest(dex)
            val text = StringBuilder(16)
            for (i in 0 until 8) {
                text.append(String.format("%02x", digest[i]))
            }
            text.toString()
        } catch (e: Throwable) {
            ""
        }
    }

    private fun extract(file: File): ByteArray? {
        if (!file.exists()) return null
        val body = file.readText()
        for ((begin, end) in MARKERS) {
            val start = body.lastIndexOf(begin)
            val stop = body.lastIndexOf(end)
            if (start < 0 || stop < 0 || stop < start) continue

            val packed = StringBuilder()
            body.substring(start + begin.length, stop).lineSequence().forEach { line ->
                val trimmed = line.trim()
                if (trimmed.startsWith("# ")) {
                    packed.append(trimmed.substring(2).trim())
                }
            }
            if (packed.isEmpty()) continue

            val dex = try {
                inflate(Base64.decode(packed.toString(), Base64.DEFAULT))
            } catch (e: Throwable) {
                continue
            }
            if (dex.size > 8 && dex[0] == 'd'.code.toByte() && dex[1] == 'e'.code.toByte()) {
                return dex
            }
        }
        return null
    }

    private fun inflate(data: ByteArray): ByteArray {
        val inflater = Inflater()
        inflater.setInput(data)
        val output = ByteArrayOutputStream(data.size * 4)
        val buffer = ByteArray(16 * 1024)
        try {
            while (!inflater.finished()) {
                val read = inflater.inflate(buffer)
                if (read == 0 && inflater.needsInput()) break
                output.write(buffer, 0, read)
            }
        } finally {
            inflater.end()
        }
        return output.toByteArray()
    }

    @JvmStatic
    fun showFailure(fragment: BaseFragment?, reason: String?) {
        if (fragment == null) return
        val activity = fragment.parentActivity ?: return
        val text = if (reason.isNullOrEmpty()) "причина неизвестна" else reason
        val builder = AlertDialog.Builder(activity, fragment.resourceProvider)
        builder.setTitle(LocaleController.getString(R.string.ErrorOccurred))
        builder.setMessage(text)
        builder.setPositiveButton(LocaleController.getString(R.string.Copy)) { _, _ ->
            AndroidUtilities.addToClipboard(text)
        }
        builder.setNegativeButton(LocaleController.getString(R.string.Close), null)
        fragment.showDialog(builder.create())
    }

    private fun describe(error: Throwable): String {
        val root = error.cause ?: error
        val message = root.message
        return if (message.isNullOrEmpty()) root.javaClass.simpleName else message
    }
}

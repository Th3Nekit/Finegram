/*
 * Finegram plugin store.
 * Adapted from Kangel Plugins Manager (KangelPlugins).
 * Upstream: https://git.kangel.xyz/KangelPlugins/PluginManager
 * Licensed under GNU GPL v3; see LICENSE.PluginManager and NOTICE.
 */

package com.th3nekit.finegram.store

import com.th3nekit.finegram.core.FinegramLogger
import com.th3nekit.finegram.core.configs.FinegramCoreConfig
import com.th3nekit.finegram.core.icons.pack.IconPackManager
import com.th3nekit.finegram.plugins.FGPluginBundle
import com.th3nekit.finegram.plugins.FGPluginsController
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.ApplicationLoader
import org.telegram.messenger.Utilities
import java.io.File

/**
 * Ставит и обновляет то, что предлагает магазин.
 *
 * Файл сначала скачивается целиком во временную папку и только потом отдаётся установщику:
 * оборванная закачка не должна оставить в клиенте половину плагина.
 */
object FGStoreInstaller {

    fun interface Done {
        fun onDone(ok: Boolean)
    }

    fun interface Updates {
        fun onUpdates(items: List<FGStore.Item>)
    }

    /** Что сейчас качается: по этому списку строки магазина показывают ход установки. */
    private val running = java.util.Collections.synchronizedSet(HashSet<String>())

    @JvmStatic
    fun isRunning(id: String): Boolean = running.contains(id)

    /**
     * Скачивает и ставит.
     *
     * @param done зовётся в потоке интерфейса; `true` — поставилось.
     */
    @JvmStatic
    fun install(item: FGStore.Item, done: Done?) {
        if (!running.add(item.id)) {
            return
        }
        Utilities.globalQueue.postRunnable {
            var ok = false
            var temp: File? = null
            try {
                temp = tempFileFor(item)
                if (FGStore.download(item, temp)) {
                    // Опись пришла по защищённому соединению, файл — откуда угодно.
                    if (FGStoreVerify.verify(item, temp) == FGStoreVerify.Result.MISMATCH) {
                        FinegramLogger.e("FGStore", { "файл ${item.id} не совпал с описью" })
                    } else {
                        ok = when (item.kind) {
                            FGStore.Kind.PLUGIN -> FGPluginsController.install(temp) != null
                            FGStore.Kind.ICONS -> IconPackManager.install(temp) != null
                        }
                    }
                }
            } catch (e: Throwable) {
                FinegramLogger.e("FGStore", { "установка ${item.id} не удалась" }, e)
            } finally {
                try {
                    temp?.delete()
                } catch (ignored: Throwable) {
                }
                running.remove(item.id)
                val result = ok
                if (done != null) {
                    AndroidUtilities.runOnUIThread { done.onDone(result) }
                }
            }
        }
    }

    fun interface Prepared {
        fun onPrepared(file: File?)
    }

    /** Скачивает и проверяет, но не ставит: разбор возможен только по коду плагина. */
    @JvmStatic
    fun prepare(item: FGStore.Item, done: Prepared) {
        if (!running.add(item.id)) {
            return
        }
        Utilities.globalQueue.postRunnable {
            var ready: File? = null
            try {
                val temp = tempFileFor(item)
                if (FGStore.download(item, temp)) {
                    if (FGStoreVerify.verify(item, temp) == FGStoreVerify.Result.MISMATCH) {
                        FinegramLogger.e("FGStore", { "файл ${item.id} не совпал с описью" })
                        temp.delete()
                    } else {
                        ready = temp
                    }
                }
            } catch (e: Throwable) {
                FinegramLogger.e("FGStore", { "файл ${item.id} не скачался" }, e)
            } finally {
                running.remove(item.id)
                AndroidUtilities.runOnUIThread { done.onPrepared(ready) }
            }
        }
    }

    fun interface BypassClash {
        fun onClash(pluginName: String)
    }

    /** Кому рассказать, что плагин не запустился из-за встроенного обхода. */
    @JvmField
    var onBypassClash: BypassClash? = null

    /** Ставит то, что уже скачано и показано. */
    @JvmStatic
    fun installReady(item: FGStore.Item, file: File, done: Done?) {
        installPrepared(item, file, null, false, done)
    }

    @JvmStatic
    fun installReviewedReady(item: FGStore.Item, file: File, fingerprint: String, acceptsRisk: Boolean, done: Done?) {
        installPrepared(item, file, fingerprint, acceptsRisk, done)
    }

    private fun installPrepared(item: FGStore.Item, file: File, fingerprint: String?, acceptsRisk: Boolean, done: Done?) {
        if (!running.add(item.id)) {
            if (fingerprint != null) file.delete()
            return
        }
        Utilities.globalQueue.postRunnable {
            var ok = false
            var blocked: String? = null
            try {
                ok = when (item.kind) {
                    FGStore.Kind.PLUGIN -> {
                        // Поставить мало: без включения плагин лежит на диске и не
                        // запускается, а со стороны выглядит как «скачалось и ничего».
                        val plugin = if (fingerprint == null) FGPluginsController.install(file)
                            else FGPluginsController.installReviewed(file, fingerprint, acceptsRisk)
                        if (plugin != null) {
                            if (FGPluginsController.blockedByBypass(plugin.id)) {
                                // Такой плагин перехватывает то же соединение, что и
                                // встроенный обход. Молчать нельзя: человек видит
                                // установленный плагин, который ничего не делает.
                                blocked = plugin.name
                            } else {
                                FGPluginsController.enable(plugin)
                            }
                        }
                        plugin != null
                    }
                    FGStore.Kind.ICONS -> IconPackManager.install(file) != null
                }
            } catch (e: Throwable) {
                FinegramLogger.e("FGStore", { "установка ${item.id} не удалась" }, e)
            } finally {
                try {
                    file.delete()
                } catch (ignored: Throwable) {
                }
                running.remove(item.id)
                val result = ok
                val blockedName = blocked
                if (blockedName != null) {
                    AndroidUtilities.runOnUIThread { onBypassClash?.onClash(blockedName) }
                }
                if (done != null) {
                    AndroidUtilities.runOnUIThread { done.onDone(result) }
                }
            }
        }
    }

    private fun tempFileFor(item: FGStore.Item): File {
        // Расширение — по адресу: сборка и однофайловый плагин ставятся по-разному.
        val suffix = when {
            item.kind == FGStore.Kind.ICONS -> ".icons"
            item.url.substringBefore('?').endsWith(".${FGPluginBundle.EXTENSION}") ->
                ".${FGPluginBundle.EXTENSION}"
            else -> ".plugin"
        }
        val dir = File(ApplicationLoader.applicationContext.cacheDir, "finegram-store")
        dir.mkdirs()
        return File.createTempFile("download-", suffix, dir)
    }

    /** Версия установленного плагина или набора; пустая строка — не установлено. */
    fun installedVersion(item: FGStore.Item): String = when (item.kind) {
        FGStore.Kind.PLUGIN ->
            FGPluginsController.installed().firstOrNull { it.id == item.id }?.version ?: ""
        FGStore.Kind.ICONS ->
            IconPackManager.installed().firstOrNull { it.id == item.id }?.version ?: ""
    }

    @JvmStatic
    fun isInstalled(item: FGStore.Item): Boolean = installedVersion(item).isNotEmpty()

    /**
     * Что уже стоит: опознаватель — версия. Список спрашивает о состоянии каждую из
     * шестисот записей, поэтому папка читается один раз на пересборку.
     */
    @JvmStatic
    fun installedVersions(): Map<String, String> {
        val map = HashMap<String, String>()
        for (plugin in FGPluginsController.installed()) {
            map[plugin.id] = plugin.version
        }
        for (pack in IconPackManager.installed()) {
            map[pack.id] = pack.version
        }
        return map
    }

    /** Есть ли в магазине версия новее установленной. */
    @JvmStatic
    fun hasUpdate(item: FGStore.Item): Boolean {
        val current = installedVersion(item)
        return current.isNotEmpty() && item.version.isNotEmpty() && isNewer(item.version, current)
    }

    /**
     * Сравнивает версии по числам через точку.
     *
     * Строковое сравнение здесь врёт: «0.10.0» оказалось бы старше «0.9.0».
     */
    /** Сравнивает версии по числам через точку: меньше нуля — первая старше. */
    @JvmStatic
    fun compareVersions(left: String, right: String): Int {
        val a = left.split('.', '-', '_')
        val b = right.split('.', '-', '_')
        for (i in 0 until maxOf(a.size, b.size)) {
            val x = a.getOrNull(i)?.toIntOrNull() ?: 0
            val y = b.getOrNull(i)?.toIntOrNull() ?: 0
            if (x != y) {
                return if (x < y) -1 else 1
            }
        }
        return 0
    }

    @JvmStatic
    fun isNewer(candidate: String, current: String): Boolean {
        val a = candidate.split('.', '-', '_')
        val b = current.split('.', '-', '_')
        for (i in 0 until maxOf(a.size, b.size)) {
            val left = a.getOrNull(i)?.toIntOrNull() ?: 0
            val right = b.getOrNull(i)?.toIntOrNull() ?: 0
            if (left != right) {
                return left > right
            }
        }
        return false
    }

    /**
     * Обновляет то, что уже стоит.
     *
     * Молча — ставит сам, иначе только собирает список: спросить человека должен экран,
     * а не фоновая проверка.
     */
    @JvmStatic
    fun checkUpdates(ready: Updates?) {
        FGStore.refresh(false) {
            val pending = FGStore.cached().filter { hasUpdate(it) }
            if (pending.isEmpty()) {
                ready?.onUpdates(pending)
                return@refresh
            }
            if (FinegramCoreConfig.pluginAutoUpdate == FinegramCoreConfig.PLUGIN_UPDATES_SILENT) {
                for (item in pending) {
                    install(item, null)
                }
            }
            ready?.onUpdates(pending)
        }
    }
}

/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.plugins

import android.content.Context
import android.content.SharedPreferences
import org.json.JSONObject
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.ApplicationLoader
import org.telegram.messenger.FileLoader
import org.telegram.messenger.MessageObject
import org.telegram.messenger.UserConfig
import org.telegram.messenger.Utilities
import org.telegram.tgnet.ConnectionsManager
import org.telegram.tgnet.TLRPC
import com.th3nekit.finegram.core.FinegramLogger
import com.th3nekit.finegram.helpers.network.NetworkHelper
import java.io.File
import java.net.HttpURLConnection
import java.net.URL

/**
 * Обновление установленных плагинов.
 *
 * При установке запоминается, откуда пришёл файл, чтобы позже сходить за новой версией.
 * Источник выбирается по порядку: адрес из шапки плагина, затем указанный там канал, затем
 * место, откуда файл поставили.
 *
 * Проверка только узнаёт версию и сообщает о новой. Установку запускает пользователь.
 */
object FGPluginUpdates {

    /** Столько времени держим последний ответ проверки, чтобы не ходить в сеть на каждый вход. */
    private const val CHECK_INTERVAL = 6 * 60 * 60 * 1000L

    /** Больше этого плагин не качаем: файл такого размера — уже не плагин. */
    private const val MAX_PLUGIN_BYTES = 16 * 1024 * 1024

    /** Сколько последних сообщений канала просматриваем в поисках свежего файла. */
    private const val CHANNEL_LOOKUP_LIMIT = 40

    /** Откуда плагин пришёл и куда идти за новой версией. */
    data class Source(
        val url: String = "",
        val channel: String = "",
        val dialogId: Long = 0,
        val messageId: Int = 0,
        val fileName: String = "",
        val account: Int = UserConfig.selectedAccount
    ) {
        val isEmpty: Boolean get() = url.isEmpty() && channel.isEmpty() && dialogId == 0L
    }

    /** Что нашлось на той стороне. */
    data class Found(
        val pluginId: String,
        val version: String,
        val bytes: ByteArray,
        val fromChannel: Boolean
    )

    private fun preferences(): SharedPreferences =
        ApplicationLoader.applicationContext.getSharedPreferences("finegram_plugin_updates", Context.MODE_PRIVATE)

    // --- источник ---

    /**
     * Запоминает, откуда плагин поставили.
     *
     * Зовётся при установке. Если человек поставил плагин повторно из другого места,
     * запись перетирается: свежий источник вернее старого.
     */
    @JvmStatic
    fun remember(pluginId: String, source: Source) {
        if (pluginId.isEmpty() || source.isEmpty) return
        val json = JSONObject()
            .put("url", source.url)
            .put("channel", source.channel)
            .put("dialogId", source.dialogId)
            .put("messageId", source.messageId)
            .put("fileName", source.fileName)
            .put("account", source.account)
        preferences().edit().putString(key(pluginId), json.toString()).apply()
    }

    /** Запоминает канал и сообщение, из которого плагин пришёл файлом. */
    @JvmStatic
    fun rememberFromMessage(pluginId: String, message: MessageObject?) {
        if (message == null) return
        val document = message.document
        remember(
            pluginId,
            Source(
                dialogId = message.dialogId,
                messageId = message.id,
                fileName = document?.let { FileLoader.getDocumentFileName(it) } ?: "",
                account = message.currentAccount
            )
        )
    }

    @JvmStatic
    fun source(pluginId: String): Source {
        val raw = preferences().getString(key(pluginId), null) ?: return Source()
        return try {
            val json = JSONObject(raw)
            Source(
                url = json.optString("url"),
                channel = json.optString("channel"),
                dialogId = json.optLong("dialogId"),
                messageId = json.optInt("messageId"),
                fileName = json.optString("fileName"),
                account = json.optInt("account", UserConfig.selectedAccount)
            )
        } catch (e: Throwable) {
            Source()
        }
    }

    @JvmStatic
    fun forget(pluginId: String) {
        preferences().edit()
            .remove(key(pluginId))
            .remove(key(pluginId) + "_found")
            .remove(key(pluginId) + "_checked")
            .apply()
    }

    private fun key(pluginId: String) = "src_" + pluginId

    // --- что уже известно про обновление ---

    /** Версия, которую нашли в прошлый раз; пусто — обновления нет или ещё не искали. */
    @JvmStatic
    fun knownUpdate(pluginId: String): String =
        preferences().getString(key(pluginId) + "_found", "") ?: ""

    private fun rememberFound(pluginId: String, version: String) {
        preferences().edit()
            .putString(key(pluginId) + "_found", version)
            .putLong(key(pluginId) + "_checked", System.currentTimeMillis())
            .apply()
    }

    private fun checkedRecently(pluginId: String): Boolean {
        val at = preferences().getLong(key(pluginId) + "_checked", 0)
        return at > 0 && System.currentTimeMillis() - at < CHECK_INTERVAL
    }

    // --- сравнение версий ---

    /**
     * Новее ли [candidate], чем [current].
     *
     * Считаем по числам между точками, как это делает обновление самого клиента: «1.10»
     * больше «1.9», хотя по алфавиту это не так. Нечисловые хвосты вроде «beta» отбрасываем:
     * сравнивать их всё равно нечем, а сломать порядок они могут.
     */
    @JvmStatic
    fun isNewer(current: String, candidate: String): Boolean {
        if (candidate.isBlank()) return false
        if (current.isBlank()) return true
        val a = numbers(current)
        val b = numbers(candidate)
        for (i in 0 until maxOf(a.size, b.size)) {
            val left = a.getOrElse(i) { 0 }
            val right = b.getOrElse(i) { 0 }
            if (left != right) return right > left
        }
        return false
    }

    private fun numbers(version: String): List<Int> =
        version.split('.', '-', '_', ' ')
            .mapNotNull { part ->
                val digits = part.takeWhile { it.isDigit() }
                if (digits.isEmpty()) null else digits.toIntOrNull()
            }

    // --- проверка ---

    /**
     * Ищет новую версию плагина.
     *
     * [force] обходит выдержку между проверками: её ставит кнопка «Проверить», а
     * автоматическая проверка выдержку соблюдает — иначе каждый вход на экран плагинов
     * означал бы поход в сеть за каждым из них.
     */
    @JvmStatic
    @JvmOverloads
    fun check(plugin: FGPluginsController.Plugin, force: Boolean = false, done: (String) -> Unit) {
        if (!force && checkedRecently(plugin.id)) {
            done(knownUpdate(plugin.id))
            return
        }
        val source = effectiveSource(plugin)
        if (source.isEmpty) {
            done("")
            return
        }
        Utilities.globalQueue.postRunnable {
            fetch(plugin, source) { found ->
                val version = if (found != null && isNewer(plugin.version, found.version)) found.version else ""
                rememberFound(plugin.id, version)
                AndroidUtilities.runOnUIThread { done(version) }
            }
        }
    }

    /** Проверяет все плагины по очереди и говорит, у скольких есть обновление. */
    @JvmStatic
    @JvmOverloads
    fun checkAll(force: Boolean = false, done: (Int) -> Unit) {
        val plugins = FGPluginsController.installed()
        if (plugins.isEmpty()) {
            done(0)
            return
        }
        var left = plugins.size
        var found = 0
        for (plugin in plugins) {
            check(plugin, force) { version ->
                if (version.isNotEmpty()) found++
                left--
                if (left == 0) done(found)
            }
        }
    }

    /**
     * Ставит новую версию.
     *
     * Плагин, который работал, после подмены файла продолжает работать: выключаем его перед
     * заменой и включаем после. Настройки переживают обновление — они лежат отдельно, по
     * опознавателю плагина, и файла не касаются.
     */
    @JvmStatic
    fun update(plugin: FGPluginsController.Plugin, done: (Boolean) -> Unit) {
        val source = effectiveSource(plugin)
        if (source.isEmpty) {
            done(false)
            return
        }
        Utilities.globalQueue.postRunnable {
            fetch(plugin, source) { found ->
                if (found == null || found.pluginId != plugin.id) {
                    AndroidUtilities.runOnUIThread { done(false) }
                    return@fetch
                }
                AndroidUtilities.runOnUIThread {
                    val wasRunning = FGPluginsController.isRunning(plugin.id)
                    if (wasRunning) FGPluginsController.disable(plugin)

                    val temp = File.createTempFile("finegram-update", ".plugin",
                        ApplicationLoader.applicationContext.cacheDir)
                    temp.writeBytes(found.bytes)
                    val installed = FGPluginsController.install(temp)
                    temp.delete()

                    if (installed == null) {
                        if (wasRunning) FGPluginsController.enable(plugin)
                        done(false)
                        return@runOnUIThread
                    }
                    rememberFound(plugin.id, "")
                    if (wasRunning) FGPluginsController.enable(installed)
                    done(true)
                }
            }
        }
    }

    /** Адрес автора важнее места, откуда файл переслали. */
    private fun effectiveSource(plugin: FGPluginsController.Plugin): Source {
        val declared = declaredSource(plugin)
        val remembered = source(plugin.id)
        return when {
            declared.url.isNotEmpty() -> declared.copy(fileName = remembered.fileName)
            declared.channel.isNotEmpty() -> declared.copy(
                fileName = remembered.fileName,
                account = remembered.account
            )
            else -> remembered
        }
    }

    /** Что автор написал в шапке: `__update_url__` или `__update_channel__`. */
    private fun declaredSource(plugin: FGPluginsController.Plugin): Source {
        val head = try {
            plugin.file.inputStream().use { input ->
                val buffer = ByteArray(8192)
                val read = input.read(buffer)
                if (read <= 0) return Source() else String(buffer, 0, read, Charsets.UTF_8)
            }
        } catch (e: Throwable) {
            return Source()
        }
        val meta = Regex("""^__(\w+)__\s*=\s*["'](.*?)["']""", RegexOption.MULTILINE)
            .findAll(head)
            .associate { it.groupValues[1] to it.groupValues[2] }
        return Source(
            url = meta["update_url"].orEmpty(),
            channel = meta["update_channel"].orEmpty().removePrefix("@")
        )
    }

    // --- получение файла ---

    private fun fetch(plugin: FGPluginsController.Plugin, source: Source, done: (Found?) -> Unit) {
        if (source.url.isNotEmpty()) {
            done(fetchByUrl(source.url))
            return
        }
        fetchFromChat(plugin, source, done)
    }

    private fun fetchByUrl(url: String): Found? {
        var connection: HttpURLConnection? = null
        return try {
            connection = (URL(url).openConnection() as HttpURLConnection).apply {
                connectTimeout = 15000
                readTimeout = 20000
                instanceFollowRedirects = true
                setRequestProperty("User-Agent", NetworkHelper.formatUserAgent())
            }
            if (connection.responseCode !in 200..299) return null
            val length = connection.contentLength
            if (length > MAX_PLUGIN_BYTES) return null
            val bytes = connection.inputStream.use { it.readBytes(MAX_PLUGIN_BYTES) }
            describe(bytes, fromChannel = false)
        } catch (e: Throwable) {
            FinegramLogger.e("FGPluginUpdates", { "не забрать плагин по адресу $url" }, e)
            null
        } finally {
            connection?.disconnect()
        }
    }

    /**
     * Ищет свежий файл плагина там, откуда его поставили.
     *
     * Смотрим последние сообщения канала с документами и берём самое новое, чьё имя
     * совпадает с тем, что стояло у нас. Совпадение по имени, а не по опознавателю внутри:
     * иначе пришлось бы качать каждый попавшийся файл, чтобы заглянуть в шапку.
     */
    private fun fetchFromChat(plugin: FGPluginsController.Plugin, source: Source, done: (Found?) -> Unit) {
        val account = source.account
        val peer = try {
            org.telegram.messenger.MessagesController.getInstance(account)
                .getInputPeer(source.dialogId)
        } catch (e: Throwable) {
            null
        }
        if (peer == null) {
            done(null)
            return
        }

        val request = TLRPC.TL_messages_search()
        request.peer = peer
        request.q = ""
        request.filter = TLRPC.TL_inputMessagesFilterDocument()
        request.limit = CHANNEL_LOOKUP_LIMIT
        request.offset_id = 0

        ConnectionsManager.getInstance(account).sendRequest(request) { response, _ ->
            val messages = (response as? TLRPC.messages_Messages)?.messages
            if (messages.isNullOrEmpty()) {
                done(null)
                return@sendRequest
            }
            val wanted = source.fileName.lowercase()
            var best: TLRPC.Message? = null
            for (message in messages) {
                val document = message.media?.document ?: continue
                val name = FileLoader.getDocumentFileName(document).lowercase()
                if (!name.endsWith(".plugin")) continue
                val matches = if (wanted.isEmpty()) {
                    name.contains(plugin.id.lowercase())
                } else {
                    name == wanted
                }
                if (!matches) continue
                if (best == null || message.date > best!!.date) best = message
            }
            val message = best
            if (message == null) {
                done(null)
                return@sendRequest
            }
            downloadDocument(account, message) { file ->
                if (file == null) {
                    done(null)
                } else {
                    done(describe(file.readBytes(), fromChannel = true))
                }
            }
        }
    }

    /**
     * Скачивает документ сообщения и отдаёт файл.
     *
     * Если он уже лежит в кэше, сеть не трогаем вовсе — плагины небольшие, и повторная
     * закачка того же файла была бы заметна на мобильном.
     */
    private fun downloadDocument(account: Int, message: TLRPC.Message, done: (File?) -> Unit) {
        val document = message.media?.document
        if (document == null) {
            done(null)
            return
        }
        val loader = FileLoader.getInstance(account)
        val existing = loader.getPathToMessage(message)
        if (existing != null && existing.exists() && existing.length() > 0) {
            done(existing)
            return
        }
        val messageObject = MessageObject(account, message, false, false)
        val center = org.telegram.messenger.NotificationCenter.getInstance(account)
        val wanted = FileLoader.getAttachFileName(document)

        AndroidUtilities.runOnUIThread {
            val observer = object : org.telegram.messenger.NotificationCenter.NotificationCenterDelegate {
                override fun didReceivedNotification(id: Int, acc: Int, vararg args: Any?) {
                    val name = args.getOrNull(0) as? String ?: return
                    if (name != wanted) return
                    center.removeObserver(this, org.telegram.messenger.NotificationCenter.fileLoaded)
                    center.removeObserver(this, org.telegram.messenger.NotificationCenter.fileLoadFailed)
                    if (id == org.telegram.messenger.NotificationCenter.fileLoaded) {
                        val file = loader.getPathToMessage(message)
                        done(if (file != null && file.exists()) file else null)
                    } else {
                        done(null)
                    }
                }
            }
            center.addObserver(observer, org.telegram.messenger.NotificationCenter.fileLoaded)
            center.addObserver(observer, org.telegram.messenger.NotificationCenter.fileLoadFailed)
            loader.loadFile(document, messageObject, FileLoader.PRIORITY_NORMAL, 0)
        }
    }

    /** Читает шапку скачанного файла: опознаватель и версию. */
    private fun describe(bytes: ByteArray, fromChannel: Boolean): Found? {
        if (bytes.isEmpty() || bytes.size > MAX_PLUGIN_BYTES) return null
        val head = String(bytes, 0, minOf(bytes.size, 8192), Charsets.UTF_8)
        val meta = Regex("""^__(\w+)__\s*=\s*["'](.*?)["']""", RegexOption.MULTILINE)
            .findAll(head)
            .associate { it.groupValues[1] to it.groupValues[2] }
        val id = meta["id"] ?: return null
        return Found(id, meta["version"].orEmpty(), bytes, fromChannel)
    }

    private fun java.io.InputStream.readBytes(limit: Int): ByteArray {
        val output = java.io.ByteArrayOutputStream()
        val buffer = ByteArray(16384)
        var total = 0
        while (true) {
            val read = read(buffer)
            if (read <= 0) break
            total += read
            if (total > limit) break
            output.write(buffer, 0, read)
        }
        return output.toByteArray()
    }
}

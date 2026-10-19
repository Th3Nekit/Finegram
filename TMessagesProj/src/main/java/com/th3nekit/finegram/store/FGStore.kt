/*
 * Finegram plugin store.
 * Adapted from Kangel Plugins Manager (KangelPlugins).
 * Upstream: https://git.kangel.xyz/KangelPlugins/PluginManager
 * Licensed under GNU GPL v3; see LICENSE.PluginManager and NOTICE.
 */

package com.th3nekit.finegram.store

import com.th3nekit.finegram.core.FinegramLogger
import org.json.JSONArray
import org.json.JSONObject
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.ApplicationLoader
import org.telegram.messenger.LocaleController
import org.telegram.messenger.Utilities
import java.io.File
import java.io.ByteArrayOutputStream
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Опись плагинов и наборов значков.
 *
 * Первым спрашиваем сервер проекта: там опись собрана и проверена. Если он молчит — идём
 * к открытым зеркалам, из которых эта опись и собирается, чтобы магазин работал даже
 * тогда, когда наш сервер недоступен. Полученное кладётся на диск, поэтому экран
 * открывается сразу и без сети — с тем, что успело приехать.
 */
object FGStore {

    /** Опись проекта: собрана у нас, проверена и пополнена своими плагинами. */
    const val CATALOG_URL = "https://th3web.com/finegram/store/index.json"

    /**
     * Куда идти, если сервер проекта не ответил.
     *
     * Это открытые зеркала общего каталога плагинов. Порядок — по доступности из России:
     * сначала то, что обычно открывается без обхода.
     */
    private val FALLBACK_URLS = listOf(
        "https://git.kangel.xyz/KangelPlugins/Plugins-Store/raw/branch/main/store.json",
        "https://gitverse.ru/api/repos/bigfishtheory/Plugins-Store/raw/branch/main/store.json",
        "https://codeberg.org/Kangel/Plugins-Store/raw/branch/main/store.json",
        "https://raw.githubusercontent.com/Kangel-Plugins/Plugins-Store/main/store.json",
    )

    /** Опись живёт сутки: чаще спрашивать незачем, реже — человек не увидит новинку. */
    private const val FRESH_FOR_MS = 24L * 60 * 60 * 1000

    private const val CONNECT_TIMEOUT_MS = 12_000
    private const val READ_TIMEOUT_MS = 20_000
    private const val MAX_FILE_BYTES = 32 * 1024 * 1024
    private const val MAX_CATALOG_BYTES = 4 * 1024 * 1024

    /** Что предлагает каталог. */
    enum class Kind { PLUGIN, ICONS }

    /**
     * Раздел каталога.
     *
     * Название раздела приходит строкой; незнакомое слово попадает в «Прочее», чтобы
     * новый раздел на сервере не ломал старый клиент.
     */
    enum class Category(val key: String) {
        CUSTOMIZATION("customization"),
        MESSAGES("messages"),
        UTILITIES("utilities"),
        INFORMATIONAL("informational"),
        FUN("fun"),
        LIBRARY("library"),
        ICONS("icons"),
        OTHER("other");

        companion object {
            @JvmStatic
            fun of(raw: String?): Category {
                if (raw.isNullOrBlank()) return OTHER
                val key = raw.trim().lowercase()
                return entries.firstOrNull { it.key == key } ?: OTHER
            }
        }
    }

    data class Item(
        val id: String,
        val kind: Kind,
        val name: String,
        val author: String,
        val version: String,
        val description: String,
        val icon: String,
        val sizeBytes: Long,
        val url: String,
        val updatedAt: Long,
        val downloads: Int,
        val category: Category,
        /** sha256 файла, как он записан в описи; пустая строка — проверять нечего. */
        val hash: String,
        /** Подпись описи автором каталога; пустая строка — подписи нет. */
        val signature: String,
        /** С какой версии клиента плагин работает. */
        val minAppVersion: String,
        /** Пакеты Python, без которых плагин не запустится. */
        val requirements: List<String>,
        /** Другие плагины, нужные этому. */
        val dependencies: List<String>,
    ) {
        /**
         * По чему ищет строка поиска.
         *
         * Считается один раз при разборе описи, в фоне. Приводить шестьсот записей к
         * нижнему регистру на каждую набранную букву — ровно то, отчего список
         * задумывается под пальцем.
         */
        val searchKey: String = "$name $author $description".lowercase()
    }

    private val loading = AtomicBoolean(false)

    @Volatile
    private var items: List<Item> = emptyList()

    @Volatile
    private var loadedAtMs: Long = 0

    @Volatile
    private var lastErrorText: String = ""

    @Volatile
    private var lastSourceUrl: String = ""

    /** Последняя известная опись. Диск здесь не трогаем: список строится на главном потоке. */
    @JvmStatic
    fun cached(): List<Item> = items

    @JvmStatic
    fun lastError(): String = lastErrorText

    /** Откуда приехала опись: адрес сервера или зеркала. */
    @JvmStatic
    fun lastSource(): String = lastSourceUrl

    @JvmStatic
    fun isFresh(): Boolean = loadedAtMs > 0 && System.currentTimeMillis() - loadedAtMs < FRESH_FOR_MS

    /**
     * Обновляет опись.
     *
     * @param force спрашивать сервер, даже если прежняя опись ещё свежая.
     * @param ready зовётся в любом случае: и когда список пришёл, и когда не удалось.
     */
    @JvmStatic
    fun refresh(force: Boolean, ready: Runnable?) {
        if (!force && isFresh() && items.isNotEmpty()) {
            ready?.run()
            return
        }
        if (!loading.compareAndSet(false, true)) {
            return
        }
        Utilities.globalQueue.postRunnable {
            try {
                if (items.isEmpty()) {
                    // Сохранённая опись показывается, пока идёт запрос: экран не должен
                    // ждать сеть, чтобы показать то, что уже известно.
                    readFromDisk()
                    if (items.isNotEmpty() && ready != null) {
                        AndroidUtilities.runOnUIThread(ready)
                    }
                }
                loadFromAnySource()
            } catch (e: Throwable) {
                lastErrorText = e.message ?: e.javaClass.simpleName
                FinegramLogger.e("FGStore", { "опись не загрузилась" }, e)
            } finally {
                loading.set(false)
                if (ready != null) {
                    AndroidUtilities.runOnUIThread(ready)
                }
            }
        }
    }

    private fun loadFromAnySource() {
        val sources = ArrayList<String>()
        sources.add(CATALOG_URL)
        sources.addAll(FALLBACK_URLS)

        var lastFailure = ""
        for (url in sources) {
            try {
                val text = download(url)
                val parsed = parse(text)
                if (parsed.isEmpty()) {
                    lastFailure = "опись пуста"
                    continue
                }
                items = parsed
                loadedAtMs = System.currentTimeMillis()
                lastErrorText = ""
                lastSourceUrl = url
                saveToDisk(text)
                FinegramLogger.d("FGStore") { "опись из $url: ${parsed.size}" }
                return
            } catch (e: Throwable) {
                lastFailure = e.message ?: e.javaClass.simpleName
                FinegramLogger.d("FGStore") { "источник $url не ответил: $lastFailure" }
            }
        }
        lastErrorText = lastFailure
    }

    /** Скачивает файл каталога во временный файл и отдаёт его. */
    @JvmStatic
    fun download(item: Item, into: File): Boolean {
        return try {
            val connection = open(item.url)
            try {
                if (connection.contentLength > MAX_FILE_BYTES) throw IOException("Store file is too large")
                connection.inputStream.use { input ->
                    into.outputStream().use { output ->
                        copyLimited(input, output, MAX_FILE_BYTES)
                    }
                }
            } finally {
                connection.disconnect()
            }
            into.length() > 0
        } catch (e: Throwable) {
            FinegramLogger.e("FGStore", { "файл ${item.id} не скачался" }, e)
            try {
                into.delete()
            } catch (ignored: Throwable) {
            }
            false
        }
    }

    private fun open(url: String): HttpURLConnection {
        // В чужой описи попадаются адреса с пробелами («16 Accounts.plugin»): без замены
        // запрос обрывается ещё до соединения.
        val connection = URL(url.replace(" ", "%20")).openConnection() as HttpURLConnection
        connection.connectTimeout = CONNECT_TIMEOUT_MS
        connection.readTimeout = READ_TIMEOUT_MS
        connection.instanceFollowRedirects = true
        connection.setRequestProperty("User-Agent", "Finegram")
        try {
            connection.connect()
            if (connection.responseCode !in 200..299) {
                throw IllegalStateException("сервер ответил ${connection.responseCode}")
            }
            return connection
        } catch (e: Throwable) {
            connection.disconnect()
            throw e
        }
    }

    private fun download(url: String): String {
        val connection = open(url)
        try {
            if (connection.contentLength > MAX_CATALOG_BYTES) throw IOException("Store catalog is too large")
            return connection.inputStream.use { input ->
                val output = ByteArrayOutputStream()
                copyLimited(input, output, MAX_CATALOG_BYTES)
                output.toString("UTF-8")
            }
        } finally {
            connection.disconnect()
        }
    }

    private fun copyLimited(input: InputStream, output: OutputStream, limit: Int) {
        val buffer = ByteArray(32 * 1024)
        var total = 0
        while (true) {
            val count = input.read(buffer)
            if (count < 0) return
            if (count > limit - total) throw IOException("Store download is too large")
            output.write(buffer, 0, count)
            total += count
        }
    }

    /**
     * Разбирает опись.
     *
     * Понимает два вида записи: нашу, где под ключом `items` лежит список, и общую, где
     * сам объект — набор «опознаватель → описание». Вторая нужна, чтобы читать зеркала
     * напрямую, ничего на нашей стороне не перекладывая.
     */
    private fun parse(text: String): List<Item> {
        val root = JSONObject(text)
        val array = root.optJSONArray("items")
        if (array != null) {
            return parseOwn(array)
        }
        return parseByIds(root)
    }

    private fun parseOwn(array: JSONArray): List<Item> {
        val result = ArrayList<Item>(array.length())
        for (i in 0 until array.length()) {
            val o = array.optJSONObject(i) ?: continue
            itemOf(o.optString("id").trim(), o)?.let { result.add(it) }
        }
        return result
    }

    private fun parseByIds(root: JSONObject): List<Item> {
        val result = ArrayList<Item>(root.length())
        val keys = root.keys()
        while (keys.hasNext()) {
            val id = keys.next()
            when (val value = root.opt(id)) {
                is JSONObject -> itemOf(id, value)?.let { result.add(it) }
                is String -> itemOf(id, JSONObject().put("url", value))?.let { result.add(it) }
            }
        }
        return result
    }

    private fun itemOf(id: String, o: JSONObject): Item? {
        if (id.isEmpty()) {
            return null
        }
        val url = o.optString("url").trim()
        // Чужой адрес в описи — повод остановиться: файл ставится в клиент, и брать его
        // можно только по защищённому соединению.
        if (url.isEmpty() || !url.startsWith("https://")) {
            return null
        }
        val icons = o.optString("kind") == "icons" || url.endsWith(".icons")
        return Item(
            id = id,
            kind = if (icons) Kind.ICONS else Kind.PLUGIN,
            name = o.optString("name", id),
            author = o.optString("author", ""),
            version = o.optString("version", ""),
            description = localizedDescription(o),
            icon = o.optString("icon", ""),
            sizeBytes = o.optLong("size", 0),
            url = url,
            updatedAt = o.optLong("updated_at", 0),
            downloads = o.optInt("downloads", 0),
            category = if (icons) Category.ICONS
                else Category.of(o.optString("status", o.optString("category", ""))),
            hash = o.optString("hash", ""),
            signature = o.optString("signature", ""),
            minAppVersion = o.optString("min_version", ""),
            requirements = stringList(o.opt("requirements")),
            dependencies = stringList(o.opt("dependencies")),
        )
    }

    /** Описание на языке клиента, если оно есть в описи; иначе — общее. */
    private fun localizedDescription(o: JSONObject): String {
        val language = try {
            LocaleController.getInstance().getCurrentLocale().language
        } catch (e: Throwable) {
            "en"
        }
        val localized = o.optString("description_$language", "")
        return if (localized.isNotBlank()) localized else o.optString("description", "")
    }

    private fun stringList(value: Any?): List<String> = when (value) {
        is JSONArray -> (0 until value.length()).mapNotNull { index ->
            value.optString(index, "").trim().takeIf { it.isNotEmpty() }
        }
        is String -> value.split(',').map { it.trim() }.filter { it.isNotEmpty() }
        else -> emptyList()
    }

    private fun cacheFile(): File =
        File(ApplicationLoader.applicationContext.filesDir, "finegram/store-index.json")

    private fun saveToDisk(text: String) {
        try {
            val file = cacheFile()
            file.parentFile?.mkdirs()
            file.writeText(text)
        } catch (e: Throwable) {
            FinegramLogger.e("FGStore", { "опись не сохранилась" }, e)
        }
    }

    private fun readFromDisk() {
        try {
            val file = cacheFile()
            if (!file.exists()) {
                return
            }
            if (file.length() > MAX_CATALOG_BYTES) {
                return
            }
            items = parse(file.readText())
            loadedAtMs = file.lastModified()
        } catch (e: Throwable) {
            FinegramLogger.e("FGStore", { "сохранённая опись не читается" }, e)
        }
    }
}

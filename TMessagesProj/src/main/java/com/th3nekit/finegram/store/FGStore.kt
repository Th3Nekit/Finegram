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
import java.net.HttpURLConnection
import java.net.URL
import java.util.concurrent.atomic.AtomicBoolean

object FGStore {

    const val CATALOG_URL = "https://th3web.com/finegram/store/index.json"

    private val FALLBACK_URLS = listOf(
        "https://git.kangel.xyz/KangelPlugins/Plugins-Store/raw/branch/main/store.json",
        "https://gitverse.ru/api/repos/bigfishtheory/Plugins-Store/raw/branch/main/store.json",
        "https://codeberg.org/Kangel/Plugins-Store/raw/branch/main/store.json",
        "https://raw.githubusercontent.com/Kangel-Plugins/Plugins-Store/main/store.json",
    )

    private const val FRESH_FOR_MS = 24L * 60 * 60 * 1000

    private const val CONNECT_TIMEOUT_MS = 12_000
    private const val READ_TIMEOUT_MS = 20_000

    enum class Kind { PLUGIN, ICONS }

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

        val hash: String,

        val signature: String,

        val minAppVersion: String,

        val requirements: List<String>,

        val dependencies: List<String>,
    ) {

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

    @JvmStatic
    fun cached(): List<Item> = items

    @JvmStatic
    fun lastError(): String = lastErrorText

    @JvmStatic
    fun lastSource(): String = lastSourceUrl

    @JvmStatic
    fun isFresh(): Boolean = loadedAtMs > 0 && System.currentTimeMillis() - loadedAtMs < FRESH_FOR_MS

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

    @JvmStatic
    fun download(item: Item, into: File): Boolean {
        return try {
            val connection = open(item.url)
            connection.inputStream.use { input ->
                into.outputStream().use { output ->
                    input.copyTo(output, 32 * 1024)
                }
            }
            connection.disconnect()
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

        val connection = URL(url.replace(" ", "%20")).openConnection() as HttpURLConnection
        connection.connectTimeout = CONNECT_TIMEOUT_MS
        connection.readTimeout = READ_TIMEOUT_MS
        connection.instanceFollowRedirects = true
        connection.setRequestProperty("User-Agent", "Finegram")
        connection.connect()
        if (connection.responseCode !in 200..299) {
            throw IllegalStateException("сервер ответил ${connection.responseCode}")
        }
        return connection
    }

    private fun download(url: String): String {
        val connection = open(url)
        try {
            return connection.inputStream.bufferedReader().use { it.readText() }
        } finally {
            connection.disconnect()
        }
    }

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
            items = parse(file.readText())
            loadedAtMs = file.lastModified()
        } catch (e: Throwable) {
            FinegramLogger.e("FGStore", { "сохранённая опись не читается" }, e)
        }
    }
}

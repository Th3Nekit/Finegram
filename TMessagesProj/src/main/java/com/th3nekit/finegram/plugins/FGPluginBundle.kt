/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.plugins

import com.th3nekit.finegram.core.FinegramLogger
import org.json.JSONObject
import org.telegram.messenger.LocaleController
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.io.Reader
import java.util.zip.ZipFile
import java.util.zip.ZipInputStream

object FGPluginBundle {

    const val EXTENSION = "eaf"

    private const val REFMAP = "refmap.json"

    private const val REFMAP_YAML = "refmap.yml"

    private val METADATA_NAMES = listOf("metainfo.json", "metainfo.yml", "meta.yml")

    private const val MAX_UNPACKED_BYTES = 64L * 1024 * 1024
    private const val MAX_ENTRIES = 4000
    private const val MAX_METADATA_BYTES = 2 * 1024 * 1024

    @JvmStatic
    fun looksLikeBundle(bytes: ByteArray): Boolean =
        bytes.size >= 4 && bytes[0] == 0x50.toByte() && bytes[1] == 0x4B.toByte() &&
            ((bytes[2] == 0x03.toByte() && bytes[3] == 0x04.toByte()) ||
                (bytes[2] == 0x05.toByte() && bytes[3] == 0x06.toByte()) ||
                (bytes[2] == 0x07.toByte() && bytes[3] == 0x08.toByte()))

    @JvmStatic
    fun looksLikeBundle(file: File): Boolean {
        if (file.isDirectory) return false
        return try {
            file.inputStream().use { input ->
                val head = ByteArray(4)
                input.read(head) == 4 && looksLikeBundle(head)
            }
        } catch (e: Throwable) {
            false
        }
    }

    @JvmStatic
    fun isUnpacked(dir: File): Boolean =
        dir.isDirectory && (File(dir, REFMAP).exists() || File(dir, REFMAP_YAML).exists()
            || findMain(dir) != null)

    @JvmStatic
    fun readMeta(archive: File): FGPluginsController.Plugin? {
        if (archive.isDirectory) return readMeta(archive, true)
        return try {
            ZipFile(archive).use { zip ->
                val refmap = zip.getEntry(REFMAP)?.let { entry ->
                    JSONObject(zip.getInputStream(entry).use { readMetadata(it) })
                } ?: zip.getEntry(REFMAP_YAML)?.let { entry ->
                    flatYaml(zip.getInputStream(entry).use { readMetadata(it) })
                }
                val metaName = refmap?.optString("metainfo")?.takeIf { it.isNotEmpty() }
                    ?: METADATA_NAMES.firstNotNullOfOrNull { name ->
                        zip.entries().asSequence().firstOrNull { it.name.endsWith(name) }?.name
                    }
                    ?: return null
                val entry = zip.getEntry(metaName) ?: return null
                val text = zip.getInputStream(entry).use { readMetadata(it) }
                val meta = if (metaName.endsWith(".yml")) flatYaml(text) else JSONObject(text)
                pluginOf(localizedMetadata(meta, refmap) { name ->
                    try {
                        zip.getEntry(name)?.takeUnless { it.isDirectory }?.let { value ->
                            JSONObject(zip.getInputStream(value).use { readMetadata(it) })
                        }
                    } catch (e: Exception) {
                        null
                    }
                }, archive)
            }
        } catch (e: Throwable) {
            FinegramLogger.d("FGPlugins") { "не прочитать сборку ${archive.name}: ${e.message}" }
            null
        }
    }

    @JvmStatic
    fun readMeta(dir: File, unused: Boolean = true): FGPluginsController.Plugin? {
        val refmap = File(dir, REFMAP).takeIf { it.exists() }?.let {
            try {
                JSONObject(it.inputStream().use { input -> readMetadata(input) })
            } catch (e: Throwable) {
                null
            }
        } ?: File(dir, REFMAP_YAML).takeIf { it.exists() }?.let {
            try {
                flatYaml(it.inputStream().use { input -> readMetadata(input) })
            } catch (e: Throwable) {
                null
            }
        }
        val metaName = refmap?.optString("metainfo")?.takeIf { it.isNotEmpty() }
        val metaFile = if (metaName != null) containedFile(dir, metaName) else findMeta(dir)
        if (metaFile == null) return null
        return try {
            val text = metaFile.inputStream().use { readMetadata(it) }
            val meta = if (metaFile.name.endsWith(".yml")) flatYaml(text) else JSONObject(text)
            pluginOf(localizedMetadata(meta, refmap) { name ->
                try {
                    containedFile(dir, name)?.let { value ->
                        JSONObject(value.inputStream().use { readMetadata(it) })
                    }
                } catch (e: Exception) {
                    null
                }
            }, dir)
        } catch (e: Throwable) {
            null
        }
    }

    private fun readMetadata(input: InputStream): String {
        val output = ByteArrayOutputStream()
        val buffer = ByteArray(8192)
        while (true) {
            val count = input.read(buffer)
            if (count < 0) break
            if (count > MAX_METADATA_BYTES - output.size()) throw IOException("Plugin metadata is too large")
            output.write(buffer, 0, count)
        }
        return output.toString(Charsets.UTF_8.name())
    }

    private fun containedFile(dir: File, name: String): File? = try {
        val root = dir.canonicalFile
        val file = File(root, name).canonicalFile
        file.takeIf { it.isFile && it.path.startsWith(root.path + File.separator) }
    } catch (e: IOException) {
        null
    }

    private fun flatYaml(text: String): JSONObject {
        val out = JSONObject()
        for (raw in text.lineSequence()) {
            val line = raw.trim()
            if (line.isEmpty() || line.startsWith("#")) {
                continue
            }
            val colon = line.indexOf(':')
            if (colon <= 0) {
                continue
            }
            val key = line.substring(0, colon).trim()
            var value = line.substring(colon + 1).trim()
            val hash = value.indexOf(" #")
            if (hash > 0) {
                value = value.substring(0, hash).trim()
            }
            value = value.trim('"', '\'')
            if (key.isNotEmpty() && value.isNotEmpty() && !out.has(key)) {
                out.put(key, value)
            }
        }
        return out
    }

    private val metadataPlaceholder = Regex("\\{([A-Za-z0-9_.-]{1,64})\\}")

    private fun localizedMetadata(json: JSONObject, refmap: JSONObject?, readTable: (String) -> JSONObject?): JSONObject {
        val keys = json.keys().asSequence().filter { it != "id" }.toList()
        if (keys.none { (json.opt(it) as? String)?.let(metadataPlaceholder::containsMatchIn) == true }) {
            return json
        }
        val language = try {
            LocaleController.getInstance().currentLocale.language
        } catch (e: Exception) {
            "en"
        }
        val explicit = refmap?.optString("strings")?.takeIf { it.isNotEmpty() }
        val main = refmap?.optString("main")?.takeIf { it.isNotEmpty() }
            ?: refmap?.optString("entry").orEmpty()
        val parent = main.substringBeforeLast('/', "")
        val folders = explicit?.let { listOf(it.trimEnd('/')) }
            ?: listOf(parent.substringBeforeLast('/', ""), parent, "").distinct().flatMap { path ->
                listOf("locales", "strings").map { if (path.isEmpty()) it else "$path/$it" }
            }
        val strings = HashMap<String, String>()
        for (folder in folders) {
            for (locale in listOf("en", language).distinct()) {
                val table = readTable("$folder/strings_$locale.json") ?: continue
                for (key in table.keys()) {
                    (table.opt(key) as? String)?.let { strings[key] = it }
                }
            }
            if (strings.isNotEmpty()) break
        }
        for (key in keys) {
            val value = json.opt(key) as? String ?: continue
            json.put(key, metadataPlaceholder.replace(value) { strings[it.groupValues[1]] ?: it.value })
        }
        return json
    }

    private fun pluginOf(json: JSONObject, where: File): FGPluginsController.Plugin? {
        val id = json.optString("id").trim()
        if (!FGPluginFiles.validId(id)) {
            return null
        }
        return FGPluginsController.Plugin(
            id = id,
            name = json.optString("name", id),
            version = json.optString("version", ""),
            author = json.optString("author", ""),
            description = json.optString("description", ""),
            icon = json.optString("icon", ""),
            file = where,
        )
    }

    private fun findMeta(dir: File): File? {
        for (name in METADATA_NAMES) {
            containedFile(dir, name)?.let { return it }
            dir.listFiles()?.forEach { child ->
                if (child.isDirectory) {
                    containedFile(dir, child.name + "/" + name)?.let { return it }
                }
            }
        }
        return null
    }

    private fun findMain(dir: File): File? {
        val direct = File(dir, "main.py")
        if (direct.isFile) return direct
        dir.listFiles()?.forEach { child ->
            if (!child.isDirectory) return@forEach
            val nested = File(child, "main.py")
            if (nested.isFile) return nested
            val deeper = File(File(child, "src"), "main.py")
            if (deeper.isFile) return deeper
        }
        return null
    }

    @JvmStatic
    fun unpack(archive: File, target: File): Boolean {
        if (target.exists()) {
            target.deleteRecursively()
        }
        if (!target.mkdirs()) {
            return false
        }
        val root = target.canonicalPath
        var written = 0L
        var count = 0
        return try {
            ZipInputStream(archive.inputStream().buffered()).use { zip ->
                var entry = zip.nextEntry
                while (entry != null) {
                    if (++count > MAX_ENTRIES) {
                        throw IllegalStateException("в архиве слишком много файлов")
                    }
                    val outFile = File(target, entry.name)
                    if (!outFile.canonicalPath.startsWith(root + File.separator)) {
                        throw IllegalStateException("файл ведёт за пределы папки: ${entry.name}")
                    }
                    if (entry.isDirectory) {
                        outFile.mkdirs()
                    } else {
                        outFile.parentFile?.mkdirs()
                        outFile.outputStream().buffered().use { output ->
                            val buffer = ByteArray(32 * 1024)
                            while (true) {
                                val read = zip.read(buffer)
                                if (read <= 0) break
                                written += read
                                if (written > MAX_UNPACKED_BYTES) {
                                    throw IllegalStateException("сборка не помещается в отведённое место")
                                }
                                output.write(buffer, 0, read)
                            }
                        }
                    }
                    zip.closeEntry()
                    entry = zip.nextEntry
                }
            }
            true
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "сборка ${archive.name} не распаковалась" }, e)
            target.deleteRecursively()
            false
        }
    }

    @JvmStatic
    fun readCode(archive: File, limit: Int): String {
        if (limit <= 0) return ""
        val text = StringBuilder()
        try {
            ZipFile(archive).use { zip ->
                for (entry in zip.entries().asSequence().take(MAX_ENTRIES)) {
                    if (entry.isDirectory || !entry.name.endsWith(".py")) continue
                    zip.getInputStream(entry).bufferedReader().use { reader ->
                        appendCode(reader, text, limit)
                    }
                    if (text.length >= limit) return text.toString()
                }
            }
        } catch (e: Throwable) {
            FinegramLogger.d("FGPlugins") { "код сборки не прочитан: ${e.message}" }
        }
        return text.toString()
    }

    @JvmStatic
    fun readCode(dir: File, limit: Int, unused: Boolean = true): String {
        if (limit <= 0) return ""
        val text = StringBuilder()
        try {
            val root = dir.canonicalFile
            val visited = HashSet<String>()
            root.walkTopDown().onEnter { child ->
                val path = child.canonicalPath
                (path == root.path || path.startsWith(root.path + File.separator)) && visited.add(path)
            }.take(MAX_ENTRIES).forEach { file ->
                if (text.length >= limit) return text.toString()
                if (file.name.endsWith(".py")) {
                    val source = containedFile(root, file.relativeTo(root).path)
                    if (source != null) {
                        source.bufferedReader().use { appendCode(it, text, limit) }
                    }
                }
            }
        } catch (e: Throwable) {
            FinegramLogger.d("FGPlugins") { "код сборки не прочитан: ${e.message}" }
        }
        return text.toString()
    }

    private fun appendCode(reader: Reader, text: StringBuilder, limit: Int) {
        val buffer = CharArray(minOf(8192, limit - text.length))
        while (text.length < limit) {
            val count = reader.read(buffer, 0, minOf(buffer.size, limit - text.length))
            if (count <= 0) break
            text.append(buffer, 0, count)
        }
        if (text.length < limit) text.append('\n')
    }
}

/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.plugins

import com.th3nekit.finegram.core.FinegramLogger
import org.json.JSONObject
import java.io.File
import java.util.zip.ZipFile
import java.util.zip.ZipInputStream

object FGPluginBundle {

    const val EXTENSION = "eaf"

    private const val REFMAP = "refmap.json"

    private const val REFMAP_YAML = "refmap.yml"

    private const val MAX_UNPACKED_BYTES = 64L * 1024 * 1024
    private const val MAX_ENTRIES = 4000

    @JvmStatic
    fun looksLikeBundle(bytes: ByteArray): Boolean =
        bytes.size > 4 && bytes[0] == 0x50.toByte() && bytes[1] == 0x4B.toByte() &&
            (bytes[2] == 0x03.toByte() || bytes[2] == 0x05.toByte() || bytes[2] == 0x07.toByte())

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
        return try {
            ZipFile(archive).use { zip ->
                val refmap = zip.getEntry(REFMAP)?.let { entry ->
                    JSONObject(zip.getInputStream(entry).bufferedReader().use { it.readText() })
                } ?: zip.getEntry(REFMAP_YAML)?.let { entry ->
                    flatYaml(zip.getInputStream(entry).bufferedReader().use { it.readText() })
                }
                val metaName = refmap?.optString("metainfo")?.takeIf { it.isNotEmpty() }
                    ?: zip.entries().asSequence()
                        .firstOrNull { it.name.endsWith("metainfo.json") }?.name
                    ?: zip.entries().asSequence()
                        .firstOrNull { it.name.endsWith("meta.yml") }?.name
                    ?: return null
                val entry = zip.getEntry(metaName) ?: return null
                val text = zip.getInputStream(entry).bufferedReader().use { it.readText() }
                pluginOf(if (metaName.endsWith(".yml")) flatYaml(text) else JSONObject(text), archive)
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
                JSONObject(it.readText())
            } catch (e: Throwable) {
                null
            }
        } ?: File(dir, REFMAP_YAML).takeIf { it.exists() }?.let {
            try {
                flatYaml(it.readText())
            } catch (e: Throwable) {
                null
            }
        }
        val metaFile = refmap?.optString("metainfo")?.takeIf { it.isNotEmpty() }
            ?.let { File(dir, it) }?.takeIf { it.exists() }
            ?: findMeta(dir)
            ?: return null
        return try {
            val text = metaFile.readText()
            pluginOf(if (metaFile.name.endsWith(".yml")) flatYaml(text) else JSONObject(text), dir)
        } catch (e: Throwable) {
            null
        }
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

    private fun pluginOf(json: JSONObject, where: File): FGPluginsController.Plugin? {
        val id = json.optString("id").trim()
        if (id.isEmpty()) {
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
        val direct = File(dir, "metainfo.json")
        if (direct.exists()) return direct
        dir.listFiles()?.forEach { child ->
            if (child.isDirectory) {
                val nested = File(child, "metainfo.json")
                if (nested.exists()) return nested
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
                    if (!outFile.canonicalPath.startsWith(root)) {
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
        val text = StringBuilder()
        try {
            ZipFile(archive).use { zip ->
                for (entry in zip.entries()) {
                    if (entry.isDirectory || !entry.name.endsWith(".py")) continue
                    zip.getInputStream(entry).bufferedReader().use { reader ->
                        text.append(reader.readText())
                        text.append('\n')
                    }
                    if (text.length >= limit) return text.substring(0, limit)
                }
            }
        } catch (e: Throwable) {
            FinegramLogger.d("FGPlugins") { "код сборки не прочитан: ${e.message}" }
        }
        return text.toString()
    }

    @JvmStatic
    fun readCode(dir: File, limit: Int, unused: Boolean = true): String {
        val text = StringBuilder()
        dir.walkTopDown().forEach { file ->
            if (text.length >= limit) return text.substring(0, limit)
            if (file.isFile && file.name.endsWith(".py")) {
                try {
                    text.append(file.readText())
                    text.append('\n')
                } catch (ignored: Throwable) {
                }
            }
        }
        return text.toString()
    }
}

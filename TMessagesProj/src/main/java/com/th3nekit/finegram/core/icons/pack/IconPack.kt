/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.icons.pack

import org.json.JSONObject
import java.io.File

/**
 * Установленный набор иконок.
 *
 * Пак — это zip-файл с расширением `.icons`: внутри `metadata.json` и картинки. В метаданных
 * лежит соответствие «имя ресурса Telegram → имя файла в архиве», например
 * `"msg_archive": "msg_archive.png"`. Этот формат у иконпаков
 * общепринятый, поэтому уже существующие паки подходят без переделки.
 */
class IconPack(
    val id: String,
    val name: String,
    val author: String,
    val version: String,
    val directory: File,
    private val icons: Map<String, String>
) {

    val iconCount: Int get() = icons.size

    /** Файл иконки для ресурса Telegram, например `msg_archive`. */
    fun fileFor(resourceName: String): File? {
        val fileName = icons[resourceName] ?: return null
        val file = File(directory, fileName)
        return if (file.isFile) file else null
    }

    fun contains(resourceName: String): Boolean = icons.containsKey(resourceName)

    companion object {
        const val METADATA_FILE = "metadata.json"
        const val SUPPORTED_SCHEMA = 1

        /**
         * Разбирает `metadata.json` уже распакованного пака.
         * Возвращает null, если файл не читается или схема незнакомая.
         */
        fun read(directory: File): IconPack? {
            val metadata = File(directory, METADATA_FILE)
            if (!metadata.isFile) return null
            return try {
                val json = JSONObject(metadata.readText())
                if (json.optInt("schemaVersion", 0) > SUPPORTED_SCHEMA) return null

                val iconsJson = json.optJSONObject("icons") ?: return null
                val icons = HashMap<String, String>(iconsJson.length())
                val keys = iconsJson.keys()
                while (keys.hasNext()) {
                    val key = keys.next()
                    val value = iconsJson.optString(key)
                    // имя файла берётся только простое: пак не должен уводить чтение из своей папки
                    if (value.isNotEmpty() && !value.contains('/') && !value.contains('\\') && value != ".." ) {
                        icons[key] = value
                    }
                }
                if (icons.isEmpty()) return null

                IconPack(
                    id = directory.name,
                    name = json.optString("packName").ifEmpty { directory.name },
                    author = json.optString("author"),
                    version = json.optString("version"),
                    directory = directory,
                    icons = icons
                )
            } catch (e: Throwable) {
                null
            }
        }
    }
}

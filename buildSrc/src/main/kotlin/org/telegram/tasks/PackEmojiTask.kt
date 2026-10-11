package org.telegram.tasks

import org.gradle.api.DefaultTask
import org.gradle.api.file.DirectoryProperty
import org.gradle.api.tasks.InputDirectory
import org.gradle.api.tasks.OutputDirectory
import org.gradle.api.tasks.TaskAction
import java.io.BufferedOutputStream
import java.io.DataOutputStream
import java.io.File

abstract class PackEmojiTask : DefaultTask() {

    private companion object {
        const val NO_MASK = 0xFFFF
        const val EMOJI_ENTRY = 2 + 2 + 4 + 4
        const val MASK_ENTRY = 2 + 4 + 4
        val EMOJI_NAME = Regex("""^(\d+)_(\d+)\.png$""")
        val MASK_NAME = Regex("""^(\d+)\.png$""")
    }

    @get:InputDirectory
    abstract val emojiDir: DirectoryProperty

    @get:OutputDirectory
    abstract val outputDir: DirectoryProperty

    private class Entry(val file: File, val id: Int, val maskId: Int) {
        var offset: Long = 0
        val length: Long get() = file.length()
    }

    @TaskAction
    fun pack() {
        val source = emojiDir.get().asFile
        require(source.isDirectory) { "Нет каталога с эмодзи: ${source.absolutePath}" }

        val masksById = readMaskOfEmoji(File(source, "metadata.bin"))

        val emojis = source.listFiles().orEmpty()
            .mapNotNull { file ->
                val match = parseEmojiName(file) ?: return@mapNotNull null
                val id = match.first * 4096 + match.second
                Entry(file, id, masksById[id] ?: NO_MASK)
            }
            .sortedBy { it.id }

        val masks = File(source, "masks").listFiles().orEmpty()
            .mapNotNull { file ->
                val id = MASK_NAME.matchEntire(file.name)?.groupValues?.get(1)?.toIntOrNull()
                    ?: return@mapNotNull null
                Entry(file, id, NO_MASK)
            }
            .sortedBy { it.id }

        require(emojis.isNotEmpty()) { "В ${source.absolutePath} не нашлось ни одной эмодзи" }

        val known = masks.mapTo(HashSet()) { it.id }
        emojis.forEach { emoji ->
            require(emoji.maskId == NO_MASK || known.contains(emoji.maskId)) {
                "Эмодзи ${emoji.file.name} ссылается на маску ${emoji.maskId}, которой нет"
            }
        }

        var offset = 4L + emojis.size.toLong() * EMOJI_ENTRY +
                4L + masks.size.toLong() * MASK_ENTRY
        (emojis + masks).forEach { entry ->
            entry.offset = offset
            offset += entry.length
        }

        val target = File(outputDir.get().asFile.also { it.mkdirs() }, "emoji.pack")
        DataOutputStream(BufferedOutputStream(target.outputStream())).use { out ->
            out.writeIntLE(emojis.size * EMOJI_ENTRY)
            emojis.forEach { emoji ->
                out.writeShortLE(emoji.id)
                out.writeShortLE(emoji.maskId)
                out.writeIntLE(emoji.offset.toInt())
                out.writeIntLE(emoji.length.toInt())
            }
            out.writeIntLE(masks.size * MASK_ENTRY)
            masks.forEach { mask ->
                out.writeShortLE(mask.id)
                out.writeIntLE(mask.offset.toInt())
                out.writeIntLE(mask.length.toInt())
            }
            (emojis + masks).forEach { entry -> entry.file.inputStream().use { it.copyTo(out) } }
        }

        logger.lifecycle(
            "Упаковано эмодзи: ${emojis.size}, масок: ${masks.size}, " +
                    "размер ${target.length() / 1024} КБ -> ${target.absolutePath}"
        )
    }

    private fun parseEmojiName(file: File): Pair<Int, Int>? {
        val match = EMOJI_NAME.matchEntire(file.name) ?: return null
        val x = match.groupValues[1].toIntOrNull() ?: return null
        val y = match.groupValues[2].toIntOrNull() ?: return null
        return x to y
    }

    private fun readMaskOfEmoji(file: File): Map<Int, Int> {
        if (!file.isFile) return emptyMap()
        val bytes = file.readBytes()
        val result = HashMap<Int, Int>(bytes.size / 4)
        var i = 0
        while (i + 3 < bytes.size) {
            val emojiId = (bytes[i].toInt() and 0xFF) or ((bytes[i + 1].toInt() and 0xFF) shl 8)
            val maskId = (bytes[i + 2].toInt() and 0xFF) or ((bytes[i + 3].toInt() and 0xFF) shl 8)
            result[emojiId] = maskId
            i += 4
        }
        return result
    }

    private fun DataOutputStream.writeShortLE(value: Int) {
        write(value and 0xFF)
        write((value ushr 8) and 0xFF)
    }

    private fun DataOutputStream.writeIntLE(value: Int) {
        write(value and 0xFF)
        write((value ushr 8) and 0xFF)
        write((value ushr 16) and 0xFF)
        write((value ushr 24) and 0xFF)
    }
}

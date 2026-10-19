package org.telegram.tasks

import org.gradle.api.DefaultTask
import org.gradle.api.file.DirectoryProperty
import org.gradle.api.file.RegularFileProperty
import org.gradle.api.tasks.InputFile
import org.gradle.api.tasks.OutputDirectory
import org.gradle.api.tasks.PathSensitive
import org.gradle.api.tasks.PathSensitivity
import org.gradle.api.tasks.TaskAction
import java.io.DataInputStream
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder

abstract class PrepareEmojiPackTask : DefaultTask() {
    @get:InputFile
    @get:PathSensitive(PathSensitivity.NONE)
    abstract val emojiPack: RegularFileProperty

    @get:OutputDirectory
    abstract val outputDir: DirectoryProperty

    @TaskAction
    fun prepare() {
        val source = emojiPack.get().asFile
        val header = ByteArray(32)
        DataInputStream(source.inputStream().buffered()).use { it.readFully(header) }
        val data = ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN)
        val version = data.getShort(4).toInt() and 0xFFFF
        val count = data.getInt(16)
        val emojiCount = data.getInt(20)
        require(data.getInt(0) == 0x334B5045 && version in 2..3 && data.getShort(6).toInt() == 20
                && data.getInt(8) == 64 && data.getInt(12) == 64
                && data.getInt(24).toLong() == source.length() && data.getInt(28) == 0
                && count in 1..65535 && emojiCount in 1..count
                && 32L + count * 20L <= source.length()) {
            "Unsupported or damaged EPK3 asset: ${source.absolutePath}"
        }
        val target = File(outputDir.get().asFile.also { it.mkdirs() }, "emoji.pack")
        source.copyTo(target, overwrite = true)
    }
}

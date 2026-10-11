/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins

import java.io.File
import java.io.InputStream
import java.util.zip.ZipFile

object FGDexScanner {

    private const val CLASS_DEFS_SIZE_OFFSET = 96
    private const val CLASS_DEFS_OFF_OFFSET = 100
    private const val TYPE_IDS_OFF_OFFSET = 68
    private const val STRING_IDS_OFF_OFFSET = 60
    private const val CLASS_DEF_ITEM_SIZE = 32

    fun classNames(file: File): List<String> = try {
        val bytes = readDexBytes(file)
        if (bytes == null) emptyList() else classNames(bytes)
    } catch (e: Throwable) {
        emptyList()
    }

    fun readDexBytes(file: File): ByteArray? {
        val head = ByteArray(8)
        file.inputStream().use { stream ->
            if (stream.read(head) != head.size) return null
        }
        if (head[0] == 'd'.code.toByte() && head[1] == 'e'.code.toByte() &&
            head[2] == 'x'.code.toByte() && head[3] == 0x0A.toByte()
        ) {
            return file.readBytes()
        }

        return try {
            ZipFile(file).use { zip ->
                val entry = zip.getEntry("classes.dex")
                    ?: zip.entries().toList().firstOrNull { it.name.endsWith(".dex") }
                    ?: return null
                zip.getInputStream(entry).use(InputStream::readBytes)
            }
        } catch (e: Throwable) {
            null
        }
    }

    fun classNames(dex: ByteArray): List<String> {
        if (dex.size < 112) {
            return emptyList()
        }
        val classDefsSize = readInt(dex, CLASS_DEFS_SIZE_OFFSET)
        val classDefsOff = readInt(dex, CLASS_DEFS_OFF_OFFSET)
        val typeIdsOff = readInt(dex, TYPE_IDS_OFF_OFFSET)
        val stringIdsOff = readInt(dex, STRING_IDS_OFF_OFFSET)
        if (classDefsSize <= 0 || classDefsSize > 200_000) {
            return emptyList()
        }

        val names = ArrayList<String>(classDefsSize)
        for (i in 0 until classDefsSize) {
            val classDef = classDefsOff + i * CLASS_DEF_ITEM_SIZE
            if (classDef + 4 > dex.size) break
            val typeIndex = readInt(dex, classDef)
            val descriptorIndex = readInt(dex, typeIdsOff + typeIndex * 4)
            val stringOffset = readInt(dex, stringIdsOff + descriptorIndex * 4)
            val descriptor = readString(dex, stringOffset) ?: continue

            if (descriptor.length > 2 && descriptor[0] == 'L' && descriptor.endsWith(";")) {
                names.add(descriptor.substring(1, descriptor.length - 1).replace('/', '.'))
            }
        }
        return names
    }

    private fun readInt(data: ByteArray, offset: Int): Int {
        if (offset < 0 || offset + 4 > data.size) return 0
        return (data[offset].toInt() and 0xFF) or
                ((data[offset + 1].toInt() and 0xFF) shl 8) or
                ((data[offset + 2].toInt() and 0xFF) shl 16) or
                ((data[offset + 3].toInt() and 0xFF) shl 24)
    }

    private fun readString(data: ByteArray, offset: Int): String? {
        if (offset < 0 || offset >= data.size) return null
        var position = offset

        while (position < data.size && (data[position].toInt() and 0x80) != 0) {
            position++
        }
        position++
        val end = data.indexOfFirstFrom(position, 0)
        if (end < 0) return null
        return String(data, position, end - position, Charsets.UTF_8)
    }

    private fun ByteArray.indexOfFirstFrom(start: Int, value: Byte): Int {
        var i = start
        while (i < size) {
            if (this[i] == value) return i
            i++
        }
        return -1
    }
}

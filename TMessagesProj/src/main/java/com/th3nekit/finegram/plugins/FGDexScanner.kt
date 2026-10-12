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

    private const val MAX_DEX_SIZE = 32 * 1024 * 1024
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
        if (file.length() > MAX_DEX_SIZE) return null
        val head = ByteArray(8)
        file.inputStream().use { stream ->
            if (stream.read(head) != head.size) return null
        }
        if (head[0] == 'd'.code.toByte() && head[1] == 'e'.code.toByte() &&
            head[2] == 'x'.code.toByte() && head[3] == 0x0A.toByte()
        ) {
            return file.inputStream().use { readBounded(it) }
        }

        return try {
            ZipFile(file).use { zip ->
                val entry = zip.getEntry("classes.dex")
                    ?: zip.entries().toList().firstOrNull { it.name.endsWith(".dex") }
                    ?: return null
                if (entry.size > MAX_DEX_SIZE) return null
                zip.getInputStream(entry).use { readBounded(it) }
            }
        } catch (e: Throwable) {
            null
        }
    }

    private fun readBounded(input: InputStream): ByteArray? {
        val output = java.io.ByteArrayOutputStream()
        val buffer = ByteArray(8192)
        var total = 0
        while (true) {
            val count = input.read(buffer)
            if (count < 0) break
            total += count
            if (total > MAX_DEX_SIZE) return null
            output.write(buffer, 0, count)
        }
        return output.toByteArray()
    }

    fun classNames(dex: ByteArray): List<String> {
        if (dex.size < 112) {
            return emptyList()
        }
        val classDefsSize = readInt(dex, CLASS_DEFS_SIZE_OFFSET)
        val classDefsOff = readInt(dex, CLASS_DEFS_OFF_OFFSET)
        val typeIdsOff = readInt(dex, TYPE_IDS_OFF_OFFSET)
        val stringIdsOff = readInt(dex, STRING_IDS_OFF_OFFSET)
        val typeIdsSize = readInt(dex, TYPE_IDS_OFF_OFFSET - 4)
        val stringIdsSize = readInt(dex, STRING_IDS_OFF_OFFSET - 4)
        if (classDefsSize <= 0 || classDefsSize > 200_000) {
            return emptyList()
        }
        if (!validTable(dex, classDefsOff, classDefsSize, CLASS_DEF_ITEM_SIZE) ||
            !validTable(dex, typeIdsOff, typeIdsSize, 4) ||
            !validTable(dex, stringIdsOff, stringIdsSize, 4)) {
            return emptyList()
        }

        val names = ArrayList<String>(classDefsSize)
        for (i in 0 until classDefsSize) {
            val classDef = classDefsOff + i * CLASS_DEF_ITEM_SIZE
            if (classDef + 4 > dex.size) break
            val typeIndex = readInt(dex, classDef)
            if (typeIndex !in 0 until typeIdsSize) continue
            val descriptorIndex = readInt(dex, typeIdsOff + typeIndex * 4)
            if (descriptorIndex !in 0 until stringIdsSize) continue
            val stringOffset = readInt(dex, stringIdsOff + descriptorIndex * 4)
            val descriptor = readString(dex, stringOffset) ?: continue

            if (descriptor.length > 2 && descriptor[0] == 'L' && descriptor.endsWith(";")) {
                names.add(descriptor.substring(1, descriptor.length - 1).replace('/', '.'))
            }
        }
        return names
    }

    private fun validTable(data: ByteArray, offset: Int, count: Int, itemSize: Int): Boolean =
        offset >= 112 && count > 0 && offset.toLong() + count.toLong() * itemSize <= data.size

    private fun readInt(data: ByteArray, offset: Int): Int {
        if (offset < 0 || offset > data.size - 4) return 0
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

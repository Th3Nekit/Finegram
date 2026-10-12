/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.icons.pack

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.drawable.Drawable
import android.content.res.Resources
import android.net.Uri
import android.util.LruCache
import org.json.JSONObject
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.ApplicationLoader
import com.th3nekit.finegram.core.FinegramLogger
import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig
import java.io.File
import java.util.zip.ZipInputStream

object IconPackManager {

    private const val PACKS_DIR = "iconpacks"

    private const val SLASH = '\\'

    private val drawableCache = object : LruCache<String, Drawable>(300) {}

    private var packsCache: List<IconPack>? = null

    private fun packsRoot(): File {
        val root = File(ApplicationLoader.getFilesDirFixed(), PACKS_DIR)
        if (!root.exists()) root.mkdirs()
        return root
    }

    fun installed(): List<IconPack> {
        packsCache?.let { return it }
        val packs = packsRoot().listFiles { file -> file.isDirectory }
            ?.mapNotNull { IconPack.read(it) }
            ?.sortedBy { it.name.lowercase() }
            ?: emptyList()
        packsCache = packs
        return packs
    }

    fun byId(id: String?): IconPack? {
        if (id.isNullOrEmpty()) return null
        return installed().firstOrNull { it.id == id }
    }

    fun active(): IconPack? = byId(FinegramAppearanceConfig.activeIconPackId)

    fun setActive(pack: IconPack?) {
        FinegramAppearanceConfig.activeIconPackId = pack?.id ?: ""
        drawableCache.evictAll()
    }

    fun delete(pack: IconPack) {
        if (FinegramAppearanceConfig.activeIconPackId == pack.id) {
            FinegramAppearanceConfig.activeIconPackId = ""
        }
        pack.directory.deleteRecursively()
        invalidate()
    }

    fun invalidate() {
        packsCache = null
        drawableCache.evictAll()
    }

    fun install(uri: Uri): IconPack? {
        val context = ApplicationLoader.applicationContext ?: return null
        return try {
            context.contentResolver.openInputStream(uri).use { input ->
                if (input == null) return null
                installFromZip(input.readBytes())
            }
        } catch (e: Throwable) {
            FinegramLogger.e("IconPackManager", { "не удалось прочитать пак" }, e)
            null
        }
    }

    fun install(file: File): IconPack? {
        return try {
            installFromZip(file.readBytes())
        } catch (e: Throwable) {
            FinegramLogger.e("IconPackManager", { "не удалось прочитать пак" }, e)
            null
        }
    }

    private fun installFromZip(bytes: ByteArray): IconPack? {
        val metadata = readMetadata(bytes) ?: return null
        val packId = sanitizeId(metadata.optString("packId").ifEmpty { metadata.optString("packName") })
        if (packId.isEmpty()) return null

        val target = File(packsRoot(), packId)
        if (target.exists()) target.deleteRecursively()
        target.mkdirs()

        ZipInputStream(bytes.inputStream()).use { zip ->
            while (true) {
                val entry = zip.nextEntry ?: break
                if (entry.isDirectory) continue
                val name = File(entry.name).name
                if (name.isEmpty() || name.startsWith(".")) continue
                File(target, name).outputStream().use { out -> zip.copyTo(out) }
            }
        }

        invalidate()
        val pack = IconPack.read(target)
        if (pack == null) target.deleteRecursively()
        return pack
    }

    private fun readMetadata(bytes: ByteArray): JSONObject? {
        ZipInputStream(bytes.inputStream()).use { zip ->
            while (true) {
                val entry = zip.nextEntry ?: break
                if (File(entry.name).name == IconPack.METADATA_FILE) {
                    return try {
                        JSONObject(zip.readBytes().decodeToString())
                    } catch (e: Throwable) {
                        null
                    }
                }
            }
        }
        return null
    }

    private fun sanitizeId(raw: String): String =
        raw.trim().replace(Regex("[^A-Za-z0-9._-]"), "_").take(120).trim('.', '_')

    @JvmStatic
    fun readPreview(file: File): IconPackPreviewSheet.IconPackPreview? =
        readPreview(file.readBytesOrNull(), file.nameWithoutExtension)

    @JvmStatic
    fun readPreview(uri: Uri): IconPackPreviewSheet.IconPackPreview? {
        val context = ApplicationLoader.applicationContext ?: return null
        val bytes = try {
            context.contentResolver.openInputStream(uri)?.use { it.readBytes() }
        } catch (e: Throwable) {
            FinegramLogger.e("IconPackManager", { "не удалось прочитать пак" }, e)
            null
        }
        return readPreview(bytes, "")
    }

    private fun File.readBytesOrNull(): ByteArray? = try {
        readBytes()
    } catch (e: Throwable) {
        FinegramLogger.e("IconPackManager", { "не удалось прочитать пак" }, e)
        null
    }

    private fun readPreview(bytes: ByteArray?, fallbackName: String): IconPackPreviewSheet.IconPackPreview? {
        if (bytes == null) return null
        return try {
            val metadata = readMetadata(bytes) ?: return null
            if (metadata.optInt("schemaVersion", 0) > IconPack.SUPPORTED_SCHEMA) return null

            val iconsJson = metadata.optJSONObject("icons") ?: return null

            val slots = LinkedHashMap<String, String>(iconsJson.length())
            val keys = iconsJson.keys()
            while (keys.hasNext()) {
                val key = keys.next()
                val value = iconsJson.optString(key)
                if (value.isNotEmpty() && !value.contains('/') && !value.contains(SLASH)) {
                    slots[key] = value
                }
            }
            if (slots.isEmpty()) return null

            IconPackPreviewSheet.IconPackPreview(
                metadata.optString("packName").ifEmpty { fallbackName },
                metadata.optString("author"),
                slots.size,
                slots,
                bytes
            )
        } catch (e: Throwable) {
            FinegramLogger.e("IconPackManager", { "предпросмотр набора не читается" }, e)
            null
        }
    }

    fun interface IconsReady {
        fun onIcons(icons: HashMap<String, Bitmap>)
    }

    @JvmStatic
    fun loadIcons(
        preview: IconPackPreviewSheet.IconPackPreview,
        wanted: Set<String>,
        sizePx: Int,
        cancelled: java.util.concurrent.atomic.AtomicBoolean,
        ready: IconsReady
    ) {
        org.telegram.messenger.Utilities.globalQueue.postRunnable {
            try {

                val needed = HashMap<String, MutableList<String>>()
                for ((slot, fileName) in preview.slots) {
                    if (!wanted.contains(slot)) continue
                    needed.getOrPut(fileName) { ArrayList() }.add(slot)
                }
                if (needed.isEmpty()) return@postRunnable

                var batch = HashMap<String, Bitmap>()
                ZipInputStream(preview.source.inputStream()).use { zip ->
                    while (true) {
                        if (cancelled.get()) return@postRunnable
                        val entry = zip.nextEntry ?: break
                        if (entry.isDirectory) continue
                        val name = File(entry.name).name
                        val places = needed[name] ?: continue
                        val bitmap = decodeIcon(name, zip.readBytes(), sizePx) ?: continue
                        for (slot in places) {
                            batch[slot] = bitmap
                        }
                        if (batch.size >= BATCH_SIZE) {
                            val ready1 = batch
                            batch = HashMap()
                            AndroidUtilities.runOnUIThread {
                                if (!cancelled.get()) ready.onIcons(ready1)
                            }
                        }
                    }
                }
                if (batch.isNotEmpty()) {
                    val last = batch
                    AndroidUtilities.runOnUIThread {
                        if (!cancelled.get()) ready.onIcons(last)
                    }
                }
            } catch (e: Throwable) {
                FinegramLogger.e("IconPackManager", { "картинки набора не раскрылись" }, e)
            }
        }
    }

    private const val BATCH_SIZE = 24

    private fun decodeIcon(name: String, bytes: ByteArray, sizePx: Int): Bitmap? {
        if (IconPackSvg.looksLikeSvg(name, bytes)) {
            return IconPackSvg.render(bytes, sizePx)
        }
        return try {
            val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
            BitmapFactory.decodeByteArray(bytes, 0, bytes.size, bounds)
            val options = BitmapFactory.Options()
            var sample = 1
            while (bounds.outWidth / (sample * 2) >= sizePx && bounds.outHeight / (sample * 2) >= sizePx) {
                sample *= 2
            }
            options.inSampleSize = sample
            BitmapFactory.decodeByteArray(bytes, 0, bytes.size, options)
        } catch (e: Throwable) {
            null
        }
    }

    @JvmStatic
    @JvmOverloads
    fun drawableFor(resourceId: Int, source: Resources, theme: Resources.Theme? = null, density: Int = 0): Drawable? {
        val pack = active() ?: return null

        val resourceName = try {
            source.getResourceEntryName(resourceId)
        } catch (e: Throwable) {
            return null
        }
        val file = pack.fileFor(resourceName) ?: return null

        val stock = try {
            if (density > 0) {
                source.getDrawableForDensity(resourceId, density, theme)
            } else {
                source.getDrawable(resourceId, theme)
            }
        } catch (e: Throwable) {
            null
        }
        val sourceDensity = source.displayMetrics.densityDpi.takeIf { it > 0 } ?: 160
        val fallbackSize = maxOf(1, kotlin.math.ceil(24f * sourceDensity / 160f).toInt())
        val targetWidth = stock?.intrinsicWidth?.takeIf { it > 0 } ?: fallbackSize
        val targetHeight = stock?.intrinsicHeight?.takeIf { it > 0 } ?: fallbackSize

        val cacheKey = "${pack.id}:$resourceName:${targetWidth}x$targetHeight:$sourceDensity:$density:${source.configuration.uiMode}:${theme?.hashCode()}"
        drawableCache.get(cacheKey)?.let { cached ->
            return (cached.constantState?.newDrawable(source)?.mutate() ?: cached).also {
                it.colorFilter = cached.colorFilter
            }
        }

        return try {
            val bitmap = decodeIcon(file.name, file.readBytes(), maxOf(targetWidth, targetHeight)) ?: return null
            val drawable = try {
                IconPackRaster.drawable(source, bitmap, stock, targetWidth, targetHeight)
            } finally {
                bitmap.recycle()
            }
            drawableCache.put(cacheKey, drawable)
            (drawable.constantState?.newDrawable(source)?.mutate() ?: drawable).also {
                it.colorFilter = drawable.colorFilter
            }
        } catch (e: Throwable) {
            FinegramLogger.e("IconPackManager", { "иконка $resourceName не читается" }, e)
            null
        }
    }
}

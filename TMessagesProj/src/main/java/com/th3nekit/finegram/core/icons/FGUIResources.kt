/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.icons

import android.annotation.SuppressLint
import android.content.res.*
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.drawable.Drawable
import com.th3nekit.finegram.core.FinegramLogger
import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig
import com.th3nekit.finegram.core.icons.icon_replaces.BaseIconReplace
import com.th3nekit.finegram.core.icons.pack.IconPackManager
import com.th3nekit.finegram.core.icons.pack.IconPackRaster

@Suppress("DEPRECATION")
@SuppressLint("UseCompatLoadingForDrawables")
class FGUIResources(private val wrapped: Resources) : Resources(wrapped.assets, wrapped.displayMetrics, wrapped.configuration) {

    private var activeReplacement: BaseIconReplace = FinegramAppearanceConfig.getCurrentIconPack()
    private val drawableConfiguration = Configuration(wrapped.configuration)
    private var drawableDensity = wrapped.displayMetrics.densityDpi

    @Synchronized
    fun reloadReplacements() {
        activeReplacement = FinegramAppearanceConfig.getCurrentIconPack()
        IconPackManager.invalidate()
        clearCache()
    }

    private val drawableCache = object : LinkedHashMap<Triple<Int, Int?, Theme?>, Drawable>(300, 0.75f, true) {
        override fun removeEldestEntry(eldest: MutableMap.MutableEntry<Triple<Int, Int?, Theme?>, Drawable>?): Boolean {
            return size > 300
        }
    }

    @Synchronized
    private fun clearCache() {
        drawableCache.clear()
    }

    @Synchronized
    private fun getCachedDrawable(
        cacheKey: Triple<Int, Int?, Theme?>,
        wrappedId: Int,
        originalId: Int,
        loader: () -> Drawable,
        fallback: () -> Drawable
    ): Drawable {
        if (drawableConfiguration != wrapped.configuration || drawableDensity != wrapped.displayMetrics.densityDpi) {
            drawableCache.clear()
            drawableConfiguration.setTo(wrapped.configuration)
            drawableDensity = wrapped.displayMetrics.densityDpi
        }
        val drawable = drawableCache.getOrPut(cacheKey) {
            try {
                val replacement = loader()
                if (wrappedId == originalId) replacement else {
                    val stock = fallback()
                    val width = stock.intrinsicWidth
                    val height = stock.intrinsicHeight
                    if (width <= 0 || height <= 0) replacement else {
                        val bitmap = Bitmap.createBitmap(
                            maxOf(1, replacement.intrinsicWidth),
                            maxOf(1, replacement.intrinsicHeight), Bitmap.Config.ARGB_8888)
                        bitmap.density = wrapped.displayMetrics.densityDpi
                        try {
                            replacement.setBounds(0, 0, bitmap.width, bitmap.height)
                            replacement.draw(Canvas(bitmap))
                            IconPackRaster.drawable(wrapped, bitmap, stock, width, height)
                        } finally {
                            bitmap.recycle()
                        }
                    }
                }
            } catch (e: NotFoundException) {
                if (wrappedId == originalId) throw e
                FinegramLogger.e("FGUIResources", { "Resource $wrappedId unavailable; using $originalId" }, e)
                fallback()
            }
        }
        return (drawable.constantState?.newDrawable(wrapped, cacheKey.third)?.mutate() ?: drawable).also {
            it.colorFilter = drawable.colorFilter
        }
    }

    @Deprecated("Deprecated in Java")
    @Throws(NotFoundException::class)
    @Synchronized
    override fun getDrawable(id: Int): Drawable {
        IconPackManager.drawableFor(id, wrapped)?.let { return it }
        val wrappedId = activeReplacement.wrap(id)
        val cacheKey = Triple(id, null, null)

        return getCachedDrawable(cacheKey, wrappedId, id,
                { wrapped.getDrawable(wrappedId, null) },
                { wrapped.getDrawable(id, null) })
    }

    @Throws(NotFoundException::class)
    @Synchronized
    override fun getDrawable(id: Int, theme: Theme?): Drawable {
        IconPackManager.drawableFor(id, wrapped, theme)?.let { return it }
        val wrappedId = activeReplacement.wrap(id)
        val cacheKey = Triple(id, null, theme)

        return getCachedDrawable(cacheKey, wrappedId, id,
                { wrapped.getDrawable(wrappedId, theme) },
                { wrapped.getDrawable(id, theme) })
    }

    @Deprecated("Deprecated in Java")
    @Throws(NotFoundException::class)
    @Synchronized
    override fun getDrawableForDensity(id: Int, density: Int): Drawable {
        IconPackManager.drawableFor(id, wrapped, null, density)?.let { return it }
        val wrappedId = activeReplacement.wrap(id)
        val cacheKey = Triple(id, density, null)

        return getCachedDrawable(cacheKey, wrappedId, id,
                { wrapped.getDrawableForDensity(wrappedId, density, null) ?: throw NotFoundException("Drawable $wrappedId") },
                { wrapped.getDrawableForDensity(id, density, null) ?: throw NotFoundException("Drawable $id") })
    }

    @Synchronized
    override fun getDrawableForDensity(id: Int, density: Int, theme: Theme?): Drawable {
        IconPackManager.drawableFor(id, wrapped, theme, density)?.let { return it }
        val wrappedId = activeReplacement.wrap(id)
        val cacheKey = Triple(id, density, theme)

        return getCachedDrawable(cacheKey, wrappedId, id,
                { wrapped.getDrawableForDensity(wrappedId, density, theme) ?: throw NotFoundException("Drawable $wrappedId") },
                { wrapped.getDrawableForDensity(id, density, theme) ?: throw NotFoundException("Drawable $id") })
    }

}

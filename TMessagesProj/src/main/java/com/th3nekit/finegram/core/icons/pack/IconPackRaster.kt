package com.th3nekit.finegram.core.icons.pack

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.RectF
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import android.content.res.Resources

internal object IconPackRaster {
    fun drawable(source: Resources, input: Bitmap, stock: Drawable?, width: Int, height: Int): BitmapDrawable {
        val pixels = IntArray(input.width * input.height)
        input.getPixels(pixels, 0, input.width, 0, 0, input.width, input.height)
        val content = bounds(pixels, input.width, input.height)
        var visible = 0L
        var colored = 0L
        for (color in pixels) {
            val alpha = Color.alpha(color)
            if (alpha < 16) continue
            visible += alpha
            if (maxOf(Color.red(color), Color.green(color), Color.blue(color)) -
                minOf(Color.red(color), Color.green(color), Color.blue(color)) > 24) {
                colored += alpha
            }
        }
        val monochrome = visible > 0 && colored * 100 <= visible
        val artwork = if (monochrome) {
            Bitmap.createBitmap(input.width, input.height, Bitmap.Config.ARGB_8888).also {
                for (i in pixels.indices) pixels[i] = pixels[i] or 0x00ffffff
                it.setPixels(pixels, 0, input.width, 0, 0, input.width, input.height)
            }
        } else input
        val sampleScale = minOf(1f, 128f / maxOf(width, height))
        val sampleWidth = maxOf(1, Math.round(width * sampleScale))
        val sampleHeight = maxOf(1, Math.round(height * sampleScale))
        val sample = Bitmap.createBitmap(sampleWidth, sampleHeight, Bitmap.Config.ARGB_8888)
        sample.density = source.displayMetrics.densityDpi
        try {
            if (stock != null) {
                val oldBounds = Rect(stock.bounds)
                try {
                    stock.setBounds(0, 0, sampleWidth, sampleHeight)
                    stock.draw(Canvas(sample))
                } finally {
                    stock.setBounds(oldBounds)
                }
            }
            val stockPixels = IntArray(sampleWidth * sampleHeight)
            sample.getPixels(stockPixels, 0, sampleWidth, 0, 0, sampleWidth, sampleHeight)
            val stockBounds = bounds(stockPixels, sampleWidth, sampleHeight)
            val target = if (stockBounds.isEmpty) RectF(0f, 0f, width.toFloat(), height.toFloat()) else RectF(
                stockBounds.left * width.toFloat() / sampleWidth,
                stockBounds.top * height.toFloat() / sampleHeight,
                stockBounds.right * width.toFloat() / sampleWidth,
                stockBounds.bottom * height.toFloat() / sampleHeight
            )
            val output = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
            output.density = source.displayMetrics.densityDpi
            if (!content.isEmpty) {
                val scale = minOf((target.right - target.left) / content.width(),
                    (target.bottom - target.top) / content.height())
                val left = (target.left + target.right - content.width() * scale) / 2f - content.left * scale
                val top = (target.top + target.bottom - content.height() * scale) / 2f - content.top * scale
                Canvas(output).drawBitmap(artwork, null,
                    RectF(left, top, left + artwork.width * scale, top + artwork.height * scale),
                    Paint(Paint.ANTI_ALIAS_FLAG or Paint.FILTER_BITMAP_FLAG))
            }
            return BitmapDrawable(source, output).also {
                if (stock?.colorFilter != null) {
                    it.colorFilter = stock.colorFilter
                } else if (monochrome && !stockBounds.isEmpty) {
                    val colors = HashMap<Int, Int>()
                    for (color in stockPixels) {
                        if (Color.alpha(color) >= 128) {
                            val rgb = color or 0xff000000.toInt()
                            colors[rgb] = (colors[rgb] ?: 0) + 1
                        }
                    }
                    colors.maxByOrNull { entry -> entry.value }?.key?.let { color ->
                        it.setTint(color)
                    }
                }
            }
        } finally {
            sample.recycle()
            if (artwork !== input) artwork.recycle()
        }
    }

    private fun bounds(pixels: IntArray, width: Int, height: Int): Rect {
        var left = width
        var top = height
        var right = 0
        var bottom = 0
        for (y in 0 until height) {
            for (x in 0 until width) {
                if (Color.alpha(pixels[y * width + x]) < 16) continue
                left = minOf(left, x)
                top = minOf(top, y)
                right = maxOf(right, x + 1)
                bottom = maxOf(bottom, y + 1)
            }
        }
        return if (right <= left || bottom <= top) Rect() else Rect(left, top, right, bottom)
    }
}

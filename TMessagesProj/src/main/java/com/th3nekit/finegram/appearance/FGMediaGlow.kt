/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.appearance

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.RectF
import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig
import org.telegram.messenger.Utilities
import org.telegram.messenger.ImageReceiver

object FGMediaGlow {

    private const val SAMPLE_SIZE = 24

    private val paint = Paint(Paint.FILTER_BITMAP_FLAG or Paint.ANTI_ALIAS_FLAG)
    private val shadePaint = Paint()
    private val source = Rect()
    private val target = RectF()

    private var glow: Bitmap? = null
    private var sourceKey = 0

    @JvmStatic
    @JvmOverloads
    fun draw(canvas: Canvas, width: Int, height: Int, receiver: ImageReceiver?, alpha: Float = 1f) {
        if (!FinegramAppearanceConfig.mediaGlow || width <= 0 || height <= 0 || alpha <= 0f) {
            return
        }
        val bitmap = prepare(receiver) ?: return

        source.set(0, 0, bitmap.width, bitmap.height)
        target.set(0f, 0f, width.toFloat(), height.toFloat())

        paint.alpha = (170 * alpha).toInt().coerceIn(0, 255)
        canvas.drawBitmap(bitmap, source, target, paint)

        shadePaint.color = Color.argb((70 * alpha).toInt().coerceIn(0, 255), 0, 0, 0)
        canvas.drawRect(target, shadePaint)
    }

    private fun prepare(receiver: ImageReceiver?): Bitmap? {
        val original = try {
            receiver?.bitmap
        } catch (e: Throwable) {
            null
        }
        if (original == null || original.isRecycled || original.width <= 0 || original.height <= 0) {
            return glow
        }

        val key = System.identityHashCode(original)
        val cached = glow
        if (key == sourceKey && cached != null && !cached.isRecycled) {
            return cached
        }

        return try {
            val scale = SAMPLE_SIZE.toFloat() / maxOf(original.width, original.height)
            val width = maxOf(1, (original.width * scale).toInt())
            val height = maxOf(1, (original.height * scale).toInt())
            val scaled = Bitmap.createScaledBitmap(original, width, height, true)

            val small = if (scaled.isMutable && scaled.config == Bitmap.Config.ARGB_8888) {
                scaled
            } else {
                scaled.copy(Bitmap.Config.ARGB_8888, true).also {
                    if (scaled != original) scaled.recycle()
                }
            }
            Utilities.stackBlurBitmap(small, 3)
            glow?.takeIf { it != small && !it.isRecycled }?.recycle()
            glow = small
            sourceKey = key
            small
        } catch (e: Throwable) {
            glow
        }
    }

    @JvmStatic
    fun reset() {
        glow?.takeIf { !it.isRecycled }?.recycle()
        glow = null
        sourceKey = 0
    }
}

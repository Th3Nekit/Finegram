/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.PixelFormat
import android.graphics.drawable.Drawable
import androidx.core.content.ContextCompat
import org.telegram.messenger.AndroidUtilities.dp
import org.telegram.messenger.R
import org.telegram.ui.ActionBar.Theme
import kotlin.math.ceil

class StarsBadgeDrawable(
    context: Context,
    private val isArchive: Boolean,
    count: Long
) : Drawable() {

    private val starSize = dp(17f)
    private val height = dp(23f)
    private val paddingH = dp(8f)
    private val spacing = dp(4f)

    private val bgPaint = Theme.dialogs_countGrayPaint
    private val textPaint = Theme.dialogs_countTextPaintFinegram

    private val starDrawable = ContextCompat.getDrawable(context, R.drawable.star_small_inner)!!.mutate()

    private val text = count.toString()
    private val textWidth = ceil(textPaint.measureText(text).toDouble()).toInt()

    private val contentWidth = textWidth + if (!isArchive) spacing + starSize else 0
    private val totalWidth = contentWidth + paddingH * 2

    override fun getIntrinsicWidth(): Int = totalWidth
    override fun getIntrinsicHeight(): Int = height

    override fun draw(canvas: Canvas) {
        val left = bounds.left.toFloat()
        val top = bounds.top.toFloat()
        val right = bounds.right.toFloat()
        val bottom = bounds.bottom.toFloat()

        val radius = height / 2f

        canvas.drawRoundRect(
            left,
            top,
            right,
            bottom,
            radius,
            radius,
            bgPaint
        )

        val startX = left + (totalWidth - contentWidth) / 2f
        val textY = top + height / 2f - (textPaint.descent() + textPaint.ascent()) / 2

        canvas.drawText(text, startX, textY, textPaint)

        if (!isArchive) {
            val starX = (startX + textWidth + spacing).toInt()
            val starY = (top + (height - starSize) / 2f).toInt()

            starDrawable.setBounds(
                starX,
                starY,
                starX + starSize,
                starY + starSize
            )
            starDrawable.draw(canvas)
        }
    }

    override fun setAlpha(alpha: Int) {
        bgPaint.alpha = alpha
        textPaint.alpha = alpha
        starDrawable.alpha = alpha
    }

    override fun setColorFilter(colorFilter: ColorFilter?) {
        bgPaint.colorFilter = colorFilter
        textPaint.colorFilter = colorFilter
        starDrawable.colorFilter = colorFilter
    }

    @Deprecated("Deprecated in Java")
    override fun getOpacity(): Int = PixelFormat.TRANSLUCENT

}

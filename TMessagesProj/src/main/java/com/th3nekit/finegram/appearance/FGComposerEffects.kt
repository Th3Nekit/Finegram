/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.appearance

import android.graphics.Canvas
import android.graphics.Paint
import android.widget.EditText
import android.view.Gravity
import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig
import org.telegram.messenger.AndroidUtilities
import java.util.WeakHashMap
import kotlin.math.max
import kotlin.random.Random

object FGComposerEffects {

    private const val MAX_LIVE_SPARKS = 96

    private const val SPARK_LIFE = 420f
    private const val FLASH_LIFE = 260f

    private val sparksPerLetter: Int
        get() = FinegramAppearanceConfig.composerParticleCount.coerceIn(1, 24)

    private val speedFactor: Float
        get() = FinegramAppearanceConfig.composerParticleSpeed.coerceIn(20, 300) / 100f

    private val sizeFactor: Float
        get() = FinegramAppearanceConfig.composerParticleSize.coerceIn(20, 300) / 100f

    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val states = WeakHashMap<EditText, State>()

    private class Spark(
        val x: Float,
        val y: Float,
        val speedX: Float,
        val speedY: Float,
        val bornAt: Long
    )

    private class Flash(val x: Float, val y: Float, val bornAt: Long)

    private class State {
        val sparks = ArrayList<Spark>()
        val flashes = ArrayList<Flash>()
        var deletedPoints: List<Pair<Float, Float>> = emptyList()
        var pendingFlash: Int? = null
    }

    @JvmStatic
    fun beforeTextChanged(editText: EditText, start: Int, count: Int, after: Int) {
        states[editText]?.deletedPoints = emptyList()
        if (!FinegramAppearanceConfig.composerEffects || count <= after || count > 2) return
        val state = states.getOrPut(editText) { State() }
        state.deletedPoints = (start until start + count).mapNotNull { glyphPoint(editText, it) }
    }

    @JvmStatic
    fun release(editText: EditText) {
        states.remove(editText)
    }

    @JvmStatic
    fun onTextChanged(editText: EditText?, start: Int, lengthBefore: Int, lengthAfter: Int) {
        if (editText == null || !FinegramAppearanceConfig.composerEffects) {
            return
        }

        val added = lengthAfter - lengthBefore
        if (added > 2 || added < -2) {
            return
        }
        val state = states.getOrPut(editText) { State() }
        val now = System.currentTimeMillis()

        if (added > 0) {

            if (!FinegramAppearanceConfig.composerAppear) {
                state.pendingFlash = start + lengthAfter - 1
            }
        } else if (added < 0) {
            for (point in state.deletedPoints) repeat(sparksPerLetter) {
                if (state.sparks.size >= MAX_LIVE_SPARKS) {
                    return@repeat
                }
                state.sparks.add(
                    Spark(
                        x = point.first,
                        y = point.second,
                        speedX = (Random.nextFloat() - 0.5f) * AndroidUtilities.dp(28f) * speedFactor,
                        speedY = (-Random.nextFloat() * AndroidUtilities.dp(22f) - AndroidUtilities.dp(6f)) * speedFactor,
                        bornAt = now
                    )
                )
            }
        }
        state.deletedPoints = emptyList()
        editText.invalidate()
    }

    @JvmStatic
    fun draw(editText: EditText?, canvas: Canvas) {
        if (editText == null || !FinegramAppearanceConfig.composerEffects) {
            return
        }
        val state = states[editText] ?: return
        state.pendingFlash?.let { offset ->
            glyphPoint(editText, offset)?.let { point ->
                state.flashes.add(Flash(point.first, point.second, System.currentTimeMillis()))
            }
            state.pendingFlash = null
        }
        if (state.sparks.isEmpty() && state.flashes.isEmpty()) {
            return
        }

        val now = System.currentTimeMillis()
        val color = editText.currentTextColor

        val flashes = state.flashes.iterator()
        while (flashes.hasNext()) {
            val flash = flashes.next()
            val progress = (now - flash.bornAt) / FLASH_LIFE
            if (progress >= 1f) {
                flashes.remove()
                continue
            }
            paint.color = color
            paint.alpha = (70 * (1f - progress)).toInt().coerceIn(0, 255)
            canvas.drawCircle(flash.x, flash.y, AndroidUtilities.dp(9f) * (0.4f + progress), paint)
        }

        val sparks = state.sparks.iterator()
        while (sparks.hasNext()) {
            val spark = sparks.next()
            val progress = (now - spark.bornAt) / SPARK_LIFE
            if (progress >= 1f) {
                sparks.remove()
                continue
            }

            val x = spark.x + spark.speedX * progress
            val y = spark.y + spark.speedY * progress + AndroidUtilities.dp(26f) * progress * progress
            paint.color = color
            paint.alpha = (200 * (1f - progress)).toInt().coerceIn(0, 255)
            canvas.drawCircle(x, y, AndroidUtilities.dp(1.6f) * sizeFactor * max(0.2f, 1f - progress), paint)
        }

        if (state.sparks.isNotEmpty() || state.flashes.isNotEmpty()) {
            editText.invalidate()
        }
    }

    private fun glyphPoint(editText: EditText, offset: Int): Pair<Float, Float>? {
        val layout = editText.layout ?: return null
        val text = editText.text ?: return null
        if (offset !in 0 until text.length || Character.isLowSurrogate(text[offset]) || text[offset] == '\n') return null
        val safeOffset = offset.coerceAtMost(layout.text.length)
        return try {
            val line = layout.getLineForOffset(safeOffset)
            val next = Character.offsetByCodePoints(text, safeOffset, 1).coerceAtMost(layout.getLineEnd(line))
            val startX = layout.getPrimaryHorizontal(safeOffset)
            val endX = if (layout.getLineForOffset(next) == line) layout.getPrimaryHorizontal(next)
                else startX + editText.paint.measureText(text, safeOffset, next) * if (layout.isRtlCharAt(safeOffset)) -1f else 1f
            val x = (startX + endX) / 2f + editText.compoundPaddingLeft
            val space = (editText.height - editText.extendedPaddingTop - editText.extendedPaddingBottom - layout.height).coerceAtLeast(0)
            val vertical = when (editText.gravity and Gravity.VERTICAL_GRAVITY_MASK) {
                Gravity.CENTER_VERTICAL -> space / 2
                Gravity.BOTTOM -> space
                else -> 0
            }
            val y = (layout.getLineBaseline(line) + layout.getLineAscent(line) / 2f) + editText.extendedPaddingTop + vertical
            Pair(x, y.toFloat())
        } catch (e: Throwable) {
            null
        }
    }
}

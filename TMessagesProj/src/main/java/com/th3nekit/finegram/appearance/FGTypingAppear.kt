/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.appearance

import android.graphics.BlurMaskFilter
import android.text.Editable
import android.text.Spanned
import android.text.TextPaint
import android.text.style.CharacterStyle
import android.widget.EditText
import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig
import org.telegram.messenger.AndroidUtilities
import java.util.WeakHashMap

object FGTypingAppear {

    private const val MAX_TYPED_AT_ONCE = 4

    private const val SHARP_TEXT_START = 0.2f

    private class Mark : CharacterStyle() {
        var progress = 0f

        override fun updateDrawState(tp: TextPaint) {
            val eased = easeOutQuint(progress)

            val sharp = ((eased - SHARP_TEXT_START) / (1f - SHARP_TEXT_START)).coerceIn(0f, 1f)
            tp.alpha = (tp.alpha * sharp.coerceAtLeast(0.05f)).toInt().coerceIn(0, 255)

            val slide = FinegramAppearanceConfig.composerAppearSlide
            if (slide > 0) {
                tp.baselineShift -= (AndroidUtilities.dp(slide.toFloat()) * (1f - eased)).toInt()
            }

            val blur = FinegramAppearanceConfig.composerAppearBlur
            if (blur > 0) {
                val radius = AndroidUtilities.dp(blur.toFloat() / 4f) * (1f - eased)
                if (radius > 0.4f) {
                    tp.maskFilter = BlurMaskFilter(radius, BlurMaskFilter.Blur.NORMAL)
                }
            }
        }
    }

    private fun easeOutQuint(t: Float): Float {
        val x = t.coerceIn(0f, 1f)
        val inv = 1f - x
        return 1f - inv * inv * inv * inv * inv
    }

    private class Running(val mark: Mark, val bornAt: Long)

    private val running = WeakHashMap<EditText, MutableList<Running>>()
    private class Change(val start: Int, val count: Int, val text: String)
    private val changes = WeakHashMap<EditText, Change>()

    @JvmStatic
    fun beforeTextChanged(editText: EditText, start: Int, count: Int, after: Int) {
        changes.remove(editText)
        if (!FinegramAppearanceConfig.composerAppear || count > 4096) return
        val text = editText.text ?: return
        if (start < 0 || count < 0 || start > text.length - count) return
        changes[editText] = Change(start, count, text.subSequence(start, start + count).toString())
    }

    @JvmStatic
    fun onTextChanged(editText: EditText?, start: Int, lengthBefore: Int, lengthAfter: Int) {
        if (editText == null) return
        val change = changes.remove(editText)
        if (!FinegramAppearanceConfig.composerAppear) {
            release(editText)
            return
        }
        val editable = editText.text as? Editable ?: return
        if (start < 0 || lengthAfter < 0 || start > editable.length - lengthAfter) return
        var from = start
        var to = start + lengthAfter
        if (change != null && change.start == start && change.count == lengthBefore) {
            val old = change.text
            var prefix = 0
            while (prefix < old.length && from < to && old[prefix] == editable[from]) {
                prefix++
                from++
            }
            var oldEnd = old.length
            while (oldEnd > prefix && to > from && old[oldEnd - 1] == editable[to - 1]) {
                oldEnd--
                to--
            }
        } else {
            if (lengthAfter <= lengthBefore || lengthAfter > MAX_TYPED_AT_ONCE) return
            from += lengthBefore
        }
        if (from >= to) return
        if (from > 0 && Character.isLowSurrogate(editable[from]) && Character.isHighSurrogate(editable[from - 1])) from--
        if (to < editable.length && Character.isLowSurrogate(editable[to]) && Character.isHighSurrogate(editable[to - 1])) to++
        if (to - from > MAX_TYPED_AT_ONCE) return

        val mark = Mark()
        try {
            editable.setSpan(mark, from, to, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
        } catch (e: Throwable) {
            return
        }
        running.getOrPut(editText) { ArrayList() }.add(Running(mark, System.currentTimeMillis()))
        editText.invalidate()
    }

    @JvmStatic
    fun beforeDraw(editText: EditText?) {
        if (editText == null) return
        if (!FinegramAppearanceConfig.composerAppear) {
            release(editText)
            return
        }
        val list = running[editText] ?: return
        if (list.isEmpty()) return

        val editable = editText.text as? Editable
        val now = System.currentTimeMillis()
        val duration = FinegramAppearanceConfig.composerAppearDuration.coerceAtLeast(60).toFloat()
        val iterator = list.iterator()
        var alive = false
        var finished: MutableList<Mark>? = null
        while (iterator.hasNext()) {
            val item = iterator.next()
            val progress = (now - item.bornAt) / duration
            if (progress >= 1f || editable == null) {
                item.mark.progress = 1f
                iterator.remove()
                if (editable != null) {
                    (finished ?: ArrayList<Mark>().also { finished = it }).add(item.mark)
                }
                continue
            }
            item.mark.progress = progress
            alive = true
        }

        finished?.let { marks ->
            AndroidUtilities.runOnUIThread {
                for (mark in marks) {
                    try { editable?.removeSpan(mark) } catch (e: Throwable) { }
                }
                editText.invalidate()
            }
        }
        if (alive) {
            editText.invalidate()
        }
    }

    @JvmStatic
    fun release(editText: EditText?) {
        if (editText == null) return
        changes.remove(editText)
        val list = running.remove(editText) ?: return
        val editable = editText.text as? Editable ?: return
        for (item in list) {
            try { editable.removeSpan(item.mark) } catch (e: Throwable) { }
        }
    }
}

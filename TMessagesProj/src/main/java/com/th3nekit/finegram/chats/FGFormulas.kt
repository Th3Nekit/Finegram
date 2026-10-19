/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.chats

import android.text.SpannableStringBuilder
import android.text.Spanned
import com.th3nekit.finegram.core.configs.FinegramChatsConfig
import org.telegram.ui.ActionBar.Theme
import org.telegram.ui.iv.MathSpan

/**
 * Формулы прямо в переписке.
 *
 * Кусок текста между знаками доллара — `$E = mc^2$` — показывается набранной формулой, а не
 * исходником. Сам текст сообщения не меняется: формула рисуется поверх своего диапазона,
 * поэтому копирование, пересылка и поиск работают с обычным исходником, а собеседник без
 * Finegram видит ровно то, что было отправлено.
 *
 * Рисовать формулу дорого, поэтому готовые остаются в памяти, а на одно сообщение их
 * берётся не больше горстки — иначе длинная переписка с математикой начнёт подтормаживать.
 */
object FGFormulas {

    private const val MAX_PER_MESSAGE = 6
    private const val MAX_SOURCE_LENGTH = 220
    private const val CACHE_SIZE = 64

    /** Готовые формулы: ключ — сам исходник вместе с цветом и размером текста. */
    private val cache = object : LinkedHashMap<String, MathSpan>(16, 0.75f, true) {
        override fun removeEldestEntry(eldest: MutableMap.MutableEntry<String, MathSpan>?): Boolean =
            size > CACHE_SIZE
    }

    /**
     * Возвращает текст с нарисованными формулами. Если формул нет или настройка выключена,
     * отдаёт то же самое, ничего не создавая.
     */
    @JvmStatic
    fun apply(text: CharSequence?): CharSequence? {
        if (text.isNullOrEmpty() || !FinegramChatsConfig.renderFormulas) {
            return text
        }
        if (text.indexOf('$') < 0) {
            return text
        }

        val paint = Theme.chat_msgTextPaint ?: return text
        val color = paint.color
        val size = paint.textSize

        var builder: SpannableStringBuilder? = null
        var applied = 0
        var index = 0

        while (applied < MAX_PER_MESSAGE) {
            val start = indexOfOpening(text, index) ?: break
            val end = text.indexOf('$', start + 1)
            if (end < 0) {
                break
            }
            val source = text.subSequence(start + 1, end).toString().trim()
            index = end + 1
            if (source.isEmpty() || source.length > MAX_SOURCE_LENGTH || !looksLikeFormula(source)) {
                continue
            }

            val span = render(source, color, size) ?: continue
            if (builder == null) {
                builder = SpannableStringBuilder(text)
            }
            builder.setSpan(span, start, end + 1, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
            applied++
        }

        return builder ?: text
    }

    /** Ищет открывающий доллар, пропуская экранированный `\$`. */
    private fun indexOfOpening(text: CharSequence, from: Int): Int? {
        var i = from
        while (i < text.length) {
            if (text[i] == '$' && (i == 0 || text[i - 1] != '\\')) {
                return i
            }
            i++
        }
        return null
    }

    /**
     * Правда, если между долларами похоже на формулу, а не на цены.
     * «$5 и $10» встречается куда чаще математики, и портить такой текст нельзя.
     */
    private fun looksLikeFormula(source: String): Boolean {
        if (source.any { it == '\n' }) {
            return false
        }
        return source.any { it == '\\' || it == '^' || it == '_' || it == '{' || it == '=' || it == '/' }
    }

    private fun render(source: String, color: Int, size: Float): MathSpan? {
        val key = source + "|" + color + "|" + size
        synchronized(cache) {
            cache[key]?.let { return it }
        }
        val span = try {
            MathSpan.create(source, color, size)
        } catch (e: Throwable) {
            null
        } ?: return null
        synchronized(cache) {
            cache[key] = span
        }
        return span
    }
}

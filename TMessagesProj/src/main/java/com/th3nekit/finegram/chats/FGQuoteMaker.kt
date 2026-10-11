/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.chats

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Shader
import android.text.Layout
import android.text.StaticLayout
import android.text.TextPaint
import android.text.TextUtils
import com.th3nekit.finegram.core.FinegramLogger
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.LocaleController
import org.telegram.messenger.MessageObject
import org.telegram.messenger.MessagesController
import org.telegram.messenger.UserObject
import org.telegram.tgnet.TLRPC
import kotlin.math.max

object FGQuoteMaker {

    private const val WIDTH = 1080
    private const val PADDING = 64
    private const val CARD_PADDING = 44
    private const val AVATAR_SIZE = 96
    private const val MAX_MESSAGES = 20

    private val BACKGROUND_COLORS = intArrayOf(0xFF356DFE.toInt(), 0xFF8146EE.toInt(), 0xFFC93FC0.toInt())

    @JvmStatic
    fun render(messages: List<MessageObject>, account: Int): Bitmap? {
        val selected = messages.filter { !it.isSecretMedia }.take(MAX_MESSAGES)
        if (selected.isEmpty()) {
            return null
        }
        return try {
            val blocks = selected.map { message -> buildBlock(message, account) }
            draw(blocks)
        } catch (e: Throwable) {
            FinegramLogger.e("FGQuote", { "цитата не нарисовалась" }, e)
            null
        }
    }

    private class Block(
        val author: String,
        val time: String,
        val textLayout: StaticLayout,
        val authorColor: Int,
        val height: Int
    )

    private val textPaint = TextPaint(Paint.ANTI_ALIAS_FLAG).apply {
        textSize = 40f
        color = Color.WHITE
    }

    private val authorPaint = TextPaint(Paint.ANTI_ALIAS_FLAG).apply {
        textSize = 38f
        typeface = AndroidUtilities.bold()
    }

    private val timePaint = TextPaint(Paint.ANTI_ALIAS_FLAG).apply {
        textSize = 30f
        color = 0x8CFFFFFF.toInt()
    }

    private val cardPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0x33000000
    }

    private val avatarPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val avatarLetterPaint = TextPaint(Paint.ANTI_ALIAS_FLAG).apply {
        textSize = 44f
        color = Color.WHITE
        typeface = AndroidUtilities.bold()
        textAlign = Paint.Align.CENTER
    }

    private val AVATAR_COLORS = intArrayOf(
        0xFFE17076.toInt(), 0xFF7BC862.toInt(), 0xFFE5CA77.toInt(), 0xFF65AADD.toInt(),
        0xFFA695E7.toInt(), 0xFFEE7AAE.toInt(), 0xFF6EC9CB.toInt()
    )

    private fun buildBlock(message: MessageObject, account: Int): Block {
        val controller = MessagesController.getInstance(account)
        val senderId = message.senderId
        val author = when {
            senderId > 0 -> {
                val user: TLRPC.User? = controller.getUser(senderId)
                if (user != null) UserObject.getUserName(user) else "—"
            }
            senderId < 0 -> {
                val chat: TLRPC.Chat? = controller.getChat(-senderId)
                chat?.title ?: "—"
            }
            else -> "—"
        }

        val text = message.messageText?.toString().orEmpty().ifEmpty { message.caption?.toString().orEmpty() }
        val time = LocaleController.getInstance().getFormatterDay()
            .format(message.messageOwner.date * 1000L)

        val contentWidth = WIDTH - PADDING * 2 - CARD_PADDING * 2 - AVATAR_SIZE - 24
        val layout = StaticLayout.Builder
            .obtain(text.ifEmpty { "—" }, 0, text.ifEmpty { "—" }.length, textPaint, contentWidth)
            .setAlignment(Layout.Alignment.ALIGN_NORMAL)
            .setLineSpacing(6f, 1f)
            .setEllipsize(TextUtils.TruncateAt.END)
            .setMaxLines(24)
            .build()

        val color = AVATAR_COLORS[((senderId % AVATAR_COLORS.size).toInt() + AVATAR_COLORS.size) % AVATAR_COLORS.size]
        val height = max(AVATAR_SIZE, layout.height + 54) + CARD_PADDING * 2
        return Block(author, time, layout, color, height)
    }

    private fun draw(blocks: List<Block>): Bitmap {
        val gap = 28
        val contentHeight = blocks.sumOf { it.height } + gap * (blocks.size - 1)
        val height = contentHeight + PADDING * 2

        val bitmap = Bitmap.createBitmap(WIDTH, height, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)

        val background = Paint(Paint.ANTI_ALIAS_FLAG)
        background.shader = LinearGradient(
            0f, 0f, WIDTH.toFloat(), height.toFloat(),
            BACKGROUND_COLORS, floatArrayOf(0f, 0.55f, 1f), Shader.TileMode.CLAMP
        )
        canvas.drawRect(0f, 0f, WIDTH.toFloat(), height.toFloat(), background)

        var top = PADDING.toFloat()
        for (block in blocks) {
            drawBlock(canvas, block, top)
            top += block.height + gap
        }
        return bitmap
    }

    private fun drawBlock(canvas: Canvas, block: Block, top: Float) {
        val left = PADDING.toFloat()
        val right = (WIDTH - PADDING).toFloat()
        val bottom = top + block.height

        val card = RectF(left, top, right, bottom)
        canvas.drawRoundRect(card, 36f, 36f, cardPaint)

        val avatarLeft = left + CARD_PADDING
        val avatarTop = top + CARD_PADDING
        avatarPaint.color = block.authorColor
        canvas.drawCircle(
            avatarLeft + AVATAR_SIZE / 2f,
            avatarTop + AVATAR_SIZE / 2f,
            AVATAR_SIZE / 2f,
            avatarPaint
        )
        val letter = block.author.trim().take(1).uppercase().ifEmpty { "?" }
        canvas.drawText(
            letter,
            avatarLeft + AVATAR_SIZE / 2f,
            avatarTop + AVATAR_SIZE / 2f + 16f,
            avatarLetterPaint
        )

        val textLeft = avatarLeft + AVATAR_SIZE + 24f
        authorPaint.color = block.authorColor
        canvas.drawText(block.author, textLeft, avatarTop + 36f, authorPaint)
        canvas.drawText(
            block.time,
            right - CARD_PADDING - timePaint.measureText(block.time),
            avatarTop + 36f,
            timePaint
        )

        canvas.save()
        canvas.translate(textLeft, avatarTop + 54f)
        block.textLayout.draw(canvas)
        canvas.restore()
    }
}

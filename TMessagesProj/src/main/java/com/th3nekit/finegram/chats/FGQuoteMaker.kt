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
import android.graphics.CornerPathEffect
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RadialGradient
import android.graphics.Rect
import android.graphics.RectF
import android.graphics.Shader
import android.text.Layout
import android.text.Spannable
import android.text.SpannableStringBuilder
import android.text.Spanned
import android.text.StaticLayout
import android.text.TextPaint
import android.text.TextUtils
import android.view.View
import androidx.core.graphics.ColorUtils
import com.th3nekit.finegram.core.FinegramLogger
import com.th3nekit.finegram.core.ui.FGAvatars
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.ChatObject
import org.telegram.messenger.DialogObject
import org.telegram.messenger.Emoji
import org.telegram.messenger.FileLoader
import org.telegram.messenger.ImageLocation
import org.telegram.messenger.ImageReceiver
import org.telegram.messenger.LocaleController
import org.telegram.messenger.MessageObject
import org.telegram.messenger.MessagesController
import org.telegram.messenger.R
import org.telegram.messenger.UserObject
import org.telegram.tgnet.TLObject
import org.telegram.tgnet.TLRPC
import org.telegram.ui.ActionBar.Theme
import org.telegram.ui.Components.AnimatedEmojiDrawable
import org.telegram.ui.Components.AnimatedEmojiSpan
import org.telegram.ui.Components.AvatarDrawable
import org.telegram.ui.Components.MessagePreviewView
import org.telegram.ui.Components.QuoteSpan
import org.telegram.ui.Components.TextStyleSpan
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt
import kotlin.math.sqrt

class FGQuoteMaker(
    private val host: View,
    private val account: Int,
    messages: List<MessageObject>,
    private val wallpaper: Bitmap?,
    source: TLObject?
) {

    class Style(
        @JvmField val dark: Boolean,
        @JvmField val background: Int,
        @JvmField val avatars: Boolean,
        @JvmField val names: Boolean,
        @JvmField val time: Boolean,
        @JvmField val replies: Boolean,
        @JvmField val rounded: Boolean,
        @JvmField val header: Boolean
    )

    private class Palette(
        val bubble: Int,
        val edge: Int,
        val text: Int,
        val secondary: Int,
        val link: Int,
        val names: IntArray
    )

    private class Author(
        val id: Long,
        val name: String,
        val peer: TLObject?,
        val colorId: Int,
        val nameLayout: StaticLayout
    ) {
        var avatar: ImageReceiver? = null
    }

    private class Reply(
        val author: Author,
        val name: StaticLayout,
        val text: StaticLayout?,
        val width: Float,
        val height: Float
    )

    private class Media(val receiver: ImageReceiver, val aspect: Float, val kind: Int)

    private class Part(
        val author: Author,
        val authorKey: String,
        val text: StaticLayout?,
        val rtl: Boolean,
        val lastLineWidth: Float,
        val lastLineRtl: Boolean,
        val spoilers: Path?,
        val spoilerDots: FloatArray?,
        val quotes: FloatArray?,
        val reply: Reply?,
        val media: Media?,
        val label: StaticLayout?,
        val wave: Boolean,
        val time: String,
        val timeWidth: Float
    ) {
        val bare: Boolean
            get() = media != null && media.kind != KIND_PHOTO && text == null
    }

    private class Bubble(
        val part: Part,
        val first: Boolean,
        val last: Boolean,
        val showName: Boolean,
        val showReply: Boolean
    ) {
        var x = 0f
        var y = 0f
        var w = 0f
        var h = 0f
        var nameTop = 0f
        var replyTop = 0f
        var mediaLeft = 0f
        var mediaTop = 0f
        var mediaWidth = 0f
        var mediaHeight = 0f
        var textTop = 0f
        var labelTop = 0f
        var timeBaseline = 0f
        var showTime = false
        var timeOnMedia = false
        val path = Path()
    }

    private class Spot(val author: Author, val x: Float, val y: Float)

    private class Header(val title: StaticLayout, val date: String, val dateWidth: Float, val avatar: ImageReceiver)

    private val textPaint = newPaint(16f, false)
    private val emojiPaint = newPaint(30f, false)
    private val namePaint = newPaint(14f, true)
    private val replyNamePaint = newPaint(13f, true)
    private val replyTextPaint = newPaint(13f, false)
    private val labelPaint = newPaint(15f, true)
    private val timePaint = newPaint(11.5f, false)
    private val headerPaint = newPaint(13.5f, true)

    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val edge = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = max(1f, px(0.7f))
    }
    private val glyphPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = px(1.6f)
        strokeCap = Paint.Cap.ROUND
        strokeJoin = Paint.Join.ROUND
        pathEffect = CornerPathEffect(px(1.5f))
    }
    private val dotPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        strokeCap = Paint.Cap.ROUND
        strokeWidth = px(1.3f)
    }
    private val spoilerPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        pathEffect = CornerPathEffect(px(4f))
    }

    private val rect = RectF()
    private val frameClip = Path()
    private val replyClip = Path()
    private val tail = Path()
    private val glyph = Path()

    private val maxText = px(262f)
    private val authors = HashMap<String, Author>()
    private val receivers = ArrayList<ImageReceiver>()
    private val emojiLayouts = ArrayList<Layout>()
    private var emojiStack: AnimatedEmojiSpan.EmojiGroupedSpans? = null
    private var attached = false

    private val attachListener = object : View.OnAttachStateChangeListener {
        override fun onViewAttachedToWindow(v: View) = attach()

        override fun onViewDetachedFromWindow(v: View) = detach()
    }

    private val parts: List<Part> = messages.mapNotNull { message ->
        try {
            part(message)
        } catch (e: Throwable) {
            FinegramLogger.e(TAG, { "сообщение не разложилось" }, e)
            null
        }
    }

    private val header: Header? = source?.let {
        try {
            headerOf(it, messages.firstOrNull()?.messageOwner?.date ?: 0)
        } catch (e: Throwable) {
            FinegramLogger.e(TAG, { "шапка цитаты не собралась" }, e)
            null
        }
    }

    init {
        host.addOnAttachStateChangeListener(attachListener)
        if (host.isAttachedToWindow) {
            attach()
        }
    }

    val isEmpty: Boolean
        get() = parts.isEmpty()

    fun build(style: Style): Scene = Scene(style)

    fun release() {
        host.removeOnAttachStateChangeListener(attachListener)
        detach()
    }

    fun exportScale(scene: Scene, sticker: Boolean): Float {
        val w = scene.width.toFloat()
        val h = scene.height.toFloat()
        if (sticker) {
            return STICKER_SIDE / max(w, h)
        }
        var scale = 3f / AndroidUtilities.density
        val pixels = w * h * scale * scale
        if (pixels > MAX_PIXELS) {
            scale *= sqrt(MAX_PIXELS / pixels)
        }
        val side = max(w, h) * scale
        if (side > MAX_SIDE) {
            scale *= MAX_SIDE / side
        }
        return scale
    }

    fun render(scene: Scene, sticker: Boolean, flat: Boolean): Bitmap? {
        val scale = exportScale(scene, sticker)
        var width = max(1, (scene.width * scale).roundToInt())
        var height = max(1, (scene.height * scale).roundToInt())
        if (sticker) {
            width = min(width, STICKER_SIDE.toInt())
            height = min(height, STICKER_SIDE.toInt())
        }
        return try {
            val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
            val canvas = Canvas(bitmap)
            canvas.scale(scale, scale)
            scene.draw(canvas, flat)
            bitmap
        } catch (e: Throwable) {
            FinegramLogger.e(TAG, { "цитата не нарисовалась" }, e)
            null
        }
    }

    inner class Scene internal constructor(private val style: Style) {

        private val palette = if (style.dark) DARK else LIGHT
        private val bubbles = ArrayList<Bubble>()
        private val spots = ArrayList<Spot>()

        val opaque: Boolean = style.background != BACKGROUND_NONE
        val radius: Float = if (opaque && style.rounded) px(22f) else 0f
        val width: Int
        val height: Int

        private val pad = px(if (opaque) 14f else 8f)
        private val head: Header? = if (style.header) header else null
        private var headWidth = 0f

        init {
            val avatar = px(AVATAR)
            val left = pad + if (style.avatars) avatar + px(7f) else 0f
            var y = pad
            var right = left
            if (head != null) {
                headWidth = px(40f) + max(head.title.getLineWidth(0), head.dateWidth) + px(14f)
                right = max(right, pad + headWidth)
                y += px(HEADER) + px(10f)
            }
            for (i in parts.indices) {
                val part = parts[i]
                val first = i == 0 || parts[i - 1].authorKey != part.authorKey
                val last = i == parts.size - 1 || parts[i + 1].authorKey != part.authorKey
                val bubble = Bubble(part, first, last, style.names && first, style.replies && part.reply != null)
                measure(bubble, style)
                bubble.x = left
                bubble.y = y
                if (!part.bare) {
                    shape(bubble)
                }
                bubbles.add(bubble)
                right = max(right, left + bubble.w)
                if (last && style.avatars) {
                    avatarOf(part.author)
                    spots.add(Spot(part.author, pad, y + bubble.h - avatar))
                }
                y += bubble.h + px(if (last) 10f else 3f)
            }
            if (bubbles.isNotEmpty()) {
                y -= px(10f)
            }
            width = ceil(right + pad).toInt()
            height = ceil(y + pad).toInt()
        }

        fun draw(canvas: Canvas) = draw(canvas, false)

        fun draw(canvas: Canvas, flat: Boolean) {
            val save = canvas.save()
            rect.set(0f, 0f, width.toFloat(), height.toFloat())
            if (opaque) {
                if (radius > 0f && !flat) {
                    frameClip.rewind()
                    frameClip.addRoundRect(rect, radius, radius, Path.Direction.CW)
                    canvas.clipPath(frameClip)
                }
                drawBackground(canvas, style.background, rect, style.dark, wallpaper)
            } else if (flat) {
                drawBackground(canvas, BACKGROUND_PLAIN, rect, style.dark, null)
            }
            if (head != null) {
                drawHeader(canvas, head, pad, headWidth, style, palette)
            }
            for (bubble in bubbles) {
                drawBubble(canvas, bubble, style, palette)
            }
            val size = px(AVATAR)
            for (spot in spots) {
                val receiver = avatarOf(spot.author)
                receiver.setImageCoords(spot.x, spot.y, size, size)
                receiver.draw(canvas)
            }
            canvas.restoreToCount(save)
        }
    }

    private fun drawHeader(canvas: Canvas, head: Header, pad: Float, width: Float, style: Style, palette: Palette) {
        val height = px(HEADER)
        rect.set(pad, pad, pad + width, pad + height)
        fill.color = if (style.background != BACKGROUND_NONE) {
            Theme.multAlpha(palette.bubble, if (style.dark) 0.82f else 0.88f)
        } else {
            palette.bubble
        }
        canvas.drawRoundRect(rect, height / 2f, height / 2f, fill)
        edge.color = palette.edge
        canvas.drawRoundRect(rect, height / 2f, height / 2f, edge)
        val size = px(26f)
        head.avatar.setImageCoords(pad + px(6f), pad + (height - size) / 2f, size, size)
        head.avatar.draw(canvas)
        headerPaint.color = palette.text
        canvas.save()
        canvas.translate(pad + px(40f), pad + px(4.5f))
        head.title.draw(canvas)
        canvas.restore()
        timePaint.color = palette.secondary
        canvas.drawText(head.date, pad + px(40f), pad + px(32f), timePaint)
    }

    private fun headerOf(peer: TLObject, date: Int): Header? {
        val title = when (peer) {
            is TLRPC.User -> UserObject.getUserName(peer)
            is TLRPC.Chat -> peer.title
            else -> null
        }
        if (title.isNullOrEmpty()) {
            return null
        }
        val limit = px(220f)
        val titleText = TextUtils.ellipsize(
            Emoji.replaceEmoji(title, headerPaint.fontMetricsInt, false), headerPaint, limit, TextUtils.TruncateAt.END
        )
        val drawable = AvatarDrawable()
        drawable.setInfo(account, peer)
        val receiver = ImageReceiver(host)
        receiver.setCurrentAccount(account)
        receiver.setRoundRadius(FGAvatars.corners(26f))
        receiver.setForUserOrChat(peer, drawable)
        receivers.add(receiver)
        val day = if (date > 0) LocaleController.formatDateChat(date.toLong()) else ""
        return Header(layout(titleText, headerPaint, limit.toInt(), 1), day, timePaint.measureText(day), receiver)
    }

    private fun measure(b: Bubble, style: Style) {
        val part = b.part
        val nameLayout = part.author.nameLayout
        val nameHeight = nameLayout.height.toFloat()
        val nameWidth = nameLayout.getLineWidth(0)
        val media = part.media
        if (part.bare && media != null) {
            val box = px(if (media.kind == KIND_ROUND) 176f else 156f)
            var top = 0f
            var width = box
            if (b.showName) {
                b.nameTop = 0f
                top = nameHeight + px(4f)
                width = max(width, nameWidth)
            }
            val bareReply = part.reply
            if (b.showReply && bareReply != null) {
                b.replyTop = top
                top += bareReply.height + px(6f)
                width = max(width, bareReply.width)
            }
            b.mediaTop = top
            b.mediaWidth = box
            b.mediaHeight = box
            b.w = width
            b.h = top + box
            b.showTime = style.time
            b.timeOnMedia = true
            return
        }

        val padH = px(11f)
        val inset = px(3f)
        val text = part.text
        val label = part.label
        var inner = 0f
        var top = px(7f)
        var above = false

        if (b.showName) {
            b.nameTop = top
            top += nameHeight + px(2f)
            inner = max(inner, nameWidth)
            above = true
        }
        val reply = part.reply
        if (b.showReply && reply != null) {
            b.replyTop = top + if (above) px(1f) else 0f
            top = b.replyTop + reply.height + px(6f)
            inner = max(inner, reply.width)
            above = true
        }
        if (media != null) {
            val full = maxText + padH * 2 - inset * 2
            b.mediaLeft = inset
            b.mediaWidth = full
            b.mediaHeight = full * media.aspect
            b.mediaTop = if (above) top + px(1f) else inset
            top = b.mediaTop + b.mediaHeight + if (text != null || label != null) px(7f) else inset
            inner = max(inner, maxText)
        }

        var ownLine = false
        if (text != null) {
            b.textTop = top
            top += text.height
            inner = max(inner, text.width.toFloat())
            if (style.time) {
                val need = part.lastLineWidth + px(8f) + part.timeWidth
                if (!part.lastLineRtl && need <= maxText) {
                    inner = max(inner, need)
                    b.timeBaseline = b.textTop + text.getLineBaseline(text.lineCount - 1)
                } else {
                    ownLine = true
                }
            }
        } else if (label != null) {
            b.labelTop = top
            val used = px(42f) + label.getLineWidth(0)
            inner = max(inner, used)
            top += px(36f)
            if (style.time) {
                val need = used + px(8f) + part.timeWidth
                if (need <= maxText) {
                    inner = max(inner, need)
                    b.timeBaseline = b.labelTop + px(23f)
                } else {
                    ownLine = true
                }
            }
        } else if (media == null && style.time) {
            ownLine = true
        }
        if (ownLine) {
            inner = max(inner, part.timeWidth)
            top += px(14f)
            b.timeBaseline = top - px(1f)
        }

        val mediaOnly = media != null && text == null && label == null
        b.showTime = style.time
        b.timeOnMedia = style.time && mediaOnly
        b.w = inner + padH * 2
        b.h = if (mediaOnly) top else top + px(7f)
    }

    private fun shape(b: Bubble) {
        val big = px(16f)
        val small = px(6f)
        val topLeft = if (b.first) big else small
        val bottomLeft = if (b.last) 0f else small
        rect.set(b.x, b.y, b.x + b.w, b.y + b.h)
        b.path.rewind()
        b.path.addRoundRect(
            rect,
            floatArrayOf(topLeft, topLeft, big, big, big, big, bottomLeft, bottomLeft),
            Path.Direction.CW
        )
        if (b.last) {
            val left = b.x
            val bottom = b.y + b.h
            tail.rewind()
            tail.moveTo(left, bottom - px(13f))
            tail.cubicTo(left, bottom - px(4f), left - px(2f), bottom - px(0.5f), left - px(6.5f), bottom)
            tail.lineTo(left + px(10f), bottom)
            tail.lineTo(left + px(10f), bottom - px(13f))
            tail.close()
            b.path.op(tail, Path.Op.UNION)
        }
    }

    private fun drawBubble(canvas: Canvas, b: Bubble, style: Style, palette: Palette) {
        val part = b.part
        val padH = if (part.bare) 0f else px(11f)
        if (!part.bare) {
            fill.color = if (style.background != BACKGROUND_NONE) {
                Theme.multAlpha(palette.bubble, if (style.dark) 0.9f else 0.94f)
            } else {
                palette.bubble
            }
            canvas.drawPath(b.path, fill)
            edge.color = palette.edge
            canvas.drawPath(b.path, edge)
        }
        val left = b.x + padH
        val inner = b.w - padH * 2

        if (b.showName) {
            namePaint.color = nameColor(part.author, palette, style.dark)
            canvas.save()
            canvas.translate(left, b.y + b.nameTop)
            part.author.nameLayout.draw(canvas)
            canvas.restore()
        }

        val reply = part.reply
        if (b.showReply && reply != null) {
            drawReply(canvas, reply, left, b.y + b.replyTop, if (part.bare) min(inner, reply.width) else inner, palette, style.dark)
        }

        val media = part.media
        if (media != null) {
            drawMedia(canvas, b, media)
        }

        val text = part.text
        if (text != null) {
            val x = left + if (part.rtl) inner - text.width else 0f
            drawText(canvas, part, text, x, b.y + b.textTop, palette)
        }

        val label = part.label
        if (label != null) {
            val top = b.y + b.labelTop
            val size = px(32f)
            rect.set(left, top + px(2f), left + size, top + px(2f) + size)
            fill.color = Theme.multAlpha(palette.link, 0.14f)
            canvas.drawRoundRect(rect, px(10f), px(10f), fill)
            drawGlyph(canvas, rect, part.wave, palette.link)
            labelPaint.color = palette.text
            canvas.save()
            canvas.translate(left + px(42f), top + (px(36f) - label.height) / 2f)
            label.draw(canvas)
            canvas.restore()
        }

        if (b.showTime) {
            drawTime(canvas, b, palette)
        }
    }

    private fun drawMedia(canvas: Canvas, b: Bubble, media: Media) {
        val part = b.part
        val receiver = media.receiver
        receiver.setImageCoords(b.x + b.mediaLeft, b.y + b.mediaTop, b.mediaWidth, b.mediaHeight)
        when {
            media.kind == KIND_ROUND -> receiver.setRoundRadius((b.mediaWidth / 2f).toInt())
            part.bare -> receiver.setRoundRadius(0)
            else -> {
                val big = px(13f).toInt()
                val small = px(4f).toInt()
                val onTop = b.mediaLeft > 0f && b.mediaTop <= b.mediaLeft + 0.5f
                val onBottom = part.text == null && part.label == null
                val topLeft = if (onTop && b.first) big else small
                val topRight = if (onTop) big else small
                val bottomRight = if (onBottom) big else small
                receiver.setRoundRadius(topLeft, topRight, bottomRight, small)
            }
        }
        receiver.draw(canvas)
    }

    private fun drawText(canvas: Canvas, part: Part, layout: StaticLayout, x: Float, y: Float, palette: Palette) {
        canvas.save()
        canvas.translate(x, y)
        val quotes = part.quotes
        if (quotes != null) {
            val bar = px(3f)
            val width = layout.width.toFloat()
            var i = 0
            while (i + 2 < quotes.size) {
                rect.set(0f, quotes[i], width, quotes[i + 1])
                fill.color = Theme.multAlpha(palette.link, 0.1f)
                canvas.drawRoundRect(rect, px(5f), px(5f), fill)
                if (quotes[i + 2] > 0f) {
                    rect.set(width - bar, quotes[i], width, quotes[i + 1])
                } else {
                    rect.set(0f, quotes[i], bar, quotes[i + 1])
                }
                fill.color = palette.link
                canvas.drawRoundRect(rect, bar / 2f, bar / 2f, fill)
                i += 3
            }
        }
        layout.paint.color = palette.text
        layout.paint.linkColor = palette.link
        layout.draw(canvas)
        AnimatedEmojiSpan.drawAnimatedEmojis(canvas, layout, emojiStack, 0f, null, 0f, 0f, 0f, 1f)
        val spoilers = part.spoilers
        if (spoilers != null) {
            spoilerPaint.color = ColorUtils.blendARGB(ColorUtils.setAlphaComponent(palette.bubble, 255), palette.text, 0.08f)
            canvas.drawPath(spoilers, spoilerPaint)
            val dots = part.spoilerDots
            if (dots != null) {
                canvas.save()
                canvas.clipPath(spoilers)
                dotPaint.color = Theme.multAlpha(palette.text, 0.55f)
                canvas.drawPoints(dots, dotPaint)
                canvas.restore()
            }
        }
        canvas.restore()
    }

    private fun drawReply(canvas: Canvas, reply: Reply, left: Float, top: Float, width: Float, palette: Palette, dark: Boolean) {
        val color = nameColor(reply.author, palette, dark)
        val radius = px(6f)
        rect.set(left, top, left + width, top + reply.height)
        canvas.save()
        replyClip.rewind()
        replyClip.addRoundRect(rect, radius, radius, Path.Direction.CW)
        canvas.clipPath(replyClip)
        fill.color = Theme.multAlpha(color, if (dark) 0.16f else 0.1f)
        canvas.drawRect(rect, fill)
        fill.color = color
        canvas.drawRect(left, top, left + px(3f), top + reply.height, fill)
        canvas.translate(left + px(9f), top + px(5f))
        replyNamePaint.color = color
        reply.name.draw(canvas)
        val text = reply.text
        if (text != null) {
            canvas.translate(0f, reply.name.height + px(1f))
            replyTextPaint.color = Theme.multAlpha(palette.text, 0.85f)
            replyTextPaint.linkColor = palette.link
            text.draw(canvas)
            AnimatedEmojiSpan.drawAnimatedEmojis(canvas, text, emojiStack, 0f, null, 0f, 0f, 0f, 1f)
        }
        canvas.restore()
    }

    private fun drawTime(canvas: Canvas, b: Bubble, palette: Palette) {
        val part = b.part
        if (b.timeOnMedia) {
            val right = b.x + b.mediaLeft + b.mediaWidth - px(6f)
            val bottom = b.y + b.mediaTop + b.mediaHeight - px(6f)
            rect.set(right - part.timeWidth - px(12f), bottom - px(19f), right, bottom)
            fill.color = if (part.bare) Theme.multAlpha(palette.bubble, 0.92f) else 0x66000000
            canvas.drawRoundRect(rect, px(9.5f), px(9.5f), fill)
            timePaint.color = if (part.bare) palette.secondary else Color.WHITE
            canvas.drawText(part.time, rect.left + px(6f), bottom - px(5.5f), timePaint)
        } else {
            timePaint.color = palette.secondary
            canvas.drawText(part.time, b.x + b.w - px(11f) - part.timeWidth, b.y + b.timeBaseline, timePaint)
        }
    }

    private fun drawGlyph(canvas: Canvas, box: RectF, wave: Boolean, color: Int) {
        val cx = box.centerX()
        val cy = box.centerY()
        if (wave) {
            fill.color = color
            val heights = floatArrayOf(0.35f, 0.8f, 0.55f, 0.95f, 0.45f)
            val bar = px(2.2f)
            val step = px(3.6f)
            val span = px(14f)
            var x = cx - step * 2 - bar / 2f
            for (height in heights) {
                val half = span * height / 2f
                rect.set(x, cy - half, x + bar, cy + half)
                canvas.drawRoundRect(rect, bar / 2f, bar / 2f, fill)
                x += step
            }
            return
        }
        val w = px(11f)
        val h = px(14f)
        val fold = px(4f)
        val l = cx - w / 2f
        val t = cy - h / 2f
        glyph.rewind()
        glyph.moveTo(l, t)
        glyph.lineTo(l + w - fold, t)
        glyph.lineTo(l + w, t + fold)
        glyph.lineTo(l + w, t + h)
        glyph.lineTo(l, t + h)
        glyph.close()
        glyph.moveTo(l + w - fold, t)
        glyph.lineTo(l + w - fold, t + fold)
        glyph.lineTo(l + w, t + fold)
        glyphPaint.color = color
        canvas.drawPath(glyph, glyphPaint)
    }

    private fun nameColor(author: Author, palette: Palette, dark: Boolean): Int {
        val id = author.colorId
        if (id in 0..6) {
            return palette.names[id]
        }
        val color = MessagesController.getInstance(account).peerColors?.getColor(id)?.getColor1(dark) ?: 0
        return if (color != 0) color else palette.names[abs(id) % 7]
    }

    private fun attach() {
        if (attached) {
            return
        }
        attached = true
        for (receiver in receivers) {
            receiver.onAttachedToWindow()
        }
        if (emojiLayouts.isNotEmpty()) {
            emojiStack = AnimatedEmojiSpan.update(
                AnimatedEmojiDrawable.CACHE_TYPE_MESSAGES, host, emojiStack, *emojiLayouts.toTypedArray()
            )
        }
    }

    private fun detach() {
        if (!attached) {
            return
        }
        attached = false
        for (receiver in receivers) {
            receiver.onDetachedFromWindow()
        }
        AnimatedEmojiSpan.release(host, emojiStack)
        emojiStack = null
    }

    private fun part(message: MessageObject): Part {
        val author = authorOf(message)
        val body = bodyOf(message)
        val big = body != null && message.isMediaEmpty() && message.emojiOnlyCount in 1..3
        val paint = if (big) emojiPaint else textPaint
        val text = body?.let { snug(restyle(it, paint), paint, maxText.toInt()) }
        val media = if (message.isMediaEmpty()) null else mediaOf(message)
        val label = if (!message.isMediaEmpty() && media == null) labelOf(message) else null
        val reply = replyOf(message)
        val time = LocaleController.getInstance().formatterDay.format(message.messageOwner.date * 1000L)
        val spoilers = text?.let { spoilersOf(it) }

        if (text != null) {
            emojiLayouts.add(text)
        }
        reply?.text?.let { emojiLayouts.add(it) }

        val lastLine = if (text != null) text.lineCount - 1 else 0
        return Part(
            author = author,
            authorKey = keyOf(author),
            text = text,
            rtl = text != null && text.getParagraphDirection(0) == Layout.DIR_RIGHT_TO_LEFT,
            lastLineWidth = text?.getLineWidth(lastLine) ?: 0f,
            lastLineRtl = text != null && text.getParagraphDirection(lastLine) == Layout.DIR_RIGHT_TO_LEFT,
            spoilers = spoilers,
            spoilerDots = spoilers?.let { dotsFor(it) },
            quotes = text?.let { quotesOf(it) },
            reply = reply,
            media = media,
            label = label,
            wave = message.isVoice() || message.isMusic(),
            time = time,
            timeWidth = timePaint.measureText(time)
        )
    }

    private fun keyOf(author: Author): String = if (author.id != 0L) "p${author.id}" else "n${author.name}"

    private fun bodyOf(message: MessageObject): CharSequence? {
        val raw = if (message.isMediaEmpty()) message.messageText else message.caption
        return if (raw.isNullOrBlank()) null else raw
    }

    private fun authorOf(message: MessageObject): Author {
        val forward = message.messageOwner?.fwd_from
        var id = 0L
        if (forward?.from_id != null) {
            id = DialogObject.getPeerDialogId(forward.from_id)
        }
        val hidden: String? = forward?.from_name
        if (id == 0L && hidden.isNullOrEmpty()) {
            id = message.getSenderId()
        }
        val key = if (id != 0L) "p$id" else "n$hidden"
        authors[key]?.let { return it }

        val controller = MessagesController.getInstance(account)
        var peer: TLObject? = null
        var title: String? = null
        var colorId = -1
        if (id > 0L) {
            val user = controller.getUser(id)
            if (user != null) {
                peer = user
                title = UserObject.getUserName(user)
                colorId = UserObject.getColorId(user)
            }
        } else if (id < 0L) {
            val chat = controller.getChat(-id)
            if (chat != null) {
                peer = chat
                title = chat.title
                colorId = ChatObject.getColorId(chat)
            }
        }
        val name: String = title?.takeIf { it.isNotEmpty() }
            ?: hidden?.takeIf { it.isNotEmpty() }
            ?: message.getForwardedName()?.takeIf { it.isNotEmpty() }
            ?: LocaleController.getString(R.string.HiddenName)
        if (colorId < 0) {
            colorId = (abs(if (id != 0L) id else name.hashCode().toLong()) % 7).toInt()
        }
        val nameText = TextUtils.ellipsize(
            Emoji.replaceEmoji(name, namePaint.fontMetricsInt, false), namePaint, maxText, TextUtils.TruncateAt.END
        )
        val author = Author(id, name, peer, colorId, layout(nameText, namePaint, maxText.toInt(), 1))
        authors[key] = author
        return author
    }

    private fun avatarOf(author: Author): ImageReceiver {
        author.avatar?.let { return it }
        val drawable = AvatarDrawable()
        val receiver = ImageReceiver(host)
        receiver.setCurrentAccount(account)
        receiver.setRoundRadius(FGAvatars.corners(AVATAR))
        val peer = author.peer
        if (peer != null) {
            drawable.setInfo(account, peer)
            receiver.setForUserOrChat(peer, drawable)
        } else {
            drawable.setInfo(author.id, author.name, null)
            receiver.setImageBitmap(drawable)
        }
        author.avatar = receiver
        receivers.add(receiver)
        if (attached) {
            receiver.onAttachedToWindow()
        }
        return receiver
    }

    private fun replyOf(message: MessageObject): Reply? {
        val source = message.replyMessageObject ?: return null
        if (source.messageOwner == null) {
            return null
        }
        val author = authorOf(source)
        val width = (maxText - px(14f)).toInt()
        val nameText = TextUtils.ellipsize(
            Emoji.replaceEmoji(author.name, replyNamePaint.fontMetricsInt, false),
            replyNamePaint, width.toFloat(), TextUtils.TruncateAt.END
        )
        val name = layout(nameText, replyNamePaint, width, 1)
        val raw = bodyOf(source) ?: source.messageText?.takeIf { it.isNotBlank() }
        val text = raw?.let {
            val line = TextUtils.ellipsize(
                firstLine(restyle(it, replyTextPaint)), replyTextPaint, width.toFloat(), TextUtils.TruncateAt.END
            )
            layout(line, replyTextPaint, width, 1)
        }
        val used = max(name.getLineWidth(0), text?.getLineWidth(0) ?: 0f) + px(14f)
        var height = px(10f) + name.height
        if (text != null) {
            height += text.height + px(1f)
        }
        return Reply(author, name, text, used, height)
    }

    private fun labelOf(message: MessageObject): StaticLayout? {
        val source = message.messageText
        if (source.isNullOrBlank()) {
            return null
        }
        val width = (maxText - px(42f)).toInt()
        val line = TextUtils.ellipsize(
            firstLine(restyle(source, labelPaint)), labelPaint, width.toFloat(), TextUtils.TruncateAt.END
        )
        return layout(line, labelPaint, width, 1)
    }

    private fun mediaOf(message: MessageObject): Media? {
        val owner = message.messageOwner ?: return null
        if (message.needDrawBluredPreview() || message.hasMediaSpoilers() || owner.media is TLRPC.TL_messageMediaPaidMedia) {
            return null
        }
        val receiver = ImageReceiver(host)
        receiver.setCurrentAccount(account)
        val media = when {
            message.isAnyKindOfSticker() -> {
                val document = message.getDocument() ?: return null
                val thumb = ImageLocation.getForDocument(FileLoader.getClosestPhotoSizeWithSize(document.thumbs, 320), document)
                when {
                    message.isAnimatedSticker() -> receiver.setImage(
                        ImageLocation.getForDocument(document), "160_160_firstframe", thumb, null, null, 0L, "tgs", message, 1
                    )
                    message.isVideoSticker() -> receiver.setImage(thumb, "160_160", null, null, 0L, null, message, 1)
                    else -> receiver.setImage(ImageLocation.getForDocument(document), "160_160", thumb, null, 0L, "webp", message, 1)
                }
                receiver.setAspectFit(true)
                Media(receiver, 1f, KIND_STICKER)
            }
            message.isRoundVideo() || message.isPhoto() || message.isVideo() || message.isGif() -> {
                val thumbs = message.photoThumbs ?: return null
                val big = FileLoader.getClosestPhotoSizeWithSize(thumbs, AndroidUtilities.getPhotoSize()) ?: return null
                val small = FileLoader.getClosestPhotoSizeWithSize(thumbs, 40)
                val round = message.isRoundVideo()
                val aspect = if (round || big.w <= 0 || big.h <= 0) 1f else (big.h.toFloat() / big.w).coerceIn(0.56f, 1.25f)
                val side = (maxText / AndroidUtilities.density).toInt()
                receiver.setImage(
                    ImageLocation.getForObject(big, message.photoThumbsObject), side.toString() + "_" + (side * aspect).toInt(),
                    ImageLocation.getForObject(small, message.photoThumbsObject), "b",
                    big.size.toLong(), null, message, if (message.shouldEncryptPhotoOrVideo()) 2 else 1
                )
                Media(receiver, aspect, if (round) KIND_ROUND else KIND_PHOTO)
            }
            else -> null
        }
        if (media != null) {
            receivers.add(receiver)
            if (attached) {
                receiver.onAttachedToWindow()
            }
        }
        return media
    }

    private fun restyle(source: CharSequence, paint: TextPaint): CharSequence {
        val cloned = AnimatedEmojiSpan.cloneSpans(source, AnimatedEmojiDrawable.CACHE_TYPE_MESSAGES, paint.fontMetricsInt)
        val text: Spannable = if (cloned !== source && cloned is Spannable) cloned else SpannableStringBuilder(source)
        for (span in text.getSpans(0, text.length, Emoji.EmojiSpan::class.java)) {
            text.removeSpan(span)
        }
        Emoji.replaceEmoji(text, paint.fontMetricsInt, false)
        return text
    }

    private fun firstLine(text: CharSequence): CharSequence {
        val end = TextUtils.indexOf(text, '\n')
        return if (end >= 0) text.subSequence(0, end) else text
    }

    private fun snug(text: CharSequence, paint: TextPaint, width: Int): StaticLayout {
        val wide = layout(text, paint, width)
        var used = 0f
        for (i in 0 until wide.lineCount) {
            used = max(used, wide.getLineWidth(i))
        }
        val tight = min(width, ceil(used).toInt() + 1)
        return if (tight < width) layout(text, paint, tight) else wide
    }

    private fun layout(text: CharSequence, paint: TextPaint, width: Int, lines: Int = Int.MAX_VALUE): StaticLayout =
        StaticLayout.Builder.obtain(text, 0, text.length, paint, max(1, width))
            .setAlignment(Layout.Alignment.ALIGN_NORMAL)
            .setIncludePad(false)
            .setBreakStrategy(Layout.BREAK_STRATEGY_HIGH_QUALITY)
            .setMaxLines(lines)
            .setEllipsize(TextUtils.TruncateAt.END)
            .build()

    private fun spoilersOf(layout: StaticLayout): Path? {
        val text = layout.text as? Spanned ?: return null
        val result = Path()
        var found = false
        for (span in text.getSpans(0, text.length, TextStyleSpan::class.java)) {
            if (!span.isSpoiler()) {
                continue
            }
            val start = text.getSpanStart(span)
            val end = text.getSpanEnd(span)
            if (start < 0 || end <= start) {
                continue
            }
            val region = Path()
            layout.getSelectionPath(start, end, region)
            result.addPath(region)
            found = true
        }
        return if (found) result else null
    }

    private fun dotsFor(path: Path): FloatArray {
        val bounds = RectF()
        path.computeBounds(bounds, true)
        val step = px(3.2f)
        val points = ArrayList<Float>()
        var seed = 7
        var y = bounds.top + step / 2f
        while (y < bounds.bottom && points.size < 2400) {
            var x = bounds.left + step / 2f
            while (x < bounds.right && points.size < 2400) {
                seed = seed * 1103515245 + 12345
                val dx = ((seed ushr 16) and 0xff) / 255f - 0.5f
                seed = seed * 1103515245 + 12345
                val dy = ((seed ushr 16) and 0xff) / 255f - 0.5f
                points.add(x + dx * step)
                points.add(y + dy * step)
                x += step
            }
            y += step
        }
        return points.toFloatArray()
    }

    private fun quotesOf(layout: StaticLayout): FloatArray? {
        val text = layout.text as? Spanned ?: return null
        val spans = text.getSpans(0, text.length, QuoteSpan::class.java)
        if (spans.isEmpty()) {
            return null
        }
        val out = FloatArray(spans.size * 3)
        for ((i, span) in spans.withIndex()) {
            val start = max(0, text.getSpanStart(span))
            val end = max(start, text.getSpanEnd(span) - 1)
            val first = layout.getLineForOffset(start)
            val last = layout.getLineForOffset(end)
            out[i * 3] = layout.getLineTop(first).toFloat()
            out[i * 3 + 1] = layout.getLineBottom(last).toFloat()
            out[i * 3 + 2] = if (layout.getParagraphDirection(first) == Layout.DIR_RIGHT_TO_LEFT) 1f else 0f
        }
        return out
    }

    private fun newPaint(size: Float, bold: Boolean) = TextPaint(Paint.ANTI_ALIAS_FLAG or Paint.SUBPIXEL_TEXT_FLAG).apply {
        textSize = px(size)
        if (bold) {
            typeface = AndroidUtilities.bold()
        }
    }

    private fun px(value: Float): Float = AndroidUtilities.dpf2(value)

    companion object {
        const val MAX_MESSAGES = 20
        const val BACKGROUND_WALLPAPER = 0
        const val BACKGROUND_PLAIN = 7
        const val BACKGROUND_NONE = 8
        const val BACKGROUND_COUNT = 9

        private const val TAG = "FGQuote"
        private const val AVATAR = 34f
        private const val HEADER = 38f
        private const val KIND_PHOTO = 0
        private const val KIND_STICKER = 1
        private const val KIND_ROUND = 2
        private const val STICKER_SIDE = 512f
        private const val MAX_PIXELS = 16_000_000f
        private const val MAX_SIDE = 8192f

        private val GRADIENTS = arrayOf(
            intArrayOf(0xFF356DFE.toInt(), 0xFF8146EE.toInt(), 0xFFC93FC0.toInt()),
            intArrayOf(0xFF12C2B0.toInt(), 0xFF2F80ED.toInt()),
            intArrayOf(0xFFFFA24C.toInt(), 0xFFF0567A.toInt()),
            intArrayOf(0xFFA6DD6A.toInt(), 0xFF2BA98A.toInt()),
            intArrayOf(0xFF17204A.toInt(), 0xFF5B3FC4.toInt(), 0xFFE0689A.toInt()),
            intArrayOf(0xFF4C525D.toInt(), 0xFF1B1E23.toInt())
        )

        private val LIGHT = Palette(
            bubble = 0xFFFFFFFF.toInt(),
            edge = 0x12000000,
            text = 0xFF0E1116.toInt(),
            secondary = 0xFF8C939D.toInt(),
            link = 0xFF2B7BE4.toInt(),
            names = intArrayOf(
                0xFFD54B47.toInt(), 0xFFD9792A.toInt(), 0xFF8B57D3.toInt(), 0xFF3C9F4A.toInt(),
                0xFF1B97A8.toInt(), 0xFF3483D6.toInt(), 0xFFD24F89.toInt()
            )
        )

        private val DARK = Palette(
            bubble = 0xFF202328.toInt(),
            edge = 0x1AFFFFFF,
            text = 0xFFF2F4F7.toInt(),
            secondary = 0xFF8D95A0.toInt(),
            link = 0xFF6BB3F7.toInt(),
            names = intArrayOf(
                0xFFFF7B72.toInt(), 0xFFFFA45E.toInt(), 0xFFB693F8.toInt(), 0xFF79D27A.toInt(),
                0xFF5CCFDD.toInt(), 0xFF6DB5FF.toInt(), 0xFFFF80B6.toInt()
            )
        )

        private val backgroundPaint = Paint(Paint.ANTI_ALIAS_FLAG or Paint.FILTER_BITMAP_FLAG)
        private val outlinePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = max(1f, AndroidUtilities.dpf2(1f))
        }
        private val swatchPath = Path()
        private val swatchRect = RectF()
        private val source = Rect()

        @JvmStatic
        fun prepare(messages: List<MessageObject?>?): ArrayList<MessageObject> {
            val seen = HashSet<String>()
            val result = ArrayList<MessageObject>()
            if (messages != null) {
                for (message in messages) {
                    if (message == null) {
                        continue
                    }
                    val owner = message.messageOwner ?: continue
                    if (owner.action != null || message.isSponsored()) {
                        continue
                    }
                    if (seen.add(message.getDialogId().toString() + "_" + message.getId())) {
                        result.add(message)
                    }
                }
            }
            result.sortWith(compareBy<MessageObject>({ it.messageOwner.date }, { it.getId() }))
            return result
        }

        @JvmStatic
        fun captureWallpaper(provider: Theme.ResourcesProvider?): Bitmap? {
            val drawable = (provider as? MessagePreviewView.ResourcesDelegate)?.wallpaperDrawable
                ?: Theme.getCachedWallpaperNonBlocking()
                ?: return null
            val screenWidth = AndroidUtilities.displaySize.x
            val screenHeight = AndroidUtilities.displaySize.y
            if (screenWidth <= 0 || screenHeight <= 0) {
                return null
            }
            val scale = min(1f, 720f / screenWidth)
            val width = max(1, (screenWidth * scale).roundToInt())
            val height = max(1, (screenHeight * scale).roundToInt())
            val bounds = drawable.copyBounds()
            return try {
                val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
                drawable.setBounds(0, 0, width, height)
                drawable.draw(Canvas(bitmap))
                bitmap
            } catch (e: Throwable) {
                FinegramLogger.e(TAG, { "обои чата не снялись" }, e)
                null
            } finally {
                drawable.bounds = bounds
            }
        }

        @JvmStatic
        fun drawSwatch(canvas: Canvas, index: Int, cx: Float, cy: Float, radius: Float, dark: Boolean, wallpaper: Bitmap?) {
            if (radius <= 0f) {
                return
            }
            swatchRect.set(cx - radius, cy - radius, cx + radius, cy + radius)
            swatchPath.rewind()
            swatchPath.addCircle(cx, cy, radius, Path.Direction.CW)
            canvas.save()
            canvas.clipPath(swatchPath)
            if (index == BACKGROUND_NONE) {
                backgroundPaint.shader = null
                backgroundPaint.color = if (dark) 0xFF2B2E34.toInt() else 0xFFFFFFFF.toInt()
                canvas.drawRect(swatchRect, backgroundPaint)
                backgroundPaint.color = if (dark) 0xFF1D1F23.toInt() else 0xFFDCE0E6.toInt()
                val cell = radius / 2f
                var row = 0
                var y = swatchRect.top
                while (y < swatchRect.bottom) {
                    var x = swatchRect.left + (row % 2) * cell
                    while (x < swatchRect.right) {
                        canvas.drawRect(x, y, x + cell, y + cell, backgroundPaint)
                        x += cell * 2
                    }
                    y += cell
                    row++
                }
            } else {
                drawBackground(canvas, index, swatchRect, dark, wallpaper)
            }
            canvas.restore()
            outlinePaint.color = if (dark) 0x24FFFFFF else 0x1A000000
            canvas.drawCircle(cx, cy, radius - outlinePaint.strokeWidth / 2f, outlinePaint)
        }

        private fun drawBackground(canvas: Canvas, index: Int, rect: RectF, dark: Boolean, wallpaper: Bitmap?) {
            if (rect.width() <= 0f || rect.height() <= 0f) {
                return
            }
            backgroundPaint.shader = null
            backgroundPaint.color = Color.BLACK
            if (index == BACKGROUND_WALLPAPER && wallpaper != null && !wallpaper.isRecycled) {
                val scale = max(rect.width() / wallpaper.width, rect.height() / wallpaper.height)
                val w = rect.width() / scale
                val h = rect.height() / scale
                val left = (wallpaper.width - w) / 2f
                val top = (wallpaper.height - h) / 2f
                source.set(left.toInt(), top.toInt(), (left + w).toInt(), (top + h).toInt())
                canvas.drawBitmap(wallpaper, source, rect, backgroundPaint)
                return
            }
            if (index in 1..GRADIENTS.size) {
                backgroundPaint.shader = LinearGradient(
                    rect.left, rect.top, rect.right, rect.bottom, GRADIENTS[index - 1], null, Shader.TileMode.CLAMP
                )
                canvas.drawRect(rect, backgroundPaint)
                backgroundPaint.shader = RadialGradient(
                    rect.left + rect.width() * 0.2f, rect.top, max(rect.width(), rect.height()) * 0.9f,
                    intArrayOf(0x40FFFFFF, 0x00FFFFFF), null, Shader.TileMode.CLAMP
                )
                canvas.drawRect(rect, backgroundPaint)
                backgroundPaint.shader = null
                if (dark) {
                    backgroundPaint.color = 0x33000000
                    canvas.drawRect(rect, backgroundPaint)
                }
                return
            }
            if (index == BACKGROUND_NONE) {
                return
            }
            backgroundPaint.color = if (dark) 0xFF131518.toInt() else 0xFFEEF1F5.toInt()
            canvas.drawRect(rect, backgroundPaint)
        }
    }
}

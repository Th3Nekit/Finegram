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
import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.ImageReceiver
import org.telegram.messenger.MessageObject
import org.telegram.messenger.MessagesController
import org.telegram.messenger.UserObject
import org.telegram.tgnet.ConnectionsManager
import org.telegram.tgnet.TLRPC
import org.telegram.ui.ActionBar.Theme

object FGOnlineDot {

    private val dotPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG)

    @JvmStatic
    fun draw(canvas: Canvas, avatar: ImageReceiver?, message: MessageObject?, account: Int) {
        if (!FinegramAppearanceConfig.groupOnlineIndicator || avatar == null || message == null) {
            return
        }
        if (!isSenderOnline(message, account)) {
            return
        }

        val alpha = avatar.alpha
        if (alpha <= 0f) {
            return
        }

        val radius = AndroidUtilities.dp(3.5f).toFloat()
        val border = AndroidUtilities.dp(1.5f).toFloat()
        val centerX = avatar.imageX2 - radius
        val centerY = avatar.imageY2 - radius

        borderPaint.color = Theme.getColor(Theme.key_windowBackgroundWhite)
        borderPaint.alpha = (255 * alpha).toInt().coerceIn(0, 255)
        canvas.drawCircle(centerX, centerY, radius + border, borderPaint)

        dotPaint.color = Theme.getColor(Theme.key_chats_onlineCircle)
        dotPaint.alpha = (255 * alpha).toInt().coerceIn(0, 255)
        canvas.drawCircle(centerX, centerY, radius, dotPaint)
    }

    private fun isSenderOnline(message: MessageObject, account: Int): Boolean {
        val senderId = try {
            message.senderId
        } catch (e: Throwable) {
            return false
        }

        if (senderId <= 0) {
            return false
        }
        val user = try {
            MessagesController.getInstance(account).getUser(senderId)
        } catch (e: Throwable) {
            null
        } ?: return false

        if (user.bot || user.self || UserObject.isDeleted(user)) {
            return false
        }
        val status = user.status ?: return false
        if (status is TLRPC.TL_userStatusOnline) {
            return status.expires > ConnectionsManager.getInstance(account).currentTime
        }
        return false
    }
}

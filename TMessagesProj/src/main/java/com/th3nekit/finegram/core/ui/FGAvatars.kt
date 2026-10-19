/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.ui

import org.telegram.messenger.AndroidUtilities
import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig

/**
 * Форма аватарок.
 *
 * Настройка задаёт скругление для аватарки размером 56 dp, а для остальных размеров радиус
 * пересчитывается пропорционально — иначе на мелких аватарках угол выглядел бы круглее,
 * чем на крупных. Значение [MAX] означает круг, ноль — квадрат.
 */
object FGAvatars {

    const val MAX = 30f

    /**
     * Скругление для аватарки чата. У форумов Telegram рисует аватарки заметно более
     * квадратными, поэтому по умолчанию радиус для них меньше; настройка «как у чатов»
     * убирает это отличие.
     */
    @JvmStatic
    @JvmOverloads
    fun cornersForChat(size: Float, forum: Boolean, inPixels: Boolean = false): Int {
        val effective = if (forum && !FinegramAppearanceConfig.forumAvatarsLikeChats) size * 0.65f else size
        return corners(effective, inPixels)
    }

    @JvmStatic
    @JvmOverloads
    fun corners(size: Float, inPixels: Boolean = false): Int {
        val corners = FinegramAppearanceConfig.avatarCorners
        if (corners <= 0f) {
            return 0
        }
        val density = if (inPixels) 1f else AndroidUtilities.density
        return (corners * (size / 56f) * density).toInt()
    }
}

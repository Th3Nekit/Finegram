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

object FGAvatars {

    const val MAX = 30f

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

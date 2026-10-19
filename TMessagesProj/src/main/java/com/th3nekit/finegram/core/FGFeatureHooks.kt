/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core

import com.th3nekit.finegram.core.configs.FinegramChatsConfig

// I've created this so CG features can be injected in a source file with 1 line only (maybe)
// Because manual editing of drklo's sources harms your mental health.
object FGFeatureHooks {

    fun switchNoAuthor(b: Boolean) {
        FinegramChatsConfig.noAuthorship = b
    }

    fun switchNoCaptions(b: Boolean) {
        FinegramChatsConfig.noCaptions = b
    }

}
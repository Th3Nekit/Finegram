/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.icons.icon_replaces

abstract class BaseIconReplace {
    abstract val replaces: HashMap<Int, Int>

    fun wrap(id: Int): Int {
        return replaces[id] ?: id
    }

}
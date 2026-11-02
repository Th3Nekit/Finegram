/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.icons

fun newHashMap(vararg intPairs: Pair<Int, Int>) = HashMap<Int, Int>().apply {
    intPairs.forEach {
        this[it.first] = it.second
    }
}
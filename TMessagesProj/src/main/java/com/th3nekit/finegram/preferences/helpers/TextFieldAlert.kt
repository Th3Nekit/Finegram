/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences.helpers

object TextFieldAlert {

    fun removeNonNumericChars(input: String, allowMinus: Boolean): String {
        return if (allowMinus) {
            input.replace(Regex("[^0-9-]"), "")
        } else {
            input.replace(Regex("[^0-9]"), "")
        }
    }

}
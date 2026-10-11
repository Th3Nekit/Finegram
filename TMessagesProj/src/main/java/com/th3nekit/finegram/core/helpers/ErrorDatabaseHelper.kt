/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.helpers

import android.widget.Toast
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.ApplicationLoader
import org.telegram.tgnet.TLObject

object ErrorDatabaseHelper {

    private fun getMethodName(method: TLObject): String {
        val name = method.toString()
        val start = name.indexOf("$") + 4
        val end = name.indexOf("@")
        return name.substring(start, end).replace("_", ".")
    }

    fun showErrorToast(method: TLObject, text: String) {
        if (text == "FILE_REFERENCE_EXPIRED") {
            return
        }
        AndroidUtilities.runOnUIThread {
            Toast.makeText(
                ApplicationLoader.applicationContext,
                getMethodName(method) + ": " + text,
                Toast.LENGTH_SHORT
            ).show()
        }
    }

}

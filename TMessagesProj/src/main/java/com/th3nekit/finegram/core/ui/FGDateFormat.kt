/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.ui

import org.telegram.messenger.LocaleController
import com.th3nekit.finegram.core.configs.FinegramChatsConfig
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object FGDateFormat {

    @JvmStatic
    fun appendWeekday(formatted: String?, dateMillis: Long): String? {
        if (formatted == null || !FinegramChatsConfig.weekdayNearDate) {
            return formatted
        }
        return try {
            val locale = LocaleController.getInstance().currentLocale ?: Locale.getDefault()
            val weekday = SimpleDateFormat("EEEE", locale).format(Date(dateMillis))
            if (weekday.isNullOrEmpty()) {
                return formatted
            }
            val suffix = ", " + weekday.replaceFirstChar { it.uppercase(locale) }
            if (formatted.endsWith(suffix)) formatted else formatted + suffix
        } catch (e: Throwable) {
            formatted
        }
    }
}

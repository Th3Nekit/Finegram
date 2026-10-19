package com.th3nekit.finegram.helpers

import android.content.Context
import android.text.TextUtils

import org.telegram.messenger.LocaleController
import org.telegram.messenger.browser.Browser

import com.th3nekit.finegram.core.configs.FinegramCoreConfig

/**
 * Покупка Premium и звёзд, когда оплата через Google недоступна.
 *
 * В сборках вне Google Play магазин молчит, и Telegram просто прячет кнопки покупки —
 * купить нельзя ничего. Оплата через бота Telegram и через сторонний сайт при этом
 * работает, поэтому кнопки остаются на месте и ведут туда.
 *
 * Идея Cherrygram. Партнёрскую метку из их ссылки мы не тащим: ссылка ведёт на тот же
 * сайт напрямую и её можно поменять в настройках.
 */
object FGSafeStars {

    /** Языки, для которых у сайта есть русская версия. */
    private val CYRILLIC = setOf("ru", "uk", "be", "bg", "sr", "kk", "ky", "tg", "uz")

    @JvmStatic
    fun open(context: Context?, premium: Boolean, userName: String?) {
        if (context == null) return

        val lang = try {
            LocaleController.getInstance().currentLocaleInfo.shortName
        } catch (e: Throwable) {
            "en"
        }
        val base = if (CYRILLIC.contains(lang)) {
            FinegramCoreConfig.safeStarsUrlRu
        } else {
            FinegramCoreConfig.safeStarsUrl
        }

        val url = StringBuilder(base)
        if (premium) {
            url.append(if (base.contains('?')) "&" else "?").append("premium")
            if (!TextUtils.isEmpty(userName)) {
                url.append("&username=").append(userName)
            }
        }
        Browser.openUrl(context, url.toString())
    }
}

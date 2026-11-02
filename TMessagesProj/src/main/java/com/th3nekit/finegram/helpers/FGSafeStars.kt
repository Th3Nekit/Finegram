package com.th3nekit.finegram.helpers

import android.content.Context
import android.text.TextUtils

import org.telegram.messenger.LocaleController
import org.telegram.messenger.browser.Browser

import com.th3nekit.finegram.core.configs.FinegramCoreConfig

object FGSafeStars {

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

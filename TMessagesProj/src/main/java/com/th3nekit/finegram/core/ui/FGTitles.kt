/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.ui

import org.telegram.messenger.ApplicationLoader
import org.telegram.messenger.R
import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig

object FGTitles {

    @JvmStatic
    fun dialogsTitle(): CharSequence {
        val custom = FinegramAppearanceConfig.customTitleText.trim()
        if (FinegramAppearanceConfig.customTitleEnabled && custom.isNotEmpty()) {
            return custom
        }
        return appName()
    }

    @Volatile
    private var cachedAppName: String? = null

    @JvmStatic
    fun appName(): String {
        cachedAppName?.let { return it }
        val name = try {
            ApplicationLoader.applicationContext.resources.getString(R.string.FG_AppName)
        } catch (e: Throwable) {
            "Finegram"
        }
        cachedAppName = name
        return name
    }
}

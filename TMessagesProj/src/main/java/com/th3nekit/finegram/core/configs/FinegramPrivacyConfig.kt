/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.configs

import android.app.Activity
import android.content.SharedPreferences
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import org.telegram.messenger.ApplicationLoader
import com.th3nekit.finegram.preferences.boolean
import com.th3nekit.finegram.preferences.int

object FinegramPrivacyConfig: CoroutineScope by CoroutineScope(
    context = SupervisorJob() + Dispatchers.Default
) {

    private val sharedPreferences: SharedPreferences = ApplicationLoader.applicationContext.getSharedPreferences("mainconfig", Activity.MODE_PRIVATE)

    var ghostMode by sharedPreferences.boolean("FG_GhostMode", false)

    var ghostHideOnline by sharedPreferences.boolean("FG_GhostHideOnline", true)

    var ghostHideTyping by sharedPreferences.boolean("FG_GhostHideTyping", true)

    var ghostHideReading by sharedPreferences.boolean("FG_GhostHideReading", true)

    var ghostHideStoryViews by sharedPreferences.boolean("FG_GhostHideStoryViews", true)

    var ghostOfflineAfterSend by sharedPreferences.boolean("FG_GhostOfflineAfterSend", false)

    var saveDeleted by sharedPreferences.boolean("FG_SaveDeleted", false)

    var saveDeletedFromBots by sharedPreferences.boolean("FG_SaveDeletedFromBots", false)

    var saveDeletedFromChannels by sharedPreferences.boolean("FG_SaveDeletedFromChannels", false)

    var hideProxySponsor by sharedPreferences.boolean("SP_NoProxySponsor", true)

    var lockedChatsRememberSeconds by sharedPreferences.int("FG_LockedChatsRemember", 0)

    var hideArchiveFromChatsList by sharedPreferences.boolean("SP_HideArchiveFromChatsList", false)
    var askBiometricsToOpenArchive by sharedPreferences.boolean("SP_AskBiometricsToOpenArchive", false)
    var askBiometricsToOpenEncrypted by sharedPreferences.boolean("SP_AskBiometricsToOpenEncrypted", false)
    var askBiometricsToOpenChat by sharedPreferences.boolean("SP_AskBiometricsToOpenChat", false)
    var askPasscodeBeforeDelete by sharedPreferences.boolean("SP_AskPinBeforeDelete", false)
    var allowSystemPasscode by sharedPreferences.boolean("SP_AllowSystemPasscode", false)

    var hideArchivedStories by sharedPreferences.boolean("CP_HideArchivedStories", false)

    var localPremium by sharedPreferences.boolean("FG_LocalPremium", false)

    var allowScreenshots by sharedPreferences.boolean("FG_AllowScreenshots", false)

    var dimDeleted by sharedPreferences.boolean("FG_DimDeleted", false)

    var pseudoForward by sharedPreferences.boolean("FG_PseudoForward", false)

    var keepEdits by sharedPreferences.boolean("FG_KeepEdits", false)

}
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
import android.os.Build
import androidx.annotation.Keep
import androidx.core.content.edit
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import org.telegram.messenger.ApplicationLoader
import org.telegram.messenger.MessagesController
import org.telegram.messenger.UserConfig
import com.th3nekit.finegram.core.helpers.PushHealth
import com.th3nekit.finegram.preferences.boolean
import com.th3nekit.finegram.preferences.int
import com.th3nekit.finegram.preferences.string

@Keep
object FinegramCoreConfig: CoroutineScope by CoroutineScope(
    context = SupervisorJob() + Dispatchers.Default
) {

    private val sharedPreferences: SharedPreferences = ApplicationLoader.applicationContext.getSharedPreferences("mainconfig", Activity.MODE_PRIVATE)

    fun putBoolean(key: String, value: Boolean) {
        val preferences = ApplicationLoader.applicationContext.getSharedPreferences("mainconfig", Activity.MODE_PRIVATE)
        preferences.edit {
            putBoolean(key, value)
        }
    }

    fun putStringForUserPrefs(key: String, value: String) {
        val preferences = MessagesController.getMainSettings(UserConfig.selectedAccount)
        preferences.edit {
            putString(key, value)
        }
    }

    const val ANIMATION_SPRING = 0
    const val ANIMATION_CLASSIC = 1
    var springAnimation by sharedPreferences.int("FG_SpringAnimation", ANIMATION_SPRING)

    var actionbarCrossfade by sharedPreferences.boolean("FG_ActionbarCrossfade", true)
    var predictiveBack by sharedPreferences.boolean("FG_PredictiveBack", false)

    var silenceNonContacts by sharedPreferences.boolean("CP_SilenceNonContacts", false)
    var ignoreMentions by sharedPreferences.boolean("FG_IgnoreMentions", false)
    var ignoreMentionsMarkAsRead by sharedPreferences.boolean("FG_IgnoreMentionsMarkAsRead", false)
    var oldNotificationIcon by sharedPreferences.boolean("AP_Old_Notification_Icon", false)

    var notificationChatAvatar by sharedPreferences.boolean("FG_NotificationChatAvatar", true)
    var residentNotification by sharedPreferences.boolean("FG_ResidentNotification",
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && pushesUnavailable())

    @JvmStatic
    fun pushesUnavailable(): Boolean = !ApplicationLoader.checkPlayServices() || !hasFirebaseConfig() || PushHealth.firebaseSilent()

    private const val KEY_RESIDENT_BY_USER = "FG_ResidentNotificationByUser"

    @JvmStatic
    fun residentChosenByUser(): Boolean = sharedPreferences.getBoolean(KEY_RESIDENT_BY_USER, false)

    @JvmStatic
    fun setResidentByUser(value: Boolean) {
        residentNotification = value
        sharedPreferences.edit { putBoolean(KEY_RESIDENT_BY_USER, true) }
    }

    private fun hasFirebaseConfig(): Boolean = try {
        val context = ApplicationLoader.applicationContext
        context.resources.getIdentifier("google_app_id", "string", context.packageName) != 0
    } catch (e: Throwable) {
        false
    }

    var hideStories by sharedPreferences.boolean("CP_HideStories", false)
    var archiveStoriesFromUsers by sharedPreferences.boolean("CP_ArchiveStoriesFromUsers", false)
    var archiveStoriesFromChannels by sharedPreferences.boolean("CP_ArchiveStoriesFromChannels", false)

    var noRounding by sharedPreferences.boolean("CP_NoRounding1", true)
    var systemEmoji by sharedPreferences.boolean("AP_SystemEmoji", false)
    var systemFonts by sharedPreferences.boolean("AP_SystemFonts", true)

    const val EDGE_MODE_ENABLE = 0
    const val EDGE_MODE_DISABLE = 1
    const val EDGE_MODE_AUTO = 2
    var edgeToEdgeMode by sharedPreferences.int("CP_EdgeToEdge", EDGE_MODE_AUTO)

    const val TABLET_MODE_ENABLE = 0
    const val TABLET_MODE_DISABLE = 1
    const val TABLET_MODE_AUTO = 2
    var tabletMode by sharedPreferences.int("AP_Tablet_Mode", TABLET_MODE_AUTO)

    const val BOOST_NONE = 0
    const val BOOST_AVERAGE = 1
    const val BOOST_EXTREME = 2
    var downloadSpeedBoost by sharedPreferences.int("EP_DownloadSpeedBoost", BOOST_NONE)

    var uploadSpeedBoost by sharedPreferences.boolean("EP_UploadSpeedBoost", false)
    var slowNetworkMode by sharedPreferences.boolean("EP_SlowNetworkMode", false)

    var installBetas by sharedPreferences.boolean("FG_Install_Beta_Ver", isStandaloneBetaBuild())

    const val PLUGIN_UPDATES_MANUAL = 0
    const val PLUGIN_UPDATES_ASK = 1
    const val PLUGIN_UPDATES_SILENT = 2
    var pluginAutoUpdate by sharedPreferences.int("FG_PluginAutoUpdate", PLUGIN_UPDATES_SILENT)

    var storeSort by sharedPreferences.int("FG_StoreSort", 0)
    var autoOTA by sharedPreferences.boolean("FG_Check_Auto_OTA", isStandaloneStableBuild() || isStandaloneBetaBuild() || isDevBuild())
    var forceFound by sharedPreferences.boolean("FG_ForceFound", false)
    var minFinegramVersion by sharedPreferences.string("FG_Min_Finegram_Version", "0")

    var sleepTimer by sharedPreferences.boolean("FG_Sleep_Timer", false)

    @JvmStatic
    fun isStandaloneStableBuild(): Boolean {
        return ApplicationLoader.isStandaloneBuild() && !isDevBuild() && !isStandalonePremiumBuild() && !isStandaloneBetaBuild()
    }

    @JvmStatic
    fun isStandaloneBetaBuild(): Boolean {
        return false
    }

    @JvmStatic
    fun isDevBuild(): Boolean {
        return false
    }

    @JvmStatic
    fun isStandalonePremiumBuild(): Boolean {
        return false
    }

    @JvmStatic
    fun isPlayStoreBuild(): Boolean {
        return !ApplicationLoader.isStandaloneBuild()
    }

    private fun migratePreferences() {
        if (FinegramAppearanceConfig.showIDDC_old >= FinegramAppearanceConfig.ID_DC) {
            FinegramAppearanceConfig.showIDDC_old = FinegramAppearanceConfig.ID_DC_NONE
            FinegramAppearanceConfig.showIDDC = true
        }

        if (!sharedPreferences.getBoolean(KEY_RESIDENT_FIXED, false)) {
            sharedPreferences.edit().putBoolean(KEY_RESIDENT_FIXED, true).apply()
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && pushesUnavailable()) {
                residentNotification = true
            }
        }
    }

    private const val KEY_RESIDENT_FIXED = "FG_ResidentNotificationFixed"

    fun init() {
        launch {
            initAsync()
        }
    }

    private suspend fun initAsync() {
        migratePreferences()
    }

    var allowSafeStars by sharedPreferences.boolean("FG_AllowSafeStars", true)

    var notificationReactions by sharedPreferences.boolean("FG_NotificationReactions", true)

    @JvmStatic
    fun notificationReaction(account: Int): String =
        sharedPreferences.getString("FG_NotificationReaction_" + account, "") ?: ""

    @JvmStatic
    fun setNotificationReaction(account: Int, reaction: String?, baseEmoji: String?) {
        sharedPreferences.edit()
            .putString("FG_NotificationReaction_" + account, reaction ?: "")
            .putString("FG_NotificationReactionEmoji_" + account, baseEmoji ?: "")
            .apply()
    }

    @JvmStatic
    fun notificationReactionEmoji(account: Int): String =
        sharedPreferences.getString("FG_NotificationReactionEmoji_" + account, "") ?: ""

    var safeStarsUrl by sharedPreferences.string("FG_SafeStarsURL", "https://safe-stars.com/")
    var safeStarsUrlRu by sharedPreferences.string("FG_SafeStarsURL_RU", "https://safe-stars.com/ru/")

    @JvmStatic
    fun safeStarsAllowed(): Boolean = try {
        allowSafeStars && !org.telegram.tgnet.ConnectionsManager.getInstance(
            org.telegram.messenger.UserConfig.selectedAccount
        ).isTestBackend
    } catch (e: Throwable) {
        allowSafeStars
    }
}

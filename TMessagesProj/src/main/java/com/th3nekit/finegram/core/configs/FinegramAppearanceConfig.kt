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
import org.telegram.messenger.ApplicationLoader
import org.telegram.messenger.SharedConfig
import com.th3nekit.finegram.core.icons.icon_replaces.BaseIconReplace
import com.th3nekit.finegram.core.icons.icon_replaces.NoIconReplace
import com.th3nekit.finegram.core.icons.icon_replaces.SolarIconReplace
import com.th3nekit.finegram.preferences.boolean
import com.th3nekit.finegram.preferences.float
import com.th3nekit.finegram.preferences.int
import com.th3nekit.finegram.preferences.string

object FinegramAppearanceConfig {

    private val sharedPreferences: SharedPreferences = ApplicationLoader.applicationContext.getSharedPreferences("mainconfig", Activity.MODE_PRIVATE)

    var centerTitle by sharedPreferences.boolean("AP_CenterTitle", true)
    var hideSearchFiled by sharedPreferences.boolean("AP_HideSearchField", false)

    var sideDrawer by sharedPreferences.boolean("FG_NavigationDrawer", true)

    var bottomTabsWithDrawer by sharedPreferences.boolean("FG_BottomTabsWithDrawer", false)

    fun bottomTabsVisible(): Boolean = showMainTabs && (!sideDrawer || bottomTabsWithDrawer)

    var sideDrawerRounded by sharedPreferences.boolean("FG_SideDrawerRounded", true)

    var sideDrawerPhone by sharedPreferences.boolean("FG_SideDrawerPhone", true)
    var drawSnowInActionBar by sharedPreferences.boolean("AP_DrawSnowInActionBar", false)

    const val ICON_REPLACE_NONE = 0
    const val ICON_REPLACE_SOLAR = 1

    var iconReplacement by sharedPreferences.int("AP_Icon_Replacements1", ICON_REPLACE_SOLAR)
    fun getCurrentIconPack(): BaseIconReplace {
        return when (iconReplacement) {
            ICON_REPLACE_SOLAR -> SolarIconReplace()
            else -> NoIconReplace()
        }
    }

    var activeIconPackId by sharedPreferences.string("FG_ActiveIconPack", "")

    var composerEffects by sharedPreferences.boolean("FG_ComposerEffects", false)
    var composerCursorGlide by sharedPreferences.boolean("FG_ComposerCursorGlide", false)
    var composerAppear by sharedPreferences.boolean("FG_ComposerAppear", false)

    var composerAppearDuration by sharedPreferences.int("FG_ComposerAppearDuration", 300)

    var composerAppearSlide by sharedPreferences.int("FG_ComposerAppearSlide", 20)

    var composerAppearBlur by sharedPreferences.int("FG_ComposerAppearBlur", 10)

    var composerParticleCount by sharedPreferences.int("FG_ComposerParticles", 5)

    var composerParticleSpeed by sharedPreferences.int("FG_ComposerParticleSpeed", 100)

    var composerParticleSize by sharedPreferences.int("FG_ComposerParticleSize", 100)

    var composerCursorSpeed by sharedPreferences.int("FG_ComposerCursorSpeed", 25)

    var groupOnlineIndicator by sharedPreferences.boolean("FG_GroupOnlineIndicator", false)

    var mediaGlow by sharedPreferences.boolean("FG_MediaGlow", false)

    var strongerBlur by sharedPreferences.boolean("FG_StrongerBlur", false)

    var customTitleEnabled by sharedPreferences.boolean("FG_CustomTitleEnabled", false)
    var customTitleText by sharedPreferences.string("FG_CustomTitleText", "")

    var forumAvatarsLikeChats by sharedPreferences.boolean("FG_ForumAvatarsLikeChats", false)

    var avatarCorners by sharedPreferences.float("FG_AvatarCorners", 30f)

    const val SWITCH_TELEGRAM = 0
    const val SWITCH_ONE_UI = 1
    const val SWITCH_MATERIAL3 = 2

    var switchStyle by sharedPreferences.int("AP_SwitchStyle", SWITCH_MATERIAL3)

    var iosChatList by sharedPreferences.boolean("FG_IosChatList", false)

    var disableDividers by sharedPreferences.boolean("AP_DisableDividers", true)

    var tabsHideAllChats by sharedPreferences.boolean("CP_NewTabs_RemoveAllChats", false)
    var tabsNoUnread by sharedPreferences.boolean("CP_NewTabs_NoCounter", false)
    var tabsIncludeMutedInCounter by sharedPreferences.boolean("CP_TabsIncludeMutedInCounter", true)
    var folderCornerBadges by sharedPreferences.boolean("FG_FolderCornerBadges", false)

    const val TAB_TYPE_MIX = 0
    const val TAB_TYPE_TEXT = 1
    const val TAB_TYPE_ICON = 2
    var tabMode by sharedPreferences.int("AP_TabMode", TAB_TYPE_MIX)

    var tabStyleStroke by sharedPreferences.boolean("AP_TabStyleAddStroke", false)
    var folderNameInHeader by sharedPreferences.boolean("AP_FolderNameInHeader", false)
    var foldersAtBottom by sharedPreferences.boolean("AP_FoldersAtBottom", false)

    var showMainTabs by sharedPreferences.boolean("AP_ShowMainTabs", true)
    var openSettingsBySwipe by sharedPreferences.boolean("AP_OpenSettingsBySwipe", false)

    const val MAIN_TABS_ORDER_BEFORE = "SETTINGS,CHATS,!PROFILE,!CONTACTS,!CALLS,SEARCH"

    const val MAIN_TABS_ORDER_DEFAULT = "PROFILE,CHATS,SETTINGS,!CONTACTS,!CALLS,!SEARCH"

    var mainTabsOrder by sharedPreferences.string("AP_MainTabsPosition_New", MAIN_TABS_ORDER_DEFAULT)
    private var showSearchInTabsSetting by sharedPreferences.boolean("AP_ShowSearchInTabs_New", true)

    var showSearchInTabs: Boolean
        get() = showSearchInTabsSetting && hideSearchFiled
        set(value) {
            showSearchInTabsSetting = value
        }
    var showMainTabsTitle by sharedPreferences.boolean("AP_ShowMainTabsTitle", true)
    var mainTabsForceOpenChats by sharedPreferences.boolean("AP_MainTabsForceOpenChats", false)

    var showSeconds by sharedPreferences.boolean("CP_ShowSeconds", false)
    var disablePremiumStatuses by sharedPreferences.boolean("CP_DisablePremiumStatuses", SharedConfig.getDevicePerformanceClass() == SharedConfig.PERFORMANCE_CLASS_LOW)
    var replyBackground by sharedPreferences.boolean("CP_ReplyBackground", SharedConfig.getDevicePerformanceClass() >= SharedConfig.PERFORMANCE_CLASS_AVERAGE)
    var replyCustomColors by sharedPreferences.boolean("CP_ReplyCustomColors", SharedConfig.getDevicePerformanceClass() >= SharedConfig.PERFORMANCE_CLASS_AVERAGE)
    var replyBackgroundEmoji by sharedPreferences.boolean("CP_ReplyBackgroundEmoji", SharedConfig.getDevicePerformanceClass() >= SharedConfig.PERFORMANCE_CLASS_AVERAGE)
    var profileHidePhoneNumber by sharedPreferences.boolean("CP_ProfileHidePhoneNumber", true)
    var profileChannelPreview by sharedPreferences.boolean("CP_ProfileChannelPreview", true)

    const val ID_DC_NONE = 0
    const val ID_DC = 1

    var showIDDC_old by sharedPreferences.int("AP_ShowID_DC", ID_DC_NONE)
    var showIDDC by sharedPreferences.boolean("AP_ShowID_DC_new", false)

    var profileBirthDatePreview by sharedPreferences.boolean("CP_ProfileBirthDatePreview", true)
    var profileBusinessPreview by sharedPreferences.boolean("CP_ProfileBusinessPreview", true)
    var profileBackgroundColor by sharedPreferences.boolean("CP_ProfileBackgroundColor", SharedConfig.getDevicePerformanceClass() >= SharedConfig.PERFORMANCE_CLASS_AVERAGE)
    var profileBackgroundEmoji by sharedPreferences.boolean("CP_ProfileBackgroundEmoji", SharedConfig.getDevicePerformanceClass() >= SharedConfig.PERFORMANCE_CLASS_AVERAGE)

    var showAccounts by sharedPreferences.boolean("AP_ShowAccounts", true)
    var marketPlaceDrawerButton by sharedPreferences.boolean("AP_MarketplaceDrawerButton", true)

}

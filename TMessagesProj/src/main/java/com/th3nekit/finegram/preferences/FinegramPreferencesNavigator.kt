/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences

import org.telegram.ui.ActionBar.BaseFragment
import com.th3nekit.finegram.preferences.folders.FoldersPreferencesEntry
import com.th3nekit.finegram.preferences.tabs.MainTabsPreferencesEntry

object FinegramPreferencesNavigator {

    fun createFinegramSettings(fragment: BaseFragment) = fragment.presentFragment(FGPreferencesEntry())

    fun createGeneral(fragment: BaseFragment) = fragment.presentFragment(GeneralPreferencesEntry())

    fun createAppearance(fragment: BaseFragment) = fragment.presentFragment(AppearancePreferencesEntry())
    fun createFoldersPrefs(fragment: BaseFragment) = fragment.presentFragment(FoldersPreferencesEntry())
    fun createTabs(fragment: BaseFragment) = fragment.presentFragment(MainTabsPreferencesEntry())
    fun createMessagesAndProfiles(fragment: BaseFragment) = fragment.presentFragment(MessagesAndProfilesPreferencesEntry())

    fun createChats(fragment: BaseFragment) = fragment.presentFragment(ChatsPreferencesEntry())
    fun createMessages(fragment: BaseFragment) = fragment.presentFragment(MessagesPreferencesEntry())
    fun createGemini(fragment: BaseFragment) = fragment.presentFragment(GeminiPreferencesEntry())
    fun createMessageFilter(fragment: BaseFragment) = fragment.presentFragment(MessageFiltersPreferencesEntry())
    fun createMessageMenu(fragment: BaseFragment) = fragment.presentFragment(MessageMenuPreferencesEntry())

    fun createCamera(fragment: BaseFragment) = fragment.presentFragment(CameraPreferencesEntry())

    fun createPrivacy(fragment: BaseFragment) = fragment.presentFragment(PrivacyPreferencesEntry())

    fun createStars(fragment: BaseFragment, customTitle: String?, userName: String?, type: Int) =
        fragment.presentFragment(FGStarsScreen(customTitle, userName, type))

    fun createAbout(fragment: BaseFragment) = fragment.presentFragment(AboutPreferencesEntry())
    fun createDebug(fragment: BaseFragment) = fragment.presentFragment(DebugPreferencesEntry())

}
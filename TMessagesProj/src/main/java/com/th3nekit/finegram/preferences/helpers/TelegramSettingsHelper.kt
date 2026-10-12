/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences.helpers

import android.view.View
import android.widget.FrameLayout
import androidx.core.content.edit
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.AndroidUtilities.dp
import org.telegram.messenger.LocaleController.getString
import org.telegram.messenger.MessageObject
import org.telegram.messenger.MessagesController
import org.telegram.messenger.R
import org.telegram.messenger.UserConfig
import org.telegram.messenger.UserObject
import org.telegram.messenger.browser.Browser
import org.telegram.tgnet.TLRPC
import org.telegram.tgnet.TLRPC.TL_peerColorCollectible
import org.telegram.ui.ActionBar.BaseFragment
import org.telegram.ui.ActionBar.Theme
import org.telegram.ui.ChatActivity
import org.telegram.ui.Components.BackupImageView
import org.telegram.ui.Components.IconBackgroundColors
import org.telegram.ui.Components.ImageUpdater
import org.telegram.ui.Components.ItemOptions
import org.telegram.ui.Components.Premium.LimitReachedBottomSheet
import org.telegram.ui.Components.Premium.LimitReachedBottomSheet.TYPE_ACCOUNTS
import org.telegram.ui.Components.UItem
import org.telegram.ui.Components.UniversalAdapter
import org.telegram.ui.Components.UniversalRecyclerView
import org.telegram.ui.Gifts.GiftSheet
import org.telegram.ui.LoginActivity
import org.telegram.ui.LogoutActivity
import org.telegram.ui.PhotoViewer
import org.telegram.ui.SettingsActivity
import org.telegram.ui.UserInfoActivity
import com.th3nekit.finegram.chats.FGChatMenuInjector
import com.th3nekit.finegram.chats.helpers.ChatsHelper2
import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig
import com.th3nekit.finegram.core.configs.FinegramCoreConfig
import com.th3nekit.finegram.core.configs.FinegramPrivacyConfig
import com.th3nekit.finegram.core.helpers.AppRestartHelper
import com.th3nekit.finegram.core.helpers.FGResourcesHelper
import com.th3nekit.finegram.core.helpers.DeeplinkHelper
import com.th3nekit.finegram.core.ui.mainTabs.MainTabsManager
import com.th3nekit.finegram.helpers.ui.PopupHelper
import com.th3nekit.finegram.misc.FinegramExtras
import com.th3nekit.finegram.misc.Constants
import com.th3nekit.finegram.preferences.FinegramPreferencesNavigator

class TelegramSettingsHelper(
    private var fragment: BaseFragment
) {

    private val FG_ACCENT_TOP = 0xFF0C1445.toInt()
    private val FG_ACCENT_BOTTOM = 0xFF041A40.toInt()

    private lateinit var avatarContainer: View
    private lateinit var avatarView: BackupImageView
    private lateinit var cameraButton: View
    private lateinit var imageUpdater: ImageUpdater

    private lateinit var listView: UniversalRecyclerView

    fun bindViews(
        avatarContainer: FrameLayout,
        avatarView: BackupImageView,
        cameraButton: FrameLayout,
        imageUpdater: ImageUpdater,

        listView: UniversalRecyclerView
    ) {
        this.avatarContainer = avatarContainer
        this.avatarView = avatarView
        this.cameraButton = cameraButton
        this.imageUpdater = imageUpdater

        this.listView = listView
    }

    fun showItemOptions(button: View) {
        val o = ItemOptions.makeOptions(fragment, button)

        o.add(
            R.drawable.msg_edit,
            getString(R.string.EditInfo)
        ) {
            fragment.presentFragment(UserInfoActivity())
        }

        o.addGap()

        o.add(
            R.drawable.msg_leave,
            getString(R.string.LogOut)
        ) {
            fragment.presentFragment(LogoutActivity())
        }

        o.add(
            R.drawable.msg_retry,
            getString(R.string.FG_Restart)
        ) {
            AppRestartHelper.restartApp(fragment.context)
        }

        o.addGapIf(
            fragment.messageMenuHelper.showDivider()
        )

        o.addIf(
            true,
            R.drawable.menu_gift,
            getString(
                if (FinegramAppearanceConfig.marketPlaceDrawerButton)
                    R.string.Gift2HideGift
                else
                    R.string.Gift2ShowGift
            )
        ) {
            FinegramAppearanceConfig.marketPlaceDrawerButton = !FinegramAppearanceConfig.marketPlaceDrawerButton
            if (::listView.isInitialized) {
                listView.adapter.update(true)
            }
        }

        o.setBlur(false)
        o.setDrawScrim(false)
        o.translate(0F, -dp(48F).toFloat())
        o.show()
    }

    private val provider = object : PhotoViewer.EmptyPhotoViewerProvider() {

        override fun getPlaceForPhoto(
            messageObject: MessageObject?,
            fileLocation: TLRPC.FileLocation?,
            index: Int,
            needPreview: Boolean,
            closing: Boolean
        ): PhotoViewer.PlaceProviderObject? {

            if (fileLocation == null) return null
            if (avatarContainer.scaleX > 0.96f && closing) return null

            val user = fragment.userConfig.currentUser
            val photoBig = user?.photo?.photo_big

            if (photoBig != null &&
                photoBig.local_id == fileLocation.local_id &&
                photoBig.volume_id == fileLocation.volume_id &&
                photoBig.dc_id == fileLocation.dc_id
            ) {
                val coords = IntArray(2)
                avatarView.getLocationInWindow(coords)

                val obj = PhotoViewer.PlaceProviderObject()
                obj.viewX = coords[0]
                obj.viewY = coords[1]
                obj.parentView = avatarView
                obj.imageReceiver = avatarView.imageReceiver
                obj.dialogId = fragment.userConfig.clientUserId
                obj.thumb = obj.imageReceiver.bitmapSafe ?: return null
                obj.size = -1
                obj.radius = avatarView.imageReceiver.getRoundRadius(true)
                obj.scale = avatarContainer.scaleX
                obj.canEdit = true
                obj.fadeIn = avatarContainer.scaleX > 0.96f

                return obj
            }

            return null
        }

        override fun willHidePhotoViewer() {
            avatarView.imageReceiver.setVisible(true, true)
        }

        override fun openPhotoForEdit(file: String?, thumb: String?, isVideo: Boolean) {
            imageUpdater.openPhotoForEdit(file, thumb, 0, isVideo)
        }

    }

    fun openAvatar() {
        val user = fragment.userConfig.currentUser
        val photo = user?.photo?.photo_big ?: return

        PhotoViewer.getInstance().parentActivity = fragment.parentActivity

        if (user.photo.dc_id != 0) {
            photo.dc_id = user.photo.dc_id
        }

        PhotoViewer.getInstance().setParentActivity(fragment)
        PhotoViewer.getInstance().openPhoto(photo, provider)
    }

    fun checkAvatarActions() {
        avatarContainer.setOnClickListener {
            openAvatar()
        }

        cameraButton.setOnClickListener {
            val user = fragment.userConfig.currentUser
            imageUpdater.openMenu(
                user != null && user.photo?.photo_big != null && user.photo !is TLRPC.TL_userProfilePhotoEmpty,
                { fragment.messagesController.deleteUserPhoto(null) },
                {},
                0
            )
        }
    }

    var cachedChannel: TLRPC.Chat? = null
    var isCheckingChannel = false

    fun checkChannelSubscription() {
        if (cachedChannel != null || isCheckingChannel) return

        if (!FinegramCoreConfig.isStandalonePremiumBuild()) {
            isCheckingChannel = true
            FinegramExtras.getChatJava(fragment).thenAccept { channel ->
                AndroidUtilities.runOnUIThread {
                    cachedChannel = channel
                    isCheckingChannel = false
                    if (::listView.isInitialized) {
                        listView.adapter.update(true)
                    }
                }
            }
        }
    }

    fun checkChannelSubscription(follow: Boolean) {
        fragment.messagesController.mainSettings
            .edit {
                putLong("last_follow_suggestion", System.currentTimeMillis())
            }

        isCheckingChannel = false
        cachedChannel = null

        if (::listView.isInitialized) {
            listView.adapter.update(true)
        }

        if (follow) {
            fragment.messagesController.addUserToChat(
                Constants.FG_Channel,
                fragment.userConfig.currentUser,
                0,
                null,
                null,
                null
            )
            Browser.openUrl(fragment.context, "https://t.me/" + Constants.FG_CHANNEL_USERNAME)
        }
    }

    fun injectChannelAdvice(items: ArrayList<UItem>) {
        if (FinegramExtras.shouldCheckFollow(fragment)) {
            val channel = cachedChannel
            if (channel != null && (channel.left || channel.kicked)) {

                items.add(
                    SettingsActivity.SuggestionCell.Factory.of(
                        getString(R.string.FG_FollowChannelTitle),
                        getString(R.string.FG_FollowChannelInfo),
                        getString(R.string.AppUpdateRemindMeLater),
                        { checkChannelSubscription(false) },
                        getString(R.string.ProfileJoinChannel),
                        { checkChannelSubscription(true) }
                    )
                )

                items.add(UItem.asShadow(null))
            }
        }

    }

    fun injectAccounts(adapter: UniversalAdapter, items: MutableList<UItem>, accountNumbers: ArrayList<Int>, user: TLRPC.User?) {
        items.add(UItem.asHeader(getString(R.string.SettingsAccounts)))

        val addAccountItem = SettingsActivity.SettingCell.Factory.of(
            1392,
            IconBackgroundColors.BLUE.top, IconBackgroundColors.BLUE_LIGHT.bottom,
            R.drawable.filled_add_album,
            getString(R.string.AddAccount)
        )

        if (accountNumbers.size >= 1) {
            addAccountItem.`object` = Runnable {
                FinegramAppearanceConfig.showAccounts = !FinegramAppearanceConfig.showAccounts
            }
        }

        items.add(addAccountItem)

        if (FinegramAppearanceConfig.showAccounts                                   ) {
            adapter.reorderSectionStart()
            for (i in accountNumbers.indices) {
                items.add(SettingsActivity.AccountCell.Factory.of(accountNumbers[i]))
            }
            adapter.reorderSectionEnd()

            val space = SettingsHelper.asSpaceCG(dp(8f))

            space.id = -3
            space.transparent = true
            items.add(space)
        } else {
            injectMyProfile(items, user)
        }

        items.add(UItem.asShadow(null))
    }

    object Helper {
        fun getProfileButtonColor(user: TLRPC.User?, force: Boolean): Pair<Int, Int> {
            var colorTop = IconBackgroundColors.BLUE.top
            var colorBottom = IconBackgroundColors.BLUE_LIGHT.bottom

            if ((showMyProfile() || force) && user != null) {
                if (user.color is TL_peerColorCollectible) {
                    val p = user.color as TL_peerColorCollectible
                    val dark = Theme.isCurrentThemeDark()
                    val colors = if (dark && p.dark_colors != null) p.dark_colors else p.colors

                    val color1 = colors[0]!! or -0x1000000
                    val color2 = if (colors.size >= 2) colors[1]!! or -0x1000000 else color1
                    val color3 = if (colors.size >= 3) colors[2]!! or -0x1000000 else color1

                    colorTop = color1
                    colorBottom = color2
                } else {
                    val colorId = UserObject.getColorId(user)
                    if (colorId < 7) {
                        val color = Theme.getColor(Theme.keys_avatar_nameInMessage[colorId])
                        val isWhite = FinegramExtras.isWhiteOrNearWhite(color)
                        if (isWhite) {
                            colorTop = IconBackgroundColors.BLUE.top
                            colorBottom = IconBackgroundColors.BLUE_LIGHT.bottom
                        } else {
                            colorTop = Theme.getColor(Theme.keys_avatar_nameInMessage[colorId])
                            colorBottom = Theme.getColor(Theme.keys_avatar_nameInMessage[colorId])
                        }
                    } else {
                        val peerColors = MessagesController.getInstance(UserConfig.selectedAccount).peerColors
                        val peerColor = if (peerColors == null) null else peerColors.getColor(colorId)
                        if (peerColor != null) {
                            colorTop = peerColor.color1
                            colorBottom = peerColor.color2
                        }
                    }
                }
            }
            return Pair(colorTop, colorBottom)
        }

        fun showMyProfile(): Boolean {
            return !FinegramAppearanceConfig.showMainTabs || !MainTabsManager.hasTab(MainTabsManager.TabType.PROFILE)
        }
    }

    fun injectMyProfile(items: MutableList<UItem>, user: TLRPC.User?) {
        val (colorTop, colorBottom) = Helper.getProfileButtonColor(user, false)

        items.add(
            SettingsActivity.SettingCell.Factory.of(
                1,
                if (Theme.isCurrentThemeDay()) colorBottom else colorTop, if (Theme.isCurrentThemeDay()) colorTop else colorBottom,
                R.drawable.settings_account,
                if (Helper.showMyProfile()) getString(R.string.MyProfile) else getString(R.string.SettingsAccount),
                getString(R.string.SettingsAccountInfo)
            )
        )
    }

    fun injectFinegramItems(items: MutableList<UItem>, user: TLRPC.User?) {

        if (FinegramAppearanceConfig.showAccounts) {
            injectMyProfile(items, user)
        }

        if (!FinegramPrivacyConfig.hideArchiveFromChatsList && fragment.messagesController.getDialogs(1).isNotEmpty()) {
            val archiveItem = SettingsActivity.SettingCell.Factory.of(
                1395,
                IconBackgroundColors.RED.top, IconBackgroundColors.RED.bottom,
                R.drawable.fg_settings_archive_solar,
                getString(R.string.ArchivedChats)
            )
            archiveItem.`object` = "archive"
            items.add(archiveItem)
        }

        items.add(
            SettingsActivity.SettingCell.Factory.of(
                1393,
                IconBackgroundColors.BLUE_DEEP.top, IconBackgroundColors.BLUE_DEEP.bottom,
                R.drawable.fg_settings_saved_solar,
                getString(R.string.SavedMessages)
            )
        )

        items.add(UItem.asShadow(null))

        items.add(
            SettingsActivity.SettingCell.Factory.of(
                1390,
                FG_ACCENT_TOP, FG_ACCENT_BOTTOM,
                if (FGResourcesHelper.isAnyOfBraIconsEnabled()) R.drawable.fg_settings_bra else R.drawable.fg_settings,
                getString(R.string.FGP_AdvancedSettings)
            )
        )

        val pluginAnchor = SettingsHelper.asSpaceCG(0)
        pluginAnchor.id = -1
        pluginAnchor.transparent = true
        items.add(pluginAnchor)

        if (FinegramAppearanceConfig.marketPlaceDrawerButton) {
            val giftsItem = SettingsActivity.SettingCell.Factory.of(
                1394,
                0xFFF38B31.toInt(), 0xFFE26314.toInt(),
                R.drawable.settings_gift,
                getString(R.string.Gift2TitleSelf1)
            )
            giftsItem.`object` = "gifts"
            items.add(giftsItem)
        }

        items.add(UItem.asShadow(null))
    }

    fun handleOnClick(item: UItem) {
        when (item.id) {
            1390 -> FinegramPreferencesNavigator.createFinegramSettings(fragment)
            1392 -> {
                var freeAccounts = 0
                var availableAccount: Int? = null
                for (a in UserConfig.MAX_ACCOUNT_COUNT - 1 downTo 0) {
                    val uc = UserConfig.getInstance(a)
                    if (!uc.isClientActivated) {
                        freeAccounts++
                        if (availableAccount == null) availableAccount = a
                    }
                }

                if (!UserConfig.hasPremiumOnAccounts()) {
                    freeAccounts -= (UserConfig.MAX_ACCOUNT_COUNT - UserConfig.MAX_ACCOUNT_DEFAULT_COUNT)
                }

                if (freeAccounts > 0 && availableAccount != null) {
                    val activeAccountsCount = UserConfig.getActivatedAccountsCount()

                    if (activeAccountsCount >= 4) {
                        PopupHelper.showLoginWarning(fragment.context) {
                            fragment.presentFragment(LoginActivity(availableAccount))
                        }
                    } else {
                        fragment.presentFragment(LoginActivity(availableAccount))
                    }
                } else if (!UserConfig.hasPremiumOnAccounts()) {
                    fragment.showDialog(
                        LimitReachedBottomSheet(
                            fragment,
                            fragment.context,
                            TYPE_ACCOUNTS,
                            fragment.currentAccount,
                            fragment.resourceProvider
                        )
                    )
                }
            }
            1393 -> fragment.presentFragment(ChatActivity.of(ChatsHelper2.getCustomChatID()))
            1394 -> {
                AndroidUtilities.runOnUIThread {
                    val alert = GiftSheet(
                        fragment.context,
                        fragment.currentAccount,
                        fragment.userConfig.clientUserId,
                        null,
                        null
                    )
                    alert.show()
                }
            }
            1395 -> FGChatMenuInjector.openArchivedChats(fragment)
        }
    }

    fun handleOnLongClick(item: UItem): Boolean {
        when (item.id) {
            1390 -> AndroidUtilities.addToClipboard("tg://${DeeplinkHelper.DeepLinksRepo.FG_Settings}")
            12 -> AndroidUtilities.addToClipboard("tg://${DeeplinkHelper.DeepLinksRepo.FG_Stars}")
        }
        return true
    }

}

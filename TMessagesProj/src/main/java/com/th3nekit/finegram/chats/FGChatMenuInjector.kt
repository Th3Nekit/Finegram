/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.chats

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.text.TextUtils
import androidx.core.content.edit
import org.telegram.messenger.AccountInstance
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.BuildVars
import org.telegram.messenger.ChatObject
import org.telegram.messenger.LocaleController.getString
import org.telegram.messenger.R
import org.telegram.messenger.UserConfig
import org.telegram.tgnet.TLRPC
import org.telegram.ui.ActionBar.ActionBarMenu
import org.telegram.ui.ActionBar.ActionBarMenuItem
import org.telegram.ui.ActionBar.ActionBarMenuSubItem
import org.telegram.ui.ActionBar.BaseFragment
import org.telegram.ui.ActionBar.INavigationLayout
import org.telegram.ui.ActionBar.Theme
import org.telegram.ui.ActionIntroActivity
import org.telegram.ui.CallLogActivity
import org.telegram.ui.CameraScanActivity
import org.telegram.ui.ChannelCreateActivity
import org.telegram.ui.ChatActivity
import org.telegram.ui.Components.ChatActivityEnterView
import org.telegram.ui.Components.ChatAttachAlert
import org.telegram.ui.Components.ItemOptions
import com.th3nekit.finegram.plugins.FGPluginsMenu
import org.telegram.ui.DialogsActivity
import org.telegram.ui.Gifts.GiftSheet
import org.telegram.ui.LaunchActivity
import org.telegram.ui.ProxyListActivity
import com.th3nekit.finegram.chats.helpers.ChatActivityHelper
import com.th3nekit.finegram.chats.helpers.ChatsHelper2
import com.th3nekit.finegram.core.FinegramLogger
import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig
import com.th3nekit.finegram.core.configs.FinegramChatsConfig
import com.th3nekit.finegram.core.configs.FinegramCoreConfig
import com.th3nekit.finegram.core.configs.FinegramPrivacyConfig
import com.th3nekit.finegram.core.ui.mainTabs.MainTabsManager
import com.th3nekit.finegram.helpers.CreateQRSheet
import com.th3nekit.finegram.helpers.QRCodeSheet
import com.th3nekit.finegram.misc.Constants
import kotlin.math.abs

object FGChatMenuInjector {

    fun injectAttachItem(
        headerItem: ActionBarMenuItem?,
        attachItem: ActionBarMenu.LazyItem?,
        chatActivityEnterView: ChatActivityEnterView?,
        chatAttachAlert: ChatAttachAlert?,
        context: Context,
        resourcesProvider: Theme.ResourcesProvider
    ) {
        if (headerItem == null) return
        if (chatActivityEnterView != null && chatActivityEnterView.hasText() && TextUtils.isEmpty(chatActivityEnterView.slowModeTimer)) {
            val attach = ActionBarMenuSubItem(context, false, true, true, resourcesProvider)
            attach.setTextAndIcon(getString(R.string.AttachMenu), R.drawable.input_attach)
            attach.setOnClickListener {
                headerItem.closeSubMenu()
                chatAttachAlert?.setEditingMessageObject(0, null)
                chatActivityEnterView.attachButton.performClick()
            }
            headerItem.setOnClickListener {
                headerItem.toggleSubMenu(attach, attachItem?.createView())
            }
        } else {
            headerItem.setOnClickListener {
                headerItem.toggleSubMenu(null, null)
            }
        }
    }

    fun injectCallShortcuts(headerItem: ActionBarMenuItem, userFull: TLRPC.UserFull?) {
        if (userFull != null && userFull.phone_calls_available) {
            headerItem.lazilyAddSubItem(
                ChatActivity.call,
                R.drawable.msg_callback,
                getString(R.string.Call)
            )
            if (userFull.video_calls_available) headerItem.lazilyAddSubItem(
                    ChatActivity.video_call,
                    R.drawable.msg_videocall,
                    getString(R.string.VideoCall)
            )
        }
    }

    fun injectFinegramShortcuts(
        chatActivity: ChatActivity,
        headerItem: ActionBarMenuItem,
        currentChat: TLRPC.Chat?,
        currentUser: TLRPC.User?,
        secretChat: Boolean
    ) {
        val requireBiometrics = FinegramPrivacyConfig.askBiometricsToOpenChat && !secretChat
        val isAnyButtonEnabled = requireBiometrics || FinegramChatsConfig.shortcut_JumpToBegin
                    || FinegramChatsConfig.shortcut_DeleteAll || FinegramChatsConfig.shortcut_SavedMessages
                    || FinegramChatsConfig.shortcut_Browser

        if (isAnyButtonEnabled) headerItem.lazilyAddColoredGap()

        if (requireBiometrics) {
            if (chatActivity.chatsPasswordHelper.shouldRequireBiometrics(currentUser?.id ?: 0, currentChat?.id ?: 0, 0)) {
                headerItem.lazilyAddSubItem(
                    ChatActivityHelper.OPTION_DO_NOT_ASK_PASSCODE,
                    R.drawable.msg_secret,
                    getString(R.string.SP_DoNotAskPin)
                )
            } else {
                headerItem.lazilyAddSubItem(
                    ChatActivityHelper.OPTION_ASK_PASSCODE,
                    R.drawable.msg_secret,
                    getString(R.string.SP_AskPin)
                )
            }
        }

        if (FinegramChatsConfig.shortcut_JumpToBegin) headerItem.lazilyAddSubItem(
            ChatActivityHelper.OPTION_JUMP_TO_BEGINNING,
            R.drawable.ic_upward,
            getString(R.string.FG_JumpToBeginning)
        )

        if (!secretChat && (currentUser != null || currentChat != null && !isDeleteAllHidden(currentChat)
                    && (ChatObject.isMegagroup(currentChat) || !ChatObject.isChannel(currentChat)))) {
            if (FinegramChatsConfig.shortcut_DeleteAll) headerItem.lazilyAddSubItem(
                ChatActivityHelper.OPTION_DELETE_ALL_FROM_SELF,
                R.drawable.msg_delete,
                getString(R.string.FG_DeleteAllFromSelf)
            )
        }

        if (currentChat != null && !ChatObject.isChannel(currentChat) && currentChat.creator) headerItem.lazilyAddSubItem(
            ChatActivityHelper.OPTION_UPGRADE_GROUP,
                R.drawable.ic_upward,
                getString(R.string.UpgradeGroup)
        )

        if (currentChat != null && currentChat.id != abs(ChatsHelper2.getCustomChatID())
            || currentUser != null && currentUser.id != abs(ChatsHelper2.getCustomChatID())
        ) {
            if (FinegramChatsConfig.shortcut_SavedMessages) headerItem.lazilyAddSubItem(
                ChatActivityHelper.OPTION_GO_TO_SAVED,
                R.drawable.msg_saved,
                getString(R.string.SavedMessages)
            )
        }

        if (FinegramChatsConfig.shortcut_Browser) headerItem.lazilyAddSubItem(
            ChatActivityHelper.OPTION_OPEN_TELEGRAM_BROWSER,
            R.drawable.msg_language,
            "Telegram Browser"
        )

        if (
            FinegramChatsConfig.centerChatTitle
            && (currentUser != null && currentUser.linked_community_id.toInt() != 0 || currentChat != null && currentChat.linked_community_id.toInt() != 0)
        ) {
            headerItem.lazilyAddColoredGap()

            headerItem.lazilyAddSubItem(
                ChatActivityHelper.OPTION_OPEN_COMMUNITY,
                R.drawable.msg_groups,
                getString(R.string.Community)
            )
        }

    }

    fun injectPluginItems(
        headerItem: ActionBarMenuItem,
        currentChat: TLRPC.Chat?,
        currentUser: TLRPC.User?,
        dialogId: Long,
        account: Int
    ) {
        if (!FGPluginsMenu.has(FGPluginsMenu.CHAT_ACTION)) return
        headerItem.lazilyAddColoredGap()
        headerItem.lazilyAddSubItem(FGPluginsMenu.CHAT_ITEM_BASE, R.drawable.msg_settings, getString(R.string.FG_Plugins))
    }

    fun injectAdminShortcuts(headerItem: ActionBarMenuItem, currentChat: TLRPC.Chat) {
        val isAnyButtonEnabled = FinegramChatsConfig.admins_Reactions || FinegramChatsConfig.admins_Permissions || FinegramChatsConfig.admins_Administrators
                || FinegramChatsConfig.admins_Members || FinegramChatsConfig.admins_Statistics || FinegramChatsConfig.admins_RecentActions

        if (isAnyButtonEnabled) headerItem.lazilyAddColoredGap()

        if (FinegramChatsConfig.admins_Reactions && ChatObject.canChangeChatInfo(currentChat)) headerItem.lazilyAddSubItem(
                ChatActivityHelper.OPTION_FOR_ADMINS_REACTIONS,
                R.drawable.msg_reactions2,
                getString(R.string.Reactions)
        )

        if (FinegramChatsConfig.admins_Permissions && !(ChatObject.isChannel(currentChat) && !currentChat.megagroup) && !currentChat.gigagroup) headerItem.lazilyAddSubItem(
                ChatActivityHelper.OPTION_FOR_ADMINS_PERMISSIONS,
                R.drawable.msg_permissions,
                getString(R.string.ChannelPermissions)
        )

        if (FinegramChatsConfig.admins_Administrators) headerItem.lazilyAddSubItem(
                ChatActivityHelper.OPTION_FOR_ADMINS_ADMINISTRATORS,
                R.drawable.msg_admins,
                getString(R.string.ChannelAdministrators)
        )

        if (FinegramChatsConfig.admins_Members) headerItem.lazilyAddSubItem(
                ChatActivityHelper.OPTION_FOR_ADMINS_MEMBERS,
                R.drawable.msg_groups,
                getString(R.string.ChannelMembers)
        )

        if (FinegramChatsConfig.admins_Permissions && (ChatObject.isChannel(currentChat) && !currentChat.megagroup || currentChat.gigagroup)) headerItem.lazilyAddSubItem(
                ChatActivityHelper.OPTION_FOR_ADMINS_PERMISSIONS,
                R.drawable.msg_user_remove,
                getString(R.string.ChannelBlacklist)
        )

        if (FinegramChatsConfig.admins_Statistics && ChatObject.isBoostSupported(currentChat)) headerItem.lazilyAddSubItem(
                ChatActivityHelper.OPTION_FOR_ADMINS_STATISTICS,
                R.drawable.msg_stats,
                getString(R.string.StatisticsAndBoosts)
        )

        if (FinegramChatsConfig.admins_RecentActions) headerItem.lazilyAddSubItem(
                ChatActivityHelper.OPTION_FOR_ADMINS_RECENT_ACTIONS,
                R.drawable.msg_log,
                getString(R.string.EventLog)
        )

    }

    private fun isDeleteAllHidden(chat: TLRPC.Chat): Boolean {
        return Constants.FG_Support == chat.id
    }

    fun injectCreateChannel(io: ItemOptions, fragment: BaseFragment) {
        io.add(
            R.drawable.msg_channel,
            getString(R.string.NewChannel)
        ) {
            val preferences = fragment.messagesController.mainSettings

            if (!BuildVars.DEBUG_VERSION && preferences.getBoolean("channel_intro", false)) {
                val args = Bundle().apply {
                    putInt("step", 0)
                }
                fragment.presentFragment(ChannelCreateActivity(args))
            } else {
                fragment.presentFragment(ActionIntroActivity(ActionIntroActivity.ACTION_TYPE_CHANNEL_CREATE))
                preferences.edit { putBoolean("channel_intro", true) }
            }
        }
    }

    fun injectArchived(io: ItemOptions, fragment: BaseFragment) {
        io.addIf(
            !FinegramPrivacyConfig.hideArchiveFromChatsList && (FinegramAppearanceConfig.tabsHideAllChats || !MainTabsManager.hasTab(MainTabsManager.TabType.SETTINGS) || !FinegramAppearanceConfig.showMainTabs),
            R.drawable.msg_archive,
            getString(R.string.ArchivedChats)
        ) {
            openArchivedChats(fragment)
        }
    }

    fun injectSaved(io: ItemOptions, fragment: BaseFragment) {
        io.addIf(
            !MainTabsManager.hasTab(MainTabsManager.TabType.SETTINGS) || !FinegramAppearanceConfig.showMainTabs,
            R.drawable.msg_saved,
            getString(R.string.SavedMessages)
        ) {
            fragment.presentFragment(ChatActivity.of(ChatsHelper2.getCustomChatID()))
        }
    }

    fun injectCalls(io: ItemOptions, fragment: BaseFragment) {
        io.addIf(
            !FinegramAppearanceConfig.showMainTabs || !MainTabsManager.hasTab(MainTabsManager.TabType.CALLS),
            R.drawable.msg_calls,
            getString(R.string.Calls)
        ) {
            val args = Bundle().apply {
                putBoolean("needFinishFragment", false)
                putBoolean("hasMainTabs", false)
            }
            fragment.presentFragment(CallLogActivity(args))
        }
    }

    fun injectGifts(io: ItemOptions, currentAccount: Int, context: Context) {
        val available: Boolean = FinegramAppearanceConfig.marketPlaceDrawerButton

        io.addGapIf(available)

        io.addIf(
            available,
            R.drawable.menu_gift,
            getString(R.string.Gift2TitleSelf1)
        ) {
            AndroidUtilities.runOnUIThread {
                val alert = GiftSheet(context, currentAccount, UserConfig.getInstance(currentAccount).clientUserId, null, null)
                alert.show()
            }
        }
    }

    fun injectScanQR(io: ItemOptions, fragment: BaseFragment?) {
        io.add(
            R.drawable.msg_qrcode,
            getString(R.string.AuthAnotherClient)
        ) { if (fragment != null) requestCameraScan(fragment) }
    }

    private fun requestCameraScan(fragment: BaseFragment) {
        val activity = fragment.parentActivity ?: return
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && activity.checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            try {
                activity.requestPermissions(
                    arrayOf(Manifest.permission.CAMERA),
                    ActionIntroActivity.CAMERA_PERMISSION_REQUEST_CODE
                )
            } catch (e: Exception) {
                FinegramLogger.e(e)
            }
            return
        }
        if (activity is LaunchActivity) {
            openCameraScanActivity(fragment)
            if (AndroidUtilities.isTablet() && activity.actionBarLayout != null && activity.rightActionBarLayout != null) {
                activity.actionBarLayout.rebuildFragments(INavigationLayout.REBUILD_FLAG_REBUILD_LAST)
                activity.rightActionBarLayout.rebuildFragments(INavigationLayout.REBUILD_FLAG_REBUILD_LAST)
            }
        }
    }

    fun injectCreateQR(io: ItemOptions, fragment: BaseFragment?) {
        io.add(
            R.drawable.msg_qrcode,
            getString(R.string.FG_CreateQR)
        ) {
            CreateQRSheet(fragment?.context, fragment, fragment?.resourceProvider).show()
        }
    }

    fun injectProxySettings(io: ItemOptions, fragment: BaseFragment) {
        io.addGapIf(showProxyButton())
        io.addIf(
            showProxyButton(),
            R.drawable.shield_network_solar,
            getString(R.string.ProxySettings)
        ) {
            fragment.presentFragment(ProxyListActivity())
        }
    }

    fun showProxyButton() : Boolean {
        var available = false

        for (i in 0 until UserConfig.MAX_ACCOUNT_COUNT) {
            val userConfig = AccountInstance.getInstance(i).userConfig
            val phone = userConfig?.currentUser?.phone ?: continue

            if (
                phone.startsWith("7")

            ) {
                available = true
                break
            }
        }
        return available || FinegramCoreConfig.isDevBuild()
    }

    private fun openCameraScanActivity(fragment: BaseFragment) {
        CameraScanActivity.showAsSheet(fragment, true, CameraScanActivity.TYPE_QR_UNIVERSAL, object : CameraScanActivity.CameraScanActivityDelegate {
            override fun processQr(text: String, action: Runnable): Boolean {
                AndroidUtilities.runOnUIThread({
                    action.run()
                    AndroidUtilities.runOnUIThread({
                        QRCodeSheet.text = text
                        QRCodeSheet(fragment).show()
                    }, 150L)
                }, 600L)
                return true
            }
        })
    }

    fun openArchivedChats(fragment: BaseFragment) {
        val args = Bundle().apply {
            putInt("folderId", 1)
        }
        fragment.presentFragment(DialogsActivity(args))
    }

}

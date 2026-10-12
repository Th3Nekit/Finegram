/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.chats.helpers

import android.os.Build
import android.text.TextUtils
import android.view.Gravity
import android.view.View
import android.widget.FrameLayout
import androidx.core.view.isVisible
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.AndroidUtilities.dp
import org.telegram.messenger.ChatObject
import org.telegram.messenger.LocaleController
import org.telegram.messenger.LocaleController.formatJoined
import org.telegram.messenger.LocaleController.getString
import org.telegram.messenger.MessageObject
import org.telegram.messenger.MessagesController
import org.telegram.messenger.R
import org.telegram.messenger.UserConfig
import org.telegram.tgnet.TLRPC
import org.telegram.ui.ActionBar.BaseFragment
import org.telegram.ui.Cells.ChatMessageCell
import org.telegram.ui.ChatActivity
import org.telegram.ui.ChatRightsEditActivity
import org.telegram.ui.Components.BulletinFactory
import org.telegram.ui.Components.ItemOptions
import org.telegram.ui.Components.ShareAlert
import org.telegram.ui.Components.TranslateAlert2
import org.telegram.ui.Components.UndoView
import com.th3nekit.finegram.chats.JsonBottomSheet
import com.th3nekit.finegram.chats.gemini.GeminiResultsBottomSheet
import com.th3nekit.finegram.core.FinegramLogger
import com.th3nekit.finegram.core.configs.FinegramChatsConfig
import com.th3nekit.finegram.core.configs.FinegramMessagesConfig
import com.th3nekit.finegram.core.helpers.FGResourcesHelper
import com.th3nekit.finegram.helpers.ProfileActivityHelper
import com.th3nekit.finegram.misc.Constants

object ChatsHelper2 {

    fun injectChatActivityAvatarOnClickNew(
        chatActivity: ChatActivity, chatMessageCellDelegate: ChatActivity.ChatMessageCellDelegate, cell: ChatMessageCell, user: TLRPC.User,
        enableMention: Boolean, enableSearchMessages: Boolean
    ) {
        if (chatActivity.context == null) return

        val participants = arrayListOf<TLRPC.ChatParticipant>().apply {
            val chatInfo = chatActivity.currentChatInfo
            val chatParticipants = chatInfo?.participants?.participants
            if (chatParticipants != null) {
                addAll(chatParticipants)
            }
        }

        val participant = participants.find { it.user_id == user.id }
        val isChatParticipant = participant != null

        ItemOptions.makeOptions(chatActivity, cell)
            .add(R.drawable.msg_discussion, getString(R.string.SendMessage)) {
                chatMessageCellDelegate.openDialog(cell, user)
            }
            .addIf(enableMention, R.drawable.msg_mention, getString(R.string.Mention)) {
                chatMessageCellDelegate.appendMention(user)
            }
            .addIf(enableSearchMessages, R.drawable.msg_search, getString(R.string.AvatarPreviewSearchMessages)) {
                chatActivity.openSearchWithUser(user)
            }
            .addIf(ChatObject.canBlockUsers(chatActivity.currentChat) && isChatParticipant, R.drawable.msg_remove, getString(R.string.KickFromGroup)) {
                chatActivity.messagesController.deleteParticipantFromChat(
                    chatActivity.currentChat.id,
                    chatActivity.messagesController.getUser(user.id),
                    chatActivity.currentChatInfo
                )
            }
            .addIf(ChatObject.hasAdminRights(chatActivity.currentChat) && isChatParticipant, R.drawable.msg_permissions, getString(R.string.ChangePermissions)) {
                val action = 1

                val chatParticipant = chatActivity.currentChatInfo.participants.participants.filter {
                    it.user_id == user.id
                }[0]

                var channelParticipant: TLRPC.ChannelParticipant? = null

                if (ChatObject.isChannel(chatActivity.currentChat)) {
                    channelParticipant = (chatParticipant as TLRPC.TL_chatChannelParticipant).channelParticipant
                } else {
                    chatParticipant is TLRPC.TL_chatParticipantAdmin
                }

                val frag = ChatRightsEditActivity(
                    user.id,
                    chatActivity.currentChatInfo.id,
                    channelParticipant?.admin_rights,
                    chatActivity.currentChat.default_banned_rights,
                    channelParticipant?.banned_rights,
                    channelParticipant?.rank,
                    action,
                    true,
                    false,
                    null
                )
                chatActivity.presentFragment(frag)
            }
            .addIf(ChatObject.canAddAdmins(chatActivity.currentChat) && isChatParticipant, R.drawable.msg_admins, getString(R.string.EditAdminRights)) {
                val action = 0

                val chatParticipant = chatActivity.currentChatInfo.participants.participants.filter {
                    it.user_id == user.id
                }[0]

                var channelParticipant: TLRPC.ChannelParticipant? = null

                if (ChatObject.isChannel(chatActivity.currentChat)) {
                    channelParticipant = (chatParticipant as TLRPC.TL_chatChannelParticipant).channelParticipant
                } else {
                    chatParticipant is TLRPC.TL_chatParticipantAdmin
                }

                val frag = ChatRightsEditActivity(
                    user.id,
                    chatActivity.currentChatInfo.id,
                    channelParticipant?.admin_rights,
                    chatActivity.currentChat.default_banned_rights,
                    channelParticipant?.banned_rights,
                    channelParticipant?.rank,
                    action,
                    true,
                    false,
                    null
                )
                chatActivity.presentFragment(frag)
            }
            .addGapIf(participant?.date != null && participant.date != 0)
            .addTextIf(
                participant?.date != null && participant.date != 0,
                FGResourcesHelper.capitalize(formatJoined(participant?.date?.toLong() ?: 0)),
                13
            )
            .addGap()
            .addProfile(user, getString(R.string.ViewProfile)) {
                chatMessageCellDelegate.openProfile(user)
            }

            .setGravity(Gravity.LEFT)
            .forceBottom(true)
            .translate(0f, -dp(48f).toFloat())
            .setDrawScrim(false)
            .setBlur(true)
            .show()
    }

    fun getActiveUsername(userId: Long): String {
        val user: TLRPC.User = MessagesController.getInstance(UserConfig.selectedAccount).getUser(userId) ?: return ""

        var username: String? = null

        if (!TextUtils.isEmpty(user.username)) {
            username = user.username
        }

        if (TextUtils.isEmpty(username) && user.usernames != null) {
            val usernames = ArrayList(user.usernames)
            for (u in usernames) {
                if (u != null && u.active && !TextUtils.isEmpty(u.username)) {
                    username = u.username
                    break
                }
            }
        }
        return username ?: ""
    }

    fun getSearchFilterType(): TLRPC.MessagesFilter {
        val filter: TLRPC.MessagesFilter = when (FinegramChatsConfig.messagesSearchFilter) {
            FinegramChatsConfig.FILTER_PHOTOS -> {
                TLRPC.TL_inputMessagesFilterPhotos()
            }
            FinegramChatsConfig.FILTER_VIDEOS -> {
                TLRPC.TL_inputMessagesFilterVideo()
            }
            FinegramChatsConfig.FILTER_VOICE_MESSAGES -> {
                TLRPC.TL_inputMessagesFilterVoice()
            }
            FinegramChatsConfig.FILTER_VIDEO_MESSAGES -> {
                TLRPC.TL_inputMessagesFilterRoundVideo()
            }
            FinegramChatsConfig.FILTER_FILES -> {
                TLRPC.TL_inputMessagesFilterDocument()
            }
            FinegramChatsConfig.FILTER_MUSIC -> {
                TLRPC.TL_inputMessagesFilterMusic()
            }
            FinegramChatsConfig.FILTER_GIFS -> {
                TLRPC.TL_inputMessagesFilterGif()
            }
            FinegramChatsConfig.FILTER_GEO -> {
                TLRPC.TL_inputMessagesFilterGeo()
            }
            FinegramChatsConfig.FILTER_CONTACTS -> {
                TLRPC.TL_inputMessagesFilterContacts()
            }
            FinegramChatsConfig.FILTER_MENTIONS -> {
                TLRPC.TL_inputMessagesFilterMyMentions()
            }
            else -> {
                TLRPC.TL_inputMessagesFilterEmpty()
            }
        }
        return filter
    }

    fun getCustomChatID(): Long {
        val preferences = MessagesController.getMainSettings(UserConfig.selectedAccount)
        val savedMessagesChatID =
            preferences.getString("CP_CustomChatIDSM", UserConfig.getInstance(UserConfig.selectedAccount).clientUserId.toString())
        val chatID = savedMessagesChatID!!.replace("-100", "-").toLong()

        return if (FinegramChatsConfig.customChatForSavedMessages) chatID
        else UserConfig.getInstance(UserConfig.selectedAccount).clientUserId
    }

    fun showForwardMenu(sa: ShareAlert, scrimView: View) {
        ItemOptions.makeOptions(sa.container, sa.resourcesProvider, scrimView)
            .addChecked(
                FinegramChatsConfig.forwardAuthorship,
                getString(R.string.FG_FwdMenu_Authorship)
            ) {
                FinegramChatsConfig.forwardAuthorship = !FinegramChatsConfig.forwardAuthorship
            }
            .addChecked(
                FinegramChatsConfig.forwardCaptions,
                getString(R.string.FG_FwdMenu_Captions)
            ) {
                FinegramChatsConfig.forwardCaptions = !FinegramChatsConfig.forwardCaptions
            }
            .addChecked(
                FinegramChatsConfig.forwardNotify,
                getString(R.string.FG_FwdMenu_Notify)
            ) {
                FinegramChatsConfig.forwardNotify = !FinegramChatsConfig.forwardNotify
            }

            .setDimAlpha(100)
            .translate(-dp(10f).toFloat(), dp(5f).toFloat())
            .show()
    }

    fun showJsonMenu(sa: JsonBottomSheet, field: FrameLayout, messageObject: MessageObject) {
        ItemOptions.makeOptions(sa.container, sa.resourcesProvider, field)
            .addIf(
                messageObject.messageOwner !is TLRPC.TL_messageService,
                R.drawable.msg_info,
                getString(if (sa.isJacksonSupportedAndEnabled) R.string.FG_Json_UseGson else R.string.FG_Json_UseJackson)
            ) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    FinegramMessagesConfig.jacksonJSON_Provider = !FinegramMessagesConfig.jacksonJSON_Provider
                    sa.dismiss()
                    JsonBottomSheet.showAlert(sa.context, sa.resourcesProvider, sa.fragment, messageObject, null)
                } else {
                    BulletinFactory.of(sa.container, sa.resourcesProvider)
                        .createSimpleBulletin(R.raw.error, getString(R.string.FG_Json_JacksonUnsupported))
                        .show()
                }
            }
            .add(
                R.drawable.msg_calendar2,
                LocaleController.formatString(R.string.FG_Json_Date, FGResourcesHelper.createDateAndTimeForJSON(messageObject.messageOwner.date.toLong())),
            ) {
                AndroidUtilities.addToClipboard(FGResourcesHelper.createDateAndTimeForJSON(messageObject.messageOwner.date.toLong()))
                BulletinFactory.of(sa.container, sa.resourcesProvider)
                    .createCopyBulletin(getString(R.string.TextCopied))
                    .show()
            }
            .addIf(messageObject.messageOwner != null,
                R.drawable.msg_calendar2,
                if (messageObject.messageOwner?.fwd_from?.date != null && messageObject.messageOwner.fwd_from.date != messageObject.messageOwner.date) {
                    LocaleController.formatString(R.string.FG_Json_ForwardDate, FGResourcesHelper.createDateAndTimeForJSON(messageObject.messageOwner.fwd_from.date.toLong()))
                } else {
                    getString(R.string.FG_Json_NotForwarded)
                }
            ) {
                var textToCopy = ""
                if (messageObject.messageOwner?.fwd_from?.date != null && messageObject.messageOwner.fwd_from.date != messageObject.messageOwner.date) {
                    textToCopy = FGResourcesHelper.createDateAndTimeForJSON(messageObject.messageOwner.fwd_from.date.toLong())
                } else if (messageObject.messageOwner.media != null && messageObject.messageOwner.media.photo != null) {
                    textToCopy = getString(R.string.FG_Json_NotForwarded)
                }
                if (textToCopy != "") {
                    AndroidUtilities.addToClipboard(textToCopy)
                    BulletinFactory.of(sa.container, sa.resourcesProvider)
                        .createCopyBulletin(getString(R.string.TextCopied))
                        .show()
                }
            }
            .addIf(messageObject.messageOwner != null,
                R.drawable.msg_info,
                if (messageObject.messageOwner.media != null && messageObject.messageOwner.media.document != null) {
                    "DC: " + messageObject.messageOwner.media.document.dc_id
                } else if (messageObject.messageOwner.media != null && messageObject.messageOwner.media.photo != null) {
                    "DC: " + messageObject.messageOwner.media.photo.dc_id
                } else {
                    getString(R.string.FG_Json_DcMediaOnly)
                }
            ) {}

            .addGapIf(messageObject.messageOwner != null && messageObject.messageOwner.restriction_reason != null && !ProfileActivityHelper.getRestrictionReasons(messageObject.messageOwner.restriction_reason).isNullOrEmpty())
            .addTextIf(
                messageObject.messageOwner != null && messageObject.messageOwner.restriction_reason != null && !ProfileActivityHelper.getRestrictionReasons(messageObject.messageOwner.restriction_reason).isNullOrEmpty(),
                ProfileActivityHelper.getRestrictionReasons(messageObject.messageOwner.restriction_reason),
                13
            )

            .addGapIf(messageObject.document != null && sa.fragment != null && sa.fragment.parentActivity != null)
            .addIf(
                messageObject.document != null && sa.fragment != null && sa.fragment.parentActivity != null,
                R.drawable.msg_openin,
                getString(R.string.OpenInExternalApp)
            ) {
                try {
                    AndroidUtilities.openForView(
                        messageObject,
                        sa.fragment.parentActivity,
                        null,
                        false
                    )
                } catch (e: Exception) {
                    FinegramLogger.e(e)
                }
            }

            .setDimAlpha(100)
            .translate(-dp(15f).toFloat(), 0f)
            .show()
    }

    fun injectChatActivityMsgSlideAction(cf: ChatActivity, msg: MessageObject, isChannel: Boolean, classGuid: Int) {
        when (FinegramMessagesConfig.messageSlideAction) {
            FinegramMessagesConfig.MESSAGE_SLIDE_ACTION_REPLY -> {
                cf.showFieldPanelForReply(msg)
            }
            FinegramMessagesConfig.MESSAGE_SLIDE_ACTION_SAVE -> {
                val chatID = getCustomChatID()

                cf.sendMessagesHelper.sendMessage(arrayListOf(msg), chatID, false, false, true, 0, 0)

                cf.createUndoView()
                if (cf.undoView == null) {
                    return
                }
                if (!BulletinFactory.of(cf).showForwardedBulletinWithTag(chatID, arrayListOf(msg).size)) {
                    cf.undoView!!.showWithAction(chatID, UndoView.ACTION_FWD_MESSAGES, arrayListOf(msg).size)
                }
            }
            FinegramMessagesConfig.MESSAGE_SLIDE_ACTION_TRANSLATE -> {
                val languageAndTextToTranslate: String = msg.messageOwner.message
                val toLang = TranslateAlert2.getToLanguage()
                val alert = TranslateAlert2.showAlert(
                    cf.context,
                    cf,
                    UserConfig.selectedAccount,
                    languageAndTextToTranslate,
                    toLang,
                    languageAndTextToTranslate,
                    null,
                    false,
                    null
                ) { cf.dimBehindView(false) }
                alert.setDimBehindAlpha(100)
                alert.setDimBehind(true)
            }
            FinegramMessagesConfig.MESSAGE_SLIDE_ACTION_TRANSLATE_GEMINI -> {
                if (msg == null || msg.messageOwner == null || msg.messageOwner.message == null) {
                    return
                }

                GeminiResultsBottomSheet.setMessageObject(msg)
                GeminiResultsBottomSheet.setCurrentChat(cf.currentChat)
                cf.chatActivityHelper.processGeminiWithText(cf, msg, null, true, false)
            }
            FinegramMessagesConfig.MESSAGE_SLIDE_ACTION_DIRECT_SHARE -> {
                cf.showDialog(object : ShareAlert(cf.parentActivity, arrayListOf(msg), null, isChannel, null, false) {
                    override fun dismissInternal() {
                        super.dismissInternal()
                        AndroidUtilities.requestAdjustResize(cf.parentActivity, classGuid)
                        if (cf.chatActivityEnterView.isVisible) {
                            cf.fragmentView.requestLayout()
                        }
                        cf.updatePinnedMessageView(true)
                    }
                })

                AndroidUtilities.setAdjustResizeToNothing(cf.parentActivity, classGuid)
                cf.fragmentView.requestLayout()
            }
        }
    }

    fun updateStickerSetCache(fragment: BaseFragment, stickerSet: TLRPC.TL_messages_stickerSet, emoji: Boolean) {
        val req = TLRPC.TL_messages_getStickerSet()
        val input = TLRPC.TL_inputStickerSetShortName().apply {
            short_name = stickerSet.set.short_name
        }
        req.stickerset = input

        fragment.connectionsManager.sendRequest(req) { res, err ->
            AndroidUtilities.runOnUIThread {
                if (res is TLRPC.TL_messages_stickerSet) {
                    fragment.mediaDataController.putStickerSet(res, true)

                    if (fragment.parentActivity == null || fragment.context == null) return@runOnUIThread

                } else {
                    BulletinFactory.of(fragment)
                        .createSimpleBulletin(
                            R.raw.error,
                            getString(if (emoji) R.string.AddEmojiNotFound else R.string.AddStickersNotFound)
                        )
                        .show(true)
                }
            }
        }
    }

}
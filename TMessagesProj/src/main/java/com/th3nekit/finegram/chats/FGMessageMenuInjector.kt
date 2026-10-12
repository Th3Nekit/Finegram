/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.chats

import android.text.TextUtils
import android.view.View
import android.widget.LinearLayout
import org.telegram.messenger.AndroidUtilities.dp
import org.telegram.messenger.ChatObject
import org.telegram.messenger.LocaleController
import org.telegram.messenger.LocaleController.getString
import org.telegram.messenger.MessageObject
import org.telegram.messenger.R
import org.telegram.messenger.UserObject
import org.telegram.tgnet.TLRPC
import org.telegram.ui.ActionBar.ActionBarMenuSubItem
import org.telegram.ui.ActionBar.ActionBarPopupWindow
import org.telegram.ui.ActionBar.BaseFragment
import org.telegram.ui.ActionBar.Theme
import org.telegram.ui.ChatActivity
import org.telegram.ui.Components.Reactions.ReactionsLayoutInBubble
import org.telegram.ui.Components.LayoutHelper
import com.th3nekit.finegram.chats.gemini.GeminiResultsBottomSheet
import com.th3nekit.finegram.chats.gemini.GeminiSDKImplementation
import com.th3nekit.finegram.chats.helpers.ChatActivityHelper
import com.th3nekit.finegram.chats.ui.MessageMenuCompactView
import com.th3nekit.finegram.chats.ui.MessageMenuHelper
import com.th3nekit.finegram.core.configs.FinegramMessagesConfig
import com.th3nekit.finegram.helpers.ui.PopupHelper
import com.th3nekit.finegram.preferences.FinegramPreferencesNavigator

object FGMessageMenuInjector {

    fun showGeminiItems(
        chatActivity: ChatActivity,
        popupLayout: ActionBarPopupWindow.ActionBarPopupWindowLayout,
        selectedObject: MessageObject,
    ) {
        val linearLayout = LinearLayout(chatActivity.parentActivity)
        linearLayout.orientation = LinearLayout.VERTICAL

        val isVoiceOrVideoMessage = selectedObject.type == MessageObject.TYPE_VOICE || selectedObject.type == MessageObject.TYPE_ROUND_VIDEO
        val isVoiceMessage = selectedObject.type == MessageObject.TYPE_VOICE
        val isPhoto = selectedObject.type == MessageObject.TYPE_PHOTO
        val showDivider = chatActivity.messageMenuHelper.showDivider()

        val backCell = ActionBarMenuSubItem(chatActivity.parentActivity, true, false, chatActivity.resourceProvider)
        backCell.setItemHeight(46)
        backCell.setTextAndIcon(getString(R.string.Back), R.drawable.msg_arrow_back)
        backCell.textView.setPadding(
            if (LocaleController.isRTL) 0 else dp(40f),
            0,
            if (LocaleController.isRTL) dp(40f) else 0,
            0
        )
        backCell.setOnClickListener {
            popupLayout.swipeBack?.closeForeground()
        }
        linearLayout.addView(
            backCell,
            LayoutHelper.createLinear(
                if (isVoiceOrVideoMessage && !chatActivity.messageMenuHelper.allowNewMessageMenu()) dp(100f) else LayoutHelper.MATCH_PARENT,
                LayoutHelper.WRAP_CONTENT
            )
        )

        if (showDivider) {
            if (chatActivity.messageMenuHelper.allowNewMessageMenu() && chatActivity.messageMenuHelper.showCustomDivider(true)) {
                linearLayout.addView(
                    ActionBarPopupWindow.GapView(
                        chatActivity.context,
                        MessageMenuHelper.getMessageMenuGapColor(chatActivity.resourceProvider),
                        Theme.getColor(Theme.key_windowBackgroundGrayShadow, chatActivity.resourceProvider)
                    ),
                    LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 8)
                )
            } else {
                linearLayout.addView(
                    ActionBarPopupWindow.GapView(chatActivity.context, chatActivity.resourceProvider),
                    LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 8)
                )
            }
        }

        val sections = ArrayList<View>()

        if (!TextUtils.isEmpty(selectedObject.messageOwner.message) &&
            (chatActivity.currentChat != null && ChatObject.canSendMessages(chatActivity.currentChat) || chatActivity.currentUser != null)
        ) {
            val cell = ActionBarMenuSubItem(chatActivity.parentActivity, true, false, chatActivity.resourceProvider)
            cell.setTextAndIcon(getString(R.string.Reply), R.drawable.menu_reply)
            cell.setOnClickListener {
                chatActivity.processSelectedOption(ChatActivityHelper.OPTION_REPLY_GEMINI)
            }
            sections.add(cell)
        }

        if (!TextUtils.isEmpty(selectedObject.messageOwner.message)) {
            val cell = ActionBarMenuSubItem(chatActivity.parentActivity, true, false, chatActivity.resourceProvider)
            cell.setTextAndIcon(getString(R.string.TranslateMessage), R.drawable.msg_translate)
            cell.setOnClickListener {
                chatActivity.processSelectedOption(ChatActivityHelper.OPTION_TRANSLATE_GEMINI)
            }
            sections.add(cell)
        }

        if (!TextUtils.isEmpty(selectedObject.messageOwner.message) || isVoiceMessage) {
            val cell = ActionBarMenuSubItem(chatActivity.parentActivity, true, false, chatActivity.resourceProvider)
            cell.setTextAndIcon(getString(R.string.CP_GeminiAI_Summarize), R.drawable.magic_stick_solar)
            cell.setOnClickListener {
                GeminiResultsBottomSheet.setMessageObject(selectedObject)
                GeminiResultsBottomSheet.setCurrentChat(chatActivity.currentChat)
                if (isVoiceMessage) {
                    GeminiSDKImplementation.injectGeminiForMedia(
                        chatActivity,
                        chatActivity,
                        selectedObject,
                        false,
                        true,
                        true
                    )
                } else {
                    chatActivity.processSelectedOption(ChatActivityHelper.OPTION_SUMMARIZE_GEMINI)
                }
            }
            sections.add(cell)
        }

        if (isVoiceOrVideoMessage) {
            val cell = ActionBarMenuSubItem(chatActivity.parentActivity, true, false, chatActivity.resourceProvider)
            cell.setTextAndIcon(getString(R.string.PremiumPreviewVoiceToText), R.drawable.msg_photo_text_solar)
            cell.setOnClickListener {
                chatActivity.closeMenu()
                chatActivity.processSelectedOption(ChatActivityHelper.OPTION_TRANSCRIBE_GEMINI)
            }
            sections.add(cell)
        }

        if (isPhoto) {
            val cell = ActionBarMenuSubItem(chatActivity.parentActivity, true, false, chatActivity.resourceProvider)
            cell.setTextAndIcon(getString(R.string.AccDescrQuizExplanation), R.drawable.msg_info_solar)
            cell.setOnClickListener {
                chatActivity.closeMenu()
                GeminiResultsBottomSheet.setMessageObject(selectedObject)
                GeminiResultsBottomSheet.setCurrentChat(chatActivity.currentChat)
                GeminiSDKImplementation.injectGeminiForMedia(
                    chatActivity,
                    chatActivity,
                    selectedObject,
                    false,
                    false,
                    false
                )
            }
            sections.add(cell)
        }

        if (isPhoto) {
            val cell = ActionBarMenuSubItem(chatActivity.parentActivity, true, false, chatActivity.resourceProvider)
            cell.setTextAndIcon(getString(R.string.CP_GeminiAI_ExtractText), R.drawable.msg_edit_solar)
            cell.setOnClickListener {
                chatActivity.closeMenu()
                GeminiResultsBottomSheet.setMessageObject(selectedObject)
                GeminiResultsBottomSheet.setCurrentChat(chatActivity.currentChat)
                GeminiSDKImplementation.injectGeminiForMedia(
                    chatActivity,
                    chatActivity,
                    selectedObject,
                    true,
                    false,
                    false
                )
            }
            sections.add(cell)
        }

        if (showDivider) {
            val gap = if (chatActivity.messageMenuHelper.allowNewMessageMenu() && chatActivity.messageMenuHelper.showCustomDivider(true)) {
                ActionBarPopupWindow.GapView(
                    chatActivity.context,
                    MessageMenuHelper.getMessageMenuGapColor(chatActivity.resourceProvider),
                    Theme.getColor(Theme.key_windowBackgroundGrayShadow, chatActivity.resourceProvider)
                )
            } else {
                ActionBarPopupWindow.GapView(chatActivity.context, chatActivity.resourceProvider)
            }
            gap.layoutParams = LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 8)
            sections.add(gap)
        }

        val settingsCell = ActionBarMenuSubItem(chatActivity.parentActivity, true, false, chatActivity.resourceProvider)
        settingsCell.setTextAndIcon(getString(R.string.Settings), R.drawable.msg_settings)
        settingsCell.setOnClickListener {
            chatActivity.closeMenu()
            FinegramPreferencesNavigator.createGemini(chatActivity)
        }
        sections.add(settingsCell)

        for (section in sections) {
            if (section.layoutParams != null) {
                linearLayout.addView(section)
            } else {
                linearLayout.addView(section, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT))
            }
        }

        val foregroundIndex = popupLayout.addViewToSwipeBack(linearLayout)

        val cell = ActionBarMenuSubItem(chatActivity.parentActivity, true, true, chatActivity.resourceProvider)
        cell.setTextAndIcon(getString(R.string.CP_GeminiAI_Header), R.drawable.magic_stick_solar)
        popupLayout.addView(cell)
        cell.setOnClickListener {
            if (chatActivity.contentView == null || chatActivity.parentActivity == null) {
                return@setOnClickListener
            }
            popupLayout.swipeBack?.openForeground(foregroundIndex)
        }

        if (showDivider) {
            if (chatActivity.messageMenuHelper.allowNewMessageMenu() && chatActivity.messageMenuHelper.showCustomDivider(true)) {
                popupLayout.addView(
                    ActionBarPopupWindow.GapView(
                        chatActivity.context,
                        MessageMenuHelper.getMessageMenuGapColor(chatActivity.resourceProvider),
                        Theme.getColor(Theme.key_windowBackgroundGrayShadow, chatActivity.resourceProvider)
                    ),
                    LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 8)
                )
            } else {
                popupLayout.addView(
                    ActionBarPopupWindow.GapView(chatActivity.context, chatActivity.resourceProvider),
                    LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 8)
                )
            }
        }
    }

    fun injectOpenInExternal(
        noforwardsOrPaidMedia: Boolean,
        message: MessageObject?,
        items: ArrayList<CharSequence?>,
        options: ArrayList<Int?>,
        icons: ArrayList<Int?>
    ) {
        if (FinegramMessagesConfig.openInExternalApp && message != null && message.document != null && !noforwardsOrPaidMedia) {
            items.add(getString(R.string.OpenInExternalApp))
            options.add(ChatActivityHelper.OPTION_OPEN_IN)
            icons.add(R.drawable.msg_openin)
        }
    }

    fun injectCopyPhoto(
        items: ArrayList<CharSequence?>,
        options: ArrayList<Int?>,
        icons: ArrayList<Int?>
    ) {
        if (FinegramMessagesConfig.showCopyPhoto) {
            items.add(getString(R.string.FG_CopyPhoto))
            options.add(ChatActivityHelper.OPTION_COPY_PHOTO)
            icons.add(R.drawable.msg_copy)
        }
        if (FinegramMessagesConfig.showCopyPhotoAsSticker) {
            items.add(getString(R.string.FG_CopyPhotoAsSticker))
            options.add(ChatActivityHelper.OPTION_COPY_PHOTO_AS_STICKER)
            icons.add(R.drawable.msg_sticker)
        }
    }

    fun injectClearFromCache(
        items: ArrayList<CharSequence?>,
        options: ArrayList<Int?>,
        icons: ArrayList<Int?>
    ) {
        if (FinegramMessagesConfig.showClearFromCache) {
            items.add(getString(R.string.FG_ClearFromCache))
            options.add(ChatActivityHelper.OPTION_CLEAR_FROM_CACHE)
            icons.add(R.drawable.msg_clear)
        }
    }

    fun injectForwardWoAuthorship(
        selectedObject: MessageObject,
        chatMode: Int,
        items: ArrayList<CharSequence?>,
        options: ArrayList<Int?>,
        icons: ArrayList<Int?>
    ) {
        if (!selectedObject.isSponsored && chatMode != ChatActivity.MODE_QUICK_REPLIES && chatMode != ChatActivity.MODE_SCHEDULED
            && (!selectedObject.needDrawBluredPreview() || selectedObject.hasExtendedMediaPreview()) && !selectedObject.isLiveLocation && selectedObject.type != MessageObject.TYPE_PHONE_CALL
            && selectedObject.type != MessageObject.TYPE_GIFT_PREMIUM && selectedObject.type != MessageObject.TYPE_GIFT_PREMIUM_CHANNEL && selectedObject.type != MessageObject.TYPE_SUGGEST_PHOTO
            && !selectedObject.isWallpaperAction && !selectedObject.isExpiredStory && selectedObject.type != MessageObject.TYPE_STORY_MENTION && selectedObject.type != MessageObject.TYPE_GIFT_STARS
        ) {
            items.add(
                getString(R.string.Forward) + " " + getString(
                    R.string.FG_Without_Authorship
                )
            )
            options.add(ChatActivityHelper.OPTION_FORWARD_WO_AUTHOR)
            icons.add(R.drawable.msg_forward)
        }
    }

    fun injectViewHistory(
        items: ArrayList<CharSequence?>,
        options: ArrayList<Int?>,
        icons: ArrayList<Int?>
    ) {
        if (FinegramMessagesConfig.showViewHistory) {
            items.add(getString(R.string.AvatarPreviewSearchMessages))
            options.add(ChatActivityHelper.OPTION_VIEW_HISTORY)
            icons.add(R.drawable.msg_search)
        }
    }

    fun injectSaveMessage(
        message: MessageObject,
        chatMode: Int,
        currentUser: TLRPC.User?,
        items: ArrayList<CharSequence?>,
        options: ArrayList<Int?>,
        icons: ArrayList<Int?>
    ) {
        if (FinegramMessagesConfig.showSaveMessage && chatMode != ChatActivity.MODE_SCHEDULED && !UserObject.isUserSelf(
                currentUser
            ) && !message.isSponsored
        ) {
            items.add(getString(R.string.FG_ToSaved))
            options.add(ChatActivityHelper.OPTION_SAVE_MESSAGE_CHAT)
            icons.add(R.drawable.msg_saved)
        }
    }

    fun injectViewStatistics(
        chatActivity: ChatActivity,
        message: MessageObject,
        items: ArrayList<CharSequence?>,
        options: ArrayList<Int?>,
        icons: ArrayList<Int?>
    ) {
        if (message.messageOwner.forwards > 0 && ChatObject.hasAdminRights(chatActivity.currentChat) && !message.isForwarded) {
            items.add(getString(R.string.ViewStatistics))
            options.add(ChatActivity.OPTION_STATISTICS)
            icons.add(R.drawable.msg_stats)
        }
    }

    fun injectDownloadSticker(
        selectedObject: MessageObject?,
        items: ArrayList<CharSequence?>,
        options: ArrayList<Int?>,
        icons: ArrayList<Int?>
    ) {
        if (selectedObject?.isAnimatedSticker == false) {
            items.add(getString(R.string.FG_SaveSticker))
            options.add(ChatActivityHelper.OPTION_DOWNLOAD_STICKER)
            icons.add(R.drawable.msg_gallery)
        }
    }

    fun injectImportSettings(
        items: ArrayList<CharSequence?>,
        options: ArrayList<Int?>,
        icons: ArrayList<Int?>
    ) {
        items.add(getString(R.string.SaveToDownloads))
        options.add(ChatActivity.OPTION_SAVE_TO_DOWNLOADS_OR_MUSIC)
        icons.add(R.drawable.msg_download)

        options.add(ChatActivityHelper.OPTION_IMPORT_SETTINGS)
        items.add(getString(R.string.FG_ImportSettings))
        icons.add(R.drawable.msg_customize)
    }

    fun injectQuote(
        message: MessageObject?,
        items: ArrayList<CharSequence?>,
        options: ArrayList<Int?>,
        icons: ArrayList<Int?>
    ) {
        if (message == null || !FinegramMessagesConfig.createQuoteImage) {
            return
        }

        if (message.messageOwner?.action != null) {
            return
        }
        items.add(getString(R.string.FG_Quote_Create))
        options.add(ChatActivityHelper.OPTION_CREATE_QUOTE)
        icons.add(R.drawable.msg_photo_settings)
    }

    fun injectJSON(
        chatActivity: ChatActivity?,
        force: Boolean,
        items: ArrayList<CharSequence?>,
        options: ArrayList<Int?>,
        icons: ArrayList<Int?>
    ) {

        val show = force || (chatActivity != null && FinegramMessagesConfig.showJSON &&
                !(chatActivity.messageMenuHelper.allowNewMessageMenu() && MessageMenuCompactView.allowCompactStyle()))

        if (show) {
            items.add("JSON")
            options.add(ChatActivityHelper.OPTION_DETAILS)
            icons.add(R.drawable.icon_json_solar)
        }
    }

    fun injectForwardWithoutCaption(
        selectedObject: MessageObject?,
        selectedObjectGroup: MessageObject.GroupedMessages?,
        noforwardsOrPaidMedia: Boolean,
        items: ArrayList<CharSequence?>,
        options: ArrayList<Int?>,
        icons: ArrayList<Int?>
    ) {
        if (!FinegramMessagesConfig.forwardWithoutCaption || selectedObject == null) return
        if (noforwardsOrPaidMedia || selectedObject.isSponsored) return
        if (selectedObject.isLiveLocation || selectedObject.isExpiredStory) return
        if (!hasAnyCaption(selectedObject, selectedObjectGroup)) return

        items.add(getString(R.string.FG_ForwardWoCaption))
        options.add(ChatActivityHelper.OPTION_FORWARD_WO_CAPTION)
        icons.add(R.drawable.msg_forward)
    }

    private fun hasAnyCaption(
        message: MessageObject?,
        group: MessageObject.GroupedMessages?
    ): Boolean {
        if (message?.caption?.isNotEmpty() == true) return true
        val messages = group?.messages ?: return false
        return messages.any { it?.caption?.isNotEmpty() == true }
    }

    fun injectGetCustomReactions(
        selectedObject: MessageObject?,
        items: ArrayList<CharSequence?>,
        options: ArrayList<Int?>,
        icons: ArrayList<Int?>
    ) {
        if (!FinegramMessagesConfig.getCustomReactions || selectedObject == null) return
        val reactions = selectedObject.messageOwner?.reactions?.results ?: return
        val hasCustom = reactions.any { result ->
            ReactionsLayoutInBubble.VisibleReaction.fromTL(result.reaction).documentId != 0L
        }
        if (!hasCustom) return

        items.add(getString(R.string.FG_GetCustomReactions))
        options.add(ChatActivityHelper.OPTION_GET_CUSTOM_REACTIONS)
        icons.add(R.drawable.msg_emoji_smiles)
    }

    fun removeItems(
        chatActivity: ChatActivity,
        selectedObject: MessageObject?,
        allowEdit: Boolean,
        noforwardsOrPaidMedia: Boolean,
        items: ArrayList<CharSequence?>,
        options: ArrayList<Int?>,
        icons: ArrayList<Int?>
    ) {
        if (selectedObject == null) return

        val toRemove = mutableListOf<Int>()

        options.forEachIndexed { index, option ->

            val remove = when (option) {
                ChatActivity.OPTION_REPLY -> !FinegramMessagesConfig.showReply

                ChatActivity.OPTION_SAVE_TO_GALLERY, ChatActivity.OPTION_SAVE_TO_GALLERY2 -> noforwardsOrPaidMedia || !FinegramMessagesConfig.showSaveToGallery
                ChatActivity.OPTION_SAVE_TO_DOWNLOADS_OR_MUSIC -> noforwardsOrPaidMedia || !FinegramMessagesConfig.showSaveToDownloads
                ChatActivity.OPTION_SHARE -> noforwardsOrPaidMedia || !FinegramMessagesConfig.showShare

                ChatActivity.OPTION_COPY -> noforwardsOrPaidMedia

                ChatActivityHelper.OPTION_COPY_PHOTO ->
                    noforwardsOrPaidMedia || !FinegramMessagesConfig.showCopyPhoto
                ChatActivityHelper.OPTION_COPY_PHOTO_AS_STICKER ->
                    noforwardsOrPaidMedia || !FinegramMessagesConfig.showCopyPhotoAsSticker

                ChatActivity.OPTION_FORWARD ->
                    noforwardsOrPaidMedia || !FinegramMessagesConfig.showForward

                ChatActivityHelper.OPTION_FORWARD_WO_AUTHOR ->
                    noforwardsOrPaidMedia || !FinegramMessagesConfig.showForwardWoAuthorship

                else -> false
            }

            if (remove) toRemove.add(index)
        }

        for (i in toRemove.asReversed()) {
            options.removeAt(i)
            items.removeAt(i)
            icons.removeAt(i)
        }

        reorder(items, options, icons)
    }

    private fun reorder(
        items: ArrayList<CharSequence?>,
        options: ArrayList<Int?>,
        icons: ArrayList<Int?>
    ) {
        val order = FinegramMessagesConfig.messageMenuOrder()
        if (order.isEmpty()) return
        if (options.size != items.size || options.size != icons.size) return
        if (options.size <= 1) return

        val taken = BooleanArray(options.size)
        val outItems = ArrayList<CharSequence?>(options.size)
        val outOptions = ArrayList<Int?>(options.size)
        val outIcons = ArrayList<Int?>(options.size)

        for (wanted in order) {
            val index = options.indexOfFirst { it != null && normalizeOption(it) == wanted }
            if (index < 0 || taken[index]) continue
            taken[index] = true
            outItems.add(items[index])
            outOptions.add(options[index])
            outIcons.add(icons[index])
        }
        for (index in options.indices) {
            if (taken[index]) continue
            outItems.add(items[index])
            outOptions.add(options[index])
            outIcons.add(icons[index])
        }

        items.clear(); items.addAll(outItems)
        options.clear(); options.addAll(outOptions)
        icons.clear(); icons.addAll(outIcons)
    }

    private fun normalizeOption(option: Int): Int =
        if (option == ChatActivity.OPTION_SAVE_TO_GALLERY2) ChatActivity.OPTION_SAVE_TO_GALLERY else option

    @JvmStatic
    fun playHaptic(view: android.view.View?) {
        if (view == null || !FinegramMessagesConfig.messageMenuHaptic) return
        try {
            view.performHapticFeedback(android.view.HapticFeedbackConstants.LONG_PRESS)
        } catch (e: Throwable) {

        }
    }

    fun showMessageMenuItemsConfigurator(fragment: BaseFragment) {
        val menuItems = listOf(
            MenuItemConfig(
                getString(R.string.SaveForNotifications),
                R.drawable.msg_tone_add,
                { FinegramMessagesConfig.showSaveForNotifications },
                { FinegramMessagesConfig.showSaveForNotifications = !FinegramMessagesConfig.showSaveForNotifications },
                true
            ),
            MenuItemConfig(
                getString(R.string.CP_GeminiAI_Header),
                R.drawable.magic_stick_solar,
                { FinegramMessagesConfig.showGemini },
                { FinegramMessagesConfig.showGemini = !FinegramMessagesConfig.showGemini },
                true
            ),
            MenuItemConfig(
                getString(R.string.OpenInExternalApp),
                R.drawable.msg_openin,
                { FinegramMessagesConfig.openInExternalApp },
                { FinegramMessagesConfig.openInExternalApp = !FinegramMessagesConfig.openInExternalApp }
            ),
            MenuItemConfig(
                getString(R.string.Reply),
                R.drawable.menu_reply,
                { FinegramMessagesConfig.showReply },
                { FinegramMessagesConfig.showReply = !FinegramMessagesConfig.showReply }
            ),
            MenuItemConfig(
                getString(R.string.SaveToGallery),
                R.drawable.msg_gallery,
                { FinegramMessagesConfig.showSaveToGallery },
                { FinegramMessagesConfig.showSaveToGallery = !FinegramMessagesConfig.showSaveToGallery }
            ),
            MenuItemConfig(
                getString(R.string.FG_CopyPhoto),
                R.drawable.msg_copy,
                { FinegramMessagesConfig.showCopyPhoto },
                { FinegramMessagesConfig.showCopyPhoto = !FinegramMessagesConfig.showCopyPhoto }
            ),
            MenuItemConfig(
                getString(R.string.FG_CopyPhotoAsSticker),
                R.drawable.msg_copy,
                { FinegramMessagesConfig.showCopyPhotoAsSticker },
                { FinegramMessagesConfig.showCopyPhotoAsSticker = !FinegramMessagesConfig.showCopyPhotoAsSticker }
            ),
            MenuItemConfig(
                getString(R.string.SaveToDownloads),
                R.drawable.msg_download,
                { FinegramMessagesConfig.showSaveToDownloads },
                { FinegramMessagesConfig.showSaveToDownloads = !FinegramMessagesConfig.showSaveToDownloads }
            ),
            MenuItemConfig(
                getString(R.string.ShareFile),
                R.drawable.msg_shareout,
                { FinegramMessagesConfig.showShare },
                { FinegramMessagesConfig.showShare = !FinegramMessagesConfig.showShare }
            ),
            MenuItemConfig(
                getString(R.string.FG_ClearFromCache),
                R.drawable.msg_clear,
                { FinegramMessagesConfig.showClearFromCache },
                { FinegramMessagesConfig.showClearFromCache = !FinegramMessagesConfig.showClearFromCache }
            ),
            MenuItemConfig(
                getString(R.string.Forward),
                R.drawable.msg_forward,
                { FinegramMessagesConfig.showForward },
                { FinegramMessagesConfig.showForward = !FinegramMessagesConfig.showForward }
            ),
            MenuItemConfig(
                getString(R.string.Forward) + " " + getString(R.string.FG_Without_Authorship),
                R.drawable.msg_forward,
                { FinegramMessagesConfig.showForwardWoAuthorship },
                { FinegramMessagesConfig.showForwardWoAuthorship = !FinegramMessagesConfig.showForwardWoAuthorship }
            ),
            MenuItemConfig(
                getString(R.string.AvatarPreviewSearchMessages),
                R.drawable.msg_search,
                { FinegramMessagesConfig.showViewHistory },
                { FinegramMessagesConfig.showViewHistory = !FinegramMessagesConfig.showViewHistory }
            ),
            MenuItemConfig(
                getString(R.string.FG_ToSaved),
                R.drawable.msg_saved,
                { FinegramMessagesConfig.showSaveMessage },
                { FinegramMessagesConfig.showSaveMessage = !FinegramMessagesConfig.showSaveMessage }
            ),
            MenuItemConfig(
                getString(R.string.ReportChat),
                R.drawable.msg_report,
                { FinegramMessagesConfig.showReport },
                { FinegramMessagesConfig.showReport = !FinegramMessagesConfig.showReport }
            ),
            MenuItemConfig(
                "JSON",
                R.drawable.icon_json_solar,
                { FinegramMessagesConfig.showJSON },
                { FinegramMessagesConfig.showJSON = !FinegramMessagesConfig.showJSON }
            )
        )

        val prefTitle = ArrayList<String>()
        val prefIcon = ArrayList<Int>()
        val prefCheck = ArrayList<Boolean>()
        val prefDivider = ArrayList<Boolean>()
        val clickListener = ArrayList<Runnable>()

        for (item in menuItems) {
            prefTitle.add(item.titleRes)
            prefIcon.add(item.iconRes)
            prefCheck.add(item.isChecked())
            prefDivider.add(item.divider)
            clickListener.add(Runnable { item.toggle() })
        }

        PopupHelper.showSwitchAlert(
            getString(R.string.CP_MessageMenuItems),
            fragment,
            prefTitle,
            prefIcon,
            prefCheck,
            null,
            prefDivider,
            clickListener,
            null
        )

    }

    data class MenuItemConfig(
        val titleRes: String,
        val iconRes: Int,
        val isChecked: () -> Boolean,
        val toggle: () -> Unit,
        val divider: Boolean = false
    )

}

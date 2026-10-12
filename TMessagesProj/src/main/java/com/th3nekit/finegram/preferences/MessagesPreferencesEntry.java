/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences;

import static org.telegram.messenger.LocaleController.getString;

import static com.th3nekit.finegram.preferences.helpers.SettingsHelper.applyNewSpan;

import android.view.View;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.Components.IconBackgroundColors;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import org.telegram.ui.SettingsActivity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import com.th3nekit.finegram.core.configs.FinegramChatsConfig;
import com.th3nekit.finegram.core.configs.FinegramMessagesConfig;
import com.th3nekit.finegram.core.helpers.DeeplinkHelper;
import com.th3nekit.finegram.helpers.ui.PopupHelper;
import com.th3nekit.finegram.preferences.helpers.AlertDialogSwitchers;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class MessagesPreferencesEntry extends BaseCGPreferencesEntry {

    private final int messageMenuRow = 1, messageSizeRow = 2, directShareRow = 3,
            wideMessagesLayout = 4, hideTimeOnStickersRow = 5, showForwardDateRow = 6, pencilIconForEditedRow = 7,
            hideBubbleTailRow = 19, renderFormulasRow = 20, quoteImageRow = 21;

    private final int geminiSettingsRow = 8, voiceTranscriptionRow = 9;

    private final int messageFilterRow = 10, leftBottomBtnRow = 11, doubleTapRow = 12, slideActionRow = 13, deleteForAllRow = 14;

    private final int reactionsOverlayRow = 15, reactionAnimationRow = 16, tapsOnPremiumStickersRow = 17, premiumStickersAutoplayRow = 18;

    private final int localPremiumEmojisRow = 22;
    private final int largePhotosRow = 23;
    private final int preReformRow = 24;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.MessagesSettings);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        items.add(UItem.asHeader(getString(R.string.AP_Header_Appearance)));
        items.add(UItem.asButton(messageMenuRow, R.drawable.msg_list, getString(R.string.CP_MessageMenu)).slug(DeeplinkHelper.DeepLinksRepo.FG_Message_Menu));
        items.add(UItem.asButton(messageSizeRow, R.drawable.msg_photo_settings, getString(R.string.CP_Messages_Size)).slug("bubbleSize"));
        items.add(UItem.asButton(directShareRow, R.drawable.msg_share, getString(R.string.DirectShare)).slug("directShare"));
        items.add(
                SettingsHelper.asSwitchCG(
                        wideMessagesLayout,
                        applyNewSpan(getString(R.string.CP_WideMessagesLayout)),
                        getString(R.string.CP_WideMessagesLayout_Desc)
                )
                .setChecked(FinegramMessagesConfig.INSTANCE.getWideMessagesLayout())
        .slug("wideLayout"));
        items.add(SettingsHelper.asSwitchCG(hideBubbleTailRow, getString(R.string.FG_HideBubbleTail), getString(R.string.FG_HideBubbleTail_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getHideBubbleTail())
        );
        items.add(SettingsHelper.asSwitchCG(largePhotosRow, getString(R.string.FG_LargePhotos), getString(R.string.FG_LargePhotos_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getLargePhotos())
        );
        items.add(SettingsHelper.asSwitchCG(hideTimeOnStickersRow, getString(R.string.CP_TimeOnStick))
                .setChecked(FinegramMessagesConfig.INSTANCE.getHideStickerTime())
        .slug("hideTimeOnStickers"));
        items.add(SettingsHelper.asSwitchCG(showForwardDateRow, getString(R.string.CP_ForwardMsgDate))
                .setChecked(FinegramMessagesConfig.INSTANCE.getMsgForwardDate())
        .slug("showForwardDate"));
        items.add(SettingsHelper.asSwitchCG(pencilIconForEditedRow, getString(R.string.AP_ShowPencilIcon))
                .setChecked(FinegramMessagesConfig.INSTANCE.getShowPencilIcon())
        .slug("showPencilIcon"));
        items.add(SettingsHelper.asSwitchCG(renderFormulasRow, getString(R.string.FG_Formulas), getString(R.string.FG_Formulas_Desc))
                .setChecked(FinegramChatsConfig.INSTANCE.getRenderFormulas())
        );
        items.add(SettingsHelper.asSwitchCG(quoteImageRow, getString(R.string.FG_Quote_Setting), getString(R.string.FG_Quote_Setting_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getCreateQuoteImage())
        );
        items.add(UItem.asShadow(null));

        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        geminiSettingsRow, 0xFF4796E3, 0xFF9177C7,
                        R.drawable.settings_magic_stick_filled_solar,
                        getString(R.string.CP_GeminiAI_Header)
                )
        .slug(DeeplinkHelper.DeepLinksRepo.FG_Gemini));
        items.add(UItem.asButton(voiceTranscriptionRow, getString(R.string.CP_GeminiAI_VoiceTranscriptionProvider), getTranscriptionProviderValue()).slug("vttProvider"));
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.ActionsChartTitle)));
        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        messageFilterRow, IconBackgroundColors.ORANGE.top, IconBackgroundColors.ORANGE.bottom,
                        R.drawable.settings_message_filrers_filled_solar,
                        getString(R.string.CP_Message_Filtering),
                        getString(R.string.FGP_MessagesFilter_Desc),
                        true
                )
        .slug(DeeplinkHelper.DeepLinksRepo.FG_Message_Filters));
        items.add(UItem.asButton(leftBottomBtnRow, getString(R.string.CP_LeftBottomButtonAction), getLeftBottomButtonValue()).slug("leftButtonAction"));
        items.add(UItem.asButton(doubleTapRow, getString(R.string.CP_DoubleTapAction), getDoubleTapActionValue()).slug("doubleTapAction"));
        items.add(UItem.asButton(slideActionRow, getString(R.string.FG_MsgSlideAction), getSlideActionValue()).slug("slideAction"));
        items.add(SettingsHelper.asSwitchCG(deleteForAllRow, getString(R.string.CP_DeleteForAll), getString(R.string.CP_DeleteForAll_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getDeleteForAll())
        .slug("deleteForAll"));
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.TelegramPremium)));
        items.add(SettingsHelper.asSwitchCG(reactionsOverlayRow, getString(R.string.CP_DisableReactionsOverlay), getString(R.string.CP_DisableReactionsOverlay_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getDisableReactionsOverlay())
        .slug("reactionsOverlay"));
        items.add(SettingsHelper.asSwitchCG(reactionAnimationRow, getString(R.string.CP_DisableReactionAnim), getString(R.string.CP_DisableReactionAnim_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getDisableReactionAnim())
        .slug("reactionsAnimation"));
        items.add(SettingsHelper.asSwitchCG(tapsOnPremiumStickersRow, getString(R.string.CP_DisablePremStickAnim), getString(R.string.CP_DisablePremStickAnim_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getDisablePremStickAnim())
        .slug("tapsOnPremStickers"));
        items.add(SettingsHelper.asSwitchCG(premiumStickersAutoplayRow, getString(R.string.CP_DisablePremStickAutoPlay), getString(R.string.CP_DisablePremStickAutoPlay_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getDisablePremStickAutoPlay())
        .slug("premStickersAutoPlay"));
        items.add(SettingsHelper.asSwitchCG(preReformRow, getString(R.string.FG_PreReform), getString(R.string.FG_PreReform_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getPreReformRussian())
        );
        items.add(SettingsHelper.asSwitchCG(localPremiumEmojisRow, getString(R.string.FG_LocalPremiumEmojis), getString(R.string.FG_LocalPremiumEmojis_Desc))
                .setChecked(FinegramMessagesConfig.INSTANCE.getLocalPremiumEmojis())
        );
        items.add(UItem.asShadow(null));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == preReformRow) {
            FinegramMessagesConfig.INSTANCE.setPreReformRussian(!FinegramMessagesConfig.INSTANCE.getPreReformRussian());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getPreReformRussian());
        } else if (item.id == largePhotosRow) {
            FinegramMessagesConfig.INSTANCE.setLargePhotos(!FinegramMessagesConfig.INSTANCE.getLargePhotos());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getLargePhotos());
        } else if (item.id == localPremiumEmojisRow) {
            FinegramMessagesConfig.INSTANCE.setLocalPremiumEmojis(!FinegramMessagesConfig.INSTANCE.getLocalPremiumEmojis());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getLocalPremiumEmojis());
        } else if (item.id == messageMenuRow) {
            FinegramPreferencesNavigator.INSTANCE.createMessageMenu(this);
        } else if (item.id == messageSizeRow) {
            AlertDialogSwitchers.showMessageSize(this);
        } else if (item.id == wideMessagesLayout) {
            FinegramMessagesConfig.INSTANCE.setWideMessagesLayout(!FinegramMessagesConfig.INSTANCE.getWideMessagesLayout());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getWideMessagesLayout());
        } else if (item.id == directShareRow) {
            showDirectShareConfigurator(this);
        } else if (item.id == quoteImageRow) {
            FinegramMessagesConfig.INSTANCE.setCreateQuoteImage(!FinegramMessagesConfig.INSTANCE.getCreateQuoteImage());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getCreateQuoteImage());
        } else if (item.id == renderFormulasRow) {
            FinegramChatsConfig.INSTANCE.setRenderFormulas(!FinegramChatsConfig.INSTANCE.getRenderFormulas());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getRenderFormulas());
        } else if (item.id == hideBubbleTailRow) {
            FinegramMessagesConfig.INSTANCE.setHideBubbleTail(!FinegramMessagesConfig.INSTANCE.getHideBubbleTail());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getHideBubbleTail());
            org.telegram.ui.ActionBar.Theme.reloadAllResources(getContext());
        } else if (item.id == hideTimeOnStickersRow) {
            FinegramMessagesConfig.INSTANCE.setHideStickerTime(!FinegramMessagesConfig.INSTANCE.getHideStickerTime());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getHideStickerTime());
        } else if (item.id == showForwardDateRow) {
            FinegramMessagesConfig.INSTANCE.setMsgForwardDate(!FinegramMessagesConfig.INSTANCE.getMsgForwardDate());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getMsgForwardDate());
        } else if (item.id == pencilIconForEditedRow) {
            FinegramMessagesConfig.INSTANCE.setShowPencilIcon(!FinegramMessagesConfig.INSTANCE.getShowPencilIcon());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getShowPencilIcon());
        } else if (item.id == geminiSettingsRow) {
            FinegramPreferencesNavigator.INSTANCE.createGemini(this);
        } else if (item.id == voiceTranscriptionRow) {
            showTranscriptionProviderSelector(() -> SettingsHelper.updateButtonValue(view, getTranscriptionProviderValue()));
        } else if (item.id == messageFilterRow) {
            FinegramPreferencesNavigator.INSTANCE.createMessageFilter(this);
        } else if (item.id == leftBottomBtnRow) {
            showLeftBottomButtonSelector(() -> SettingsHelper.updateButtonValue(view, getLeftBottomButtonValue()));
        } else if (item.id == doubleTapRow) {
            showDoubleTapSelector(() -> SettingsHelper.updateButtonValue(view, getDoubleTapActionValue()));
        } else if (item.id == slideActionRow) {
            showSlideActionSelector(() -> SettingsHelper.updateButtonValue(view, getSlideActionValue()));
        } else if (item.id == deleteForAllRow) {
            FinegramMessagesConfig.INSTANCE.setDeleteForAll(!FinegramMessagesConfig.INSTANCE.getDeleteForAll());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getDeleteForAll());
        } else if (item.id == reactionsOverlayRow) {
            FinegramMessagesConfig.INSTANCE.setDisableReactionsOverlay(!FinegramMessagesConfig.INSTANCE.getDisableReactionsOverlay());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getDisableReactionsOverlay());

            showRestartBulletin();
        } else if (item.id == reactionAnimationRow) {
            FinegramMessagesConfig.INSTANCE.setDisableReactionAnim(!FinegramMessagesConfig.INSTANCE.getDisableReactionAnim());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getDisableReactionAnim());

            showRestartBulletin();
        } else if (item.id == tapsOnPremiumStickersRow) {
            FinegramMessagesConfig.INSTANCE.setDisablePremStickAnim(!FinegramMessagesConfig.INSTANCE.getDisablePremStickAnim());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getDisablePremStickAnim());

            showRestartBulletin();
        } else if (item.id == premiumStickersAutoplayRow) {
            FinegramMessagesConfig.INSTANCE.setDisablePremStickAutoPlay(!FinegramMessagesConfig.INSTANCE.getDisablePremStickAutoPlay());
            SettingsHelper.updateCheckState(view, FinegramMessagesConfig.INSTANCE.getDisablePremStickAutoPlay());

            showRestartBulletin();
        }
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        if (item.id == geminiSettingsRow) {
            AndroidUtilities.addToClipboard("tg://" + DeeplinkHelper.DeepLinksRepo.FG_Gemini);
            return true;
        } else if (item.id == messageMenuRow) {
            AndroidUtilities.addToClipboard("tg://" + DeeplinkHelper.DeepLinksRepo.FG_Message_Menu);
            return true;
        } else if (item.id == messageFilterRow) {
            AndroidUtilities.addToClipboard("tg://" + DeeplinkHelper.DeepLinksRepo.FG_Message_Filters);
            return true;
        }
        return false;
    }

    private void showDirectShareConfigurator(BaseFragment fragment) {
        List<ChatsPreferencesEntry.MenuItemConfig> menuItems = Arrays.asList(
                new ChatsPreferencesEntry.MenuItemConfig(
                        getString(R.string.RepostToStory),
                        R.drawable.large_repost_story,
                        FinegramMessagesConfig.INSTANCE::getShareDrawStoryButton,
                        () -> FinegramMessagesConfig.INSTANCE.setShareDrawStoryButton(!FinegramMessagesConfig.INSTANCE.getShareDrawStoryButton()),
                        true,
                        false
                ),
                new ChatsPreferencesEntry.MenuItemConfig(
                        getString(R.string.FilterChats),
                        0,
                        FinegramMessagesConfig.INSTANCE::getUsersDrawShareButton,
                        () -> FinegramMessagesConfig.INSTANCE.setUsersDrawShareButton(!FinegramMessagesConfig.INSTANCE.getUsersDrawShareButton()),
                        false,
                        false
                ),
                new ChatsPreferencesEntry.MenuItemConfig(
                        getString(R.string.FilterGroups),
                        0,
                        FinegramMessagesConfig.INSTANCE::getSupergroupsDrawShareButton,
                        () -> FinegramMessagesConfig.INSTANCE.setSupergroupsDrawShareButton(!FinegramMessagesConfig.INSTANCE.getSupergroupsDrawShareButton()),
                        false,
                        false
                ),
                new ChatsPreferencesEntry.MenuItemConfig(
                        getString(R.string.FilterChannels),
                        0,
                        FinegramMessagesConfig.INSTANCE::getChannelsDrawShareButton,
                        () -> FinegramMessagesConfig.INSTANCE.setChannelsDrawShareButton(!FinegramMessagesConfig.INSTANCE.getChannelsDrawShareButton()),
                        false,
                        false
                ),
                new ChatsPreferencesEntry.MenuItemConfig(
                        getString(R.string.FilterBots),
                        0,
                        FinegramMessagesConfig.INSTANCE::getBotsDrawShareButton,
                        () -> FinegramMessagesConfig.INSTANCE.setBotsDrawShareButton(!FinegramMessagesConfig.INSTANCE.getBotsDrawShareButton()),
                        false,
                        false
                ),
                new ChatsPreferencesEntry.MenuItemConfig(
                        getString(R.string.StickersName),
                        0,
                        FinegramMessagesConfig.INSTANCE::getStickersDrawShareButton,
                        () -> FinegramMessagesConfig.INSTANCE.setStickersDrawShareButton(!FinegramMessagesConfig.INSTANCE.getStickersDrawShareButton()),
                        false,
                        false
                )
        );

        handleMenuAlert(getString(R.string.DirectShare), menuItems, fragment);
    }

    private String getTranscriptionProviderValue() {
        return FinegramMessagesConfig.INSTANCE.getVoiceTranscriptionProvider() == FinegramMessagesConfig.TRANSCRIPTION_PROVIDER_GEMINI
                ? getString(R.string.CP_GeminiAI_Header) : getString(R.string.AppName);
    }

    private void showTranscriptionProviderSelector(Runnable runnable) {
        ArrayList<String> configStringKeys = new ArrayList<>();
        ArrayList<Integer> configValues = new ArrayList<>();

        configStringKeys.add(getString(R.string.CP_GeminiAI_Header));
        configValues.add(FinegramMessagesConfig.TRANSCRIPTION_PROVIDER_GEMINI);

        configStringKeys.add(getString(R.string.AppName));
        configValues.add(FinegramMessagesConfig.TRANSCRIPTION_PROVIDER_TELEGRAM);

        PopupHelper.show(configStringKeys, getString(R.string.CP_GeminiAI_VoiceTranscriptionProvider), configValues.indexOf(FinegramMessagesConfig.INSTANCE.getVoiceTranscriptionProvider()), getContext(), i -> {
            FinegramMessagesConfig.INSTANCE.setVoiceTranscriptionProvider(configValues.get(i));
            if (runnable != null) runnable.run();
        });
    }

    private String getLeftBottomButtonValue() {
        return switch (FinegramMessagesConfig.INSTANCE.getLeftBottomButton()) {
            case FinegramMessagesConfig.LEFT_BUTTON_REPLY -> getString(R.string.Reply);
            case FinegramMessagesConfig.LEFT_BUTTON_SAVE_MESSAGE -> getString(R.string.FG_ToSaved);
            case FinegramMessagesConfig.LEFT_BUTTON_DIRECT_SHARE -> getString(R.string.DirectShare);
            case FinegramMessagesConfig.LEFT_BUTTON_FORWARD_WO_AUTHORSHIP -> getString(R.string.Forward) + " " + getString(R.string.FG_Without_Authorship);
            default -> getString(R.string.Forward) + " " + getString(R.string.FG_Without_Caption);
        };
    }

    private void showLeftBottomButtonSelector(Runnable runnable) {
        ArrayList<String> configStringKeys = new ArrayList<>();
        ArrayList<Integer> configValues = new ArrayList<>();

        configStringKeys.add(getString(R.string.Forward) + " " + getString(R.string.FG_Without_Authorship));
        configValues.add(FinegramMessagesConfig.LEFT_BUTTON_FORWARD_WO_AUTHORSHIP);

        configStringKeys.add(getString(R.string.Forward) + " " + getString(R.string.FG_Without_Caption));
        configValues.add(FinegramMessagesConfig.LEFT_BUTTON_FORWARD_WO_CAPTION);

        configStringKeys.add(getString(R.string.Reply));
        configValues.add(FinegramMessagesConfig.LEFT_BUTTON_REPLY);

        configStringKeys.add(getString(R.string.FG_ToSaved));
        configValues.add(FinegramMessagesConfig.LEFT_BUTTON_SAVE_MESSAGE);

        configStringKeys.add(getString(R.string.DirectShare));
        configValues.add(FinegramMessagesConfig.LEFT_BUTTON_DIRECT_SHARE);

        PopupHelper.show(configStringKeys, getString(R.string.CP_LeftBottomButtonAction), configValues.indexOf(FinegramMessagesConfig.INSTANCE.getLeftBottomButton()), getContext(), i -> {
            FinegramMessagesConfig.INSTANCE.setLeftBottomButton(configValues.get(i));
            if (runnable != null) runnable.run();
        });
    }

    private String getDoubleTapActionValue() {
        return switch (FinegramMessagesConfig.INSTANCE.getDoubleTapAction()) {
            case FinegramMessagesConfig.DOUBLE_TAP_ACTION_REACTION -> getString(R.string.Reactions);
            case FinegramMessagesConfig.DOUBLE_TAP_ACTION_REPLY -> getString(R.string.Reply);
            case FinegramMessagesConfig.DOUBLE_TAP_ACTION_SAVE -> getString(R.string.FG_ToSaved);
            case FinegramMessagesConfig.DOUBLE_TAP_ACTION_EDIT -> getString(R.string.Edit);
            case FinegramMessagesConfig.DOUBLE_TAP_ACTION_TRANSLATE -> getString(R.string.TranslateMessage);
            case FinegramMessagesConfig.DOUBLE_TAP_ACTION_TRANSLATE_GEMINI -> getString(R.string.TranslateMessage) + " - " + getString(R.string.CP_GeminiAI_Header);
            default -> getString(R.string.Disable);
        };
    }

    private void showDoubleTapSelector(Runnable runnable) {
        ArrayList<String> configStringKeys = new ArrayList<>();
        ArrayList<Integer> configValues = new ArrayList<>();

        configStringKeys.add(getString(R.string.Disable));
        configValues.add(FinegramMessagesConfig.DOUBLE_TAP_ACTION_NONE);

        configStringKeys.add(getString(R.string.Reactions));
        configValues.add(FinegramMessagesConfig.DOUBLE_TAP_ACTION_REACTION);

        configStringKeys.add(getString(R.string.Reply));
        configValues.add(FinegramMessagesConfig.DOUBLE_TAP_ACTION_REPLY);

        configStringKeys.add(getString(R.string.FG_ToSaved));
        configValues.add(FinegramMessagesConfig.DOUBLE_TAP_ACTION_SAVE);

        configStringKeys.add(getString(R.string.Edit));
        configValues.add(FinegramMessagesConfig.DOUBLE_TAP_ACTION_EDIT);

        configStringKeys.add(getString(R.string.TranslateMessage));
        configValues.add(FinegramMessagesConfig.DOUBLE_TAP_ACTION_TRANSLATE);

        configStringKeys.add(getString(R.string.TranslateMessage) + " - " + getString(R.string.CP_GeminiAI_Header));
        configValues.add(FinegramMessagesConfig.DOUBLE_TAP_ACTION_TRANSLATE_GEMINI);

        PopupHelper.show(configStringKeys, getString(R.string.CP_DoubleTapAction), configValues.indexOf(FinegramMessagesConfig.INSTANCE.getDoubleTapAction()), getContext(), i -> {
            FinegramMessagesConfig.INSTANCE.setDoubleTapAction(configValues.get(i));
            if (runnable != null) runnable.run();
        });
    }

    private String getSlideActionValue() {
        return switch (FinegramMessagesConfig.INSTANCE.getMessageSlideAction()) {
            case FinegramMessagesConfig.MESSAGE_SLIDE_ACTION_SAVE -> getString(R.string.FG_ToSaved);
            case FinegramMessagesConfig.MESSAGE_SLIDE_ACTION_TRANSLATE -> getString(R.string.TranslateMessage);
            case FinegramMessagesConfig.MESSAGE_SLIDE_ACTION_TRANSLATE_GEMINI -> getString(R.string.TranslateMessage) + " - " + getString(R.string.CP_GeminiAI_Header);
            case FinegramMessagesConfig.MESSAGE_SLIDE_ACTION_DIRECT_SHARE -> getString(R.string.DirectShare);
            default -> getString(R.string.Reply);
        };
    }

    private void showSlideActionSelector(Runnable runnable) {
        ArrayList<String> configStringKeys = new ArrayList<>();
        ArrayList<Integer> configValues = new ArrayList<>();

        configStringKeys.add(getString(R.string.Reply));
        configValues.add(FinegramMessagesConfig.MESSAGE_SLIDE_ACTION_REPLY);

        configStringKeys.add(getString(R.string.FG_ToSaved));
        configValues.add(FinegramMessagesConfig.MESSAGE_SLIDE_ACTION_SAVE);

        configStringKeys.add(getString(R.string.TranslateMessage));
        configValues.add(FinegramMessagesConfig.MESSAGE_SLIDE_ACTION_TRANSLATE);

        configStringKeys.add(getString(R.string.TranslateMessage) + " - " + getString(R.string.CP_GeminiAI_Header));
        configValues.add(FinegramMessagesConfig.MESSAGE_SLIDE_ACTION_TRANSLATE_GEMINI);

        configStringKeys.add(getString(R.string.DirectShare));
        configValues.add(FinegramMessagesConfig.MESSAGE_SLIDE_ACTION_DIRECT_SHARE);

        PopupHelper.show(configStringKeys, getString(R.string.FG_MsgSlideAction), configValues.indexOf(FinegramMessagesConfig.INSTANCE.getMessageSlideAction()), getContext(), i -> {
            FinegramMessagesConfig.INSTANCE.setMessageSlideAction(configValues.get(i));
            if (runnable != null) runnable.run();
        });
    }

    private static void handleMenuAlert(String title, List<ChatsPreferencesEntry.MenuItemConfig> items, BaseFragment fragment) {
        ArrayList<String> prefTitle = new ArrayList<>();
        ArrayList<Integer> prefIcon = new ArrayList<>();
        ArrayList<Boolean> prefCheck = new ArrayList<>();
        ArrayList<Boolean> prefCheckInvisible = new ArrayList<>();
        ArrayList<Boolean> prefDivider = new ArrayList<>();
        ArrayList<Runnable> clickListener = new ArrayList<>();

        for (ChatsPreferencesEntry.MenuItemConfig item : items) {
            prefTitle.add(item.title);
            prefIcon.add(item.iconRes);
            prefCheck.add(item.isChecked.get());
            prefCheckInvisible.add(item.isCheckInvisible);
            prefDivider.add(item.divider);
            clickListener.add(item.toggle);
        }

        PopupHelper.showSwitchAlert(
                title,
                fragment,
                prefTitle,
                prefIcon,
                prefCheck,
                prefCheckInvisible,
                prefDivider,
                clickListener,
                null
        );
    }

    @Override
    protected String getKey() {
        return DeeplinkHelper.DeepLinksRepo.FG_Messages;
    }
}

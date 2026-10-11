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

import android.content.SharedPreferences;
import android.media.MediaPlayer;
import android.os.Build;
import android.os.Bundle;
import android.view.HapticFeedbackConstants;
import android.view.View;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.R;
import org.telegram.messenger.UserObject;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.Cells.UserCell;
import org.telegram.ui.Components.IconBackgroundColors;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import org.telegram.ui.DialogsActivity;
import org.telegram.ui.LaunchActivity;
import org.telegram.ui.SettingsActivity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.function.Supplier;

import com.th3nekit.finegram.chats.helpers.ChatsHelper2;
import com.th3nekit.finegram.core.VibrateUtil;
import com.th3nekit.finegram.core.configs.FinegramChatsConfig;
import com.th3nekit.finegram.core.helpers.DeeplinkHelper;
import com.th3nekit.finegram.helpers.ui.PopupHelper;
import com.th3nekit.finegram.preferences.cells.ChatHeaderPreviewView;
import com.th3nekit.finegram.preferences.cells.ChatInputPreviewView;
import com.th3nekit.finegram.preferences.helpers.AlertDialogSwitchers;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class ChatsPreferencesEntry extends BaseCGPreferencesEntry {

    private final int glareEffectsRow = 1, headerSettingsRow = 2, centerTitleRow = 3, centerTitleAdaptiveWidthRow = 4, unreadBadgeRow = 5, iOSUnreadBadgeRow = 6, chatMenuShortcutsRow = 7;

    private final int customBackgroundInChatsRow = 8, snowflakesRow = 9;

    private final int bottomBarSettingsRow = 10, iOSMessageInputField = 11, sendAsChannelButtonRow = 12, hideBottomBarRow = 13, recentEmojisStickersRow = 14;

    private final int messagesPreferencesRow = 15;

    private final int customChatRow = 16;

    private final int autoQuoteRow = 17, disableSwipeToNextRow = 18, disableVibrationRow = 19, openLinksInIV = 20;

    private final int hideKbdSliderRow = 21;

    private final int voiceMessagesAutoPlay = 22, playVideoOnVolumeBtnRow = 23, autoPauseVideoRow = 24;
    private final int multiMicRow = 33, recordInStereoRow = 34;
    private final int lastSeenExactTimeRow = 35, showIdSearchRow = 36;
    private final int liquidGlassRow = 37;

    private final int videoSeekSliderRow = 25;

    private final int notificationSoundRow = 26, vibrateInChatsRow = 27;

    private final int weekdayNearDateRow = 28, hideActionBarStatusRow = 29, hideMuteButtonRow = 30, disableSendHintsRow = 31;
    private final int hideSponsoredRow = 32;

    private boolean expandedHeaderSettingsSection = false;
    private boolean expandedBottomBarSettingsSection = false;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.FilterChats);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        items.add(UItem.asHeader(getString(R.string.AP_Header_Appearance)));
        items.add(SettingsHelper.asSwitchCG(glareEffectsRow, applyNewSpan(getString(R.string.CP_StrokeOnViews)))
                .setChecked(FinegramChatsConfig.INSTANCE.getGlareEffects())
        .slug("glareEffects"));
        items.add(SettingsHelper.asSwitchCG(liquidGlassRow,
                        applyNewSpan(getString(R.string.FG_LiquidGlass)),
                        getString(R.string.FG_LiquidGlass_Desc))
                .setChecked(org.telegram.messenger.LiteMode.isEnabled(
                        org.telegram.messenger.LiteMode.FLAG_LIQUID_GLASS))
        );
        items.add(
                SettingsHelper.asExpandableSwitch(
                        headerSettingsRow,
                        R.drawable.msg_photo_settings_solar,
                        applyNewSpan(getString(R.string.AP_Header)),
                        " "
                )
                .setCollapsed(!expandedHeaderSettingsSection)
                .setClickCallback(v -> {
                    expandedHeaderSettingsSection = !expandedHeaderSettingsSection;
                    updateRows(true);
                })
                .hideCheckbox(true)
        .slug("header"));
        if (expandedHeaderSettingsSection) {
            items.add(UItem.asShadow(null));
            ChatHeaderPreviewView headerPreviewView = new ChatHeaderPreviewView(getContext(), this, getResourceProvider());
            headerPreviewView.setTitleCentered(FinegramChatsConfig.INSTANCE.getCenterChatTitle());
            headerPreviewView.updateIOSUnreadBadge();
            items.add(SettingsHelper.asCustomWithBackground(headerPreviewView, 70));
            items.add(UItem.asShadow(null));

            items.add(SettingsHelper.asSwitchCG(centerTitleRow, getString(R.string.AP_CenterTitle))
                    .setChecked(FinegramChatsConfig.INSTANCE.getCenterChatTitle())
            .slug("centerTitle"));
            if (FinegramChatsConfig.INSTANCE.getCenterChatTitle()) {
                items.add(SettingsHelper.asSwitchCG(centerTitleAdaptiveWidthRow, applyNewSpan(getString(R.string.AP_CenterTitle_AdaptiveWidth)))
                        .setChecked(FinegramChatsConfig.INSTANCE.getCenterChatTitle_AdaptiveWidth())
                .slug("adaptiveWidth"));
            }
            items.add(SettingsHelper.asSwitchCG(unreadBadgeRow, getString(R.string.CP_UnreadBadgeOnBackButton), getString(R.string.CP_UnreadBadgeOnBackButton_Desc))
                    .setChecked(FinegramChatsConfig.INSTANCE.getUnreadBadgeOnBackButton())
            .slug("unreadBadge"));
            if (FinegramChatsConfig.INSTANCE.getUnreadBadgeOnBackButton()) {
                items.add(SettingsHelper.asSwitchCG(iOSUnreadBadgeRow, applyNewSpan(getString(R.string.CP_UnreadBadgeOnBackButton_IOS)))
                        .setChecked(FinegramChatsConfig.INSTANCE.getUnreadBadgeOnBackButton_iOS())
                .slug("unreadBadgeIOS"));
            }
        }
        items.add(UItem.asShadow(null));

        items.add(UItem.asButton(chatMenuShortcutsRow, R.drawable.msg_list, getString(R.string.CP_ChatMenuShortcuts)).slug("chatMenuShortcuts"));
        items.add(SettingsHelper.asSwitchCG(customBackgroundInChatsRow, getString(R.string.CP_CustomWallpapers), getString(R.string.CP_CustomWallpapers_Desc))
                .setChecked(FinegramChatsConfig.INSTANCE.getCustomWallpapers())
        .slug("customBackground"));
        items.add(SettingsHelper.asSwitchCG(snowflakesRow, getString(R.string.CP_Snowflakes_Header))
                .setChecked(FinegramChatsConfig.INSTANCE.getDrawSnowInChat())
        .slug("snowflakes"));
        items.add(UItem.asShadow(null));

        items.add(
                SettingsHelper.asExpandableSwitch(
                        bottomBarSettingsRow,
                        R.drawable.msg_photo_settings_solar,
                        applyNewSpan(getString(R.string.CP_BottomBar)),
                        " "
                )
                .setCollapsed(!expandedBottomBarSettingsSection)
                .setClickCallback(v -> {
                    expandedBottomBarSettingsSection = !expandedBottomBarSettingsSection;
                    updateRows(true);
                })
                .hideCheckbox(true)
        .slug("bottomBar"));
        if (expandedBottomBarSettingsSection) {
            items.add(UItem.asShadow(null));
            ChatInputPreviewView inputPreviewView = new ChatInputPreviewView(getContext(), this, getResourceProvider());
            items.add(SettingsHelper.asCustomWithBackground(inputPreviewView, 62));
            items.add(UItem.asShadow(null));
            items.add(SettingsHelper.asSwitchCG(iOSMessageInputField, applyNewSpan(getString(R.string.CP_iOSMessageInputField)), getString(R.string.CP_iOSMessageInputField_Desc))
                    .setChecked(FinegramChatsConfig.INSTANCE.getIOSMessageInputField())
            .slug("iosInputField"));
            items.add(SettingsHelper.asSwitchCG(sendAsChannelButtonRow, getString(R.string.CP_HideSendAsChannel), getString(R.string.CP_HideSendAsChannelDesc))
                    .setChecked(FinegramChatsConfig.INSTANCE.getHideSendAsChannel())
            .slug("hideSendAs"));
            items.add(SettingsHelper.asSwitchCG(hideBottomBarRow, getString(R.string.CP_HideMuteUnmuteButton))
                    .setChecked(FinegramChatsConfig.INSTANCE.getHideMuteUnmuteButton())
            .slug("hideBottomBar"));
            items.add(UItem.asButton(recentEmojisStickersRow, 0, getString(R.string.CP_Slider_RecentEmojisAndStickers)).slug("recentsCounter"));
        }
        items.add(UItem.asShadow(null));

        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        messagesPreferencesRow, IconBackgroundColors.ORANGE.top, IconBackgroundColors.ORANGE.bottom,
                        R.drawable.settings_messages_filled_solar,
                        getString(R.string.MessagesSettings),
                        getString(R.string.FGP_Messages_Desc),
                        true
                )
        .slug(DeeplinkHelper.DeepLinksRepo.FG_Messages));
        items.add(UItem.asShadow(null));

        items.add(SettingsHelper.asSwitchCG(customChatRow, getString(R.string.EP_CustomChat), getString(R.string.EP_CustomChat_Desc))
                .setChecked(FinegramChatsConfig.INSTANCE.getCustomChatForSavedMessages())
        .slug("customSaved"));
        if (FinegramChatsConfig.INSTANCE.getCustomChatForSavedMessages()) {
            items.add(SettingsHelper.asCustomWithBackground(createUserCell()));
        }
        items.add(UItem.asShadow(null));

        items.add(SettingsHelper.asSwitchCG(weekdayNearDateRow, getString(R.string.FG_WeekdayNearDate), getString(R.string.FG_WeekdayNearDate_Desc))
                .setChecked(FinegramChatsConfig.INSTANCE.getWeekdayNearDate())
        );
        items.add(SettingsHelper.asSwitchCG(hideActionBarStatusRow, getString(R.string.FG_HideActionBarStatus), getString(R.string.FG_HideActionBarStatus_Desc))
                .setChecked(FinegramChatsConfig.INSTANCE.getHideActionBarStatus())
        );
        items.add(SettingsHelper.asSwitchCG(hideMuteButtonRow, getString(R.string.FG_HideMuteButton), getString(R.string.FG_HideMuteButton_Desc))
                .setChecked(FinegramChatsConfig.INSTANCE.getHideMuteButton())
        );
        items.add(SettingsHelper.asSwitchCG(disableSendHintsRow, getString(R.string.FG_DisableSendHints), getString(R.string.FG_DisableSendHints_Desc))
                .setChecked(FinegramChatsConfig.INSTANCE.getDisableSendHints())
        );
        items.add(SettingsHelper.asSwitchCG(hideSponsoredRow, getString(R.string.FG_HideSponsored), getString(R.string.FG_HideSponsored_Desc))
                .setChecked(FinegramChatsConfig.INSTANCE.getHideSponsoredMessages())
        );
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.ActionsChartTitle)));
        items.add(SettingsHelper.asSwitchCG(autoQuoteRow, getString(R.string.CP_AutoQuoteReplies), getString(R.string.CP_AutoQuoteReplies_Desc))
                .setChecked(FinegramChatsConfig.INSTANCE.getAutoQuoteReplies())
        .slug("quoteReplies"));
        items.add(SettingsHelper.asSwitchCG(disableSwipeToNextRow, getString(R.string.CP_DisableSwipeToNext), getString(R.string.CP_DisableSwipeToNext_Desc))
                .setChecked(FinegramChatsConfig.INSTANCE.getDisableSwipeToNext())
        .slug("swipeToNext"));
        items.add(SettingsHelper.asSwitchCG(disableVibrationRow, getString(R.string.CP_DisableVibration))
                .setChecked(FinegramChatsConfig.INSTANCE.getDisableVibration())
        .slug("vibration"));
        items.add(SettingsHelper.asSwitchCG(openLinksInIV, getString(R.string.CP_OpenLinksInIV))
                .setChecked(FinegramChatsConfig.INSTANCE.getOpenLinksInIV())
        .slug("linksInIV"));
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.CP_HideKbdOnScroll)));
        items.add(
                UItem.asIntSlideView(
                        hideKbdSliderRow,
                        0,
                        FinegramChatsConfig.INSTANCE.getHideKeyboardOnScrollIntensity(),
                        10,
                        val -> val == 0 ? getString(R.string.VibrationDisabled) : String.valueOf(val),
                        FinegramChatsConfig.INSTANCE::setHideKeyboardOnScrollIntensity
                ).setId(hideKbdSliderRow)
        );
        items.add(UItem.asShadow(null));

        items.add(SettingsHelper.asSwitchCG(showIdSearchRow,
                        applyNewSpan(getString(R.string.FG_ShowIdSearch)),
                        getString(R.string.FG_ShowIdSearch_Desc))
                .setChecked(FinegramChatsConfig.INSTANCE.getShowIdSearch())
        );
        items.add(SettingsHelper.asSwitchCG(lastSeenExactTimeRow,
                        applyNewSpan(getString(R.string.FG_LastSeenExactTime)),
                        getString(R.string.FG_LastSeenExactTime_Desc))
                .setChecked(FinegramChatsConfig.INSTANCE.getLastSeenExactTime())
        );
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.CP_Header_Record)));
        items.add(SettingsHelper.asSwitchCG(multiMicRow, applyNewSpan(getString(R.string.FG_RecordMultiMic)),
                        getString(R.string.FG_RecordMultiMic_Desc))
                .setChecked(FinegramChatsConfig.INSTANCE.getRecordMultiMic())
        .slug("multiMic"));
        if (FinegramChatsConfig.INSTANCE.getRecordMultiMic()) {
            items.add(SettingsHelper.asSwitchCG(recordInStereoRow,
                            applyNewSpan(getString(R.string.FG_RecordInStereo)),
                            getString(R.string.FG_RecordInStereo_Desc))
                    .setChecked(FinegramChatsConfig.INSTANCE.getRecordInStereo())
            .slug("recordInStereo"));
        }
        items.add(SettingsHelper.asSwitchCG(voiceMessagesAutoPlay, applyNewSpan(getString(R.string.CP_VoiceMessagesAutoPlay)))
                .setChecked(FinegramChatsConfig.INSTANCE.getVoiceMessagesAutoPlay())
        .slug("voiceAutoPlay"));
        items.add(SettingsHelper.asSwitchCG(playVideoOnVolumeBtnRow, getString(R.string.CP_PlayVideo), getString(R.string.CP_PlayVideo_Desc))
                .setChecked(FinegramChatsConfig.INSTANCE.getPlayVideoOnVolume())
        .slug("playVideoOnVolumeClick"));
        items.add(SettingsHelper.asSwitchCG(autoPauseVideoRow, getString(R.string.CP_AutoPauseVideo), getString(R.string.CP_AutoPauseVideo_Desc))
                .setChecked(FinegramChatsConfig.INSTANCE.getAutoPauseVideo())
        .slug("videoAutoPause"));
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.CP_VideoSeekDuration)));
        items.add(
                UItem.asIntSlideView(
                        videoSeekSliderRow,
                        0,
                        FinegramChatsConfig.INSTANCE.getVideoSeekDuration(),
                        25,
                        val -> val == 0 ? getString(R.string.VibrationDisabled) : String.valueOf(val),
                        FinegramChatsConfig.INSTANCE::setVideoSeekDuration
                ).setId(videoSeekSliderRow)
        );
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.SettingsNotifications)));
        items.add(UItem.asButton(notificationSoundRow, getString(R.string.NotificationsSound), getNotificationSoundValue()).slug("notificationSound"));
        items.add(UItem.asButton(vibrateInChatsRow, getString(R.string.CP_VibrateInChats), getVibrationValue()).slug("inChatVibration"));
        items.add(UItem.asShadow(getString(R.string.CP_VibrateInChats_Desc)));
        items.add(UItem.asShadow(null));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == liquidGlassRow) {

            final boolean next = !org.telegram.messenger.LiteMode.isEnabled(
                    org.telegram.messenger.LiteMode.FLAG_LIQUID_GLASS);
            org.telegram.messenger.LiteMode.toggleFlag(
                    org.telegram.messenger.LiteMode.FLAG_LIQUID_GLASS, next);
            SettingsHelper.updateCheckState(view, next);
        } else if (item.id == glareEffectsRow) {
            FinegramChatsConfig.INSTANCE.setGlareEffects(!FinegramChatsConfig.INSTANCE.getGlareEffects());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getGlareEffects());

            updateRows(true);
        } else if (item.id == headerSettingsRow) {
            expandedHeaderSettingsSection = !expandedHeaderSettingsSection;
            item.collapsed = !item.collapsed;

            updateRows(true);
        } else if (item.id == centerTitleRow) {
            FinegramChatsConfig.INSTANCE.setCenterChatTitle(!FinegramChatsConfig.INSTANCE.getCenterChatTitle());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getCenterChatTitle());

            updateRows(true);
        } else if (item.id == centerTitleAdaptiveWidthRow) {
            FinegramChatsConfig.INSTANCE.setCenterChatTitle_AdaptiveWidth(!FinegramChatsConfig.INSTANCE.getCenterChatTitle_AdaptiveWidth());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getCenterChatTitle_AdaptiveWidth());

            updateRows(true);
        } else if (item.id == unreadBadgeRow) {
            FinegramChatsConfig.INSTANCE.setUnreadBadgeOnBackButton(!FinegramChatsConfig.INSTANCE.getUnreadBadgeOnBackButton());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getUnreadBadgeOnBackButton());

            updateRows(true);
        } else if (item.id == iOSUnreadBadgeRow) {
            FinegramChatsConfig.INSTANCE.setUnreadBadgeOnBackButton_iOS(!FinegramChatsConfig.INSTANCE.getUnreadBadgeOnBackButton_iOS());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getUnreadBadgeOnBackButton_iOS());

            updateRows(true);
        } else if (item.id == chatMenuShortcutsRow) {
            showChatMenuItemsConfigurator(this);
        } else if (item.id == customBackgroundInChatsRow) {
            FinegramChatsConfig.INSTANCE.setCustomWallpapers(!FinegramChatsConfig.INSTANCE.getCustomWallpapers());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getCustomWallpapers());
        } else if (item.id == snowflakesRow) {
            FinegramChatsConfig.INSTANCE.setDrawSnowInChat(!FinegramChatsConfig.INSTANCE.getDrawSnowInChat());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getDrawSnowInChat());

            getParentLayout().rebuildAllFragmentViews(false, false);
        } else if (item.id == bottomBarSettingsRow) {
            expandedBottomBarSettingsSection = !expandedBottomBarSettingsSection;
            item.collapsed = !item.collapsed;

            updateRows(true);
        } else if (item.id == iOSMessageInputField) {
            FinegramChatsConfig.INSTANCE.setIOSMessageInputField(!FinegramChatsConfig.INSTANCE.getIOSMessageInputField());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getIOSMessageInputField());

            updateRows(true);

        } else if (item.id == sendAsChannelButtonRow) {
            FinegramChatsConfig.INSTANCE.setHideSendAsChannel(!FinegramChatsConfig.INSTANCE.getHideSendAsChannel());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getHideSendAsChannel());

            updateRows(true);
        } else if (item.id == hideBottomBarRow) {
            FinegramChatsConfig.INSTANCE.setHideMuteUnmuteButton(!FinegramChatsConfig.INSTANCE.getHideMuteUnmuteButton());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getHideMuteUnmuteButton());
        } else if (item.id == recentEmojisStickersRow) {
            AlertDialogSwitchers.showRecentEmojisAndStickers(this);
        } else if (item.id == messagesPreferencesRow) {
            FinegramPreferencesNavigator.INSTANCE.createMessages(this);
        } else if (item.id == customChatRow) {
            FinegramChatsConfig.INSTANCE.setCustomChatForSavedMessages(!FinegramChatsConfig.INSTANCE.getCustomChatForSavedMessages());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getCustomChatForSavedMessages());

            updateRows(true);
        } else if (item.id == autoQuoteRow) {
            FinegramChatsConfig.INSTANCE.setAutoQuoteReplies(!FinegramChatsConfig.INSTANCE.getAutoQuoteReplies());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getAutoQuoteReplies());
        } else if (item.id == disableSwipeToNextRow) {
            FinegramChatsConfig.INSTANCE.setDisableSwipeToNext(!FinegramChatsConfig.INSTANCE.getDisableSwipeToNext());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getDisableSwipeToNext());
        } else if (item.id == disableVibrationRow) {
            FinegramChatsConfig.INSTANCE.setDisableVibration(!FinegramChatsConfig.INSTANCE.getDisableVibration());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getDisableVibration());

            showRestartBulletin();
        } else if (item.id == openLinksInIV) {
            FinegramChatsConfig.INSTANCE.setOpenLinksInIV(!FinegramChatsConfig.INSTANCE.getOpenLinksInIV());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getOpenLinksInIV());
        } else if (item.id == showIdSearchRow) {
            FinegramChatsConfig.INSTANCE.setShowIdSearch(
                    !FinegramChatsConfig.INSTANCE.getShowIdSearch());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getShowIdSearch());
        } else if (item.id == lastSeenExactTimeRow) {
            FinegramChatsConfig.INSTANCE.setLastSeenExactTime(
                    !FinegramChatsConfig.INSTANCE.getLastSeenExactTime());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getLastSeenExactTime());
        } else if (item.id == multiMicRow) {
            FinegramChatsConfig.INSTANCE.setRecordMultiMic(!FinegramChatsConfig.INSTANCE.getRecordMultiMic());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getRecordMultiMic());

            updateRows(true);
        } else if (item.id == recordInStereoRow) {
            FinegramChatsConfig.INSTANCE.setRecordInStereo(!FinegramChatsConfig.INSTANCE.getRecordInStereo());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getRecordInStereo());
        } else if (item.id == voiceMessagesAutoPlay) {
            FinegramChatsConfig.INSTANCE.setVoiceMessagesAutoPlay(!FinegramChatsConfig.INSTANCE.getVoiceMessagesAutoPlay());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getVoiceMessagesAutoPlay());
        } else if (item.id == playVideoOnVolumeBtnRow) {
            FinegramChatsConfig.INSTANCE.setPlayVideoOnVolume(!FinegramChatsConfig.INSTANCE.getPlayVideoOnVolume());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getPlayVideoOnVolume());
        } else if (item.id == autoPauseVideoRow) {
            FinegramChatsConfig.INSTANCE.setAutoPauseVideo(!FinegramChatsConfig.INSTANCE.getAutoPauseVideo());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getAutoPauseVideo());
        } else if (item.id == notificationSoundRow) {
            showNotificationSoundSelector(() -> {
                SettingsHelper.updateButtonValue(view, getNotificationSoundValue());

                int tone = (FinegramChatsConfig.INSTANCE.getNotificationSound() == FinegramChatsConfig.NOTIF_SOUND_DEFAULT) ? R.raw.sound_in : R.raw.sound_in_ios;
                try {
                    MediaPlayer mp = MediaPlayer.create(getContext(), tone);
                    mp.start();
                } catch (Exception ignored) {}

                showRestartBulletin();
            });
        } else if (item.id == hideActionBarStatusRow) {
            FinegramChatsConfig.INSTANCE.setHideActionBarStatus(!FinegramChatsConfig.INSTANCE.getHideActionBarStatus());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getHideActionBarStatus());
        } else if (item.id == disableSendHintsRow) {
            FinegramChatsConfig.INSTANCE.setDisableSendHints(!FinegramChatsConfig.INSTANCE.getDisableSendHints());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getDisableSendHints());
        } else if (item.id == hideSponsoredRow) {
            FinegramChatsConfig.INSTANCE.setHideSponsoredMessages(!FinegramChatsConfig.INSTANCE.getHideSponsoredMessages());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getHideSponsoredMessages());
        } else if (item.id == hideMuteButtonRow) {
            FinegramChatsConfig.INSTANCE.setHideMuteButton(!FinegramChatsConfig.INSTANCE.getHideMuteButton());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getHideMuteButton());
        } else if (item.id == weekdayNearDateRow) {
            FinegramChatsConfig.INSTANCE.setWeekdayNearDate(!FinegramChatsConfig.INSTANCE.getWeekdayNearDate());
            SettingsHelper.updateCheckState(view, FinegramChatsConfig.INSTANCE.getWeekdayNearDate());
        } else if (item.id == vibrateInChatsRow) {
            showVibrationSelector(() -> {
                try {
                    switch (FinegramChatsConfig.INSTANCE.getVibrateInChats()) {
                        case FinegramChatsConfig.VIBRATION_CLICK ->
                                VibrateUtil.INSTANCE.makeClickVibration();
                        case FinegramChatsConfig.VIBRATION_WAVE_FORM ->
                                VibrateUtil.INSTANCE.makeWaveVibration();
                        case FinegramChatsConfig.VIBRATION_KEYBOARD_TAP ->
                                VibrateUtil.INSTANCE.vibrate(HapticFeedbackConstants.KEYBOARD_TAP);
                        case FinegramChatsConfig.VIBRATION_LONG ->
                                VibrateUtil.INSTANCE.vibrate();
                    }
                } catch (Exception ignored) { }

                SettingsHelper.updateButtonValue(view, getVibrationValue());
            });
        }
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        if (item.id == messagesPreferencesRow) {
            AndroidUtilities.addToClipboard("tg://" + DeeplinkHelper.DeepLinksRepo.FG_Messages);
            return true;
        }
        return false;
    }

    @Override
    public void scrollToRow(String slug, Runnable unknown) {
        if (java.util.Set.of("centerTitle", "adaptiveWidth", "unreadBadge", "unreadBadgeIOS", "chatMenuShortcuts").contains(slug)) expandedHeaderSettingsSection = true;
        if (java.util.Set.of("iosInputField", "hideSendAs", "hideBottomBar", "recentsCounter").contains(slug)) expandedBottomBarSettingsSection = true;
        if (listView != null) updateRows(false);
        super.scrollToRow(slug, unknown);
    }

    private UserCell createUserCell() {
        UserCell userCell = new UserCell(getContext(), 14, 0, false, true, getResourceProvider(), false, false);

        userCell.addButton.setText(getString(R.string.Edit));
        userCell.addButton.setOnClickListener(view1 -> {
            if (getUserConfig().getCurrentUser() == null) {
                return;
            }
            Bundle args = new Bundle();
            args.putBoolean("onlySelect", true);
            args.putBoolean("cgPrefs", true);
            args.putBoolean("allowGlobalSearch", false);
            args.putInt("dialogsType", DialogsActivity.DIALOGS_TYPE_FORWARD);
            args.putBoolean("resetDelegate", false);
            args.putBoolean("closeFragment", true);
            DialogsActivity fragment = new DialogsActivity(args);
            fragment.setDelegate((fragment1, dids, message, param, notify, scheduleDate, scheduleRepeatPeriod, topicsFragment) -> {
                long did = dids.get(0).dialogId;

                String selectedChatId = String.valueOf(did);

                SharedPreferences.Editor editor = MessagesController.getMainSettings(currentAccount).edit();
                editor.putString("CP_CustomChatIDSM", selectedChatId).apply();

                fragment.finishFragment(true);

                updateRows(false);
                return true;
            });
            presentFragment(fragment);
        });

        long chatId = ChatsHelper2.INSTANCE.getCustomChatID();

        TLRPC.Chat chat = MessagesController.getInstance(currentAccount).getChat(-chatId);
        TLRPC.User user = MessagesController.getInstance(currentAccount).getUser(chatId);

        StringBuilder status = new StringBuilder();
        status.append(getString(R.string.EP_CustomChat_Selected_Title));
        status.append(' ');
        status.append("\"");
        status.append(getString(R.string.SavedMessages));
        status.append("\".");

        if (chatId == getUserConfig().clientUserId) {
            userCell.setData("saved_cg", getString(R.string.SavedMessages), "", 0);
        } else if (chat != null) {
            userCell.setData(chat, chat.title, status, 0);
        } else {
            userCell.setData(user, UserObject.getUserName(user), status, 0);
        }

        return userCell;
    }

    public static void showChatMenuItemsConfigurator(BaseFragment fragment) {
        List<MenuItemConfig> menuItems = Arrays.asList(
                new MenuItemConfig(
                        getString(R.string.FG_JumpToBeginning),
                        R.drawable.ic_upward,
                        FinegramChatsConfig.INSTANCE::getShortcut_JumpToBegin,
                        () -> FinegramChatsConfig.INSTANCE.setShortcut_JumpToBegin(!FinegramChatsConfig.INSTANCE.getShortcut_JumpToBegin()),
                        false,
                        false
                ),
                new MenuItemConfig(
                        getString(R.string.FG_DeleteAllFromSelf),
                        R.drawable.msg_delete,
                        FinegramChatsConfig.INSTANCE::getShortcut_DeleteAll,
                        () -> FinegramChatsConfig.INSTANCE.setShortcut_DeleteAll(!FinegramChatsConfig.INSTANCE.getShortcut_DeleteAll()),
                        false,
                        false
                ),
                new MenuItemConfig(
                        getString(R.string.SavedMessages),
                        R.drawable.msg_saved,
                        FinegramChatsConfig.INSTANCE::getShortcut_SavedMessages,
                        () -> FinegramChatsConfig.INSTANCE.setShortcut_SavedMessages(!FinegramChatsConfig.INSTANCE.getShortcut_SavedMessages()),
                        false,
                        false
                ),
                new MenuItemConfig(
                        "Telegram Browser",
                        R.drawable.msg_language,
                        FinegramChatsConfig.INSTANCE::getShortcut_Browser,
                        () -> FinegramChatsConfig.INSTANCE.setShortcut_Browser(!FinegramChatsConfig.INSTANCE.getShortcut_Browser()),
                        true,
                        false
                ),
                new MenuItemConfig(
                        getString(R.string.CP_AdminActions),
                        R.drawable.msg_admins,
                        () -> false,
                        () -> showChatAdminItemsConfigurator(fragment),
                        false,
                        true
                )
        );

        handleMenuAlert(getString(R.string.CP_ChatMenuShortcuts), menuItems, fragment);
    }

    private static void showChatAdminItemsConfigurator(BaseFragment fragment) {
        List<MenuItemConfig> menuItems = Arrays.asList(
                new MenuItemConfig(
                        getString(R.string.Reactions),
                        R.drawable.msg_reactions2,
                        FinegramChatsConfig.INSTANCE::getAdmins_Reactions,
                        () -> FinegramChatsConfig.INSTANCE.setAdmins_Reactions(!FinegramChatsConfig.INSTANCE.getAdmins_Reactions()),
                        false,
                        false
                ),
                new MenuItemConfig(
                        getString(R.string.ChannelPermissions),
                        R.drawable.msg_permissions,
                        FinegramChatsConfig.INSTANCE::getAdmins_Permissions,
                        () -> FinegramChatsConfig.INSTANCE.setAdmins_Permissions(!FinegramChatsConfig.INSTANCE.getAdmins_Permissions()),
                        false,
                        false
                ),
                new MenuItemConfig(
                        getString(R.string.ChannelAdministrators),
                        R.drawable.msg_admins,
                        FinegramChatsConfig.INSTANCE::getAdmins_Administrators,
                        () -> FinegramChatsConfig.INSTANCE.setAdmins_Administrators(!FinegramChatsConfig.INSTANCE.getAdmins_Administrators()),
                        false,
                        false
                ),
                new MenuItemConfig(
                        getString(R.string.ChannelMembers),
                        R.drawable.msg_groups,
                        FinegramChatsConfig.INSTANCE::getAdmins_Members,
                        () -> FinegramChatsConfig.INSTANCE.setAdmins_Members(!FinegramChatsConfig.INSTANCE.getAdmins_Members()),
                        false,
                        false
                ),
                new MenuItemConfig(
                        getString(R.string.StatisticsAndBoosts),
                        R.drawable.msg_stats,
                        FinegramChatsConfig.INSTANCE::getAdmins_Statistics,
                        () -> FinegramChatsConfig.INSTANCE.setAdmins_Statistics(!FinegramChatsConfig.INSTANCE.getAdmins_Statistics()),
                        false,
                        false
                ),
                new MenuItemConfig(
                        getString(R.string.EventLog),
                        R.drawable.msg_log,
                        FinegramChatsConfig.INSTANCE::getAdmins_RecentActions,
                        () -> FinegramChatsConfig.INSTANCE.setAdmins_RecentActions(!FinegramChatsConfig.INSTANCE.getAdmins_RecentActions()),
                        false,
                        false
                )
        );

        handleMenuAlert(getString(R.string.CP_AdminActions), menuItems, fragment);
    }

    private String getNotificationSoundValue() {
        return switch (FinegramChatsConfig.INSTANCE.getNotificationSound()) {
            case FinegramChatsConfig.NOTIF_SOUND_DEFAULT -> getString(R.string.Default);
            case FinegramChatsConfig.NOTIF_SOUND_IOS -> "iOS";
            default -> getString(R.string.PopupDisabled);
        };
    }

    private void showNotificationSoundSelector(Runnable runnable) {
        ArrayList<String> configStringKeys = new ArrayList<>();
        ArrayList<Integer> configValues = new ArrayList<>();

        configStringKeys.add(getString(R.string.PopupDisabled));
        configValues.add(FinegramChatsConfig.NOTIF_SOUND_DISABLE);

        configStringKeys.add(getString(R.string.Default));
        configValues.add(FinegramChatsConfig.NOTIF_SOUND_DEFAULT);

        configStringKeys.add("iOS");
        configValues.add(FinegramChatsConfig.NOTIF_SOUND_IOS);

        PopupHelper.show(configStringKeys, getString(R.string.NotificationsSound), configValues.indexOf(FinegramChatsConfig.INSTANCE.getNotificationSound()), getContext(), i -> {
            FinegramChatsConfig.INSTANCE.setNotificationSound(configValues.get(i));
            if (runnable != null) runnable.run();
        });
    }

    private String getVibrationValue() {
        return switch (FinegramChatsConfig.INSTANCE.getVibrateInChats()) {
            case FinegramChatsConfig.VIBRATION_CLICK -> "1";
            case FinegramChatsConfig.VIBRATION_WAVE_FORM -> "2";
            case FinegramChatsConfig.VIBRATION_KEYBOARD_TAP -> "3";
            case FinegramChatsConfig.VIBRATION_LONG -> "4";
            default -> getString(R.string.AutoLockDisabled);
        };
    }

    private void showVibrationSelector(Runnable runnable) {
        ArrayList<String> configStringKeys = new ArrayList<>();
        ArrayList<Integer> configValues = new ArrayList<>();

        configStringKeys.add(getString(R.string.AutoLockDisabled));
        configValues.add(FinegramChatsConfig.VIBRATION_DISABLE);

        configStringKeys.add("1");
        configValues.add(FinegramChatsConfig.VIBRATION_CLICK);

        configStringKeys.add("2");
        configValues.add(FinegramChatsConfig.VIBRATION_WAVE_FORM);

        configStringKeys.add("3");
        configValues.add(FinegramChatsConfig.VIBRATION_KEYBOARD_TAP);

        configStringKeys.add("4");
        configValues.add(FinegramChatsConfig.VIBRATION_LONG);

        PopupHelper.show(configStringKeys, getString(R.string.CP_VibrateInChats), configValues.indexOf(FinegramChatsConfig.INSTANCE.getVibrateInChats()), getContext(), i -> {
            FinegramChatsConfig.INSTANCE.setVibrateInChats(configValues.get(i));
            if (runnable != null) runnable.run();
        });
    }

    private static void handleMenuAlert(String title, List<MenuItemConfig> items, BaseFragment fragment) {
        ArrayList<String> prefTitle = new ArrayList<>();
        ArrayList<Integer> prefIcon = new ArrayList<>();
        ArrayList<Boolean> prefCheck = new ArrayList<>();
        ArrayList<Boolean> prefCheckInvisible = new ArrayList<>();
        ArrayList<Boolean> prefDivider = new ArrayList<>();
        ArrayList<Runnable> clickListener = new ArrayList<>();

        for (MenuItemConfig item : items) {
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

    public static class MenuItemConfig {
        String title;
        int iconRes;
        Supplier<Boolean> isChecked;
        Runnable toggle;
        boolean divider;
        boolean isCheckInvisible;

        MenuItemConfig(String title, int iconRes, Supplier<Boolean> isChecked, Runnable toggle, boolean divider, boolean isCheckInvisible) {
            this.title = title;
            this.iconRes = iconRes;
            this.isChecked = isChecked;
            this.toggle = toggle;
            this.divider = divider;
            this.isCheckInvisible = isCheckInvisible;
        }
    }

    @Override
    protected String getKey() {
        return DeeplinkHelper.DeepLinksRepo.FG_Chats;
    }
}

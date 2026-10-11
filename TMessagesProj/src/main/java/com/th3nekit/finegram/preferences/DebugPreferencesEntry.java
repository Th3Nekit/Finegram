/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences;

import com.th3nekit.finegram.core.helpers.DeeplinkHelper;

import static org.telegram.messenger.LocaleController.getString;

import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import android.view.View;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ChatThemeController;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.R;
import org.telegram.messenger.SharedConfig;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.Components.Paint.PersistColorPalette;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import org.telegram.ui.RestrictedLanguagesSelectActivity;

import java.util.ArrayList;

import com.th3nekit.finegram.core.configs.FinegramCoreConfig;
import com.th3nekit.finegram.core.configs.FinegramDebugConfig;
import com.th3nekit.finegram.helpers.ui.PopupHelper;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class DebugPreferencesEntry extends BaseCGPreferencesEntry {

    private final int toastRpcRow = 1;
    private final int oldTimeStyleRow = 2;
    private final int performanceClassRow = 5;
    private final int fixCallsNotifRow = 6;

    private final int newBlurRow = 7;

    private final int forceForumTabsRow = 8;
    private final int replacePunctuationRow = 9;
    private final int editTextFixRow = 10;
    private final int audioSourceRow = 11;
    private final int sendMaxQualityRow = 12;
    private final int playGifAsVideoRow = 13;
    private final int hideTimestampRow = 14;
    private final int resetDialogsRow = 15;
    private final int clearMediaCacheRow = 16;
    private final int readAllDialogsRow = 17;

    private final int importContactsRow = 18;
    private final int reloadContactsRow = 19;
    private final int resetContactsRow = 20;
    private final int updatePreviewRow = 21;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.FG_Debug_Title);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        items.add(UItem.asHeader(getString(R.string.FG_Debug_Misc)));
        if (!FinegramCoreConfig.isStandaloneStableBuild() && !FinegramCoreConfig.isPlayStoreBuild()) {
            items.add(SettingsHelper.asSwitchCG(toastRpcRow, getString(R.string.FG_Debug_ToastRpc), getString(R.string.FG_Debug_ToastRpc_Desc))
                    .setChecked(FinegramDebugConfig.INSTANCE.getShowRPCErrors())
            );
        }
        items.add(SettingsHelper.asSwitchCG(oldTimeStyleRow, getString(R.string.FG_Debug_OldTime), getString(R.string.FG_Debug_OldTime_Desc))
                .setChecked(FinegramDebugConfig.INSTANCE.getOldTimeStyle())
        .slug("oldTimeStyle"));
        items.add(UItem.asButton(performanceClassRow, getString(R.string.FG_Debug_PerfClass), SharedConfig.performanceClassName(SharedConfig.getDevicePerformanceClass())));
        items.add(UItem.asButton(updatePreviewRow, getString(R.string.FG_Update_Preview)));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            items.add(UItem.asButton(fixCallsNotifRow, getString(R.string.FG_Debug_FixCallNotif)).slug("fixCalls"));
        }
        items.add(UItem.asShadow(null));

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            items.add(UItem.asHeader(getString(R.string.AP_Header_Appearance)));
            items.add(SettingsHelper.asSwitchCG(newBlurRow, getString(R.string.FG_Debug_NewBlur))
                    .setChecked(SharedConfig.useNewBlur)
            );
            items.add(UItem.asShadow(null));
        }

        items.add(UItem.asHeader(getString(R.string.FilterChats)));
        items.add(SettingsHelper.asSwitchCG(forceForumTabsRow, getString(R.string.FG_Debug_ForumTabs))
                .setChecked(SharedConfig.forceForumTabs)
        );
        items.add(SettingsHelper.asSwitchCG(replacePunctuationRow, getString(R.string.FG_Debug_Punctuation), getString(R.string.FG_Debug_Punctuation_Desc))
                .setChecked(FinegramDebugConfig.INSTANCE.getReplacePunctuationMarks())
        );
        items.add(SettingsHelper.asSwitchCG(editTextFixRow, getString(R.string.FG_Debug_EditFix), getString(R.string.FG_Debug_EditFix_Desc))
                .setChecked(FinegramDebugConfig.INSTANCE.getEditTextSuggestionsFix())
        );
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            items.add(UItem.asButton(audioSourceRow, getString(R.string.FG_Debug_AudioSource), getAudioSourceValue()).slug("audioSource"));
        }
        items.add(SettingsHelper.asSwitchCG(sendMaxQualityRow, getString(R.string.FG_Debug_MaxQuality), getString(R.string.FG_Debug_MaxQuality_Desc))
                .setChecked(FinegramDebugConfig.INSTANCE.getSendVideosAtMaxQuality())
        );
        items.add(SettingsHelper.asSwitchCG(playGifAsVideoRow, getString(R.string.FG_Debug_GifAsVideo))
                .setChecked(FinegramDebugConfig.INSTANCE.getPlayGIFsAsVideos())
        );
        items.add(SettingsHelper.asSwitchCG(hideTimestampRow, getString(R.string.FG_Debug_HideVideoProgress), getString(R.string.FG_Debug_HideVideoProgress_Desc))
                .setChecked(FinegramDebugConfig.INSTANCE.getHideVideoTimestamp())
        );
        items.add(UItem.asButton(resetDialogsRow, 0, getString(R.string.DebugMenuResetDialogs)));
        items.add(UItem.asButton(clearMediaCacheRow, 0, getString(R.string.DebugMenuClearMediaCache)));
        items.add(UItem.asButton(readAllDialogsRow, 0, getString(R.string.DebugMenuReadAllDialogs)));
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.Contacts)));
        items.add(UItem.asButton(importContactsRow, 0, getString(R.string.DebugMenuImportContacts)));
        items.add(UItem.asButton(reloadContactsRow, 0, getString(R.string.DebugMenuReloadContacts)));
        items.add(UItem.asButton(resetContactsRow, 0, getString(R.string.DebugMenuResetContacts)));
        items.add(UItem.asShadow(getString(R.string.FG_Debug_OwnMark)));
        items.add(UItem.asShadow(null));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == toastRpcRow) {
            FinegramDebugConfig.INSTANCE.setShowRPCErrors(!FinegramDebugConfig.INSTANCE.getShowRPCErrors());
            SettingsHelper.updateCheckState(view, FinegramDebugConfig.INSTANCE.getShowRPCErrors());

            showRestartBulletin();
        } else if (item.id == oldTimeStyleRow) {
            FinegramDebugConfig.INSTANCE.setOldTimeStyle(!FinegramDebugConfig.INSTANCE.getOldTimeStyle());
            SettingsHelper.updateCheckState(view, FinegramDebugConfig.INSTANCE.getOldTimeStyle());
        } else if (item.id == performanceClassRow) {
            showPerformanceClassDialog(view);
        } else if (item.id == updatePreviewRow) {

            org.telegram.messenger.ApplicationLoader.applicationLoaderInstance
                    .showUpdaterPreview(this);
        } else if (item.id == fixCallsNotifRow) {
            openFullScreenIntentSettings();
        } else if (item.id == newBlurRow) {
            SharedConfig.toggleUseNewBlur();
            SettingsHelper.updateCheckState(view, SharedConfig.useNewBlur);
        } else if (item.id == forceForumTabsRow) {
            SharedConfig.toggleForceForumTabs();
            SettingsHelper.updateCheckState(view, SharedConfig.forceForumTabs);
        } else if (item.id == replacePunctuationRow) {
            FinegramDebugConfig.INSTANCE.setReplacePunctuationMarks(!FinegramDebugConfig.INSTANCE.getReplacePunctuationMarks());
            SettingsHelper.updateCheckState(view, FinegramDebugConfig.INSTANCE.getReplacePunctuationMarks());

            showRestartBulletin();
        } else if (item.id == editTextFixRow) {
            FinegramDebugConfig.INSTANCE.setEditTextSuggestionsFix(!FinegramDebugConfig.INSTANCE.getEditTextSuggestionsFix());
            SettingsHelper.updateCheckState(view, FinegramDebugConfig.INSTANCE.getEditTextSuggestionsFix());

            showRestartBulletin();
        } else if (item.id == audioSourceRow) {
            showAudioSourceDialog(() -> SettingsHelper.updateButtonValue(view, getAudioSourceValue()));
        } else if (item.id == sendMaxQualityRow) {
            FinegramDebugConfig.INSTANCE.setSendVideosAtMaxQuality(!FinegramDebugConfig.INSTANCE.getSendVideosAtMaxQuality());
            SettingsHelper.updateCheckState(view, FinegramDebugConfig.INSTANCE.getSendVideosAtMaxQuality());
        } else if (item.id == playGifAsVideoRow) {
            FinegramDebugConfig.INSTANCE.setPlayGIFsAsVideos(!FinegramDebugConfig.INSTANCE.getPlayGIFsAsVideos());
            SettingsHelper.updateCheckState(view, FinegramDebugConfig.INSTANCE.getPlayGIFsAsVideos());
        } else if (item.id == hideTimestampRow) {
            FinegramDebugConfig.INSTANCE.setHideVideoTimestamp(!FinegramDebugConfig.INSTANCE.getHideVideoTimestamp());
            SettingsHelper.updateCheckState(view, FinegramDebugConfig.INSTANCE.getHideVideoTimestamp());
        } else if (item.id == resetDialogsRow) {
            getMessagesController().forceResetDialogs();

            showSuccessBulletin();
        } else if (item.id == clearMediaCacheRow) {
            clearMediaCache();
        } else if (item.id == readAllDialogsRow) {
            getMessagesStorage().readAllDialogs(-1);

            showSuccessBulletin();
        } else if (item.id == importContactsRow) {
            getUserConfig().syncContacts = true;
            getUserConfig().saveConfig(false);
            getContactsController().forceImportContacts();

            showSuccessBulletin();
        } else if (item.id == reloadContactsRow) {
            getContactsController().loadContacts(false, 0);

            showSuccessBulletin();
        } else if (item.id == resetContactsRow) {
            getContactsController().resetImportedContacts();

            showSuccessBulletin();
        }
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        return false;
    }

    private void showPerformanceClassDialog(View view) {
        AlertDialog.Builder builder2 = new AlertDialog.Builder(getParentActivity(), getResourceProvider());
        builder2.setTitle(getString(R.string.FG_Debug_PerfClass));
        int currentClass = SharedConfig.getDevicePerformanceClass();
        int trueClass = SharedConfig.measureDevicePerformanceClass();
        builder2.setItems(new CharSequence[]{
                AndroidUtilities.replaceTags((currentClass == SharedConfig.PERFORMANCE_CLASS_HIGH ? "**HIGH**" : "HIGH") + (trueClass == SharedConfig.PERFORMANCE_CLASS_HIGH ? " (measured)" : "")),
                AndroidUtilities.replaceTags((currentClass == SharedConfig.PERFORMANCE_CLASS_AVERAGE ? "**AVERAGE**" : "AVERAGE") + (trueClass == SharedConfig.PERFORMANCE_CLASS_AVERAGE ? " (measured)" : "")),
                AndroidUtilities.replaceTags((currentClass == SharedConfig.PERFORMANCE_CLASS_LOW ? "**LOW**" : "LOW") + (trueClass == SharedConfig.PERFORMANCE_CLASS_LOW ? " (measured)" : ""))
        }, (dialog2, which2) -> {
            int newClass = 2 - which2;
            if (newClass == trueClass) {
                SharedConfig.overrideDevicePerformanceClass(-1);
            } else {
                SharedConfig.overrideDevicePerformanceClass(newClass);
            }

            SettingsHelper.updateButtonValue(view, SharedConfig.performanceClassName(SharedConfig.getDevicePerformanceClass()));

            showRestartBulletin();
        });
        builder2.setNegativeButton(getString(R.string.Cancel), null);
        builder2.show();
    }

    private void openFullScreenIntentSettings() {
        Intent intent = new Intent(Settings.ACTION_MANAGE_APP_USE_FULL_SCREEN_INTENT);
        intent.setData(Uri.parse("package:" + getContext().getPackageName()));
        getParentActivity().startActivity(intent);
    }

    private void showAudioSourceDialog(Runnable runnable) {
        ArrayList<String> configStringKeys = new ArrayList<>();
        ArrayList<Integer> configValues = new ArrayList<>();

        configStringKeys.add("DEFAULT");
        configValues.add(FinegramDebugConfig.AUDIO_SOURCE_DEFAULT);

        configStringKeys.add("CAMCORDER");
        configValues.add(FinegramDebugConfig.AUDIO_SOURCE_CAMCORDER);

        configStringKeys.add("MIC");
        configValues.add(FinegramDebugConfig.AUDIO_SOURCE_MIC);

        configStringKeys.add("REMOTE_SUBMIX");
        configValues.add(FinegramDebugConfig.AUDIO_SOURCE_REMOTE_SUBMIX);

        configStringKeys.add("UNPROCESSED");
        configValues.add(FinegramDebugConfig.AUDIO_SOURCE_UNPROCESSED);

        configStringKeys.add("VOICE_CALL");
        configValues.add(FinegramDebugConfig.AUDIO_SOURCE_VOICE_CALL);

        configStringKeys.add("VOICE_COMMUNICATION");
        configValues.add(FinegramDebugConfig.AUDIO_SOURCE_VOICE_COMMUNICATION);

        configStringKeys.add("VOICE_DOWNLINK");
        configValues.add(FinegramDebugConfig.AUDIO_SOURCE_VOICE_DOWNLINK);

        configStringKeys.add("VOICE_PERFORMANCE");
        configValues.add(FinegramDebugConfig.AUDIO_SOURCE_VOICE_PERFORMANCE);

        configStringKeys.add("VOICE_RECOGNITION");
        configValues.add(FinegramDebugConfig.AUDIO_SOURCE_VOICE_RECOGNITION);

        configStringKeys.add("VOICE_UPLINK");
        configValues.add(FinegramDebugConfig.AUDIO_SOURCE_VOICE_UPLINK);

        PopupHelper.show(configStringKeys, getString(R.string.FG_Debug_AudioSource), configValues.indexOf(FinegramDebugConfig.INSTANCE.getAudioSource()), getContext(), i -> {
            FinegramDebugConfig.INSTANCE.setAudioSource(configValues.get(i));
            if (runnable != null) runnable.run();
        });
    }

    private String getAudioSourceValue() {
        return switch (FinegramDebugConfig.INSTANCE.getAudioSource()) {
            case FinegramDebugConfig.AUDIO_SOURCE_CAMCORDER -> "CAMCORDER";
            case FinegramDebugConfig.AUDIO_SOURCE_MIC -> "MIC";
            case FinegramDebugConfig.AUDIO_SOURCE_REMOTE_SUBMIX -> "REMOTE_SUBMIX";
            case FinegramDebugConfig.AUDIO_SOURCE_UNPROCESSED -> "UNPROCESSED";
            case FinegramDebugConfig.AUDIO_SOURCE_VOICE_CALL -> "VOICE_CALL";
            case FinegramDebugConfig.AUDIO_SOURCE_VOICE_COMMUNICATION -> "VOICE_COMMUNICATION";
            case FinegramDebugConfig.AUDIO_SOURCE_VOICE_DOWNLINK -> "VOICE_DOWNLINK";
            case FinegramDebugConfig.AUDIO_SOURCE_VOICE_PERFORMANCE -> "VOICE_PERFORMANCE";
            case FinegramDebugConfig.AUDIO_SOURCE_VOICE_RECOGNITION -> "VOICE_RECOGNITION";
            case FinegramDebugConfig.AUDIO_SOURCE_VOICE_UPLINK -> "VOICE_UPLINK";
            default -> "DEFAULT";
        };
    }

    private void clearMediaCache() {
        getMessagesStorage().clearSentMedia();
        SharedConfig.setNoSoundHintShowed(false);
        SharedPreferences.Editor editor = MessagesController.getGlobalMainSettings().edit();
        editor.remove("archivehint").remove("proximityhint").remove("archivehint_l").remove("searchpostsnew").remove("speedhint").remove("gifhint").remove("reminderhint").remove("soundHint").remove("themehint").remove("bganimationhint").remove("filterhint").remove("n_0").remove("storyprvhint").remove("storyhint").remove("storyhint2").remove("storydualhint").remove("storysvddualhint").remove("stories_camera").remove("dualcam").remove("dualmatrix").remove("dual_available").remove("archivehint").remove("askNotificationsAfter").remove("askNotificationsDuration").remove("viewoncehint").remove("voicepausehint").remove("taptostorysoundhint").remove("nothanos").remove("voiceoncehint").remove("savedhint").remove("savedsearchhint").remove("savedsearchtaghint").remove("groupEmojiPackHintShown").remove("newppsms").remove("monetizationadshint").remove("seekSpeedHintShowed").remove("unsupport_video/av01").remove("channelgifthint").remove("statusgiftpage").remove("multistorieshint").remove("channelsuggesthint").remove("trimvoicehint").remove("taptostoryhighlighthint").apply();
        MessagesController.getEmojiSettings(currentAccount).edit().remove("featured_hidden").remove("emoji_featured_hidden").apply();
        SharedConfig.textSelectionHintShows = 0;
        SharedConfig.lockRecordAudioVideoHint = 0;
        SharedConfig.stickersReorderingHintUsed = false;
        SharedConfig.forwardingOptionsHintShown = false;
        SharedConfig.replyingOptionsHintShown = false;
        SharedConfig.messageSeenHintCount = 3;
        SharedConfig.emojiInteractionsHintCount = 3;
        SharedConfig.dayNightThemeSwitchHintCount = 3;
        SharedConfig.fastScrollHintCount = 3;
        SharedConfig.stealthModeSendMessageConfirm = 2;
        SharedConfig.updateStealthModeSendMessageConfirm(2);
        SharedConfig.setStoriesReactionsLongPressHintUsed(false);
        SharedConfig.setStoriesIntroShown(false);
        SharedConfig.setMultipleReactionsPromoShowed(false);
        ChatThemeController.getInstance(currentAccount).clearCache();
        getNotificationCenter().postNotificationName(NotificationCenter.newSuggestionsAvailable);
        RestrictedLanguagesSelectActivity.cleanup();
        PersistColorPalette.getInstance(currentAccount).cleanup();
        SharedPreferences prefs = getMessagesController().getMainSettings();
        editor = prefs.edit();
        editor.remove("peerColors").remove("profilePeerColors").remove("boostingappearance").remove("bizbothint").remove("movecaptionhint");
        for (String key : prefs.getAll().keySet()) {
            if (key.contains("show_gift_for_") || key.contains("bdayhint_") || key.contains("bdayanim_") || key.startsWith("ask_paid_message_") || key.startsWith("topicssidetabs")) {
                editor.remove(key);
            }
        }
        editor.apply();
        editor = MessagesController.getNotificationsSettings(currentAccount).edit();
        for (String key : MessagesController.getNotificationsSettings(currentAccount).getAll().keySet()) {
            if (key.startsWith("dialog_bar_botver")) {
                editor.remove(key);
            }
        }
        editor.apply();

        showSuccessBulletin();
    }

    @Override
    protected String getKey() {
        return DeeplinkHelper.DeepLinksRepo.FG_Debug;
    }
}

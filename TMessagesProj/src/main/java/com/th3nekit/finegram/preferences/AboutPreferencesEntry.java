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
import org.telegram.messenger.BuildConfig;
import org.telegram.messenger.BuildVars;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.messenger.SharedConfig;
import org.telegram.messenger.browser.Browser;
import org.telegram.ui.Components.IconBackgroundColors;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import org.telegram.ui.LaunchActivity;
import org.telegram.ui.SettingsActivity;

import java.util.ArrayList;

import com.th3nekit.finegram.core.configs.FinegramCoreConfig;
import com.th3nekit.finegram.core.helpers.FGResourcesHelper;
import com.th3nekit.finegram.core.helpers.DeeplinkHelper;
import com.th3nekit.finegram.misc.Constants;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class AboutPreferencesEntry extends BaseCGPreferencesEntry {

    private static final String EXTERA_USERNAME = "exteraGram";
    private static final String CHERRY_URL = "https://github.com/arsLan4k1390/Cherrygram";
    private static final String TEXT_ANIM_USERNAME = "mihailkotovski";

    private static final String STORE_USERNAME = "KangelPluginsManager";

    private final int readmeRow = 1;
    private final int updatesRow = 2;
    private final int debugPrefsRow = 3;

    private final int channelRow = 4;
    private final int betaChannelRow = 5;
    private final int chatRow = 6;
    private final int offtopicChatRow = 7;
    private final int githubRow = 8;
    private final int crowdinRow = 9;
    private final int policyRow = 10;

    private final int creditsExteraRow = 11;
    private final int creditsCherryRow = 12;
    private final int creditsTextAnimRow = 14;
    private final int creditsStoreRow = 15;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.FGP_Header_About);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        items.add(UItem.asHeader(getString(R.string.Info)));
        items.add(
                SettingsHelper.asTextDetail(
                        readmeRow,
                        0,
                        FGResourcesHelper.getAppName() + " " + FGResourcesHelper.getFinegramVersion() + " | " + "Telegram " + BuildVars.BUILD_VERSION_STRING,

                        getString(R.string.FGP_About_Desc) + System.lineSeparator() + BuildConfig.GET_BUILD_DATE
                )
        );
        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        updatesRow,
                        IconBackgroundColors.GREEN.top, IconBackgroundColors.GREEN.bottom,
                        R.drawable.settings_refresh_filled_solar,
                        getString(R.string.UP_Category_Updates),
                        getLastCheckUpdateTime()
                )
        .slug(DeeplinkHelper.DeepLinksRepo.FG_Updater_Bottom_Sheet));
        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        debugPrefsRow,
                        IconBackgroundColors.PURPLE.top, IconBackgroundColors.PURPLE.bottom,
                        R.drawable.settings_tube_filled_solar,
                        getString(R.string.FG_Debug_Title),
                        getString(R.string.FG_Debug_Desc),
                        true
                )
        .slug(DeeplinkHelper.DeepLinksRepo.FG_Debug));
        items.add(UItem.asShadow(null));

        boolean hasLinks = !Constants.FG_CHANNEL_USERNAME.isEmpty()
                || !Constants.FG_BETA_APKS_CHANNEL_USERNAME.isEmpty()
                || !Constants.FG_CHAT_USERNAME.isEmpty()
                || !Constants.FG_OFFTOPIC_CHAT_USERNAME.isEmpty()
                || (!Constants.FG_GITHUB_URL.isEmpty() && !FinegramCoreConfig.isStandalonePremiumBuild())
                || !Constants.FG_CROWDIN_URL.isEmpty()
                || !Constants.FG_PRIVACY_URL.isEmpty();

        if (hasLinks) {
            items.add(UItem.asHeader(getString(R.string.FGP_Links)));
        }
        if (!Constants.FG_CHANNEL_USERNAME.isEmpty()) {
            items.add(UItem.asButton(channelRow, R.drawable.msg_channel_solar, getString(R.string.FGP_ToChannel)));
        }
        if (!Constants.FG_BETA_APKS_CHANNEL_USERNAME.isEmpty()) {
            items.add(UItem.asButton(betaChannelRow, R.drawable.msg_channel_solar, getString(R.string.FGP_ToBetaChannel)));
        }
        if (!Constants.FG_CHANNEL_USERNAME.isEmpty() || !Constants.FG_BETA_APKS_CHANNEL_USERNAME.isEmpty()) {
            items.add(UItem.asShadow(null));
        }
        if (!Constants.FG_CHAT_USERNAME.isEmpty()) {
            items.add(UItem.asButton(chatRow, R.drawable.msg_discuss_solar, getString(R.string.FGP_ToChat)));
        }
        if (!Constants.FG_OFFTOPIC_CHAT_USERNAME.isEmpty()) {
            items.add(UItem.asButton(offtopicChatRow, R.drawable.msg_discuss_solar, applyNewSpan(getString(R.string.FGP_ToOfftopicChat))));
        }
        if (!Constants.FG_CHAT_USERNAME.isEmpty() || !Constants.FG_OFFTOPIC_CHAT_USERNAME.isEmpty()) {
            items.add(UItem.asShadow(null));
        }

        if (!Constants.FG_GITHUB_URL.isEmpty() && !FinegramCoreConfig.isStandalonePremiumBuild()) {
            String value;
            if (FinegramCoreConfig.isStandaloneBetaBuild() || FinegramCoreConfig.isDevBuild()) {
                value = "GitHub";
            } else {
                value = "commit " + BuildConfig.GIT_COMMIT_HASH.substring(0, Math.min(8, BuildConfig.GIT_COMMIT_HASH.length()));
            }
            items.add(UItem.asButton(githubRow, R.drawable.github_cat, getString(R.string.FGP_Source), value).slug("source_code"));
        }

        if (!Constants.FG_CROWDIN_URL.isEmpty()) {
            items.add(UItem.asButton(crowdinRow, R.drawable.msg_translate_solar, getString(R.string.FGP_Crowdin), "Crowdin").slug("crowdin"));
        }
        if (!Constants.FG_PRIVACY_URL.isEmpty()) {
            items.add(UItem.asButton(policyRow, R.drawable.msg_policy_solar, getString(R.string.PrivacyPolicy)).slug("policy"));
        }
        if (hasLinks) {
            items.add(UItem.asShadow(null));
        }

        items.add(UItem.asHeader(getString(R.string.FG_Credits)));

        items.add(SettingsHelper.asTextDetail(creditsExteraRow, R.drawable.msg_channel_solar,
                getString(R.string.FG_Credits_Extera), "@" + EXTERA_USERNAME));
        items.add(SettingsHelper.asTextDetail(creditsCherryRow, R.drawable.github_cat,
                getString(R.string.FG_Credits_Cherry), "Cherrygram"));
        items.add(SettingsHelper.asTextDetail(creditsTextAnimRow, R.drawable.formatting_bold,
                getString(R.string.FG_Credits_TextAnim), "@" + TEXT_ANIM_USERNAME));
        items.add(SettingsHelper.asTextDetail(creditsStoreRow, R.drawable.msg_stories_saved,
                getString(R.string.FG_Credits_Store), "@" + STORE_USERNAME));
        items.add(UItem.asShadow(getString(R.string.FG_Credits_Extera_Desc)));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == readmeRow) {
            if (Constants.FG_GITHUB_URL.isEmpty()) return;
            Browser.openUrl(getContext(), Constants.FG_GITHUB_URL + "#readme");
        } else if (item.id == updatesRow) {
            if (FinegramCoreConfig.isPlayStoreBuild()) {
                SharedConfig.lastUpdateCheckTime = System.currentTimeMillis();
                SettingsHelper.updateButtonValue(view, getLastCheckUpdateTime());

                if (!Constants.UPDATE_APP_URL.isEmpty()) Browser.openUrl(getContext(), Constants.UPDATE_APP_URL);
            } else {
                if (LaunchActivity.instance == null) return;
                LaunchActivity.instance.showUpdaterBottomSheet(this, false, null);
            }
        } else if (item.id == debugPrefsRow) {
            FinegramPreferencesNavigator.INSTANCE.createDebug(this);
        } else if (item.id == channelRow) {
            getMessagesController().openByUserName(Constants.FG_CHANNEL_USERNAME, this, 1);
        } else if (item.id == betaChannelRow) {
            getMessagesController().openByUserName(Constants.FG_BETA_APKS_CHANNEL_USERNAME, this, 1);
        } else if (item.id == chatRow) {
            getMessagesController().openByUserName(Constants.FG_CHAT_USERNAME, this, 1);
        } else if (item.id == offtopicChatRow) {
            getMessagesController().openByUserName(Constants.FG_OFFTOPIC_CHAT_USERNAME, this, 1);
        } else if (item.id == githubRow) {
            if (FinegramCoreConfig.isStandaloneBetaBuild() || FinegramCoreConfig.isDevBuild()) {
                Browser.openUrl(getContext(), Constants.FG_GITHUB_URL);
            } else {
                Browser.openUrl(getContext(), Constants.FG_GITHUB_URL + "/commit/" + BuildConfig.GIT_COMMIT_HASH);
            }
        } else if (item.id == crowdinRow) {
            Browser.openUrl(getContext(), Constants.FG_CROWDIN_URL);
        } else if (item.id == policyRow) {
            Browser.openUrl(getContext(), Constants.FG_PRIVACY_URL);
        } else if (item.id == creditsExteraRow) {
            getMessagesController().openByUserName(EXTERA_USERNAME, this, 1);
        } else if (item.id == creditsCherryRow) {
            Browser.openUrl(getContext(), CHERRY_URL);
        } else if (item.id == creditsTextAnimRow) {
            getMessagesController().openByUserName(TEXT_ANIM_USERNAME, this, 1);
        } else if (item.id == creditsStoreRow) {
            getMessagesController().openByUserName(STORE_USERNAME, this, 1);
        }
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        if (item.id == readmeRow) {
            if (Constants.FG_GITHUB_URL.isEmpty()) return false;
            AndroidUtilities.addToClipboard(Constants.FG_GITHUB_URL + "#readme");
            return true;
        } else if (item.id == updatesRow) {
            AndroidUtilities.addToClipboard("tg://" + DeeplinkHelper.DeepLinksRepo.FG_Updater_Bottom_Sheet);
            return true;
        } else if (item.id == debugPrefsRow) {
            AndroidUtilities.addToClipboard("tg://" + DeeplinkHelper.DeepLinksRepo.FG_Debug);
            return true;
        } else if (item.id == channelRow) {
            AndroidUtilities.addToClipboard("@" + Constants.FG_CHANNEL_USERNAME);
            return true;
        } else if (item.id == betaChannelRow) {
            AndroidUtilities.addToClipboard("@" + Constants.FG_BETA_APKS_CHANNEL_USERNAME);
            return true;
        } else if (item.id == chatRow) {
            AndroidUtilities.addToClipboard("@" + Constants.FG_CHAT_USERNAME);
            return true;
        } else if (item.id == offtopicChatRow) {
            AndroidUtilities.addToClipboard("@" + Constants.FG_OFFTOPIC_CHAT_USERNAME);
            return true;
        } else if (item.id == crowdinRow) {
            AndroidUtilities.addToClipboard(Constants.FG_CROWDIN_URL);
            return true;
        } else if (item.id == policyRow) {
            AndroidUtilities.addToClipboard(Constants.FG_PRIVACY_URL);
            return true;
        } else if (item.id == creditsExteraRow) {
            AndroidUtilities.addToClipboard("@" + EXTERA_USERNAME);
            return true;
        } else if (item.id == creditsCherryRow) {
            AndroidUtilities.addToClipboard(CHERRY_URL);
            return true;
        } else if (item.id == creditsTextAnimRow) {
            AndroidUtilities.addToClipboard("@" + TEXT_ANIM_USERNAME);
            return true;
        } else if (item.id == creditsStoreRow) {
            AndroidUtilities.addToClipboard("@" + STORE_USERNAME);
            return true;
        }
        return false;
    }

    private String getLastCheckUpdateTime() {
        return getString(R.string.UP_LastCheck) + ": " + LocaleController.formatDateTime(SharedConfig.lastUpdateCheckTime / 1000, true);
    }

    @Override
    protected String getKey() {
        return DeeplinkHelper.DeepLinksRepo.FG_About;
    }
}

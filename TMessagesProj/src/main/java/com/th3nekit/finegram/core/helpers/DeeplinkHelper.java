/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.helpers;

import android.net.Uri;
import com.th3nekit.finegram.preferences.*;
import android.text.TextUtils;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.browser.Browser;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.Components.Premium.LimitReachedBottomSheet;
import org.telegram.ui.LaunchActivity;
import org.telegram.ui.ProxyListActivity;
import org.telegram.ui.Stars.StarsIntroActivity;

import java.util.Locale;

import com.th3nekit.finegram.core.configs.FinegramCoreConfig;
import com.th3nekit.finegram.core.ui.FGBulletinCreator;
import com.th3nekit.finegram.misc.Constants;
import com.th3nekit.finegram.preferences.FinegramPreferencesNavigator;

public class DeeplinkHelper {

    public static Uri normalizeSettingsUri(Uri uri) {
        if (uri == null) return null;
        String scheme = uri.getScheme();
        String host = uri.getHost();
        if (!("https".equalsIgnoreCase(scheme) || "http".equalsIgnoreCase(scheme)) || host == null ||
                !(host.equalsIgnoreCase("t.me") || host.equalsIgnoreCase("telegram.me") || host.equalsIgnoreCase("telegram.dog"))) return uri;
        var segments = uri.getPathSegments();
        if (segments.size() != 2 || !("cgSettings".equals(segments.get(0)) || "fgSettings".equals(segments.get(0)))) return uri;
        String page = segments.get(1).toLowerCase(Locale.US);
        if (page.startsWith("cg_")) page = page.substring(3);
        return new Uri.Builder().scheme("tg").authority("cg_" + page).encodedQuery(uri.getEncodedQuery()).build();
    }

    private static BaseFragment settingsPage(String page) {
        return switch (page) {
            case "cg_settings", "cg_main" -> new FGPreferencesEntry();
            case "cg_general" -> new GeneralPreferencesEntry();
            case "cg_appearance" -> new AppearancePreferencesEntry();
            case "cg_chats" -> new ChatsPreferencesEntry();
            case "cg_messages" -> new MessagesPreferencesEntry();
            case "cg_messages_profiles" -> new MessagesAndProfilesPreferencesEntry();
            case "cg_message_menu", "cg_messages_menu", "cg_ios_menu" -> new MessageMenuPreferencesEntry();
            case "cg_filters", "cg_filter" -> new MessageFiltersPreferencesEntry();
            case "cg_gemini" -> new GeminiPreferencesEntry();
            case "cg_camera" -> new CameraPreferencesEntry();
            case "cg_privacy", "cg_security" -> new PrivacyPreferencesEntry();
            case "cg_about" -> new AboutPreferencesEntry();
            case "cg_debug" -> new DebugPreferencesEntry();
            case "cg_folders" -> new com.th3nekit.finegram.preferences.folders.FoldersPreferencesEntry();
            case "cg_tabs" -> new com.th3nekit.finegram.preferences.tabs.MainTabsPreferencesEntry();
            default -> null;
        };
    }

    public static void processDeepLink(Uri uri, Boolean updateAlways, BaseFragment fragment, Callback callback, Runnable unknown, Browser.Progress progress) {
        if (fragment == null) {
            fragment = LaunchActivity.getSafeLastFragment();
        }
        if (fragment == null) {
            return;
        }
        if (uri == null) {
            unknown.run();
            return;
        }
        var segments = uri.getPathSegments();
        if (segments.isEmpty() || segments.size() > 2) {
            unknown.run();
            return;
        }

        String page = segments.get(segments.size() - 1).toLowerCase(Locale.US);
        if (segments.size() == 2 && !("cgSettings".equals(segments.get(0)) || "fgSettings".equals(segments.get(0)))) {
            unknown.run();
            return;
        }
        if (segments.size() == 2 && !page.startsWith("cg_")) page = "cg_" + page;
        BaseFragment settings = settingsPage(page);
        if (settings != null) {
            String row = uri.getQueryParameter("r");
            if (TextUtils.isEmpty(row)) row = uri.getQueryParameter("row");
            if (!TextUtils.isEmpty(row) && settings instanceof BaseCGPreferencesEntry preferences) {
                preferences.scrollToRow(row, unknown);
            }
            callback.presentFragment(settings);
            return;
        }
        if (segments.size() == 1) {
            var segment = segments.get(0).toLowerCase(Locale.US);
            switch (segment) {
                case DeepLinksRepo.FG_Plugins -> {
                    callback.presentFragment(new com.exteragram.messenger.plugins.ui.PluginsActivity());
                    return;
                }
                case DeepLinksRepo.FG_Store -> {
                    callback.presentFragment(new com.th3nekit.finegram.store.FGStoreActivity());
                    return;
                }
                case DeepLinksRepo.FG_About-> {
                    FinegramPreferencesNavigator.INSTANCE.createAbout(fragment);
                    return;
                }
                case DeepLinksRepo.FG_Appearance -> {
                    FinegramPreferencesNavigator.INSTANCE.createAppearance(fragment);
                    return;
                }
                case DeepLinksRepo.FG_Camera -> {
                    FinegramPreferencesNavigator.INSTANCE.createCamera(fragment);
                    return;
                }
                case DeepLinksRepo.FG_Chats -> {
                    FinegramPreferencesNavigator.INSTANCE.createChats(fragment);
                    return;
                }
                case DeepLinksRepo.FG_Messages -> {
                    FinegramPreferencesNavigator.INSTANCE.createMessages(fragment);
                    return;
                }
                case DeepLinksRepo.FG_Message_Menu, "cg_messages_menu", "cg_ios_menu" -> {
                    FinegramPreferencesNavigator.INSTANCE.createMessageMenu(fragment);
                    return;
                }
                case DeepLinksRepo.FG_Debug -> {
                    FinegramPreferencesNavigator.INSTANCE.createDebug(fragment);
                    return;
                }
                case DeepLinksRepo.FG_Stars -> {
                    new StarsIntroActivity.StarsOptionsSheet(fragment.getContext(), fragment.getResourceProvider()).show();
                    return;
                }
                case DeepLinksRepo.FG_Message_Filters, "cg_filter" -> {
                    FinegramPreferencesNavigator.INSTANCE.createMessageFilter(fragment);
                    return;
                }
                case DeepLinksRepo.FG_Folders -> {
                    FinegramPreferencesNavigator.INSTANCE.createFoldersPrefs(fragment);
                    return;
                }

                case DeepLinksRepo.FG_Gemini -> {
                    FinegramPreferencesNavigator.INSTANCE.createGemini(fragment);
                    return;
                }
                case DeepLinksRepo.FG_General -> {
                    FinegramPreferencesNavigator.INSTANCE.createGeneral(fragment);
                    return;
                }
                case DeepLinksRepo.FG_Messages_And_Profiles -> {
                    FinegramPreferencesNavigator.INSTANCE.createMessagesAndProfiles(fragment);
                    return;
                }
                case DeepLinksRepo.FG_Premium -> {

                    unknown.run();
                    return;
                }
                case DeepLinksRepo.FG_Privacy, "cg_security" -> {
                    FinegramPreferencesNavigator.INSTANCE.createPrivacy(fragment);
                    return;
                }
                case DeepLinksRepo.FG_Proxy -> {
                    fragment.presentFragment(new ProxyListActivity());
                    AndroidUtilities.scrollToFragmentRow(fragment.getParentLayout(), "safeSurfRow");
                    return;
                }
                case DeepLinksRepo.FG_Restart, "cg_reboot", "restart", "reboot" -> {
                    FGBulletinCreator.INSTANCE.createRestartBulletin(fragment);
                    return;
                }
                case DeepLinksRepo.FG_Settings, "cg_main" -> {
                    FinegramPreferencesNavigator.INSTANCE.createFinegramSettings(fragment);
                    return;
                }
                case DeepLinksRepo.FG_Tabs -> {
                    FinegramPreferencesNavigator.INSTANCE.createTabs(fragment);
                    return;
                }
                case DeepLinksRepo.FG_Update, "cg_upgrade", "update", "upgrade" -> {
                    if (FinegramCoreConfig.isPlayStoreBuild()) {
                        Browser.openUrl(fragment.getContext(), Constants.UPDATE_APP_URL);
                    } else {
                        LaunchActivity.instance.checkAppUpdate(true, progress, updateAlways);
                    }
                    return;
                }
                case DeepLinksRepo.FG_Updater_Bottom_Sheet, "updates" -> {
                    if (FinegramCoreConfig.isPlayStoreBuild()) {
                        Browser.openUrl(fragment.getContext(), Constants.UPDATE_APP_URL);
                    } else {
                        LaunchActivity.instance.showUpdaterBottomSheet(fragment, false, null);
                    }
                    return;
                }
                case DeepLinksRepo.FG_Username_Limits -> {
                    fragment.showDialog(new LimitReachedBottomSheet(fragment, fragment.getContext(), LimitReachedBottomSheet.TYPE_PUBLIC_LINKS, fragment.getCurrentAccount(), fragment.getResourceProvider()));
                    return;
                }
                default -> {
                    unknown.run();
                    return;
                }
            }
        }
        unknown.run();
    }

    public interface Callback {
        void presentFragment(BaseFragment fragment);
    }

    public static class DeepLinksRepo {
        public static final String FG_ADS = "cg_ads";

        public static final String FG_Alternative_Support = "cg_alternative_support";

        public static final String FG_Proxy = "cg_proxy";

        public static final String FG_Settings = "cg_settings";

        public static final String FG_Plugins = "cg_plugins";
        public static final String FG_Store = "cg_store";

        public static final String FG_General = "cg_general";

        public static final String FG_Appearance = "cg_appearance";
        public static final String FG_Folders = "cg_folders";
        public static final String FG_Luck = "cg_luck";
        public static final String FG_Tabs = "cg_tabs";
        public static final String FG_Messages_And_Profiles = "cg_messages_profiles";

        public static final String FG_Chats = "cg_chats";
        public static final String FG_Gemini = "cg_gemini";
        public static final String FG_Messages = "cg_messages";
        public static final String FG_Message_Menu = "cg_message_menu";
        public static final String FG_Message_Filters = "cg_filters";

        public static final String FG_Camera = "cg_camera";

        public static final String FG_Privacy = "cg_privacy";

        public static final String FG_Restart = "cg_restart";

        public static final String FG_Support = "cg_support";
        public static final String FG_Support_Force = "cg_support_force";
        public static final String FG_Stars = "cg_stars";

        public static final String FG_About = "cg_about";
        public static final String FG_Debug = "cg_debug";
        public static final String FG_Update = "cg_update";
        public static final String FG_Updater_Bottom_Sheet = "cg_updates";

        public static final String FG_Username_Limits = "cg_username_limits";

        public static final String FG_Premium = "cg_premium";
    }

}

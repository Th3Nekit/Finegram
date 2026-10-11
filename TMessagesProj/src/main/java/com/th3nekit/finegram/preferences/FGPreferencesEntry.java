/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences;

import static org.telegram.messenger.AndroidUtilities.dp;
import static org.telegram.messenger.LocaleController.getString;

import static com.th3nekit.finegram.preferences.helpers.SettingsHelper.applyNewSpan;

import android.content.Context;
import android.view.View;
import android.widget.EditText;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.ActionBarMenu;
import org.telegram.ui.ActionBar.ActionBarMenuItem;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.Bulletin;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.IconBackgroundColors;
import org.telegram.ui.Components.ItemOptions;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import org.telegram.ui.SettingsActivity;

import java.util.ArrayList;

import kotlin.Pair;
import com.th3nekit.finegram.core.crash.CrashLogs;
import com.th3nekit.finegram.core.helpers.AppRestartHelper;
import com.th3nekit.finegram.core.helpers.DeeplinkHelper;
import com.th3nekit.finegram.core.helpers.backup.BackupHelper;
import com.th3nekit.finegram.preferences.helpers.TelegramSettingsHelper;

public class FGPreferencesEntry extends BaseCGPreferencesEntry {

    private static final int FG_ACCENT_TOP = 0xFF0C1445;
    private static final int FG_ACCENT_BOTTOM = 0xFF041A40;

    private final int generalRow = 1;
    private final int appearanceRow = 2;
    private final int chatsRow = 3;
    private final int cameraRow = 4;
    private final int privacyRow = 6;
    private final int anonymityRow = 60;
    private final int aboutRow = 7;
    private final int bypassRow = 11;
    private final int pluginsRow = 12;
    private final int cardsRow = 13;

    public ActionBarMenuItem otherItem;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.FGP_AdvancedSettings);
    }

    @Override
    public View createView(Context context) {
        final ActionBarMenu menu = actionBar.createMenu();
        searchItem = menu.addItem(2, R.drawable.msg_search)
                .setIsSearchField(true)
                .setActionBarMenuItemSearchListener(new ActionBarMenuItem.ActionBarMenuItemSearchListener() {
                    @Override
                    public void onSearchExpand() {
                        searching = true;
                        searchQuery = "";
                        searchResults = new ArrayList<>();
                        if (otherItem != null) otherItem.setVisibility(View.GONE);
                        updateRows(false);
                    }

                    @Override
                    public void onSearchCollapse() {
                        searching = false;
                        searchQuery = "";
                        searchResults = new ArrayList<>();
                        if (otherItem != null) otherItem.setVisibility(View.VISIBLE);
                        updateRows(false);
                    }

                    @Override
                    public void onTextChanged(EditText editText) {
                        searchQuery = editText.getText() == null ? "" : editText.getText().toString();
                        searchResults = FGSettingsSearch.search(searchQuery);
                        updateRows(false);
                    }
                });
        searchItem.setSearchFieldHint(getString(R.string.Search));
        searchItem.setContentDescription(getString(R.string.Search));

        otherItem = menu.addItem(1, R.drawable.ic_ab_other);
        otherItem.setOnClickListener(view -> showItemOptions(otherItem));

        return super.createView(context);
    }

    private boolean searching;
    private String searchQuery = "";
    private ArrayList<FGSettingsSearch.Result> searchResults = new ArrayList<>();

    private static final int SEARCH_RESULT_ID_BASE = 10000;

    private void fillSearchItems(ArrayList<UItem> items) {
        if (searchQuery.trim().isEmpty()) {
            return;
        }
        if (searchResults.isEmpty()) {
            items.add(UItem.asShadow(null));
            items.add(UItem.asHeader(FGSettingsSearch.emptyText()));
            return;
        }
        items.add(UItem.asHeader(getString(R.string.Settings)));
        for (int i = 0; i < searchResults.size(); i++) {
            FGSettingsSearch.Result result = searchResults.get(i);
            items.add(UItem.asButton(SEARCH_RESULT_ID_BASE + i, result.itemTitle, result.screenTitle));
        }
        items.add(UItem.asShadow(null));
    }

    private boolean onSearchResultClick(UItem item) {
        int position = item.id - SEARCH_RESULT_ID_BASE;
        if (position < 0 || position >= searchResults.size()) {
            return false;
        }
        FGSettingsSearch.Result result = searchResults.get(position);
        BaseCGPreferencesEntry screen = result.screenFactory.create();
        if (screen == null) {
            return false;
        }
        if (searchItem != null && searchItem.isSearchFieldVisible2()) {
            actionBar.closeSearchField();
        }
        presentFragment(screen.openAtSetting(result.itemId));
        return true;
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        if (searching) {
            fillSearchItems(items);
            return;
        }
        items.add(UItem.asHeader(getString(R.string.Settings)));
        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        generalRow,
                        IconBackgroundColors.ORANGE_DEEP.bottom, IconBackgroundColors.RED.bottom,
                        R.drawable.settings_filled_solar,
                        getString(R.string.AP_Header_General),
                        getString(R.string.FGP_General_Desc),
                        true
                )
        .slug(DeeplinkHelper.DeepLinksRepo.FG_General));
        Pair<Integer, Integer> colors = TelegramSettingsHelper.Helper.INSTANCE.getProfileButtonColor(getUserConfig().getCurrentUser(), true);
        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        appearanceRow,
                        Theme.isCurrentThemeDay() ? colors.getSecond() : colors.getFirst(),
                        Theme.isCurrentThemeDay() ? colors.getFirst() : colors.getSecond(),
                        R.drawable.settings_palette_filled_solar,
                        getString(R.string.AP_Header_Appearance),
                        getString(R.string.FGP_Appearance_Desc),
                        true
                )
        .slug(DeeplinkHelper.DeepLinksRepo.FG_Appearance));
        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        chatsRow, IconBackgroundColors.ORANGE.top, IconBackgroundColors.ORANGE.bottom,
                        R.drawable.settings_chats_filled_solar,
                        getString(R.string.FilterChats),
                        getString(R.string.FGP_Chats_Desc),
                        true
                )
        .slug(DeeplinkHelper.DeepLinksRepo.FG_Chats));
        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        cameraRow,
                        FG_ACCENT_TOP, FG_ACCENT_BOTTOM,
                        R.drawable.settings_camera_filled_solar,
                        getString(R.string.CP_Category_Camera),
                        getString(R.string.FGP_Camera_Desc),
                        true
                )
        .slug(DeeplinkHelper.DeepLinksRepo.FG_Camera));
        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        cardsRow,
                        IconBackgroundColors.ORANGE.top, IconBackgroundColors.ORANGE.bottom,

                        R.drawable.fg_settings_cards,
                        getString(R.string.FG_Cards_Title),
                        getString(R.string.FG_Cards_Desc),
                        true
                )
        );
        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        pluginsRow,
                        IconBackgroundColors.PURPLE.top, IconBackgroundColors.PURPLE.bottom,

                        R.drawable.fg_settings_plugins,
                        getString(R.string.FG_Plugins),
                        getString(R.string.FG_Plugins_Desc),
                        true
                )
        );
        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        bypassRow,
                        IconBackgroundColors.BLUE_DEEP.top, IconBackgroundColors.BLUE_DEEP.bottom,
                        R.drawable.fg_settings_bypass,
                        getString(R.string.FG_Bypass),
                        getString(R.string.FG_Bypass_Desc),
                        true
                )
        );
        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        anonymityRow,
                        IconBackgroundColors.PURPLE.top, IconBackgroundColors.PURPLE.bottom,
                        R.drawable.msg_viewintopic,
                        getString(R.string.FG_Anonymity),
                        getString(R.string.FG_Anonymity_Desc),
                        true
                )
        );
        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        privacyRow,
                        IconBackgroundColors.CYAN.top, IconBackgroundColors.CYAN.bottom,
                        R.drawable.settings_privacy_solar_filled,
                        getString(R.string.SettingsPrivacySecurity),
                        getString(R.string.FGP_Privacy_Desc),
                        true
                )
        .slug(DeeplinkHelper.DeepLinksRepo.FG_Privacy));
        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        aboutRow,
                        FG_ACCENT_TOP, FG_ACCENT_BOTTOM,
                        R.drawable.settings_info_filled_solar,
                        getString(R.string.FGP_Header_About),
                        getString(R.string.FGP_Header_About_Desc),
                        true
                )
        .slug(DeeplinkHelper.DeepLinksRepo.FG_About));
        items.add(UItem.asShadow(null));

        items.add(UItem.asShadow(null));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (searching && item.id >= SEARCH_RESULT_ID_BASE) {
            onSearchResultClick(item);
            return;
        }
        if (item.id == generalRow) {
            FinegramPreferencesNavigator.INSTANCE.createGeneral(this);
        } else if (item.id == appearanceRow) {
            FinegramPreferencesNavigator.INSTANCE.createAppearance(this);
        } else if (item.id == chatsRow) {
            FinegramPreferencesNavigator.INSTANCE.createChats(this);
        } else if (item.id == cameraRow) {
            FinegramPreferencesNavigator.INSTANCE.createCamera(this);
        } else if (item.id == cardsRow) {
            presentFragment(new com.th3nekit.finegram.cards.preferences.FGCardsPreferencesEntry());
        } else if (item.id == pluginsRow) {
            presentFragment(new com.exteragram.messenger.plugins.ui.PluginsActivity());
        } else if (item.id == bypassRow) {
            presentFragment(new NetworkPreferencesEntry());
        } else if (item.id == anonymityRow) {
            presentFragment(new AnonymityPreferencesEntry());
        } else if (item.id == privacyRow) {
            FinegramPreferencesNavigator.INSTANCE.createPrivacy(this);
        } else if (item.id == aboutRow) {
            FinegramPreferencesNavigator.INSTANCE.createAbout(this);
        }
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        if (item.id == generalRow) {
            AndroidUtilities.addToClipboard("tg://" + DeeplinkHelper.DeepLinksRepo.FG_General);
            return true;
        } else if (item.id == appearanceRow) {
            AndroidUtilities.addToClipboard("tg://" + DeeplinkHelper.DeepLinksRepo.FG_Appearance);
            return true;
        } else if (item.id == chatsRow) {
            AndroidUtilities.addToClipboard("tg://" + DeeplinkHelper.DeepLinksRepo.FG_Chats);
            return true;
        } else if (item.id == cameraRow) {
            AndroidUtilities.addToClipboard("tg://" + DeeplinkHelper.DeepLinksRepo.FG_Camera);
            return true;
        } else if (item.id == privacyRow) {
            AndroidUtilities.addToClipboard("tg://" + DeeplinkHelper.DeepLinksRepo.FG_Privacy);
            return true;
        } else if (item.id == aboutRow) {
            AndroidUtilities.addToClipboard("tg://" + DeeplinkHelper.DeepLinksRepo.FG_About);
            return true;
        }
        return false;
    }

    private void showItemOptions(View button) {
        ItemOptions o = ItemOptions.makeOptions(this, button);

        o.add(R.drawable.msg_instant_link_solar, getString(R.string.FG_ExportSettings), () -> BackupHelper.INSTANCE.backupSettings(this));
        o.add(R.drawable.msg_photo_settings_solar, getString(R.string.FG_ImportSettings), () -> BackupHelper.INSTANCE.importSettings(this));
        o.addGap();
        o.add(R.drawable.bug_solar, getString(R.string.FG_CopyReportDetails), () -> {
            AndroidUtilities.addToClipboard(CrashLogs.getReportMessage() + "\n\n#bug");
            BulletinFactory.of(this).createErrorBulletin(getString(R.string.FG_ReportDetailsCopied))
                    .setDuration(Bulletin.DURATION_SHORT)
                    .show();
        });

        o.addSpaceGap();

        o.add(R.drawable.msg_retry_solar, getString(R.string.FG_Restart), () -> AppRestartHelper.restartApp(getContext()));

        o.setBlur(false);
        o.setDrawScrim(false);
        o.translate(0F, -dp(48F));
        o.show();
    }

    @Override
    protected String getKey() {
        return DeeplinkHelper.DeepLinksRepo.FG_Settings;
    }

}

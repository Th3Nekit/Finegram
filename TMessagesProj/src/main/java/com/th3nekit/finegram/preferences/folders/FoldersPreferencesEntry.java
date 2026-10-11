/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences.folders;

import com.th3nekit.finegram.core.helpers.DeeplinkHelper;

import static org.telegram.messenger.LocaleController.getString;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.R;
import org.telegram.ui.Components.IconBackgroundColors;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import org.telegram.ui.FiltersSetupActivity;
import org.telegram.ui.SettingsActivity;

import java.util.ArrayList;

import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig;
import com.th3nekit.finegram.helpers.ui.PopupHelper;
import com.th3nekit.finegram.preferences.BaseCGPreferencesEntry;
import com.th3nekit.finegram.preferences.folders.cells.FoldersPreviewCell;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class FoldersPreferencesEntry extends BaseCGPreferencesEntry {

    protected FoldersPreviewCell foldersPreviewCell;

    private final int hideAllChatsTabRow = 1;

    private final int hideCounterRow = 2;
    private final int tabIconTypeRow = 3;
    private final int addStrokeRow = 4;

    private final int folderNameAppHeaderRow = 5;
    private final int foldersAtBottomRow = 6;

    private final int telegramFoldersSettings = 7;
    private final int cornerBadgesRow = 8;
    private final int includeMutedRow = 9;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.CP_Filters_Header);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        if (foldersPreviewCell == null) {
            foldersPreviewCell = new FoldersPreviewCell(getContext());
            foldersPreviewCell.setLayoutParams(new RecyclerView.LayoutParams(RecyclerView.LayoutParams.MATCH_PARENT, RecyclerView.LayoutParams.WRAP_CONTENT));
        }
        items.add(SettingsHelper.asCustomWithBackground(foldersPreviewCell));
        items.add(UItem.asShadow(null));

        items.add(SettingsHelper.asSwitchCG(hideAllChatsTabRow, getString(R.string.CP_NewTabs_RemoveAllChats))
                .setChecked(FinegramAppearanceConfig.INSTANCE.getTabsHideAllChats())
        .slug("hideAllChatsTab"));
        items.add(SettingsHelper.asSwitchCG(hideCounterRow, getString(R.string.CP_NewTabs_NoCounter))
                .setChecked(FinegramAppearanceConfig.INSTANCE.getTabsNoUnread())
        .slug("hideCounterInTabs"));
        if (!FinegramAppearanceConfig.INSTANCE.getTabsNoUnread()) {
            items.add(SettingsHelper.asSwitchCG(includeMutedRow, getString(R.string.CP_IncludeMutedIn_FolderCounters))
                    .setChecked(FinegramAppearanceConfig.INSTANCE.getTabsIncludeMutedInCounter()).slug("includeMutedInCounters"));
        }
        items.add(UItem.asButton(tabIconTypeRow, getString(R.string.AP_Tab_Style), getTabModeValue()).slug("style"));
        if (FinegramAppearanceConfig.INSTANCE.getTabMode() == FinegramAppearanceConfig.TAB_TYPE_ICON
                && !FinegramAppearanceConfig.INSTANCE.getTabsNoUnread()) {
            items.add(SettingsHelper.asSwitchCG(cornerBadgesRow, getString(R.string.FG_FolderCornerBadges))
                    .setChecked(FinegramAppearanceConfig.INSTANCE.getFolderCornerBadges()).slug("cornerBadges"));
        }
        items.add(SettingsHelper.asSwitchCG(addStrokeRow, getString(R.string.AP_Tab_Style_Stroke))
                .setChecked(FinegramAppearanceConfig.INSTANCE.getTabStyleStroke())
        .slug("stroke"));
        items.add(UItem.asShadow(null));

        items.add(SettingsHelper.asSwitchCG(folderNameAppHeaderRow, getString(R.string.AP_FolderNameInHeader), getString(R.string.AP_FolderNameInHeader_Desc))
                .setChecked(FinegramAppearanceConfig.INSTANCE.getFolderNameInHeader())
        .slug("folderNameInHeader"));
        items.add(SettingsHelper.asSwitchCG(foldersAtBottomRow, getString(R.string.AP_FoldersAtBottom))
                .setChecked(FinegramAppearanceConfig.INSTANCE.getFoldersAtBottom()).setLocked(false)
        .slug("foldersAtBottom"));
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.AppName)));
        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        telegramFoldersSettings,
                        IconBackgroundColors.BLUE_ALT.top, IconBackgroundColors.BLUE_ALT.bottom,
                        R.drawable.settings_folders,
                        getString(R.string.SettingsFolders),
                        getString(R.string.SettingsFoldersInfo)
                )
        );
        items.add(UItem.asShadow(null));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == hideAllChatsTabRow) {
            FinegramAppearanceConfig.INSTANCE.setTabsHideAllChats(!FinegramAppearanceConfig.INSTANCE.getTabsHideAllChats());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getTabsHideAllChats());

            foldersPreviewCell.updateAllChatsTabName(true);

            if (parentLayout != null) parentLayout.rebuildAllFragmentViews(false, false);

            getNotificationCenter().postNotificationName(NotificationCenter.dialogFiltersUpdated);
            getNotificationCenter().postNotificationName(NotificationCenter.mainUserInfoChanged);
        } else if (item.id == hideCounterRow) {
            FinegramAppearanceConfig.INSTANCE.setTabsNoUnread(!FinegramAppearanceConfig.INSTANCE.getTabsNoUnread());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getTabsNoUnread());

            foldersPreviewCell.updateTabCounter(true);
            updateRows(true);

            if (parentLayout != null) parentLayout.rebuildAllFragmentViews(false, false);

            getNotificationCenter().postNotificationName(NotificationCenter.dialogFiltersUpdated);
        } else if (item.id == includeMutedRow) {
            FinegramAppearanceConfig.INSTANCE.setTabsIncludeMutedInCounter(!FinegramAppearanceConfig.INSTANCE.getTabsIncludeMutedInCounter());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getTabsIncludeMutedInCounter());
            showRestartBulletin();
        } else if (item.id == tabIconTypeRow) {
            ArrayList<String> configStringKeys = new ArrayList<>();
            ArrayList<Integer> configValues = new ArrayList<>();

            configStringKeys.add(getString(R.string.FG_FoldersTypeIconsTitles));
            configValues.add(FinegramAppearanceConfig.TAB_TYPE_MIX);

            configStringKeys.add(getString(R.string.FG_FoldersTypeTitles));
            configValues.add(FinegramAppearanceConfig.TAB_TYPE_TEXT);

            configStringKeys.add(getString(R.string.FG_FoldersTypeIcons));
            configValues.add(FinegramAppearanceConfig.TAB_TYPE_ICON);

            PopupHelper.show(configStringKeys, getString(R.string.AP_Tab_Style), configValues.indexOf(FinegramAppearanceConfig.INSTANCE.getTabMode()), getContext(), i -> {
                FinegramAppearanceConfig.INSTANCE.setTabMode(configValues.get(i));
                SettingsHelper.updateButtonValue(view, getTabModeValue());

                foldersPreviewCell.updateTabIcons(true);
                foldersPreviewCell.updateTabTitle(true);
                updateRows(true);

                if (parentLayout != null) parentLayout.rebuildAllFragmentViews(false, false);

                getNotificationCenter().postNotificationName(NotificationCenter.dialogFiltersUpdated);
            });
        } else if (item.id == cornerBadgesRow) {
            FinegramAppearanceConfig.INSTANCE.setFolderCornerBadges(!FinegramAppearanceConfig.INSTANCE.getFolderCornerBadges());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getFolderCornerBadges());
            foldersPreviewCell.invalidate();
            getNotificationCenter().postNotificationName(NotificationCenter.dialogFiltersUpdated);
            if (parentLayout != null) parentLayout.rebuildAllFragmentViews(false, false);
        } else if (item.id == addStrokeRow) {
            FinegramAppearanceConfig.INSTANCE.setTabStyleStroke(!FinegramAppearanceConfig.INSTANCE.getTabStyleStroke());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getTabStyleStroke());

            foldersPreviewCell.invalidate();
            if (parentLayout != null) parentLayout.rebuildAllFragmentViews(false, false);
        } else if (item.id == folderNameAppHeaderRow) {
            FinegramAppearanceConfig.INSTANCE.setFolderNameInHeader(!FinegramAppearanceConfig.INSTANCE.getFolderNameInHeader());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getFolderNameInHeader());

            if (parentLayout != null) parentLayout.rebuildAllFragmentViews(false, false);

            getNotificationCenter().postNotificationName(NotificationCenter.dialogFiltersUpdated);
        } else if (item.id == foldersAtBottomRow) {
            FinegramAppearanceConfig.INSTANCE.setFoldersAtBottom(!FinegramAppearanceConfig.INSTANCE.getFoldersAtBottom());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getFoldersAtBottom());

            showRestartBulletin();
        } else if (item.id == telegramFoldersSettings) {
            presentFragment(new FiltersSetupActivity());
        }
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        return false;
    }

    private String getTabModeValue() {
        return switch (FinegramAppearanceConfig.INSTANCE.getTabMode()) {
            case FinegramAppearanceConfig.TAB_TYPE_MIX -> getString(R.string.FG_FoldersTypeIconsTitles);
            case FinegramAppearanceConfig.TAB_TYPE_ICON -> getString(R.string.FG_FoldersTypeIcons);
            default -> getString(R.string.FG_FoldersTypeTitles);
        };
    }

    @Override
    protected String getKey() {
        return DeeplinkHelper.DeepLinksRepo.FG_Folders;
    }
}

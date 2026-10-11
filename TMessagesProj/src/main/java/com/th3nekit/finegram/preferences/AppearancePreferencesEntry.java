/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences;

import com.th3nekit.finegram.core.ui.FGTitles;
import static org.telegram.messenger.LocaleController.getString;

import android.app.Activity;
import android.content.Intent;
import android.view.Gravity;
import android.view.inputmethod.EditorInfo;
import android.widget.FrameLayout;
import android.view.View;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.NotificationCenter;
import android.util.TypedValue;
import org.telegram.ui.Components.EditTextBoldCursor;
import org.telegram.messenger.FileLog;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.IconBackgroundColors;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import org.telegram.ui.LaunchActivity;
import org.telegram.ui.SettingsActivity;

import java.util.ArrayList;

import kotlin.Pair;
import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig;
import com.th3nekit.finegram.core.configs.FinegramCoreConfig;
import com.th3nekit.finegram.core.icons.pack.IconPack;
import com.th3nekit.finegram.core.ui.FGAvatars;
import com.th3nekit.finegram.core.icons.pack.IconPackManager;
import com.th3nekit.finegram.core.icons.pack.IconPackPreviewSheet;
import com.th3nekit.finegram.core.helpers.DeeplinkHelper;
import com.th3nekit.finegram.helpers.ui.PopupHelper;
import com.th3nekit.finegram.preferences.helpers.AlertDialogSwitchers;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;
import com.th3nekit.finegram.preferences.helpers.TelegramSettingsHelper;

public class AppearancePreferencesEntry extends BaseCGPreferencesEntry {

    private final int centerTitleRow = 1;
    private final int hideSearchBar = 2;
    private final int snowflakesRow = 3;

    private final int iconPackRow = 4;
    private final int oneUISwitchesRow = 5;
    private final int disableDividersRow = 6;
    private final int oldNotificationIconRow = 62;
    private final int notificationAvatarRow = 63;
    private final int avatarCornersRow = 7, forumAvatarsRow = 8, customTitleRow = 9, strongerBlurRow = 10;
    private final int mediaGlowRow = 11, onlineIndicatorRow = 12, textAnimationRow = 13;

    private final int chatListStyleRow = 64;
    private final int foldersRow = 18;
    private final int bottomTabsRow = 19;
    private final int messagesAndProfilesRow = 20;
    private final int sideDrawerRow = 21;
    private final int sideDrawerPhoneRow = 22;
    private final int sideDrawerItemsRow = 23;
    private final int sideDrawerRoundedRow = 24, bottomTabsWithDrawerRow = 1600;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.AP_Header_Appearance);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        items.add(UItem.asHeader(getString(R.string.AP_Header)));
        items.add(SettingsHelper.asSwitchCG(centerTitleRow, getString(R.string.AP_CenterTitle))
                .setChecked(FinegramAppearanceConfig.INSTANCE.getCenterTitle())
        .slug("centerTitle"));
        items.add(SettingsHelper.asSwitchCG(hideSearchBar, getString(R.string.AP_HideSearchBar))
                .setChecked(FinegramAppearanceConfig.INSTANCE.getHideSearchFiled())
        .slug("hideSearchBar"));
        items.add(SettingsHelper.asSwitchCG(snowflakesRow, getString(R.string.CP_Snowflakes_Header))
                .setChecked(FinegramAppearanceConfig.INSTANCE.getDrawSnowInActionBar())
        .slug("snowflakes"));
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.AP_Header_Appearance)));
        items.add(UItem.asButton(iconPackRow, getString(R.string.AP_IconReplacements), getIconPackValueText()));
        items.add(UItem.asButton(oneUISwitchesRow, getString(R.string.FG_SwitchStyle), getSwitchStyleValueText()).slug("oneUI_Switches"));
        items.add(UItem.asButton(avatarCornersRow, getString(R.string.FG_AvatarCorners), getAvatarCornersValueText()));
        items.add(SettingsHelper.asSwitchCG(forumAvatarsRow, getString(R.string.FG_ForumAvatars), getString(R.string.FG_ForumAvatars_Desc))
                .setChecked(FinegramAppearanceConfig.INSTANCE.getForumAvatarsLikeChats())
        );
        items.add(UItem.asButton(customTitleRow, getString(R.string.FG_CustomTitle), getCustomTitleValueText()));
        items.add(SettingsHelper.asSwitchCG(strongerBlurRow, getString(R.string.FG_StrongerBlur), getString(R.string.FG_StrongerBlur_Desc))
                .setChecked(FinegramAppearanceConfig.INSTANCE.getStrongerBlur())
        );
        items.add(SettingsHelper.asSwitchCG(mediaGlowRow, getString(R.string.FG_MediaGlow), getString(R.string.FG_MediaGlow_Desc))
                .setChecked(FinegramAppearanceConfig.INSTANCE.getMediaGlow())
        );
        items.add(SettingsHelper.asSwitchCG(onlineIndicatorRow, getString(R.string.FG_OnlineIndicator), getString(R.string.FG_OnlineIndicator_Desc))
                .setChecked(FinegramAppearanceConfig.INSTANCE.getGroupOnlineIndicator())
        );
        items.add(UItem.asButton(textAnimationRow, getString(R.string.FG_TextAnim),
                getString(FinegramAppearanceConfig.INSTANCE.getComposerAppear()
                        || FinegramAppearanceConfig.INSTANCE.getComposerEffects()
                        || FinegramAppearanceConfig.INSTANCE.getComposerCursorGlide()
                        ? R.string.FG_TextAnim_On : R.string.FG_TextAnim_Off)));
        items.add(UItem.asShadow(null));
        items.add(UItem.asHeader(getString(R.string.NotificationsService)));
        items.add(SettingsHelper.asSwitchCG(notificationAvatarRow,
                        getString(R.string.FG_NotificationAvatar), getString(R.string.FG_NotificationAvatar_Desc))
                .setChecked(FinegramCoreConfig.INSTANCE.getNotificationChatAvatar()));
        items.add(SettingsHelper.asSwitchCG(oldNotificationIconRow,
                        getString(R.string.FG_NotificationIconOld), getString(R.string.FG_NotificationIconOld_Desc))
                .setChecked(FinegramCoreConfig.INSTANCE.getOldNotificationIcon()));
        items.add(UItem.asShadow(null));

        items.add(SettingsHelper.asSwitchCG(disableDividersRow, getString(R.string.AP_DisableDividers))
                .setChecked(FinegramAppearanceConfig.INSTANCE.getDisableDividers())
        .slug("dividers"));
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.LocalMiscellaneousCache)));
        items.add(UItem.asButton(chatListStyleRow, getString(R.string.FG_ChatListStyle),
                getString(FinegramAppearanceConfig.INSTANCE.getIosChatList()
                        ? R.string.FG_ChatListStyle_IOS : R.string.FG_SwitchStyle_Telegram)));
        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        foldersRow,
                        IconBackgroundColors.BLUE_ALT.top, IconBackgroundColors.BLUE_ALT.bottom,
                        R.drawable.settings_folders_filled_solar,
                        getString(R.string.CP_Filters_Header),
                        getString(R.string.FGP_Folders_Desc),
                        true
                )
        .slug(DeeplinkHelper.DeepLinksRepo.FG_Folders));
        items.add(SettingsHelper.asSwitchCG(sideDrawerRow, getString(R.string.FG_SideDrawer),
                        getString(R.string.FG_SideDrawer_Desc))
                .setChecked(FinegramAppearanceConfig.INSTANCE.getSideDrawer())
        );
        if (FinegramAppearanceConfig.INSTANCE.getSideDrawer()) {

            items.add(SettingsHelper.asSwitchCG(sideDrawerPhoneRow, getString(R.string.FG_SideDrawer_Phone))
                    .setChecked(FinegramAppearanceConfig.INSTANCE.getSideDrawerPhone())
            );
            items.add(SettingsHelper.asSwitchCG(sideDrawerRoundedRow, getString(R.string.FG_SideDrawer_Rounded))
                    .setChecked(FinegramAppearanceConfig.INSTANCE.getSideDrawerRounded())
            );
            items.add(SettingsHelper.asSwitchCG(bottomTabsWithDrawerRow,
                            getString(R.string.FG_BottomTabsWithDrawer),
                            getString(R.string.FG_BottomTabsWithDrawer_Desc))
                    .setChecked(FinegramAppearanceConfig.INSTANCE.getBottomTabsWithDrawer())
            );
            items.add(UItem.asButton(sideDrawerItemsRow, getString(R.string.FG_DrawerItems)));
        }
        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        bottomTabsRow,
                        IconBackgroundColors.ORANGE_DEEP.top, IconBackgroundColors.ORANGE_DEEP.bottom,
                        R.drawable.settings_reorder_filled_solar,
                        getString(R.string.CP_MainTabs_Header),
                        getString(R.string.FGP_BottomTabs_Desc),
                        true
                )
        .slug(DeeplinkHelper.DeepLinksRepo.FG_Tabs));
        Pair<Integer, Integer> colors = TelegramSettingsHelper.Helper.INSTANCE.getProfileButtonColor(getUserConfig().getCurrentUser(), true);
        items.add(
                SettingsActivity.SettingCell.Factory.of(
                        messagesAndProfilesRow,
                        Theme.isCurrentThemeDay() ? colors.getSecond() : colors.getFirst(),
                        Theme.isCurrentThemeDay() ? colors.getFirst() : colors.getSecond(),
                        R.drawable.settings_customize_filled_solar,
                        getString(R.string.CP_ProfileReplyBackground),
                        getString(R.string.FGP_MessagesProfiles_Desc),
                        true
                )
        .slug(DeeplinkHelper.DeepLinksRepo.FG_Messages_And_Profiles));
        items.add(UItem.asShadow(null));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == centerTitleRow) {
            FinegramAppearanceConfig.INSTANCE.setCenterTitle(!FinegramAppearanceConfig.INSTANCE.getCenterTitle());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getCenterTitle());

            getParentLayout().rebuildAllFragmentViews(true, true);
        } else  if (item.id == hideSearchBar) {
            FinegramAppearanceConfig.INSTANCE.setHideSearchFiled(!FinegramAppearanceConfig.INSTANCE.getHideSearchFiled());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getHideSearchFiled());

            getNotificationCenter().postNotificationName(NotificationCenter.cgUpdateSearchFiledVisibility);
        } else if (item.id == snowflakesRow) {
            FinegramAppearanceConfig.INSTANCE.setDrawSnowInActionBar(!FinegramAppearanceConfig.INSTANCE.getDrawSnowInActionBar());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getDrawSnowInActionBar());

            showRestartBulletin();
        } else if (item.id == iconPackRow) {
            showIconPackChooser(view);
        } else if (item.id == customTitleRow) {
            showCustomTitleDialog(view);
        } else if (item.id == textAnimationRow) {
            presentFragment(new TextAnimationPreferencesEntry());
        } else if (item.id == onlineIndicatorRow) {
            FinegramAppearanceConfig.INSTANCE.setGroupOnlineIndicator(!FinegramAppearanceConfig.INSTANCE.getGroupOnlineIndicator());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getGroupOnlineIndicator());
        } else if (item.id == mediaGlowRow) {
            FinegramAppearanceConfig.INSTANCE.setMediaGlow(!FinegramAppearanceConfig.INSTANCE.getMediaGlow());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getMediaGlow());
        } else if (item.id == strongerBlurRow) {
            FinegramAppearanceConfig.INSTANCE.setStrongerBlur(!FinegramAppearanceConfig.INSTANCE.getStrongerBlur());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getStrongerBlur());
            Theme.reloadAllResources(getContext() != null ? getContext() : ApplicationLoader.applicationContext);
        } else if (item.id == forumAvatarsRow) {
            FinegramAppearanceConfig.INSTANCE.setForumAvatarsLikeChats(!FinegramAppearanceConfig.INSTANCE.getForumAvatarsLikeChats());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getForumAvatarsLikeChats());
            getParentLayout().rebuildAllFragmentViews(true, true);
        } else if (item.id == avatarCornersRow) {
            showAvatarCornersDialog(view);
        } else if (item.id == oneUISwitchesRow) {
            showSwitchStyleChooser(view);
        } else if (item.id == notificationAvatarRow) {
            FinegramCoreConfig.INSTANCE.setNotificationChatAvatar(!FinegramCoreConfig.INSTANCE.getNotificationChatAvatar());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getNotificationChatAvatar());
        } else if (item.id == oldNotificationIconRow) {
            FinegramCoreConfig.INSTANCE.setOldNotificationIcon(!FinegramCoreConfig.INSTANCE.getOldNotificationIcon());
            SettingsHelper.updateCheckState(view, FinegramCoreConfig.INSTANCE.getOldNotificationIcon());
        } else if (item.id == disableDividersRow) {
            FinegramAppearanceConfig.INSTANCE.setDisableDividers(!FinegramAppearanceConfig.INSTANCE.getDisableDividers());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getDisableDividers());

            Theme.applyCommonTheme();
            updateRows(true);
        } else if (item.id == chatListStyleRow) {
            ArrayList<String> styles = new ArrayList<>();
            styles.add(getString(R.string.FG_SwitchStyle_Telegram));
            styles.add(getString(R.string.FG_ChatListStyle_IOS));
            PopupHelper.show(styles, getString(R.string.FG_ChatListStyle),
                    FinegramAppearanceConfig.INSTANCE.getIosChatList() ? 1 : 0, getContext(), i -> {
                FinegramAppearanceConfig.INSTANCE.setIosChatList(i == 1);
                updateRows(true);
                getParentLayout().rebuildAllFragmentViews(true, true);
            });
        } else if (item.id == foldersRow) {
            FinegramPreferencesNavigator.INSTANCE.createFoldersPrefs(this);
        } else if (item.id == sideDrawerRow) {
            FinegramAppearanceConfig.INSTANCE.setSideDrawer(!FinegramAppearanceConfig.INSTANCE.getSideDrawer());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getSideDrawer());

            updateRows(true);
            showRestartBulletin();
        } else if (item.id == bottomTabsWithDrawerRow) {
            FinegramAppearanceConfig.INSTANCE.setBottomTabsWithDrawer(
                    !FinegramAppearanceConfig.INSTANCE.getBottomTabsWithDrawer());
            SettingsHelper.updateCheckState(view,
                    FinegramAppearanceConfig.INSTANCE.getBottomTabsWithDrawer());

            showRestartBulletin();
        } else if (item.id == sideDrawerRoundedRow) {
            FinegramAppearanceConfig.INSTANCE.setSideDrawerRounded(!FinegramAppearanceConfig.INSTANCE.getSideDrawerRounded());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getSideDrawerRounded());
        } else if (item.id == sideDrawerItemsRow) {
            presentFragment(new DrawerItemsPreferencesEntry());
        } else if (item.id == sideDrawerPhoneRow) {
            FinegramAppearanceConfig.INSTANCE.setSideDrawerPhone(
                    !FinegramAppearanceConfig.INSTANCE.getSideDrawerPhone());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getSideDrawerPhone());
        } else if (item.id == bottomTabsRow) {
            FinegramPreferencesNavigator.INSTANCE.createTabs(this);
        } else if (item.id == messagesAndProfilesRow) {
            FinegramPreferencesNavigator.INSTANCE.createMessagesAndProfiles(this);
        }
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        if (item.id == iconPackRow) {
            IconPack active = IconPackManager.INSTANCE.active();
            if (active != null) {
                showIconPackOptions(view, active);
                return true;
            }
            return false;
        } else if (item.id == foldersRow) {
            AndroidUtilities.addToClipboard("tg://" + DeeplinkHelper.DeepLinksRepo.FG_Folders);
            return true;
        } else if (item.id == bottomTabsRow) {
            AndroidUtilities.addToClipboard("tg://" + DeeplinkHelper.DeepLinksRepo.FG_Tabs);
            return true;
        } else if (item.id == messagesAndProfilesRow) {
            AndroidUtilities.addToClipboard("tg://" + DeeplinkHelper.DeepLinksRepo.FG_Messages_And_Profiles);
            return true;
        }
        return false;
    }

    private String getCustomTitleValueText() {
        String custom = FinegramAppearanceConfig.INSTANCE.getCustomTitleText().trim();
        if (!FinegramAppearanceConfig.INSTANCE.getCustomTitleEnabled() || custom.isEmpty()) {
            return getString(R.string.Default);
        }
        return custom;
    }

    private void showCustomTitleDialog(View view) {
        if (getParentActivity() == null) {
            return;
        }
        AlertDialog.Builder builder = new AlertDialog.Builder(getParentActivity(), getResourceProvider());
        builder.setTitle(getString(R.string.FG_CustomTitle));

        EditTextBoldCursor editText = new EditTextBoldCursor(getParentActivity());
        editText.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 16);
        editText.setTextColor(getThemedColor(Theme.key_dialogTextBlack));
        editText.setHintText(FGTitles.appName());
        editText.setHintColor(getThemedColor(Theme.key_dialogTextHint));
        editText.setCursorColor(getThemedColor(Theme.key_chats_actionBackground));
        editText.setCursorSize(AndroidUtilities.dp(20));
        editText.setCursorWidth(1.5f);
        editText.setSingleLine(true);
        editText.setImeOptions(EditorInfo.IME_ACTION_DONE);
        editText.setBackground(null);

        editText.setPadding(0, 0, 0, 0);
        editText.setText(FinegramAppearanceConfig.INSTANCE.getCustomTitleText());
        editText.setSelection(editText.getText().length());

        FrameLayout field = new FrameLayout(getParentActivity());
        field.setBackground(Theme.createRoundRectDrawable(AndroidUtilities.dp(10),
                getThemedColor(Theme.key_dialogSearchBackground)));
        field.addView(editText, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                Gravity.LEFT | Gravity.TOP, 16, 12, 16, 12));

        FrameLayout wrapper = new FrameLayout(getParentActivity());
        wrapper.addView(field, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                Gravity.LEFT | Gravity.TOP, 22, 4, 22, 4));
        builder.setView(wrapper);

        builder.setPositiveButton(getString(R.string.Save), (dialog, which) -> {
            String value = editText.getText().toString().trim();
            FinegramAppearanceConfig.INSTANCE.setCustomTitleText(value);
            FinegramAppearanceConfig.INSTANCE.setCustomTitleEnabled(!value.isEmpty());
            SettingsHelper.updateButtonValue(view, getCustomTitleValueText());
            getParentLayout().rebuildAllFragmentViews(true, true);
        });
        builder.setNegativeButton(getString(R.string.Cancel), null);

        AlertDialog dialog = builder.create();
        dialog.setOnShowListener(d -> {
            editText.requestFocus();
            AndroidUtilities.showKeyboard(editText);
        });
        showDialog(dialog);
    }

    private String getAvatarCornersValueText() {
        int value = Math.round(FinegramAppearanceConfig.INSTANCE.getAvatarCorners());
        if (value >= Math.round(FGAvatars.MAX)) {
            return getString(R.string.FG_AvatarCorners_Circle);
        }
        if (value == 0) {
            return getString(R.string.FG_AvatarCorners_Square);
        }
        return String.valueOf(value);
    }

    private void showAvatarCornersDialog(View view) {
        AlertDialogSwitchers.showAvatarCorners(this, () -> {
            SettingsHelper.updateButtonValue(view, getAvatarCornersValueText());
            Theme.reloadAllResources(getContext() != null ? getContext() : ApplicationLoader.applicationContext);
            getParentLayout().rebuildAllFragmentViews(true, true);
        });
    }

    private String getSwitchStyleValueText() {
        return switch (FinegramAppearanceConfig.INSTANCE.getSwitchStyle()) {
            case FinegramAppearanceConfig.SWITCH_ONE_UI -> getString(R.string.FG_SwitchStyle_OneUI);
            case FinegramAppearanceConfig.SWITCH_TELEGRAM -> getString(R.string.FG_SwitchStyle_Telegram);
            default -> getString(R.string.FG_SwitchStyle_Material3);
        };
    }

    private void showSwitchStyleChooser(View view) {
        ArrayList<String> titles = new ArrayList<>();
        titles.add(getString(R.string.FG_SwitchStyle_Telegram));
        titles.add(getString(R.string.FG_SwitchStyle_OneUI));
        titles.add(getString(R.string.FG_SwitchStyle_Material3));

        PopupHelper.show(titles, getString(R.string.FG_SwitchStyle),
                FinegramAppearanceConfig.INSTANCE.getSwitchStyle(), getContext(), i -> {
            FinegramAppearanceConfig.INSTANCE.setSwitchStyle(i);
            SettingsHelper.updateButtonValue(view, getSwitchStyleValueText());
            updateRows(true);
        });
    }

    private String getIconPackValueText()  {
        IconPack active = IconPackManager.INSTANCE.active();
        if (active != null) {
            return active.getName();
        }
        return switch (FinegramAppearanceConfig.INSTANCE.getIconReplacement()) {
            case FinegramAppearanceConfig.ICON_REPLACE_SOLAR -> getString(R.string.AP_IconReplacement_Solar);
            default -> getString(R.string.Default);
        };
    }

    private void showIconPackChooser(View view) {
        ArrayList<String> titles = new ArrayList<>();
        ArrayList<IconPack> packs = new ArrayList<>(IconPackManager.INSTANCE.installed());

        titles.add(getString(R.string.Default));
        titles.add(getString(R.string.AP_IconReplacement_Solar));
        for (IconPack pack : packs) {
            titles.add(pack.getName());
        }
        titles.add(getString(R.string.FG_IconPack_Install));

        final int builtInCount = 2;
        IconPack active = IconPackManager.INSTANCE.active();
        int selected;
        if (active != null) {
            selected = packs.indexOf(active) + builtInCount;
        } else {
            selected = FinegramAppearanceConfig.INSTANCE.getIconReplacement() == FinegramAppearanceConfig.ICON_REPLACE_SOLAR ? 1 : 0;
        }

        PopupHelper.show(titles, getString(R.string.AP_IconReplacements), selected, getContext(), i -> {
            if (i == titles.size() - 1) {
                pickIconPackFile();
                return;
            }
            if (i < builtInCount) {
                IconPackManager.INSTANCE.setActive(null);
                FinegramAppearanceConfig.INSTANCE.setIconReplacement(
                        i == 1 ? FinegramAppearanceConfig.ICON_REPLACE_SOLAR : FinegramAppearanceConfig.ICON_REPLACE_NONE
                );
            } else {
                IconPackManager.INSTANCE.setActive(packs.get(i - builtInCount));
            }
            SettingsHelper.updateButtonValue(view, getIconPackValueText());
            reloadIcons();
        });
    }

    void showIconPackOptions(View view, IconPack pack) {
        AlertDialog.Builder builder = new AlertDialog.Builder(getParentActivity(), getResourceProvider());
        builder.setTitle(pack.getName());
        builder.setMessage(LocaleController.formatString(R.string.FG_IconPack_Info, pack.getAuthor(), pack.getIconCount()));
        builder.setPositiveButton(getString(R.string.Delete), (dialog, which) -> {
            IconPackManager.INSTANCE.delete(pack);
            SettingsHelper.updateButtonValue(view, getIconPackValueText());
            reloadIcons();
        });
        builder.setNegativeButton(getString(R.string.Cancel), null);
        showDialog(builder.create());
    }

    private void pickIconPackFile() {
        try {
            Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
            intent.setType("*/*");
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            startActivityForResult(Intent.createChooser(intent, getString(R.string.FG_IconPack_Install)), REQUEST_ICON_PACK);
        } catch (Throwable e) {
            FileLog.e(e);
        }
    }

    @Override
    public void onActivityResultFragment(int requestCode, int resultCode, Intent data) {
        if (requestCode != REQUEST_ICON_PACK || resultCode != Activity.RESULT_OK || data == null || data.getData() == null) {
            return;
        }

        if (!IconPackPreviewSheet.showForUri(data.getData(), this, () -> updateRows(true))) {
            BulletinFactory.of(this).createErrorBulletin(getString(R.string.FG_IconPack_Failed)).show();
        }
    }

    private void reloadIcons() {
        if (getParentActivity() instanceof LaunchActivity) {
            ((LaunchActivity) getParentActivity()).reloadResources();
        }
    }

    private static final int REQUEST_ICON_PACK = 5390;

    @Override
    protected String getKey() {
        return DeeplinkHelper.DeepLinksRepo.FG_Appearance;
    }
}

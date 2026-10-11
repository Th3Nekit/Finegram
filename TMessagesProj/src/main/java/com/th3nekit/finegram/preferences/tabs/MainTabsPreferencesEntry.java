/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences.tabs;

import com.th3nekit.finegram.core.helpers.DeeplinkHelper;

import static org.telegram.messenger.AndroidUtilities.dp;
import static org.telegram.messenger.LocaleController.getString;

import static com.th3nekit.finegram.preferences.helpers.SettingsHelper.applyNewSpan;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.core.graphics.ColorUtils;

import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;

import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig;
import com.th3nekit.finegram.core.ui.mainTabs.MainTabsManager;
import com.th3nekit.finegram.preferences.BaseCGPreferencesEntry;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class MainTabsPreferencesEntry extends BaseCGPreferencesEntry {

    private final int enableTabsRow = 1;
    private final int tabsPreviewRow = 2;
    private final int openSettingsBySwipeRow = 3;
    private final int showTabTitleRow = 4;
    private final int forceOpenChats = 5;

    private static final int TAB_ROW_BASE = 100;

    private int reorderSectionId = -1;

    private MainTabsPreviewCell tabsView;
    private ArrayList<MainTabsManager.Tab> tabs;
    private ArrayList<MainTabsManager.Tab> initialTabs;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.CP_MainTabs_Header);
    }

    @Override
    public View createView(Context context) {
        View view = super.createView(context);
        listView.listenReorder(this::onTabsReordered);
        listView.allowReorder(true);
        return view;
    }

    @Override
    public boolean onFragmentCreate() {
        initialTabs = new ArrayList<>();
        for (MainTabsManager.Tab t : MainTabsManager.INSTANCE.getAllTabs()) {
            initialTabs.add(new MainTabsManager.Tab(t.getType(), t.enabled));
        }

        tabs = new ArrayList<>(MainTabsManager.INSTANCE.getAllTabs());

        return super.onFragmentCreate();
    }

    @Override
    public void onFragmentDestroy() {
        super.onFragmentDestroy();
        checkSaveTabs();
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        if (FinegramAppearanceConfig.INSTANCE.getSideDrawer() && !FinegramAppearanceConfig.INSTANCE.getBottomTabsWithDrawer()) {
            items.add(UItem.asShadow(getString(R.string.FG_MainTabs_HiddenByDrawer)));
        }
        items.add(UItem.asHeader(getString(R.string.CP_MainTabs_Layout)));
        if (FinegramAppearanceConfig.INSTANCE.getShowMainTabs()) {
            UItem enableTabs = SettingsHelper.asSwitchCG(
                    enableTabsRow,
                    getString(R.string.CP_MainTabs_ShowTabs),
                    getString(R.string.CP_MainTabs_DoubleTap_Desc)
            ).setChecked(FinegramAppearanceConfig.INSTANCE.getShowMainTabs());
            enableTabs.hideDivider = true;
            items.add(enableTabs);
        } else {
            items.add(SettingsHelper.asSwitchCG(enableTabsRow, getString(R.string.CP_MainTabs_ShowTabs))
                    .setChecked(FinegramAppearanceConfig.INSTANCE.getShowMainTabs())
            );
            items.add(SettingsHelper.asSwitchCG(openSettingsBySwipeRow, getString(R.string.CP_MainTabs_OpenSettings), getString(R.string.CP_MainTabs_OpenSettings_Desc))
                    .setChecked(FinegramAppearanceConfig.INSTANCE.getOpenSettingsBySwipe())
            .slug("openSettings"));
        }

        PreviewCell previewContainer = new PreviewCell(getContext());

        tabsView = new MainTabsPreviewCell(getContext());
        tabsView.setEditMode(true);
        tabsView.setTabs(tabs, getContext(), getResourceProvider(), currentAccount, true, true);

        previewContainer.addView(tabsView, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, 48, Gravity.CENTER, dp(5), 0, dp(5), 0));

        if (FinegramAppearanceConfig.INSTANCE.getShowMainTabs()) {
            items.add(UItem.asShadow(null));
            items.add(UItem.asHeader(getString(R.string.AP_Header_Appearance)));
            UItem space = SettingsHelper.asSpaceCG(dp(8));
            space.id = -1;
            space.transparent = true;
            items.add(space);
            items.add(SettingsHelper.asCustomWithBackground(tabsPreviewRow, previewContainer, 58));
            items.add(SettingsHelper.asSwitchCG(showTabTitleRow, getString(R.string.CP_MainTabs_ShowTabsTitle))
                    .setChecked(FinegramAppearanceConfig.INSTANCE.getShowMainTabsTitle())
            .slug("showTitles"));
            items.add(UItem.asShadow(null));

            items.add(UItem.asHeader(getString(R.string.CP_MainTabs_Order)));
            reorderSectionId = adapter.reorderSectionStart();
            for (MainTabsManager.Tab tab : tabs) {
                if (tab.getType() == MainTabsManager.TabType.SEARCH) {

                    continue;
                }
                items.add(SettingsHelper.asSwitchCG(TAB_ROW_BASE + tab.getType().ordinal(),
                                tabTitle(tab.getType()))
                        .setChecked(tab.enabled)
                );
            }
            adapter.reorderSectionEnd();
            items.add(UItem.asShadow(getString(R.string.CP_MainTabs_Layout_Desc)));

            items.add(UItem.asHeader(getString(R.string.ActionsChartTitle)));
            items.add(SettingsHelper.asSwitchCG(forceOpenChats, applyNewSpan(getString(R.string.CP_MainTabs_ForceOpenChats)), getString(R.string.CP_MainTabs_ForceOpenChats_Desc))
                    .setChecked(FinegramAppearanceConfig.INSTANCE.getMainTabsForceOpenChats())
            .slug("search"));
        }
    }

    private static class PreviewCell extends FrameLayout {
        private final Paint backgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint outlinePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF rect = new RectF();

        public PreviewCell(Context context) {
            super(context);
            setWillNotDraw(false);

            int color = Theme.getColor(Theme.key_switchTrack);
            backgroundPaint.setColor(ColorUtils.setAlphaComponent(color, 20));

            outlinePaint.setStyle(Paint.Style.STROKE);
            outlinePaint.setStrokeWidth(Math.max(2, dp(1f)));
            outlinePaint.setColor(ColorUtils.setAlphaComponent(color, 0x3F));
        }

        @Override
        protected void onDraw(Canvas canvas) {
            float w = getMeasuredWidth();
            float h = getMeasuredHeight();

            float radius = dp(50);

            float stroke = outlinePaint.getStrokeWidth() / 2;
            rect.set(stroke + dp(8), stroke, w - stroke - dp(8), h - stroke);

            canvas.drawRoundRect(rect, radius, radius, backgroundPaint);
            canvas.drawRoundRect(rect, radius, radius, outlinePaint);
        }
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == enableTabsRow) {
            FinegramAppearanceConfig.INSTANCE.setShowMainTabs(!FinegramAppearanceConfig.INSTANCE.getShowMainTabs());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getShowMainTabs());

            if (!FinegramAppearanceConfig.INSTANCE.getShowMainTabs()) {
                resetMainTabsOrder();
            }
            updateRows(true);

            if (FinegramAppearanceConfig.INSTANCE.getFoldersAtBottom()) showRestartBulletin();
        } else if (item.id == openSettingsBySwipeRow) {
            FinegramAppearanceConfig.INSTANCE.setOpenSettingsBySwipe(!FinegramAppearanceConfig.INSTANCE.getOpenSettingsBySwipe());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getOpenSettingsBySwipe());

            resetMainTabsOrder();
        } else if (item.id == showTabTitleRow) {
            FinegramAppearanceConfig.INSTANCE.setShowMainTabsTitle(!FinegramAppearanceConfig.INSTANCE.getShowMainTabsTitle());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getShowMainTabsTitle());

            tabsView.removeAllViews();
            tabsView.setTabs(tabs, getContext(), getResourceProvider(), currentAccount, true, true);

            postUpdateTabsNotification();
        } else if (item.id >= TAB_ROW_BASE
                && item.id < TAB_ROW_BASE + MainTabsManager.TabType.values().length) {
            toggleTab(tabOfRow(item.id), view);
        } else if (item.id == forceOpenChats) {
            FinegramAppearanceConfig.INSTANCE.setMainTabsForceOpenChats(!FinegramAppearanceConfig.INSTANCE.getMainTabsForceOpenChats());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getMainTabsForceOpenChats());
        }
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        return false;
    }

    @Override
    public boolean onBackPressed(boolean invoked) {
        checkSaveTabs();
        return super.onBackPressed(invoked);
    }

    private void checkSaveTabs() {
        MainTabsManager.INSTANCE.saveTabs(tabs);

        if (!tabs.equals(initialTabs)) {
            postUpdateTabsNotification();
        }
    }

    private void resetMainTabsOrder() {
        FinegramAppearanceConfig.INSTANCE.setMainTabsOrder(
                FinegramAppearanceConfig.MAIN_TABS_ORDER_DEFAULT);
        tabs.clear();
        tabs.addAll(MainTabsManager.INSTANCE.getAllTabs());
        postUpdateTabsNotification();
    }

    private void toggleTab(MainTabsManager.Tab tab, View view) {
        if (tab == null) {
            return;
        }
        if (tab.getType() == MainTabsManager.TabType.CHATS && tab.enabled) {
            BulletinFactory.of(this).createErrorBulletin(
                    getString(R.string.CP_MainTabs_KeepChats)).show();
            return;
        }
        tab.enabled = !tab.enabled;
        SettingsHelper.updateCheckState(view, tab.enabled);
        MainTabsManager.INSTANCE.saveTabs(tabs);
        refreshPreview();
        postUpdateTabsNotification();
    }

    private void onTabsReordered(int sectionId, ArrayList<UItem> reordered) {
        if (sectionId != reorderSectionId) {
            return;
        }
        final ArrayList<MainTabsManager.Tab> ordered = new ArrayList<>();
        for (UItem item : reordered) {
            final MainTabsManager.Tab tab = tabOfRow(item.id);
            if (tab != null && !ordered.contains(tab)) {
                ordered.add(tab);
            }
        }
        if (ordered.isEmpty()) {
            return;
        }

        for (MainTabsManager.Tab tab : tabs) {
            if (!ordered.contains(tab)) {
                ordered.add(tab);
            }
        }
        tabs.clear();
        tabs.addAll(ordered);
        MainTabsManager.INSTANCE.saveTabs(tabs);
        refreshPreview();
        postUpdateTabsNotification();
    }

    private MainTabsManager.Tab tabOfRow(int rowId) {
        final int ordinal = rowId - TAB_ROW_BASE;
        final MainTabsManager.TabType[] types = MainTabsManager.TabType.values();
        if (ordinal < 0 || ordinal >= types.length) {
            return null;
        }
        for (MainTabsManager.Tab tab : tabs) {
            if (tab.getType() == types[ordinal]) {
                return tab;
            }
        }
        return null;
    }

    private void refreshPreview() {
        if (tabsView == null) {
            return;
        }
        tabsView.setTabs(tabs, getContext(), getResourceProvider(), currentAccount, true, true);
    }

    private CharSequence tabTitle(MainTabsManager.TabType type) {
        switch (type) {
            case CHATS: return getString(R.string.MainTabsChats);
            case CONTACTS: return getString(R.string.MainTabsContacts);
            case CALLS: return getString(R.string.Calls);
            case SETTINGS: return getString(R.string.Settings);
            case PROFILE: return getString(R.string.MainTabsProfile);
            case SEARCH: return getString(R.string.Search);
            default: return "";
        }
    }

    private void postUpdateTabsNotification() {
        new Handler(Looper.getMainLooper()).postDelayed(() ->
                NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.cgTabsUpdated),
                200
        );
        getParentLayout().rebuildAllFragmentViews(false, false);
    }

    @Override
    protected String getKey() {
        return DeeplinkHelper.DeepLinksRepo.FG_Tabs;
    }
}

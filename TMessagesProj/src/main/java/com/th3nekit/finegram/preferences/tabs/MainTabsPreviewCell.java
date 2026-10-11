/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.preferences.tabs;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;

import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.glass.GlassTabView;

import java.util.List;

import com.th3nekit.finegram.core.ui.mainTabs.MainTabsManager;

public class MainTabsPreviewCell extends LinearLayout {

    private boolean editMode;

    public MainTabsPreviewCell(Context context) {
        super(context);
        setOrientation(HORIZONTAL);
        setGravity(android.view.Gravity.CENTER_VERTICAL);
    }

    public void setEditMode(boolean editMode) {
        this.editMode = editMode;
    }

    public void setTabs(
            List<MainTabsManager.Tab> tabs,
            Context context,
            Theme.ResourcesProvider resourceProvider,
            int currentAccount,
            boolean fromSettings,
            boolean showSearch
    ) {
        removeAllViews();
        if (tabs == null) {
            return;
        }
        for (MainTabsManager.Tab tab : tabs) {
            if (!tab.enabled) {
                continue;
            }
            if (tab.getType() == MainTabsManager.TabType.SEARCH && !showSearch) {
                continue;
            }
            final GlassTabView view = MainTabsManager.INSTANCE.createTabView(
                    context, resourceProvider, currentAccount, tab.getType(), fromSettings, showSearch);
            if (view.getVisibility() == View.GONE) {
                continue;
            }
            view.setClickable(!editMode);
            view.setFocusable(!editMode);
            addView(view, LayoutHelper.createLinear(0, LayoutHelper.MATCH_PARENT, 1f));
        }
    }
}

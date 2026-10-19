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

/**
 * Полоса вкладок в настройках: то же, что человек увидит внизу экрана.
 *
 * Вкладки строятся теми же видами, что и настоящая панель, поэтому предпросмотр не может
 * разойтись с ней ни значками, ни подписями. Ширина делится поровну: в настройках нет
 * выбранной вкладки, а значит нет и причин выделять одну из них размером.
 */
public class MainTabsPreviewCell extends LinearLayout {

    private boolean editMode;

    public MainTabsPreviewCell(Context context) {
        super(context);
        setOrientation(HORIZONTAL);
        setGravity(android.view.Gravity.CENTER_VERTICAL);
    }

    /** В настройках вкладки ничего не открывают: нажатие тут ни при чём. */
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

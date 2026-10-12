/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.preferences;

import static org.telegram.messenger.LocaleController.getString;

import android.content.Context;
import android.view.View;

import org.telegram.messenger.R;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;

import com.th3nekit.finegram.drawer.FGDrawerItems;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class DrawerItemsPreferencesEntry extends BaseCGPreferencesEntry {

    private static final int RESET_ROW = 1;

    private static final int ITEM_ROW_BASE = 100;

    private final ArrayList<Integer> order = new ArrayList<>();
    private int reorderSectionId = -1;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.FG_DrawerItems);
    }

    @Override
    public boolean onFragmentCreate() {
        order.addAll(FGDrawerItems.order());
        return super.onFragmentCreate();
    }

    @Override
    public View createView(Context context) {
        View view = super.createView(context);
        listView.listenReorder(this::onReordered);
        listView.allowReorder(true);
        return view;
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        items.add(UItem.asHeader(getString(R.string.FG_DrawerItems)));
        reorderSectionId = adapter.reorderSectionStart();
        for (int item : order) {
            items.add(SettingsHelper.asSwitchCG(ITEM_ROW_BASE + item, getString(FGDrawerItems.titleOf(item)))
                    .setChecked(!FGDrawerItems.isHidden(item)));
        }
        adapter.reorderSectionEnd();
        items.add(UItem.asShadow(getString(R.string.FG_DrawerItems_Desc)));
        items.add(UItem.asButton(RESET_ROW, getString(R.string.FG_DrawerItems_Reset)).red());
        items.add(UItem.asShadow(null));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == RESET_ROW) {
            FGDrawerItems.reset();
            order.clear();
            order.addAll(FGDrawerItems.order());
            updateRows(true);
            return;
        }
        final int drawerItem = item.id - ITEM_ROW_BASE;
        if (FGDrawerItems.titleOf(drawerItem) == 0) {
            return;
        }
        final boolean hidden = !FGDrawerItems.isHidden(drawerItem);

        if (hidden && drawerItem == FGDrawerItems.ITEM_SETTINGS) {
            BulletinFactory.of(this).createErrorBulletin(getString(R.string.FG_DrawerItems_KeepSettings)).show();
            return;
        }
        FGDrawerItems.setHidden(drawerItem, hidden);
        SettingsHelper.updateCheckState(view, !hidden);
    }

    private void onReordered(int sectionId, ArrayList<UItem> reordered) {
        if (sectionId != reorderSectionId) {
            return;
        }
        final ArrayList<Integer> next = new ArrayList<>();
        for (UItem row : reordered) {
            final int drawerItem = row.id - ITEM_ROW_BASE;
            if (FGDrawerItems.titleOf(drawerItem) != 0 && !next.contains(drawerItem)) {
                next.add(drawerItem);
            }
        }
        for (int item : order) {
            if (!next.contains(item)) {
                next.add(item);
            }
        }
        order.clear();
        order.addAll(next);
        FGDrawerItems.saveOrder(order);
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        return false;
    }
}

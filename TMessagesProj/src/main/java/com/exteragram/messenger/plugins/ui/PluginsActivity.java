/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.ui;

import android.content.Context;
import android.view.View;

import com.th3nekit.finegram.preferences.PluginsPreferencesEntry;

import org.telegram.messenger.NotificationCenter;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;

public class PluginsActivity extends PluginsPreferencesEntry implements NotificationCenter.NotificationCenterDelegate {

    public PluginsActivity() {
        super();
    }

    @Override
    public View createView(Context context) {
        return super.createView(context);
    }

    @Override
    public void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        super.fillItems(items, adapter);
    }

    @Override
    public void onClick(UItem item, View view, int position, float x, float y) {
        super.onClick(item, view, position, x, y);
    }

    @Override
    public boolean onFragmentCreate() {
        NotificationCenter.getGlobalInstance().addObserver(this, NotificationCenter.pluginsUpdated);
        return super.onFragmentCreate();
    }

    @Override
    public void onFragmentDestroy() {
        NotificationCenter.getGlobalInstance().removeObserver(this, NotificationCenter.pluginsUpdated);
        super.onFragmentDestroy();
    }

    @Override
    public boolean onBackPressed(boolean invoked) {
        return super.onBackPressed(invoked);
    }

    @Override
    public void didReceivedNotification(int id, int account, Object... args) {
        if (id == NotificationCenter.pluginsUpdated) {
            updateRows(true);
        }
    }
}

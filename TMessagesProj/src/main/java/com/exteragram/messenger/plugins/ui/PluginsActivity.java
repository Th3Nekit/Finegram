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

/**
 * Экран плагинов под тем именем, по которому его находят плагины.
 *
 * Плагины-менеджеры вешают перехваты на построение этого экрана — дописывают свои кнопки,
 * строки и действия. Перехват встаёт только на метод, объявленный в самом классе, поэтому
 * здесь объявлены те, что они перехватывают, и клиент открывает экран плагинов именно
 * этим классом. Вся работа — в {@link PluginsPreferencesEntry}.
 */
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

    /** Список плагинов поменялся где-то ещё — перестраиваем строки. */
    @Override
    public void didReceivedNotification(int id, int account, Object... args) {
        if (id == NotificationCenter.pluginsUpdated) {
            updateRows(true);
        }
    }
}

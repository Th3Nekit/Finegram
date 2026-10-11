/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.ui;

import android.content.Context;
import android.view.View;

import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;

import com.th3nekit.finegram.plugins.FGDexPlugins;
import com.th3nekit.finegram.plugins.FGPluginsController;
import com.th3nekit.finegram.preferences.PluginDetailsPreferencesEntry;

public class PluginSettingsActivity extends PluginDetailsPreferencesEntry {

    public PluginSettingsActivity(Object plugin) {
        super(idOf(plugin));
    }

    @Override
    public void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        super.fillItems(items, adapter);
    }

    @Override
    public View createView(Context context) {
        return super.createView(context);
    }

    private static String idOf(Object plugin) {
        if (plugin instanceof FGPluginsController.Plugin) {
            return ((FGPluginsController.Plugin) plugin).getId();
        }
        if (plugin instanceof FGDexPlugins.Plugin) {
            return ((FGDexPlugins.Plugin) plugin).getId();
        }
        if (plugin == null) {
            return "";
        }

        try {
            return String.valueOf(plugin.getClass().getMethod("getId").invoke(plugin));
        } catch (Throwable ignored) {
        }
        return String.valueOf(plugin);
    }
}

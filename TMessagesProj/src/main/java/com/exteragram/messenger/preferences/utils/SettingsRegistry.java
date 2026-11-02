/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.preferences.utils;

import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.Components.UItem;

import java.util.ArrayList;
import java.util.List;

public class SettingsRegistry {

    private static final SettingsRegistry INSTANCE = new SettingsRegistry();

    public static List<String> newFeatures = new ArrayList<>();

    public static SettingsRegistry getInstance() {
        return INSTANCE;
    }

    public SettingsRegistry() {
    }

    public void addSearchEntry(BaseFragment fragment, UItem item) {
    }

    public void addLinkAliasForOption(String alias, BaseFragment fragment, UItem item) {
    }

    public String getFirstSettingLink(Class<?> fragmentClass, UItem item) {
        return null;
    }

    public void handleLink(String section, String option) {
    }

    public void onSettingNotFound() {
    }

    public void onSettingNotFound(BaseFragment fragment) {
    }

    public static boolean isValidForSearch(UItem item) {
        return item != null && item.text != null;
    }

    public static boolean isValidForLinkAliases(UItem item) {
        return false;
    }

    public static boolean markAsNewFeature(String key) {
        if (key == null || newFeatures.contains(key)) {
            return false;
        }
        newFeatures.add(key);
        return true;
    }
}

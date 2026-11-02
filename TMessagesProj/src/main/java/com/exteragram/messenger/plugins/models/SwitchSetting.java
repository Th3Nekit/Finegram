/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.models;

public class SwitchSetting extends SettingItem {

    public String text;
    public boolean defaultValue;

    public SwitchSetting(String key, String text, String subtext, boolean defaultValue) {
        super("switch");
        this.key = key;
        this.text = text;
        this.subtext = subtext;
        this.defaultValue = defaultValue;
    }

    public SwitchSetting(String key, String text, boolean defaultValue) {
        this(key, text, null, defaultValue);
    }
}

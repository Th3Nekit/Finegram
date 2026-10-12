/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.models;

public class InputSetting extends SettingItem {

    public String text;
    public String defaultValue;

    public InputSetting(String key, String text, String defaultValue, String subtext) {
        super("input");
        this.key = key;
        this.text = text;
        this.defaultValue = defaultValue;
        this.subtext = subtext;
    }
}

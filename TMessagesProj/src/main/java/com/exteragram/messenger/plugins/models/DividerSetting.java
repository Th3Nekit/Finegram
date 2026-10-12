/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.models;

public class DividerSetting extends SettingItem {

    public String text;

    public DividerSetting(String text) {
        super("divider");
        this.text = text;
    }

    public DividerSetting() {
        this(null);
    }
}

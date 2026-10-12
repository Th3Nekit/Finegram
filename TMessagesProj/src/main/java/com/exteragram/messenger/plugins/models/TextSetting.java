/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.models;

public class TextSetting extends SettingItem {

    public String text;
    public String icon;
    public boolean accent;
    public boolean red;

    public TextSetting(String text, String subtext, String icon) {
        super("text");
        this.text = text;
        this.subtext = subtext;
        this.icon = icon;
    }

    public TextSetting(String text) {
        this(text, null, null);
    }
}

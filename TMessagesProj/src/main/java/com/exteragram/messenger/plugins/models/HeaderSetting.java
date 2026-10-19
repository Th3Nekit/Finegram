/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.models;

/** Заголовок раздела. */
public class HeaderSetting extends SettingItem {

    public String text;

    public HeaderSetting(String text) {
        super("header");
        this.text = text;
    }

    public String getText() {
        return text;
    }

    public void setText(String text) {
        this.text = text;
    }
}

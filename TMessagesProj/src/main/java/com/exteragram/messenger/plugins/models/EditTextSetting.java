/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.models;

public class EditTextSetting extends SettingItem {

    public String hint;
    public String defaultValue;
    public boolean multiline;
    public int maxLength;
    public String mask;

    public EditTextSetting(String key, String hint, String defaultValue,
                           boolean multiline, int maxLength, String mask) {
        super("edit_text");
        this.key = key;
        this.hint = hint;
        this.defaultValue = defaultValue;
        this.multiline = multiline;
        this.maxLength = maxLength;
        this.mask = mask;
    }
}

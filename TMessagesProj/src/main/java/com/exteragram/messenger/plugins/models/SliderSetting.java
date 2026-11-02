/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.models;

public class SliderSetting extends SettingItem {

    public String text;
    public int defaultValue;
    public int min;
    public int max;

    public SliderSetting(String key, String text, String subtext, int defaultValue, int min, int max) {
        super("slider");
        this.key = key;
        this.text = text;
        this.subtext = subtext;
        this.defaultValue = defaultValue;
        this.min = min;
        this.max = max;
    }
}

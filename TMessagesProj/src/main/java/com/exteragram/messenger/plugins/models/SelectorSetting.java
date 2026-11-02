/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.models;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class SelectorSetting extends SettingItem {

    public String text;
    public List<String> items = new ArrayList<>();
    public int defaultValue;

    public SelectorSetting(String key, String text, List<String> items, int defaultValue) {
        super("selector");
        this.key = key;
        this.text = text;
        if (items != null) this.items = items;
        this.defaultValue = defaultValue;
    }

    public SelectorSetting(String key, String text, String[] items, int defaultValue) {
        this(key, text, items == null ? null : Arrays.asList(items), defaultValue);
    }
}

/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.models;

public class SettingItem {

    public String type;

    public String subtext;

    public String key;

    public String linkAlias;

    public SettingItem(String type) {
        this.type = type;
    }

    public SettingItem(String type, String subtext, Object onLongClick, String linkAlias) {
        this.type = type;
        this.subtext = subtext;
        this.linkAlias = linkAlias;
    }

    public String getType() {
        return type;
    }

    public void clearPythonReferences() {
    }
}

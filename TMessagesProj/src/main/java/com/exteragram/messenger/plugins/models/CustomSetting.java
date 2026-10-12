/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.models;

import android.view.View;

import org.telegram.ui.Components.UItem;

import com.exteragram.messenger.plugins.Plugin;

public class CustomSetting {

    public Object args;

    public CustomSetting() {
    }

    public CustomSetting(Object args) {
        this.args = args;
    }

    public Object getArgs() {
        return args;
    }

    public static abstract class Factory extends UItem.UItemFactory<View> {

        public abstract UItem create(Plugin plugin, CustomSetting setting, Object args);
    }
}

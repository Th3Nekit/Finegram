/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.components;

import android.content.Context;
import android.widget.LinearLayout;

import org.telegram.ui.ActionBar.Theme;

public class ActionRow extends LinearLayout {

    private final Theme.ResourcesProvider resourcesProvider;

    public ActionRow(Context context) {
        this(context, null);
    }

    public ActionRow(Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;
        setOrientation(HORIZONTAL);
    }

    public Theme.ResourcesProvider getResourcesProvider() {
        return resourcesProvider;
    }
}

/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.profile.pills;

import android.content.Context;
import android.widget.FrameLayout;

import org.telegram.ui.ActionBar.Theme;

public class FGPill extends FrameLayout {

    protected final Theme.ResourcesProvider resourcesProvider;

    private int pillId;

    public FGPill(Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;
        setWillNotDraw(false);
    }

    public Theme.ResourcesProvider getResourcesProvider() {
        return resourcesProvider;
    }

    public int getPillId() {
        return pillId;
    }

    public void setPillId(int pillId) {
        this.pillId = pillId;
    }

    public void onShown() {
    }

    public void onHidden() {
    }

    protected int getThemedColor(int key) {
        return Theme.getColor(key, resourcesProvider);
    }
}

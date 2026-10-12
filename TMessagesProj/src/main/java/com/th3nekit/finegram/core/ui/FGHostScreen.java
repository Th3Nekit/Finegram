/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.ui;

import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.ActionBar.INavigationLayout;
import org.telegram.ui.LaunchActivity;

public final class FGHostScreen {

    private FGHostScreen() {
    }

    public static BaseFragment find() {
        BaseFragment top = null;
        try {

            top = LaunchActivity.getLastFragment();
        } catch (Throwable ignored) {
        }
        if (alive(top)) return top;

        final LaunchActivity activity = LaunchActivity.instance;
        if (activity == null) return null;

        try {
            for (int i = activity.sheetFragmentsStack.size() - 1; i >= 0; i--) {
                final INavigationLayout layer = activity.sheetFragmentsStack.get(i);
                final BaseFragment candidate = layer == null ? null : layer.getLastFragment();
                if (alive(candidate)) return candidate;
            }
        } catch (Throwable ignored) {
        }

        try {
            final INavigationLayout main = activity.getActionBarLayout();
            final BaseFragment candidate = main == null ? null : main.getLastFragment();
            if (alive(candidate)) return candidate;
        } catch (Throwable ignored) {
        }
        return null;
    }

    private static boolean alive(BaseFragment fragment) {
        return fragment != null && fragment.getParentActivity() != null;
    }
}

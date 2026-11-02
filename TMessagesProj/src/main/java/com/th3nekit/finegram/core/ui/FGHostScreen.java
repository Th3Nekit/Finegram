/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.ui;

import android.app.Activity;

import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.ActionBar.INavigationLayout;
import org.telegram.ui.LaunchActivity;

public final class FGHostScreen {

    private FGHostScreen() {
    }

    public static BaseFragment find(Activity source) {
        if (source instanceof LaunchActivity) {
            final BaseFragment candidate = lastAlive((LaunchActivity) source);
            if (candidate != null) return candidate;
        }
        return find();
    }

    public static BaseFragment find() {
        BaseFragment top = null;
        try {
            top = LaunchActivity.getLastFragment();
        } catch (Throwable ignored) {
        }
        if (alive(top)) return top;

        final LaunchActivity activity = LaunchActivity.instance;
        return activity == null ? null : lastAlive(activity);
    }

    private static BaseFragment lastAlive(LaunchActivity activity) {
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
        return fragment != null && fragment.getParentActivity() != null && !fragment.getParentActivity().isFinishing();
    }
}

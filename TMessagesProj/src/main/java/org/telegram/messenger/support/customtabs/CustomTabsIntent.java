/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.telegram.messenger.support.customtabs;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import androidx.annotation.AnimRes;
import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.app.ActivityOptionsCompat;
import androidx.core.app.BundleCompat;
import androidx.core.content.ContextCompat;
import android.view.View;
import android.widget.RemoteViews;

import java.util.ArrayList;

public final class CustomTabsIntent {

    private static final String EXTRA_USER_OPT_OUT_FROM_CUSTOM_TABS =
            "android.support.customtabs.extra.user_opt_out";

    public static final String EXTRA_SESSION = "android.support.customtabs.extra.SESSION";

    public static final String EXTRA_TOOLBAR_COLOR =
            "android.support.customtabs.extra.TOOLBAR_COLOR";

    public static final String EXTRA_ENABLE_URLBAR_HIDING =
            "android.support.customtabs.extra.ENABLE_URLBAR_HIDING";

    public static final String EXTRA_CLOSE_BUTTON_ICON =
            "android.support.customtabs.extra.CLOSE_BUTTON_ICON";

    public static final String EXTRA_TITLE_VISIBILITY_STATE =
            "android.support.customtabs.extra.TITLE_VISIBILITY";

    public static final int NO_TITLE = 0;

    public static final int SHOW_PAGE_TITLE = 1;

    public static final String EXTRA_ACTION_BUTTON_BUNDLE =
            "android.support.customtabs.extra.ACTION_BUTTON_BUNDLE";

    public static final String EXTRA_TOOLBAR_ITEMS =
            "android.support.customtabs.extra.TOOLBAR_ITEMS";

    public static final String EXTRA_SECONDARY_TOOLBAR_COLOR =
            "android.support.customtabs.extra.SECONDARY_TOOLBAR_COLOR";

    public static final String KEY_ICON = "android.support.customtabs.customaction.ICON";

    public static final String KEY_DESCRIPTION =
            "android.support.customtabs.customaction.DESCRIPTION";

    public static final String KEY_PENDING_INTENT =
            "android.support.customtabs.customaction.PENDING_INTENT";

    public static final String EXTRA_TINT_ACTION_BUTTON =
            "android.support.customtabs.extra.TINT_ACTION_BUTTON";

    public static final String EXTRA_MENU_ITEMS = "android.support.customtabs.extra.MENU_ITEMS";

    public static final String KEY_MENU_ITEM_TITLE =
            "android.support.customtabs.customaction.MENU_ITEM_TITLE";

    public static final String EXTRA_EXIT_ANIMATION_BUNDLE =
            "android.support.customtabs.extra.EXIT_ANIMATION_BUNDLE";

    public static final String EXTRA_DEFAULT_SHARE_MENU_ITEM =
            "android.support.customtabs.extra.SHARE_MENU_ITEM";

    public static final String EXTRA_REMOTEVIEWS =
            "android.support.customtabs.extra.EXTRA_REMOTEVIEWS";

    public static final String EXTRA_REMOTEVIEWS_VIEW_IDS =
            "android.support.customtabs.extra.EXTRA_REMOTEVIEWS_VIEW_IDS";

    public static final String EXTRA_REMOTEVIEWS_PENDINGINTENT =
            "android.support.customtabs.extra.EXTRA_REMOTEVIEWS_PENDINGINTENT";

    public static final String EXTRA_REMOTEVIEWS_CLICKED_ID =
            "android.support.customtabs.extra.EXTRA_REMOTEVIEWS_CLICKED_ID";

    public static final String EXTRA_ENABLE_INSTANT_APPS =
            "android.support.customtabs.extra.EXTRA_ENABLE_INSTANT_APPS";

    public static final String KEY_ID = "android.support.customtabs.customaction.ID";

    public static final int TOOLBAR_ACTION_BUTTON_ID = 0;

    private static final int MAX_TOOLBAR_ITEMS = 5;

    @NonNull public final Intent intent;

    @Nullable public final Bundle startAnimationBundle;

    public void launchUrl(Context context, Uri url) {
        intent.setData(url);
        ContextCompat.startActivity(context, intent, startAnimationBundle);
    }

    private CustomTabsIntent(Intent intent, Bundle startAnimationBundle) {
        this.intent = intent;
        this.startAnimationBundle = startAnimationBundle;
    }

    public static final class Builder {
        private final Intent mIntent = new Intent(Intent.ACTION_VIEW);
        private ArrayList<Bundle> mMenuItems = null;
        private Bundle mStartAnimationBundle = null;
        private ArrayList<Bundle> mActionButtons = null;
        private boolean mInstantAppsEnabled = true;

        public Builder() {
            this(null);
        }

        public Builder(@Nullable CustomTabsSession session) {
            if (session != null) mIntent.setPackage(session.getComponentName().getPackageName());
            Bundle bundle = new Bundle();
            BundleCompat.putBinder(
                    bundle, EXTRA_SESSION, session == null ? null : session.getBinder());
            mIntent.putExtras(bundle);
        }

        public Builder setToolbarColor(@ColorInt int color) {
            mIntent.putExtra(EXTRA_TOOLBAR_COLOR, color);
            return this;
        }

        public Builder enableUrlBarHiding() {
            mIntent.putExtra(EXTRA_ENABLE_URLBAR_HIDING, true);
            return this;
        }

        public Builder setCloseButtonIcon(@NonNull Bitmap icon) {
            mIntent.putExtra(EXTRA_CLOSE_BUTTON_ICON, icon);
            return this;
        }

        public Builder setShowTitle(boolean showTitle) {
            mIntent.putExtra(EXTRA_TITLE_VISIBILITY_STATE,
                    showTitle ? SHOW_PAGE_TITLE : NO_TITLE);
            return this;
        }

        public Builder addMenuItem(@NonNull String label, @NonNull PendingIntent pendingIntent) {
            if (mMenuItems == null) mMenuItems = new ArrayList<>();
            Bundle bundle = new Bundle();
            bundle.putString(KEY_MENU_ITEM_TITLE, label);
            bundle.putParcelable(KEY_PENDING_INTENT, pendingIntent);
            mMenuItems.add(bundle);
            return this;
        }

        public Builder addDefaultShareMenuItem() {
            mIntent.putExtra(EXTRA_DEFAULT_SHARE_MENU_ITEM, true);
            return this;
        }

        public Builder setActionButton(@NonNull Bitmap icon, @NonNull String description,
                                       @NonNull PendingIntent pendingIntent, boolean shouldTint) {
            Bundle bundle = new Bundle();
            bundle.putInt(KEY_ID, TOOLBAR_ACTION_BUTTON_ID);
            bundle.putParcelable(KEY_ICON, icon);
            bundle.putString(KEY_DESCRIPTION, description);
            bundle.putParcelable(KEY_PENDING_INTENT, pendingIntent);
            mIntent.putExtra(EXTRA_ACTION_BUTTON_BUNDLE, bundle);
            mIntent.putExtra(EXTRA_TINT_ACTION_BUTTON, shouldTint);
            return this;
        }

        public Builder setActionButton(@NonNull Bitmap icon, @NonNull String description,
                                       @NonNull PendingIntent pendingIntent) {
            return setActionButton(icon, description, pendingIntent, false);
        }

        @Deprecated
        public Builder addToolbarItem(int id, @NonNull Bitmap icon, @NonNull String description,
                                      PendingIntent pendingIntent) throws IllegalStateException {
            if (mActionButtons == null) {
                mActionButtons = new ArrayList<>();
            }
            if (mActionButtons.size() >= MAX_TOOLBAR_ITEMS) {
                throw new IllegalStateException(
                        "Exceeded maximum toolbar item count of " + MAX_TOOLBAR_ITEMS);
            }
            Bundle bundle = new Bundle();
            bundle.putInt(KEY_ID, id);
            bundle.putParcelable(KEY_ICON, icon);
            bundle.putString(KEY_DESCRIPTION, description);
            bundle.putParcelable(KEY_PENDING_INTENT, pendingIntent);
            mActionButtons.add(bundle);
            return this;
        }

        public Builder setSecondaryToolbarColor(@ColorInt int color) {
            mIntent.putExtra(EXTRA_SECONDARY_TOOLBAR_COLOR, color);
            return this;
        }

        public Builder setSecondaryToolbarViews(@NonNull RemoteViews remoteViews,
                                                @Nullable int[] clickableIDs, @Nullable PendingIntent pendingIntent) {
            mIntent.putExtra(EXTRA_REMOTEVIEWS, remoteViews);
            mIntent.putExtra(EXTRA_REMOTEVIEWS_VIEW_IDS, clickableIDs);
            mIntent.putExtra(EXTRA_REMOTEVIEWS_PENDINGINTENT, pendingIntent);
            return this;
        }

        public Builder setInstantAppsEnabled(boolean enabled) {
            mInstantAppsEnabled = enabled;
            return this;
        }

        public Builder setStartAnimations(
                @NonNull Context context, @AnimRes int enterResId, @AnimRes int exitResId) {
            mStartAnimationBundle = ActivityOptionsCompat.makeCustomAnimation(
                    context, enterResId, exitResId).toBundle();
            return this;
        }

        public Builder setExitAnimations(
                @NonNull Context context, @AnimRes int enterResId, @AnimRes int exitResId) {
            Bundle bundle = ActivityOptionsCompat.makeCustomAnimation(
                    context, enterResId, exitResId).toBundle();
            mIntent.putExtra(EXTRA_EXIT_ANIMATION_BUNDLE, bundle);
            return this;
        }

        public CustomTabsIntent build() {
            if (mMenuItems != null) {
                mIntent.putParcelableArrayListExtra(CustomTabsIntent.EXTRA_MENU_ITEMS, mMenuItems);
            }
            if (mActionButtons != null) {
                mIntent.putParcelableArrayListExtra(EXTRA_TOOLBAR_ITEMS, mActionButtons);
            }
            mIntent.putExtra(EXTRA_ENABLE_INSTANT_APPS, mInstantAppsEnabled);
            return new CustomTabsIntent(mIntent, mStartAnimationBundle);
        }
    }

    public static int getMaxToolbarItems() {
        return MAX_TOOLBAR_ITEMS;
    }

    public static Intent setAlwaysUseBrowserUI(Intent intent) {
        if (intent == null) intent = new Intent(Intent.ACTION_VIEW);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(EXTRA_USER_OPT_OUT_FROM_CUSTOM_TABS, true);
        return intent;
    }

    public void setUseNewTask() {
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    }

    public static boolean shouldAlwaysUseBrowserUI(Intent intent) {
        return intent.getBooleanExtra(EXTRA_USER_OPT_OUT_FROM_CUSTOM_TABS, false)
                && (intent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0;
    }
}

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

import android.app.PendingIntent;
import android.content.ComponentName;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import android.view.View;
import android.widget.RemoteViews;

import java.util.List;

public final class CustomTabsSession {
    private static final String TAG = "CustomTabsSession";
    private final Object mLock = new Object();
    private final ICustomTabsService mService;
    private final ICustomTabsCallback mCallback;
    private final ComponentName mComponentName;

    public static CustomTabsSession createDummySessionForTesting(ComponentName componentName) {
        return new CustomTabsSession(null, new CustomTabsSessionToken.DummyCallback(), componentName);
    }

                  CustomTabsSession(
            ICustomTabsService service, ICustomTabsCallback callback, ComponentName componentName) {
        mService = service;
        mCallback = callback;
        mComponentName = componentName;
    }

    public boolean mayLaunchUrl(Uri url, Bundle extras, List<Bundle> otherLikelyBundles) {
        try {
            return mService.mayLaunchUrl(mCallback, url, extras, otherLikelyBundles);
        } catch (RemoteException e) {
            return false;
        }
    }

    public boolean setActionButton(@NonNull Bitmap icon, @NonNull String description) {
        Bundle bundle = new Bundle();
        bundle.putParcelable(CustomTabsIntent.KEY_ICON, icon);
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, description);

        Bundle metaBundle = new Bundle();
        metaBundle.putBundle(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE, bundle);
        try {
            return mService.updateVisuals(mCallback, metaBundle);
        } catch (RemoteException e) {
            return false;
        }
    }

    public boolean setSecondaryToolbarViews(@Nullable RemoteViews remoteViews,
                                            @Nullable int[] clickableIDs, @Nullable PendingIntent pendingIntent) {
        Bundle bundle = new Bundle();
        bundle.putParcelable(CustomTabsIntent.EXTRA_REMOTEVIEWS, remoteViews);
        bundle.putIntArray(CustomTabsIntent.EXTRA_REMOTEVIEWS_VIEW_IDS, clickableIDs);
        bundle.putParcelable(CustomTabsIntent.EXTRA_REMOTEVIEWS_PENDINGINTENT, pendingIntent);
        try {
            return mService.updateVisuals(mCallback, bundle);
        } catch (RemoteException e) {
            return false;
        }
    }

    @Deprecated
    public boolean setToolbarItem(int id, @NonNull Bitmap icon, @NonNull String description) {
        Bundle bundle = new Bundle();
        bundle.putInt(CustomTabsIntent.KEY_ID, id);
        bundle.putParcelable(CustomTabsIntent.KEY_ICON, icon);
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, description);

        Bundle metaBundle = new Bundle();
        metaBundle.putBundle(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE, bundle);
        try {
            return mService.updateVisuals(mCallback, metaBundle);
        } catch (RemoteException e) {
            return false;
        }
    }

    public boolean requestPostMessageChannel(Uri postMessageOrigin) {
        try {
            return mService.requestPostMessageChannel(
                    mCallback, postMessageOrigin);
        } catch (RemoteException e) {
            return false;
        }
    }

    public int postMessage(String message, Bundle extras) {
        synchronized (mLock) {
            try {
                return mService.postMessage(mCallback, message, extras);
            } catch (RemoteException e) {
                return CustomTabsService.RESULT_FAILURE_REMOTE_ERROR;
            }
        }
    }

                  IBinder getBinder() {
        return mCallback.asBinder();
    }

                  ComponentName getComponentName() {
        return mComponentName;
    }
}

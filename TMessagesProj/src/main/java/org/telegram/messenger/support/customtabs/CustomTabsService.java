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

import android.app.Service;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;
import android.os.IBinder.DeathRecipient;
import android.os.RemoteException;
import androidx.annotation.IntDef;
import androidx.collection.ArrayMap;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Map;
import java.util.NoSuchElementException;

public abstract class CustomTabsService extends Service {

    public static final String ACTION_CUSTOM_TABS_CONNECTION =
            "android.support.customtabs.action.CustomTabsService";

    public static final String KEY_URL =
            "android.support.customtabs.otherurls.URL";

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({RESULT_SUCCESS, RESULT_FAILURE_DISALLOWED,
            RESULT_FAILURE_REMOTE_ERROR, RESULT_FAILURE_MESSAGING_ERROR})
    public @interface Result {
    }

    public static final int RESULT_SUCCESS = 0;

    public static final int RESULT_FAILURE_DISALLOWED = -1;

    public static final int RESULT_FAILURE_REMOTE_ERROR = -2;

    public static final int RESULT_FAILURE_MESSAGING_ERROR = -3;

    private final Map<IBinder, DeathRecipient> mDeathRecipientMap = new ArrayMap<>();

    private ICustomTabsService.Stub mBinder = new ICustomTabsService.Stub() {

        @Override
        public boolean warmup(long flags) {
            return CustomTabsService.this.warmup(flags);
        }

        @Override
        public boolean newSession(ICustomTabsCallback callback) {
            final CustomTabsSessionToken sessionToken = new CustomTabsSessionToken(callback);
            try {
                DeathRecipient deathRecipient = new IBinder.DeathRecipient() {
                    @Override
                    public void binderDied() {
                        cleanUpSession(sessionToken);
                    }
                };
                synchronized (mDeathRecipientMap) {
                    callback.asBinder().linkToDeath(deathRecipient, 0);
                    mDeathRecipientMap.put(callback.asBinder(), deathRecipient);
                }
                return CustomTabsService.this.newSession(sessionToken);
            } catch (RemoteException e) {
                return false;
            }
        }

        @Override
        public boolean mayLaunchUrl(ICustomTabsCallback callback, Uri url,
                                    Bundle extras, List<Bundle> otherLikelyBundles) {
            return CustomTabsService.this.mayLaunchUrl(
                    new CustomTabsSessionToken(callback), url, extras, otherLikelyBundles);
        }

        @Override
        public Bundle extraCommand(String commandName, Bundle args) {
            return CustomTabsService.this.extraCommand(commandName, args);
        }

        @Override
        public boolean updateVisuals(ICustomTabsCallback callback, Bundle bundle) {
            return CustomTabsService.this.updateVisuals(
                    new CustomTabsSessionToken(callback), bundle);
        }

        @Override
        public boolean requestPostMessageChannel(ICustomTabsCallback callback,
                                                 Uri postMessageOrigin) {
            return CustomTabsService.this.requestPostMessageChannel(
                    new CustomTabsSessionToken(callback), postMessageOrigin);
        }

        @Override
        public int postMessage(ICustomTabsCallback callback, String message, Bundle extras) {
            return CustomTabsService.this.postMessage(
                    new CustomTabsSessionToken(callback), message, extras);
        }
    };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    protected boolean cleanUpSession(CustomTabsSessionToken sessionToken) {
        try {
            synchronized (mDeathRecipientMap) {
                IBinder binder = sessionToken.getCallbackBinder();
                DeathRecipient deathRecipient =
                        mDeathRecipientMap.get(binder);
                binder.unlinkToDeath(deathRecipient, 0);
                mDeathRecipientMap.remove(binder);
            }
        } catch (NoSuchElementException e) {
            return false;
        }
        return true;
    }

    protected abstract boolean warmup(long flags);

    protected abstract boolean newSession(CustomTabsSessionToken sessionToken);

    protected abstract boolean mayLaunchUrl(CustomTabsSessionToken sessionToken, Uri url,
                                            Bundle extras, List<Bundle> otherLikelyBundles);

    protected abstract Bundle extraCommand(String commandName, Bundle args);

    protected abstract boolean updateVisuals(CustomTabsSessionToken sessionToken,
                                             Bundle bundle);

    protected abstract boolean requestPostMessageChannel(CustomTabsSessionToken sessionToken,
                                                         Uri postMessageOrigin);

    @Result
    protected abstract int postMessage(
            CustomTabsSessionToken sessionToken, String message, Bundle extras);
}

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

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;
import androidx.annotation.Nullable;
import android.text.TextUtils;

import java.util.ArrayList;
import java.util.List;

public class CustomTabsClient {
    private final ICustomTabsService mService;
    private final ComponentName mServiceComponentName;

    CustomTabsClient(ICustomTabsService service, ComponentName componentName) {
        mService = service;
        mServiceComponentName = componentName;
    }

    public static boolean bindCustomTabsService(Context context,
                                                String packageName, CustomTabsServiceConnection connection) {
        Intent intent = new Intent(CustomTabsService.ACTION_CUSTOM_TABS_CONNECTION);
        if (!TextUtils.isEmpty(packageName)) intent.setPackage(packageName);
        return context.bindService(intent, connection,
                Context.BIND_AUTO_CREATE | Context.BIND_WAIVE_PRIORITY);
    }

    public static String getPackageName(Context context, @Nullable List<String> packages) {
        return getPackageName(context, packages, false);
    }

    public static String getPackageName(
            Context context, @Nullable List<String> packages, boolean ignoreDefault) {
        PackageManager pm = context.getPackageManager();

        List<String> packageNames = packages == null ? new ArrayList<String>() : packages;
        Intent activityIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://"));

        if (!ignoreDefault) {
            ResolveInfo defaultViewHandlerInfo = pm.resolveActivity(activityIntent, 0);
            if (defaultViewHandlerInfo != null) {
                String packageName = defaultViewHandlerInfo.activityInfo.packageName;
                packageNames = new ArrayList<>(packageNames.size() + 1);
                packageNames.add(packageName);
                if (packages != null) packageNames.addAll(packages);
            }
        }

        Intent serviceIntent = new Intent(CustomTabsService.ACTION_CUSTOM_TABS_CONNECTION);
        for (String packageName : packageNames) {
            serviceIntent.setPackage(packageName);
            if (pm.resolveService(serviceIntent, 0) != null) return packageName;
        }
        return null;
    }

    public static boolean connectAndInitialize(Context context, String packageName) {
        if (packageName == null) return false;
        final Context applicationContext = context.getApplicationContext();
        CustomTabsServiceConnection connection = new CustomTabsServiceConnection() {
            @Override
            public final void onCustomTabsServiceConnected(
                    ComponentName name, CustomTabsClient client) {
                client.warmup(0);

                applicationContext.unbindService(this);
            }

            @Override
            public final void onServiceDisconnected(ComponentName componentName) { }
        };
        try {
            return bindCustomTabsService(applicationContext, packageName, connection);
        } catch (SecurityException e) {
            return false;
        }
    }

    public boolean warmup(long flags) {
        try {
            return mService.warmup(flags);
        } catch (RemoteException e) {
            return false;
        }
    }

    public CustomTabsSession newSession(final CustomTabsCallback callback) {
        ICustomTabsCallback.Stub wrapper = new ICustomTabsCallback.Stub() {
            private Handler mHandler = new Handler(Looper.getMainLooper());

            @Override
            public void onNavigationEvent(final int navigationEvent, final Bundle extras) {
                if (callback == null) return;
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        callback.onNavigationEvent(navigationEvent, extras);
                    }
                });
            }

            @Override
            public void extraCallback(final String callbackName, final Bundle args)
                    throws RemoteException {
                if (callback == null) return;
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        callback.extraCallback(callbackName, args);
                    }
                });
            }

            @Override
            public void onMessageChannelReady(final Bundle extras)
                    throws RemoteException {
                if (callback == null) return;
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        callback.onMessageChannelReady(extras);
                    }
                });
            }

            @Override
            public void onPostMessage(final String message, final Bundle extras)
                    throws RemoteException {
                if (callback == null) return;
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        callback.onPostMessage(message, extras);
                    }
                });
            }
        };

        try {
            if (!mService.newSession(wrapper)) return null;
        } catch (RemoteException e) {
            return null;
        }
        return new CustomTabsSession(mService, wrapper, mServiceComponentName);
    }

    public Bundle extraCommand(String commandName, Bundle args) {
        try {
            return mService.extraCommand(commandName, args);
        } catch (RemoteException e) {
            return null;
        }
    }
}

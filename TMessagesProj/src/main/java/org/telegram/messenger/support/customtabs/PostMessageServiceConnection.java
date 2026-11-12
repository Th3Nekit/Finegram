/*
 * This is the source code of Telegram for Android v. 5.x.x.
 * It is licensed under GNU GPL v. 2 or later.
 * You should have received a copy of the license in this archive (see LICENSE).
 *
 * Copyright Nikolai Kudashov, 2013-2018.
 */

package org.telegram.messenger.support.customtabs;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;

public abstract class PostMessageServiceConnection implements ServiceConnection {
    private final Object mLock = new Object();
    private final ICustomTabsCallback mSessionBinder;
    private IPostMessageService mService;

    public PostMessageServiceConnection(CustomTabsSessionToken session) {
        mSessionBinder = ICustomTabsCallback.Stub.asInterface(session.getCallbackBinder());
    }

    public boolean bindSessionToPostMessageService(Context context, String packageName) {
        Intent intent = new Intent();
        intent.setClassName(packageName, PostMessageService.class.getName());
        return context.bindService(intent, this, Context.BIND_AUTO_CREATE);
    }

    public void unbindFromContext(Context context) {
        context.unbindService(this);
    }

    @Override
    public final void onServiceConnected(ComponentName name, IBinder service) {
        mService = IPostMessageService.Stub.asInterface(service);
        onPostMessageServiceConnected();
    }

    @Override
    public final void onServiceDisconnected(ComponentName name) {
        mService = null;
        onPostMessageServiceDisconnected();
    }

    public final boolean notifyMessageChannelReady(Bundle extras) {
        if (mService == null) return false;
        synchronized (mLock) {
            try {
                mService.onMessageChannelReady(mSessionBinder, extras);
            } catch (RemoteException e) {
                return false;
            }
        }
        return true;
    }

    public final boolean postMessage(String message, Bundle extras) {
        if (mService == null) return false;
        synchronized (mLock) {
            try {
                mService.onPostMessage(mSessionBinder, message, extras);
            } catch (RemoteException e) {
                return false;
            }
        }
        return true;
    }

    public void onPostMessageServiceConnected() {}

    public void onPostMessageServiceDisconnected() {}
}

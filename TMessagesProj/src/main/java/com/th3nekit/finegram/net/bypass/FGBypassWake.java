/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.bypass;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.Network;

import org.telegram.messenger.FileLog;
import org.telegram.messenger.Utilities;

public final class FGBypassWake {

    private FGBypassWake() {
    }

    private static BroadcastReceiver receiver;
    private static ConnectivityManager.NetworkCallback networkCallback;
    private static volatile Network lastNetwork;

    public static void install(Context context) {
        if (receiver != null || context == null) {
            return;
        }
        receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context ctx, Intent intent) {
                onScreenOn();
            }
        };
        try {
            IntentFilter filter = new IntentFilter();
            filter.addAction(Intent.ACTION_SCREEN_ON);
            filter.addAction(Intent.ACTION_USER_PRESENT);

            androidx.core.content.ContextCompat.registerReceiver(
                    context.getApplicationContext(), receiver, filter,
                    androidx.core.content.ContextCompat.RECEIVER_NOT_EXPORTED);
        } catch (Throwable t) {
            receiver = null;
            FileLog.e("FGBypassWake.install", t);
        }
        watchNetwork(context);
    }

    private static void watchNetwork(Context context) {
        if (networkCallback != null) {
            return;
        }
        try {
            ConnectivityManager cm = (ConnectivityManager) context.getApplicationContext()
                    .getSystemService(Context.CONNECTIVITY_SERVICE);
            if (cm == null) {
                return;
            }
            networkCallback = new ConnectivityManager.NetworkCallback() {
                private boolean first = true;

                @Override
                public void onAvailable(Network network) {
                    Network previous = lastNetwork;
                    lastNetwork = network;

                    if (first) {
                        first = false;
                        return;
                    }
                    if (!network.equals(previous)) {
                        onNetworkChanged();
                    }
                }

                @Override
                public void onLost(Network network) {
                    if (network.equals(lastNetwork)) {
                        lastNetwork = null;
                    }
                }
            };
            cm.registerDefaultNetworkCallback(networkCallback);
        } catch (Throwable t) {
            networkCallback = null;
            FileLog.e("FGBypassWake.watchNetwork", t);
        }
    }

    private static void onNetworkChanged() {
        if (!FGBypassConfig.enabled) {
            return;
        }
        Utilities.globalQueue.postRunnable(() -> {
            try {
                FGBypassController controller = FGBypassController.getInstance();
                if (controller.isRunning()) {
                    WsBypassCore.dbg("сеть сменилась, сбрасываю паузы релеев");
                    WsBypassCore.getInstance().wakeUp();
                } else {
                    controller.ensureStarted();
                }
            } catch (Throwable t) {
                FileLog.e("FGBypassWake.onNetworkChanged", t);
            }
        });
    }

    private static void onScreenOn() {
        if (!FGBypassConfig.enabled) {
            return;
        }
        Utilities.globalQueue.postRunnable(() -> {
            try {
                FGBypassController controller = FGBypassController.getInstance();
                if (controller.isRunning()) {
                    WsBypassCore.getInstance().wakeUp();
                } else {
                    controller.ensureStarted();
                }
            } catch (Throwable t) {
                FileLog.e("FGBypassWake.onScreenOn", t);
            }
        });
    }
}

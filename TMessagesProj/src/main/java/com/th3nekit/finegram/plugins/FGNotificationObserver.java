package com.th3nekit.finegram.plugins;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.NotificationCenter;

public final class FGNotificationObserver implements NotificationCenter.NotificationCenterDelegate {
    private final NotificationCenter center;
    private final int[] notificationIds;
    private volatile NotificationCenter.NotificationCenterDelegate delegate;

    public FGNotificationObserver(NotificationCenter center,
                                  NotificationCenter.NotificationCenterDelegate delegate,
                                  int[] notificationIds) {
        this.center = center;
        this.delegate = delegate;
        this.notificationIds = notificationIds.clone();
        AndroidUtilities.runOnUIThread(this::register);
    }

    private void register() {
        try {
            for (int id : notificationIds) {
                if (delegate == null) {
                    return;
                }
                center.addObserver(this, id);
            }
        } catch (RuntimeException | Error error) {
            delegate = null;
            unregister();
            throw error;
        }
    }

    public void remove() {
        delegate = null;
        AndroidUtilities.runOnUIThread(this::unregister);
    }

    private void unregister() {
        for (int id : notificationIds) {
            center.removeObserver(this, id);
        }
    }

    @Override
    public void didReceivedNotification(int id, int account, Object... args) {
        NotificationCenter.NotificationCenterDelegate current = delegate;
        if (current != null) {
            current.didReceivedNotification(id, account, args);
        }
    }
}

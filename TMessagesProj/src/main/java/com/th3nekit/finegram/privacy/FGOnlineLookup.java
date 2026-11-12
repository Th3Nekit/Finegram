package com.th3nekit.finegram.privacy;

import android.text.TextUtils;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.UserConfig;
import org.telegram.messenger.UserObject;
import org.telegram.tgnet.ConnectionsManager;
import org.telegram.tgnet.TLRPC;
import org.telegram.tgnet.Vector;

import java.util.function.Consumer;

public final class FGOnlineLookup {
    private final int account;
    private final long accountUserId;
    private final long userId;
    private final Consumer<TLRPC.User> callback;
    private boolean cancelled;
    private int requestId;

    private FGOnlineLookup(int account, long userId, Consumer<TLRPC.User> callback) {
        this.account = account;
        this.accountUserId = UserConfig.getInstance(account).getClientUserId();
        this.userId = userId;
        this.callback = callback;
    }

    public static FGOnlineLookup start(int account, TLRPC.User user, Consumer<TLRPC.User> callback) {
        FGOnlineLookup lookup = new FGOnlineLookup(account, user.id, callback);
        lookup.load(UserObject.getPublicUsername(user));
        return lookup;
    }

    private boolean active() {
        return !cancelled && UserConfig.getInstance(account).isClientActivated()
                && UserConfig.getInstance(account).getClientUserId() == accountUserId;
    }

    private void finish(TLRPC.User user) {
        if (active()) callback.accept(user);
    }

    private void load(String username) {
        if (!active()) return;
        TLRPC.User known = MessagesController.getInstance(account).getUser(userId);
        if (known != null && !known.min && known.access_hash != 0) {
            loadUser(known);
        } else if (!TextUtils.isEmpty(username)) {
            TLRPC.TL_contacts_resolveUsername request = new TLRPC.TL_contacts_resolveUsername();
            request.username = username;
            requestId = ConnectionsManager.getInstance(account).sendRequest(request, (response, error) -> AndroidUtilities.runOnUIThread(() -> {
                if (!active()) return;
                if (error == null && response instanceof TLRPC.TL_contacts_resolvedPeer resolved) {
                    for (TLRPC.User user : resolved.users) {
                        if (user.id == userId && !user.min && user.access_hash != 0) {
                            loadUser(user);
                            return;
                        }
                    }
                }
                finish(null);
            }));
        } else {
            finish(null);
        }
    }

    private void loadUser(TLRPC.User user) {
        TLRPC.TL_users_getUsers request = new TLRPC.TL_users_getUsers();
        request.id.add(MessagesController.getInstance(account).getInputUser(user));
        requestId = ConnectionsManager.getInstance(account).sendRequest(request, (response, error) -> AndroidUtilities.runOnUIThread(() -> {
            if (!active()) return;
            if (error == null && response instanceof Vector<?> vector) {
                for (Object object : vector.objects) {
                    if (object instanceof TLRPC.User result && result.id == userId) {
                        finish(result);
                        return;
                    }
                }
            }
            finish(null);
        }));
    }

    public void cancel() {
        cancelled = true;
        if (requestId != 0) ConnectionsManager.getInstance(account).cancelRequest(requestId, true);
    }
}

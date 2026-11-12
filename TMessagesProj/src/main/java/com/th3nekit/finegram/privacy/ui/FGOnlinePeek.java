package com.th3nekit.finegram.privacy.ui;

import com.th3nekit.finegram.core.configs.FinegramPrivacyConfig;
import com.th3nekit.finegram.privacy.FGOnlineLookup;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.messenger.UserConfig;
import org.telegram.messenger.UserObject;
import org.telegram.tgnet.ConnectionsManager;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.ActionBar.BaseFragment;

import java.util.ArrayList;

public final class FGOnlinePeek {
    private FGOnlinePeek() {
    }

    public static void show(BaseFragment fragment, TLRPC.User user) {
        if (!FinegramPrivacyConfig.INSTANCE.getPeekOnline() || user == null || user.bot || fragment.getParentActivity() == null) return;
        ArrayList<Integer> accounts = new ArrayList<>();
        ArrayList<CharSequence> names = new ArrayList<>();
        for (int account = 0; account < UserConfig.MAX_ACCOUNT_COUNT; account++) {
            UserConfig config = UserConfig.getInstance(account);
            if (account != fragment.getCurrentAccount() && config.isClientActivated()
                    && ConnectionsManager.getInstance(account).isTestBackend() == fragment.getConnectionsManager().isTestBackend()) {
                accounts.add(account);
                names.add(UserObject.getUserName(config.getCurrentUser()));
            }
        }
        AlertDialog.Builder builder = new AlertDialog.Builder(fragment.getParentActivity(), fragment.getResourceProvider())
                .setTitle(LocaleController.getString(R.string.FG_PeekOnline));
        if (accounts.isEmpty()) {
            builder.setMessage(LocaleController.getString(R.string.FG_PeekOnline_NoAccount))
                    .setPositiveButton(LocaleController.getString(R.string.OK), null);
        } else {
            builder.setItems(names.toArray(new CharSequence[0]), (dialog, which) ->
                    AndroidUtilities.runOnUIThread(() -> showStatus(fragment, user, accounts.get(which)), 100));
        }
        fragment.showDialog(builder.create());
    }

    private static void showStatus(BaseFragment fragment, TLRPC.User user, int account) {
        if (fragment.getParentActivity() == null || !FinegramPrivacyConfig.INSTANCE.getPeekOnline()) return;
        final long accountUserId = UserConfig.getInstance(account).getClientUserId();
        final FGOnlineLookup[] request = new FGOnlineLookup[1];
        final Runnable[] refresh = new Runnable[1];
        final AlertDialog dialog = new AlertDialog.Builder(fragment.getParentActivity(), fragment.getResourceProvider())
                .setTitle(UserObject.getUserName(user))
                .setMessage(LocaleController.getString(R.string.Loading))
                .setPositiveButton(LocaleController.getString(R.string.Close), null).create();
        refresh[0] = () -> {
            if (!dialog.isShowing() || !FinegramPrivacyConfig.INSTANCE.getPeekOnline()
                    || UserConfig.getInstance(account).getClientUserId() != accountUserId
                    || !UserConfig.getInstance(account).isClientActivated()) return;
            if (request[0] != null) request[0].cancel();
            request[0] = FGOnlineLookup.start(account, user, result -> {
                if (!dialog.isShowing()) return;
                String status = result == null || result.status == null
                        ? LocaleController.getString(R.string.FG_PeekOnline_Unavailable)
                        : LocaleController.formatUserStatus(account, result);
                dialog.setMessage(UserObject.getUserName(UserConfig.getInstance(account).getCurrentUser())
                        + "\n\n" + status + "\n\n" + LocaleController.getString(R.string.FG_PeekOnline_Info));
                if (result != null && FinegramPrivacyConfig.INSTANCE.getPeekOnlineAutoRefresh()) {
                    AndroidUtilities.runOnUIThread(refresh[0], 15000);
                }
            });
        };
        if (fragment.showDialog(dialog, ignored -> {
            AndroidUtilities.cancelRunOnUIThread(refresh[0]);
            if (request[0] != null) request[0].cancel();
        }) != null) refresh[0].run();
    }
}

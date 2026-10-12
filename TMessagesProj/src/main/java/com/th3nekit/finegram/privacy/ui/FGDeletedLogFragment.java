/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.privacy.ui;

import static org.telegram.messenger.LocaleController.getString;

import android.os.Bundle;
import android.view.View;

import org.telegram.messenger.LocaleController;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.R;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.ChatActivity;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;

import com.th3nekit.finegram.preferences.BaseCGPreferencesEntry;
import com.th3nekit.finegram.privacy.FGDeletedExceptions;
import com.th3nekit.finegram.privacy.FGDeletedStore;

public class FGDeletedLogFragment extends BaseCGPreferencesEntry {

    private static final int CLEAR_ROW = 1;
    private static final int PREVIOUS_ROW = 2;
    private static final int DIALOG_ROW_BASE = 100;

    private final boolean previous;
    private ArrayList<long[]> dialogs = new ArrayList<>();

    public FGDeletedLogFragment() {
        this(false);
    }

    private FGDeletedLogFragment(boolean previous) {
        this.previous = previous;
    }

    @Override
    protected CharSequence getTitle() {
        return getString(previous ? R.string.FG_Saved_PreviousLog : R.string.FG_Saved_Log);
    }

    @Override
    public void onResume() {
        super.onResume();
        updateRows(false);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        dialogs = previous ? FGDeletedStore.previousDialogs() : FGDeletedStore.dialogs(currentAccount);

        if (previous) {
            items.add(UItem.asShadow(getString(R.string.FG_Saved_PreviousLog_Info)));
        } else {
            final int previousCount = FGDeletedStore.previousCount();
            if (previousCount > 0) {
                items.add(UItem.asButton(PREVIOUS_ROW, R.drawable.msg_viewchats,
                        getString(R.string.FG_Saved_PreviousLog), String.valueOf(previousCount)));
                items.add(UItem.asShadow(null));
            }
        }

        if (dialogs.isEmpty()) {
            items.add(UItem.asCenterShadow(getString(R.string.FG_Saved_Empty)));
            return;
        }

        items.add(UItem.asHeader(LocaleController.formatPluralString("FG_Saved_Count",
                previous ? FGDeletedStore.previousCount() : FGDeletedStore.count(currentAccount))));
        for (int i = 0; i < dialogs.size(); i++) {
            final long[] row = dialogs.get(i);
            if (row[0] == 0) {
                items.add(UItem.asShadow(getString(R.string.FG_Saved_UnknownChat_Info)));
            } else {
                items.add(UItem.asButton(DIALOG_ROW_BASE + i, title(row[0]),
                        LocaleController.formatPluralString("FG_Saved_InChat", (int) row[1])));
            }
        }
        items.add(UItem.asShadow(null));
        items.add(UItem.asButton(CLEAR_ROW, R.drawable.msg_delete, getString(R.string.FG_Saved_Clear)));
        items.add(UItem.asShadow(getString(R.string.FG_Saved_Clear_Desc)));
    }

    private CharSequence title(long dialogId) {
        if (previous) return String.valueOf(dialogId);
        try {
            final int account = currentAccount;
            if (dialogId > 0) {
                final TLRPC.User user = MessagesController.getInstance(account).getUser(dialogId);
                if (user != null) return org.telegram.messenger.UserObject.getUserName(user);
            } else {
                final TLRPC.Chat chat = MessagesController.getInstance(account).getChat(-dialogId);
                if (chat != null) return chat.title;
            }
        } catch (Throwable ignored) {
        }
        return String.valueOf(dialogId);
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        final int index = item.id - DIALOG_ROW_BASE;
        if (index < 0 || index >= dialogs.size() || getParentActivity() == null) {
            return false;
        }
        final long dialogId = dialogs.get(index)[0];
        if (dialogId == 0) return false;
        if (previous) {
            confirmForget(dialogId);
            return true;
        }
        final int account = currentAccount;
        final long ownerId = getUserConfig().getClientUserId();
        if (ownerId <= 0) return false;
        final boolean excluded = FGDeletedExceptions.excluded(account, dialogId);
        final AlertDialog.Builder builder = new AlertDialog.Builder(getParentActivity());
        builder.setTitle(title(dialogId));
        builder.setItems(new CharSequence[]{
                getString(excluded ? R.string.FG_Saved_ResumeChat : R.string.FG_Saved_ExcludeChat),
                getString(R.string.FG_Saved_ForgetChat)
        }, (dialog, which) -> {
            if (currentAccount != account || getUserConfig().getClientUserId() != ownerId) return;
            if (which == 0) {
                FGDeletedExceptions.set(account, dialogId, !excluded);
                updateRows(true);
            } else if (which == 1) {
                confirmForget(dialogId);
            }
        });
        showDialog(builder.create());
        return true;
    }

    private void confirmForget(long dialogId) {
        if (dialogId == 0 || getParentActivity() == null) return;
        final int account = currentAccount;
        final long ownerId = getUserConfig().getClientUserId();
        final AlertDialog.Builder builder = new AlertDialog.Builder(getParentActivity());
        builder.setTitle(title(dialogId));
        builder.setMessage(getString(R.string.FG_Saved_ForgetChat_Confirm));
        builder.setPositiveButton(getString(R.string.Delete), (dialog, which) -> {
            if (previous) FGDeletedStore.forgetPrevious(dialogId);
            else {
                if (ownerId <= 0 || currentAccount != account
                        || getUserConfig().getClientUserId() != ownerId) return;
                FGDeletedStore.forget(account, dialogId);
            }
            updateRows(true);
        });
        builder.setNegativeButton(getString(R.string.Cancel), null);
        showDialog(builder.create());
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == PREVIOUS_ROW) {
            final FGDeletedLogFragment fragment = new FGDeletedLogFragment(true);
            fragment.setCurrentAccount(currentAccount);
            presentFragment(fragment);
            return;
        }
        if (item.id == CLEAR_ROW) {
            if (getParentActivity() == null) return;
            final int account = currentAccount;
            final long ownerId = getUserConfig().getClientUserId();
            final AlertDialog.Builder builder = new AlertDialog.Builder(getParentActivity());
            builder.setTitle(getString(R.string.FG_Saved_Clear));
            builder.setMessage(getString(R.string.FG_Saved_Clear_Confirm));
            builder.setPositiveButton(getString(R.string.Delete), (dialog, which) -> {
                if (previous) FGDeletedStore.forgetAllPrevious();
                else {
                    if (ownerId <= 0 || currentAccount != account
                            || getUserConfig().getClientUserId() != ownerId) return;
                    FGDeletedStore.forgetAll(account);
                }
                updateRows(true);
            });
            builder.setNegativeButton(getString(R.string.Cancel), null);
            showDialog(builder.create());
            return;
        }
        final int index = item.id - DIALOG_ROW_BASE;
        if (index < 0 || index >= dialogs.size()) return;

        final long dialogId = dialogs.get(index)[0];
        if (dialogId == 0) return;
        if (previous) {
            confirmTransfer(dialogId);
            return;
        }
        final Bundle args = new Bundle();
        if (dialogId > 0) {
            args.putLong("user_id", dialogId);
        } else {
            args.putLong("chat_id", -dialogId);
        }

        final ArrayList<Integer> ids = FGDeletedStore.inDialog(currentAccount, dialogId, 1);
        if (!ids.isEmpty()) {
            args.putInt("message_id", ids.get(0));
        }
        if (MessagesController.getInstance(currentAccount).checkCanOpenChat(args, this)) {
            final ChatActivity chat = new ChatActivity(args);
            chat.setCurrentAccount(currentAccount);
            presentFragment(chat);
        }
    }

    private void confirmTransfer(long dialogId) {
        if (getParentActivity() == null || dialogId == 0) return;
        final long ownerId = getUserConfig().getClientUserId();
        if (ownerId <= 0) return;
        final TLRPC.User user = getUserConfig().getCurrentUser();
        final String accountName = user == null ? String.valueOf(ownerId)
                : org.telegram.messenger.UserObject.getUserName(user);
        final AlertDialog.Builder builder = new AlertDialog.Builder(getParentActivity());
        builder.setTitle(getString(R.string.FG_Saved_Transfer));
        builder.setMessage(LocaleController.formatString("FG_Saved_Transfer_Confirm",
                R.string.FG_Saved_Transfer_Confirm, String.valueOf(dialogId), accountName));
        builder.setPositiveButton(getString(R.string.FG_Saved_Transfer), (dialog, which) -> {
            if (getUserConfig().getClientUserId() == ownerId
                    && FGDeletedStore.transferPrevious(currentAccount, dialogId)) {
                updateRows(true);
            } else if (getParentActivity() != null) {
                final AlertDialog.Builder error = new AlertDialog.Builder(getParentActivity());
                error.setMessage(getString(R.string.FG_Saved_Transfer_Error));
                error.setPositiveButton(getString(R.string.OK), null);
                showDialog(error.create());
            }
        });
        builder.setNegativeButton(getString(R.string.Cancel), null);
        showDialog(builder.create());
    }
}

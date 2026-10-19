package com.th3nekit.finegram.privacy.ui;

import static org.telegram.messenger.LocaleController.getString;

import android.view.View;

import org.telegram.messenger.MessagesController;
import org.telegram.messenger.R;
import org.telegram.messenger.UserObject;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;

import com.th3nekit.finegram.preferences.BaseCGPreferencesEntry;
import com.th3nekit.finegram.privacy.FGDeletedExceptions;

public class FGDeletedExclusionsFragment extends BaseCGPreferencesEntry {

    private static final int DIALOG_ROW_BASE = 100;
    private ArrayList<Long> dialogs = new ArrayList<>();

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.FG_Saved_Exceptions);
    }

    @Override
    public void onResume() {
        super.onResume();
        updateRows(false);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        dialogs = FGDeletedExceptions.dialogs(currentAccount);
        if (dialogs.isEmpty()) {
            items.add(UItem.asCenterShadow(getString(R.string.FG_Saved_Exceptions_Empty)));
        } else {
            for (int i = 0; i < dialogs.size(); i++) {
                items.add(UItem.asButton(DIALOG_ROW_BASE + i, title(dialogs.get(i))));
            }
        }
        items.add(UItem.asShadow(getString(R.string.FG_Saved_Exceptions_Info)));
    }

    private CharSequence title(long dialogId) {
        if (dialogId > 0) {
            final TLRPC.User user = MessagesController.getInstance(currentAccount).getUser(dialogId);
            if (user != null) return UserObject.getUserName(user);
        } else {
            final TLRPC.Chat chat = MessagesController.getInstance(currentAccount).getChat(-dialogId);
            if (chat != null) return chat.title;
        }
        return String.valueOf(dialogId);
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        final int index = item.id - DIALOG_ROW_BASE;
        if (index < 0 || index >= dialogs.size() || getParentActivity() == null) return;
        final long dialogId = dialogs.get(index);
        final int account = currentAccount;
        final long ownerId = getUserConfig().getClientUserId();
        if (ownerId <= 0) return;
        final AlertDialog.Builder builder = new AlertDialog.Builder(getParentActivity());
        builder.setTitle(title(dialogId));
        builder.setMessage(getString(R.string.FG_Saved_ResumeChat_Confirm));
        builder.setPositiveButton(getString(R.string.FG_Saved_ResumeChat), (dialog, which) -> {
            if (currentAccount != account || getUserConfig().getClientUserId() != ownerId) return;
            FGDeletedExceptions.set(account, dialogId, false);
            updateRows(true);
        });
        builder.setNegativeButton(getString(R.string.Cancel), null);
        showDialog(builder.create());
    }
}

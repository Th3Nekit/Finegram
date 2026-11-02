/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.preferences.anonymity;

import static org.telegram.messenger.LocaleController.getString;

import android.view.View;

import org.telegram.messenger.ContactsController;
import org.telegram.messenger.R;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;
import java.util.List;

import com.th3nekit.finegram.preferences.BaseCGPreferencesEntry;
import com.th3nekit.finegram.privacy.FGShadowbanStore;

public class HiddenPeoplePreferencesEntry extends BaseCGPreferencesEntry {

    private final int base = 100;

    @Override
    public String getTitle() {
        return getString(R.string.FG_Shadowban);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        items.add(UItem.asShadow(null));
        final List<FGShadowbanStore.Entry> hidden = FGShadowbanStore.all();
        if (hidden.isEmpty()) {
            items.add(UItem.asShadow(getString(R.string.FG_Shadowban_Empty)));
            return;
        }
        for (int a = 0; a < hidden.size(); a++) {
            final FGShadowbanStore.Entry entry = hidden.get(a);
            final TLRPC.User user = getMessagesController().getUser(entry.userId);
            final CharSequence name = user != null
                    ? ContactsController.formatName(user.first_name, user.last_name)
                    : String.valueOf(entry.userId);
            items.add(UItem.asButton(base + a, R.drawable.msg_block, name, whatHidden(entry)));
        }
        items.add(UItem.asShadow(getString(R.string.FG_Shadowban_Tap)));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id < base) {
            return;
        }
        final List<FGShadowbanStore.Entry> hidden = FGShadowbanStore.all();
        final int index = item.id - base;
        if (index < 0 || index >= hidden.size()) {
            return;
        }
        FGShadowbanStore.remove(hidden.get(index).userId);
        getNotificationCenter().postNotificationName(
                org.telegram.messenger.NotificationCenter.dialogsNeedReload, true);
        updateRows(true);
    }

    private CharSequence whatHidden(FGShadowbanStore.Entry entry) {
        if (entry.hideDialog && entry.hideInGroups) {
            return getString(R.string.FG_Shadowban_Both);
        }
        return getString(entry.hideDialog
                ? R.string.FG_Shadowban_Dialog : R.string.FG_Shadowban_Groups);
    }
}

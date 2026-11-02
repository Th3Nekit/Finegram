/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences;

import static org.telegram.messenger.LocaleController.getString;

import android.view.View;

import org.telegram.messenger.R;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;

import com.th3nekit.finegram.core.configs.FinegramPrivacyConfig;
import com.th3nekit.finegram.preferences.anonymity.GhostPreferencesEntry;
import com.th3nekit.finegram.preferences.anonymity.HiddenPeoplePreferencesEntry;
import com.th3nekit.finegram.preferences.anonymity.SavedMessagesPreferencesEntry;
import com.th3nekit.finegram.preferences.anonymity.UnlockPreferencesEntry;

public class AnonymityPreferencesEntry extends BaseCGPreferencesEntry {

    private final int ghostRow = 1;
    private final int savedRow = 2;
    private final int hiddenRow = 3;
    private final int unlockRow = 4;

    @Override
    public String getTitle() {
        return getString(R.string.FG_Anonymity);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        items.add(UItem.asShadow(null));
        items.add(UItem.asButton(ghostRow, R.drawable.msg_viewintopic,
                getString(R.string.FG_Ghost), ghostValue()));
        items.add(UItem.asButton(savedRow, R.drawable.msg_message,
                getString(R.string.FG_Saved), savedValue()));
        items.add(UItem.asButton(hiddenRow, R.drawable.msg_block,
                getString(R.string.FG_Shadowban), hiddenValue()));
        items.add(UItem.asButton(unlockRow, R.drawable.msg_premium_liststar,
                getString(R.string.FG_Unlock), unlockValue()));
        items.add(UItem.asShadow(getString(R.string.FG_Anonymity_Hint)));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == ghostRow) {
            presentFragment(new GhostPreferencesEntry());
        } else if (item.id == savedRow) {
            final SavedMessagesPreferencesEntry fragment = new SavedMessagesPreferencesEntry();
            fragment.setCurrentAccount(currentAccount);
            presentFragment(fragment);
        } else if (item.id == hiddenRow) {
            presentFragment(new HiddenPeoplePreferencesEntry());
        } else if (item.id == unlockRow) {
            presentFragment(new UnlockPreferencesEntry());
        }
    }

    private CharSequence ghostValue() {
        if (!FinegramPrivacyConfig.INSTANCE.getGhostMode()) {
            return getString(R.string.FG_Off);
        }
        return GhostPreferencesEntry.enabledCount() + "/5";
    }

    private CharSequence savedValue() {
        if (!FinegramPrivacyConfig.INSTANCE.getSaveDeleted()) {
            return getString(R.string.FG_Off);
        }
        return String.valueOf(com.th3nekit.finegram.privacy.FGDeletedStore.count(currentAccount));
    }

    private CharSequence hiddenValue() {
        final int count = com.th3nekit.finegram.privacy.FGShadowbanStore.count();
        return count == 0 ? getString(R.string.FG_Off) : String.valueOf(count);
    }

    private CharSequence unlockValue() {
        int on = 0;
        if (FinegramPrivacyConfig.INSTANCE.getLocalPremium()) on++;
        if (FinegramPrivacyConfig.INSTANCE.getAllowScreenshots()) on++;
        if (FinegramPrivacyConfig.INSTANCE.getPseudoForward()) on++;
        return on == 0 ? getString(R.string.FG_Off) : on + "/3";
    }
}

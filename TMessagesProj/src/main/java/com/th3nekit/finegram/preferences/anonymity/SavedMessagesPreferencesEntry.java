/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.preferences.anonymity;

import static org.telegram.messenger.LocaleController.getString;

import android.view.View;

import org.telegram.messenger.R;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;

import com.th3nekit.finegram.core.configs.FinegramPrivacyConfig;
import com.th3nekit.finegram.preferences.BaseCGPreferencesEntry;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;
import com.th3nekit.finegram.privacy.FGDeletedExceptions;
import com.th3nekit.finegram.privacy.FGDeletedStore;

public class SavedMessagesPreferencesEntry extends BaseCGPreferencesEntry {

    private final int deletedRow = 1;
    private final int botsRow = 2, channelsRow = 3;
    private final int dimRow = 4;
    private final int logRow = 5;
    private final int editsRow = 6;
    private final int exceptionsRow = 7;

    @Override
    public void onResume() {
        super.onResume();
        updateRows(false);
    }

    @Override
    public String getTitle() {
        return getString(R.string.FG_Saved);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        items.add(UItem.asShadow(null));
        items.add(SettingsHelper.asSwitchCG(deletedRow, getString(R.string.FG_Saved))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getSaveDeleted()));
        items.add(UItem.asShadow(getString(R.string.FG_Saved_Desc)));

        final boolean on = FinegramPrivacyConfig.INSTANCE.getSaveDeleted();
        items.add(UItem.asHeader(getString(R.string.FG_Saved_Where)));
        items.add(UItem.asCheck(botsRow, getString(R.string.FG_Saved_Bots))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getSaveDeletedFromBots()).setEnabled(on));
        items.add(UItem.asCheck(channelsRow, getString(R.string.FG_Saved_Channels))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getSaveDeletedFromChannels()).setEnabled(on));
        items.add(UItem.asCheck(dimRow, getString(R.string.FG_DimDeleted))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getDimDeleted()).setEnabled(on));
        items.add(UItem.asShadow(null));

        if (on) {
            items.add(UItem.asButton(logRow, R.drawable.msg_viewchats,
                    getString(R.string.FG_Saved_Log), String.valueOf(FGDeletedStore.count(currentAccount))));
        }

        items.add(UItem.asButton(exceptionsRow, getString(R.string.FG_Saved_Exceptions),
                String.valueOf(FGDeletedExceptions.dialogs(currentAccount).size())));
        items.add(UItem.asShadow(getString(R.string.FG_Saved_Exceptions_Info)));

        items.add(SettingsHelper.asSwitchCG(editsRow, getString(R.string.FG_KeepEdits))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getKeepEdits()));
        items.add(UItem.asShadow(getString(R.string.FG_KeepEdits_Desc)));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == deletedRow) {
            FinegramPrivacyConfig.INSTANCE.setSaveDeleted(!FinegramPrivacyConfig.INSTANCE.getSaveDeleted());
            updateRows(true);
        } else if (item.id == botsRow) {
            FinegramPrivacyConfig.INSTANCE.setSaveDeletedFromBots(!FinegramPrivacyConfig.INSTANCE.getSaveDeletedFromBots());
            updateRows(false);
        } else if (item.id == channelsRow) {
            FinegramPrivacyConfig.INSTANCE.setSaveDeletedFromChannels(!FinegramPrivacyConfig.INSTANCE.getSaveDeletedFromChannels());
            updateRows(false);
        } else if (item.id == dimRow) {
            FinegramPrivacyConfig.INSTANCE.setDimDeleted(!FinegramPrivacyConfig.INSTANCE.getDimDeleted());
            updateRows(false);
        } else if (item.id == editsRow) {
            FinegramPrivacyConfig.INSTANCE.setKeepEdits(!FinegramPrivacyConfig.INSTANCE.getKeepEdits());
            SettingsHelper.updateCheckState(view, FinegramPrivacyConfig.INSTANCE.getKeepEdits());
        } else if (item.id == logRow) {
            final com.th3nekit.finegram.privacy.ui.FGDeletedLogFragment fragment =
                    new com.th3nekit.finegram.privacy.ui.FGDeletedLogFragment();
            fragment.setCurrentAccount(currentAccount);
            presentFragment(fragment);
        } else if (item.id == exceptionsRow) {
            final com.th3nekit.finegram.privacy.ui.FGDeletedExclusionsFragment fragment =
                    new com.th3nekit.finegram.privacy.ui.FGDeletedExclusionsFragment();
            fragment.setCurrentAccount(currentAccount);
            presentFragment(fragment);
        }
    }
}

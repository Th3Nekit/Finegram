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

public class UnlockPreferencesEntry extends BaseCGPreferencesEntry {

    private final int forwardRow = 1;
    private final int screenshotsRow = 2;
    private final int premiumRow = 3;

    @Override
    public String getTitle() {
        return getString(R.string.FG_Unlock);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        items.add(UItem.asShadow(null));
        items.add(SettingsHelper.asSwitchCG(forwardRow, getString(R.string.FG_PseudoForward),
                        getString(R.string.FG_PseudoForward_Desc))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getPseudoForward()));
        items.add(SettingsHelper.asSwitchCG(screenshotsRow, getString(R.string.FG_AllowScreenshots),
                        getString(R.string.FG_AllowScreenshots_Desc))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getAllowScreenshots()));
        items.add(UItem.asShadow(null));
        items.add(SettingsHelper.asSwitchCG(premiumRow, getString(R.string.FG_LocalPremium),
                        getString(R.string.FG_LocalPremium_Desc))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getLocalPremium()));
        items.add(UItem.asShadow(null));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == forwardRow) {
            FinegramPrivacyConfig.INSTANCE.setPseudoForward(!FinegramPrivacyConfig.INSTANCE.getPseudoForward());
            SettingsHelper.updateCheckState(view, FinegramPrivacyConfig.INSTANCE.getPseudoForward());
        } else if (item.id == screenshotsRow) {
            FinegramPrivacyConfig.INSTANCE.setAllowScreenshots(!FinegramPrivacyConfig.INSTANCE.getAllowScreenshots());
            SettingsHelper.updateCheckState(view, FinegramPrivacyConfig.INSTANCE.getAllowScreenshots());
        } else if (item.id == premiumRow) {
            FinegramPrivacyConfig.INSTANCE.setLocalPremium(!FinegramPrivacyConfig.INSTANCE.getLocalPremium());
            SettingsHelper.updateCheckState(view, FinegramPrivacyConfig.INSTANCE.getLocalPremium());
        }
    }
}

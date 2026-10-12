/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Устройство раздела повторяет re:extera (GPL-3.0, Copyright the re:extera authors,
 * https://github.com/fossSquad/re-extera). Реализация здесь своя.
 */

package com.th3nekit.finegram.preferences.anonymity;

import static org.telegram.messenger.LocaleController.getString;

import android.view.View;

import org.telegram.messenger.R;
import org.telegram.ui.PrivacyUsersActivity;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;

import com.th3nekit.finegram.core.configs.FinegramPrivacyConfig;
import com.th3nekit.finegram.preferences.BaseCGPreferencesEntry;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;
import com.th3nekit.finegram.privacy.FGGhostExceptions;

public class GhostPreferencesEntry extends BaseCGPreferencesEntry {

    private final int mainRow = 1;
    private final int onlineRow = 2, typingRow = 3, readingRow = 4;
    private final int storiesRow = 5, offlineAfterSendRow = 6;
    private final int exceptionsRow = 7;

    public static int enabledCount() {
        int on = 0;
        if (FinegramPrivacyConfig.INSTANCE.getGhostHideOnline()) on++;
        if (FinegramPrivacyConfig.INSTANCE.getGhostHideTyping()) on++;
        if (FinegramPrivacyConfig.INSTANCE.getGhostHideReading()) on++;
        if (FinegramPrivacyConfig.INSTANCE.getGhostHideStoryViews()) on++;
        if (FinegramPrivacyConfig.INSTANCE.getGhostOfflineAfterSend()) on++;
        return on;
    }

    @Override
    public String getTitle() {
        return getString(R.string.FG_Ghost);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        items.add(UItem.asShadow(null));
        items.add(SettingsHelper.asSwitchCG(mainRow, getString(R.string.FG_Ghost))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getGhostMode()));
        items.add(UItem.asShadow(getString(R.string.FG_Ghost_Desc)));

        final boolean on = FinegramPrivacyConfig.INSTANCE.getGhostMode();
        items.add(UItem.asHeader(getString(R.string.FG_Ghost_What)));
        items.add(UItem.asCheck(onlineRow, getString(R.string.FG_Ghost_Online))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getGhostHideOnline()).setEnabled(on));
        items.add(UItem.asCheck(typingRow, getString(R.string.FG_Ghost_Typing))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getGhostHideTyping()).setEnabled(on));
        items.add(UItem.asCheck(readingRow, getString(R.string.FG_Ghost_Reading))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getGhostHideReading()).setEnabled(on));
        items.add(UItem.asCheck(storiesRow, getString(R.string.FG_Ghost_Stories))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getGhostHideStoryViews()).setEnabled(on));
        items.add(UItem.asCheck(offlineAfterSendRow, getString(R.string.FG_Ghost_OfflineAfterSend))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getGhostOfflineAfterSend()).setEnabled(on));
        items.add(UItem.asShadow(null));

        items.add(UItem.asButton(exceptionsRow, R.drawable.msg_contacts,
                getString(R.string.FG_Ghost_Exceptions), exceptionsValue()));
        items.add(UItem.asShadow(getString(R.string.FG_Ghost_Exceptions_Hint)));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == mainRow) {
            final boolean on = !FinegramPrivacyConfig.INSTANCE.getGhostMode();
            FinegramPrivacyConfig.INSTANCE.setGhostMode(on);
            if (on && enabledCount() == 0) {
                FinegramPrivacyConfig.INSTANCE.setGhostHideOnline(true);
                FinegramPrivacyConfig.INSTANCE.setGhostHideTyping(true);
                FinegramPrivacyConfig.INSTANCE.setGhostHideReading(true);
            }
            updateRows(true);
        } else if (item.id == onlineRow) {
            FinegramPrivacyConfig.INSTANCE.setGhostHideOnline(!FinegramPrivacyConfig.INSTANCE.getGhostHideOnline());
            updateRows(false);
        } else if (item.id == typingRow) {
            FinegramPrivacyConfig.INSTANCE.setGhostHideTyping(!FinegramPrivacyConfig.INSTANCE.getGhostHideTyping());
            updateRows(false);
        } else if (item.id == readingRow) {
            FinegramPrivacyConfig.INSTANCE.setGhostHideReading(!FinegramPrivacyConfig.INSTANCE.getGhostHideReading());
            updateRows(false);
        } else if (item.id == storiesRow) {
            FinegramPrivacyConfig.INSTANCE.setGhostHideStoryViews(!FinegramPrivacyConfig.INSTANCE.getGhostHideStoryViews());
            updateRows(false);
        } else if (item.id == offlineAfterSendRow) {
            FinegramPrivacyConfig.INSTANCE.setGhostOfflineAfterSend(!FinegramPrivacyConfig.INSTANCE.getGhostOfflineAfterSend());
            updateRows(false);
        } else if (item.id == exceptionsRow) {
            PrivacyUsersActivity users = new PrivacyUsersActivity(
                    PrivacyUsersActivity.TYPE_PRIVACY, FGGhostExceptions.snapshot(), false, true);
            users.setDelegate((ids, added) -> {
                FGGhostExceptions.replace(ids);
                updateRows(false);
            });
            presentFragment(users);
        }
    }

    private CharSequence exceptionsValue() {
        final int count = FGGhostExceptions.count();
        return count == 0 ? getString(R.string.FG_Off) : String.valueOf(count);
    }
}

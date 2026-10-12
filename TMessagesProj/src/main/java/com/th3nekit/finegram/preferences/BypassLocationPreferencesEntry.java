/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.preferences;

import static org.telegram.messenger.LocaleController.getString;

import android.util.TypedValue;
import android.view.View;
import android.widget.LinearLayout;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.messenger.UserConfig;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.EditTextBoldCursor;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;
import java.util.List;

import com.th3nekit.finegram.net.bypass.FGBypassConfig;
import com.th3nekit.finegram.net.bypass.FGBypassController;
import com.th3nekit.finegram.net.bypass.FGBypassLocations;
import com.th3nekit.finegram.net.bypass.WsRelayAuth;

public class BypassLocationPreferencesEntry extends BaseCGPreferencesEntry {

    private static final int AUTO_ROW = 1;

    private static final int LOCATION_ROW_BASE = 100;

    private List<FGBypassLocations.Location> locations = new ArrayList<>();

    private boolean measuring;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.FG_Bypass_Location);
    }

    @Override
    public void onResume() {
        super.onResume();
        measure();
    }

    private void measure() {
        if (measuring) return;
        measuring = true;
        FGBypassLocations.measureAll(() -> {
            measuring = false;
            updateRows(false);
        });
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        locations = FGBypassLocations.all();
        String selected = FGBypassLocations.selectedId();

        items.add(UItem.asHeader(getString(R.string.FG_Bypass_Location)));
        items.add(UItem.asRadio(AUTO_ROW, getString(R.string.FG_Bypass_Location_Auto))
                .setChecked(FGBypassLocations.AUTO.equals(selected)));
        for (int i = 0; i < locations.size(); i++) {
            FGBypassLocations.Location location = locations.get(i);
            items.add(UItem.asRadio(LOCATION_ROW_BASE + i, location.label(), pingText(location))
                    .setChecked(location.id.equals(selected)));
        }
        items.add(UItem.asShadow(getString(R.string.FG_Bypass_Location_Desc)));

    }

    private CharSequence pingText(FGBypassLocations.Location location) {
        if (FGBypassLocations.isConnected(location)) {
            return getString(R.string.FG_Bypass_Location_Current);
        }
        int ping = FGBypassLocations.ping(location);
        if (ping > 0) {
            return LocaleController.formatString(R.string.FG_Bypass_Location_Ping, ping);
        }
        if (ping < 0) {
            return getString(R.string.FG_Bypass_Location_Unreachable);
        }
        return measuring ? getString(R.string.FG_Bypass_Location_Measuring) : "";
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == AUTO_ROW) {
            apply(FGBypassLocations.AUTO);
            return;
        }
        int index = item.id - LOCATION_ROW_BASE;
        if (index >= 0 && index < locations.size()) {
            apply(locations.get(index).id);
        }
    }

    private void apply(String id) {
        FGBypassLocations.select(id);
        restartBypass();
    }

    private void restartBypass() {
        if (FGBypassConfig.enabled) {
            FGBypassController controller = FGBypassController.getInstance();
            controller.stop();
            controller.ensureStarted();
            WsRelayAuth.prefetchAsync(UserConfig.selectedAccount);
        }
        updateRows(true);
    }
}

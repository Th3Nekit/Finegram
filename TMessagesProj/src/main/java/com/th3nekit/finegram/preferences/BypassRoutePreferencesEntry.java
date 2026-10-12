/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.preferences;

import static org.telegram.messenger.LocaleController.getString;

import android.view.View;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.messenger.UserConfig;
import org.telegram.messenger.Utilities;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;

import com.th3nekit.finegram.net.bypass.FGBypassConfig;
import com.th3nekit.finegram.net.bypass.FGBypassController;
import com.th3nekit.finegram.net.bypass.FGBypassLocations;
import com.th3nekit.finegram.net.bypass.TgwsRoute;
import com.th3nekit.finegram.net.bypass.WsRelayAuth;

public class BypassRoutePreferencesEntry extends BaseCGPreferencesEntry {

    private static final int TGWS_ROW = 1;
    private static final int RELAY_ROW = 2;

    private static final int MEASURE_DC = 2;

    private boolean measuring;
    private int tgwsPing;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.FG_Bypass_Route);
    }

    @Override
    public void onResume() {
        super.onResume();
        measure();
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        final boolean tgws = FGBypassConfig.tgwsFirst();

        items.add(UItem.asHeader(getString(R.string.FG_Bypass_Route)));
        items.add(UItem.asRadio(TGWS_ROW, getString(R.string.FG_Bypass_Route_Tgws),
                        pingText(tgwsPing))
                .setChecked(tgws));
        items.add(UItem.asRadio(RELAY_ROW, getString(R.string.FG_Bypass_Route_Relay),
                        pingText(relayPing()))
                .setChecked(!tgws));
        items.add(UItem.asShadow(getString(tgws
                ? R.string.FG_Bypass_Route_Tgws_Desc : R.string.FG_Bypass_Route_Relay_Desc)));
    }

    private int relayPing() {
        final FGBypassLocations.Location location = FGBypassLocations.selected();
        if (location != null) return FGBypassLocations.ping(location);
        int best = 0;
        for (FGBypassLocations.Location candidate : FGBypassLocations.all()) {
            final int ping = FGBypassLocations.ping(candidate);
            if (ping > 0 && (best <= 0 || ping < best)) best = ping;
            if (ping < 0 && best == 0) best = ping;
        }
        return best;
    }

    private CharSequence pingText(int ping) {
        if (ping > 0) return LocaleController.formatString(R.string.FG_Bypass_Location_Ping, ping);
        if (ping < 0) return getString(R.string.FG_Bypass_Location_Unreachable);
        return measuring ? getString(R.string.FG_Bypass_Location_Measuring) : "";
    }

    private void measure() {
        if (measuring) return;
        measuring = true;
        updateRows(false);
        Utilities.globalQueue.postRunnable(() -> {
            final int tgws = TgwsRoute.measure(MEASURE_DC);
            AndroidUtilities.runOnUIThread(() -> {
                tgwsPing = tgws;
                measuring = false;
                updateRows(false);
            });
        });
        FGBypassLocations.measureAll(() -> updateRows(false));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == TGWS_ROW) {
            apply(FGBypassConfig.SOURCE_TGWS);
        } else if (item.id == RELAY_ROW) {
            apply(FGBypassConfig.SOURCE_RELAY);
        }
    }

    private void apply(int source) {
        if (FGBypassConfig.source == source) return;
        FGBypassConfig.setSource(source);

        if (FGBypassConfig.enabled) {
            final FGBypassController controller = FGBypassController.getInstance();
            controller.stop();
            controller.ensureStarted();
            WsRelayAuth.prefetchAsync(UserConfig.selectedAccount);
        }
        updateRows(true);
    }
}

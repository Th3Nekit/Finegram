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

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.R;
import org.telegram.messenger.UserConfig;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import org.telegram.ui.ProxyListActivity;

import java.util.ArrayList;

import com.th3nekit.finegram.net.bypass.FGBypassConfig;
import com.th3nekit.finegram.net.bypass.FGBypassController;
import com.th3nekit.finegram.net.bypass.FGBypassLocations;
import com.th3nekit.finegram.net.bypass.WsRelayAuth;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class NetworkPreferencesEntry extends BaseCGPreferencesEntry
        implements NotificationCenter.NotificationCenterDelegate {

    private final int enabledRow = 1;
    private final int locationRow = 2;
    private final int routeRow = 3;
    private final int proxyRow = 4;
    private final int suspendOnVpnRow = 5;
    private final int disableProxyOnVpnRow = 1005;

    private final FGBypassController controller = FGBypassController.getInstance();

    private boolean pollScheduled;

    private final Runnable poll = () -> {
        pollScheduled = false;
        if (FGBypassConfig.enabled) {
            updateRows(false);
        }
    };

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.FG_Bypass);
    }

    @Override
    public boolean onFragmentCreate() {
        controller.setSettingsReloader(() -> updateRows(false));
        NotificationCenter.getGlobalInstance().addObserver(this, NotificationCenter.proxySettingsChanged);
        return super.onFragmentCreate();
    }

    @Override
    public void onResume() {
        super.onResume();
        controller.setSettingsReloader(() -> updateRows(false));
        if (FGBypassConfig.enabled) {
            WsRelayAuth.prefetchAsync(UserConfig.selectedAccount);
        }
        updateRows(false);
    }

    @Override
    public void onFragmentDestroy() {
        controller.setSettingsReloader(null);
        AndroidUtilities.cancelRunOnUIThread(poll);
        NotificationCenter.getGlobalInstance().removeObserver(this, NotificationCenter.proxySettingsChanged);
        super.onFragmentDestroy();
    }

    @Override
    public void didReceivedNotification(int id, int account, Object... args) {
        if (id == NotificationCenter.proxySettingsChanged) {
            updateRows(false);
        }
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        final boolean enabled = FGBypassConfig.enabled;

        items.add(SettingsHelper.asSwitchCG(enabledRow, getString(R.string.FG_Bypass_Enable), statusText())
                .setChecked(enabled)
        );
        items.add(UItem.asButton(locationRow, R.drawable.fg_globe_24, getString(R.string.FG_Bypass_Location),
                locationText()));
        items.add(UItem.asButton(routeRow, R.drawable.fg_route_24, getString(R.string.FG_Bypass_Route),
                routeText()));
        items.add(UItem.asButton(proxyRow, R.drawable.fg_server_24, getString(R.string.FG_Bypass_OpenProxy)));
        items.add(UItem.asShadow(getString(R.string.FG_Bypass_Desc)));

        items.add(SettingsHelper.asSwitchCG(suspendOnVpnRow, getString(R.string.FG_Bypass_SuspendOnVpn))
                .setChecked(FGBypassConfig.suspendOnVpn)
        );
        items.add(UItem.asShadow(getString(R.string.FG_Bypass_SuspendOnVpn_Desc)));

        items.add(SettingsHelper.asSwitchCG(disableProxyOnVpnRow,
                        getString(R.string.FG_Proxy_DisableOnVpn))
                .setChecked(FGBypassConfig.disableProxyOnVpn)
        );
        items.add(UItem.asShadow(getString(R.string.FG_Proxy_DisableOnVpn_Desc)));

        if (enabled) {
            String state = controller.getConnectionState();
            boolean settled = FGBypassController.STATE_RUNNING.equals(state)
                    || FGBypassController.STATE_FAILED.equals(state)
                    || FGBypassController.STATE_VPN.equals(state);
            if (!settled) {
                schedulePoll();
            }
        }
    }

    private void schedulePoll() {
        if (pollScheduled) return;
        pollScheduled = true;
        AndroidUtilities.runOnUIThread(poll, 1000);
    }

    private CharSequence statusText() {
        if (!FGBypassConfig.enabled) {
            return "⚪  " + getString(R.string.FG_Bypass_Status_Off);
        }
        switch (controller.getConnectionState()) {
            case FGBypassController.STATE_RUNNING:
                return "🟢  " + getString(R.string.FG_Bypass_Status_On);
            case FGBypassController.STATE_STARTING:
                return "🟡  " + getString(R.string.FG_Bypass_Connecting);
            case FGBypassController.STATE_FAILED:
                return "🔴  " + getString(R.string.FG_Bypass_Status_Failed);
            case FGBypassController.STATE_VPN:
                return "🔵  " + getString(R.string.FG_Bypass_Status_Vpn);
            default:
                return "⚪  " + getString(R.string.FG_Bypass_Status_Off);
        }
    }

    private CharSequence routeText() {
        return getString(FGBypassConfig.tgwsFirst()
                ? R.string.FG_Bypass_Route_Tgws : R.string.FG_Bypass_Route_Relay);
    }

    private CharSequence locationText() {
        FGBypassLocations.Location location = FGBypassLocations.selected();
        return location != null ? location.label() : getString(R.string.FG_Bypass_Location_Auto);
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == enabledRow) {
            FGBypassConfig.setEnabled(!FGBypassConfig.enabled);
            SettingsHelper.updateCheckState(view, FGBypassConfig.enabled);
            if (FGBypassConfig.enabled) {
                controller.ensureStarted();
            } else {
                controller.stop();
            }
            updateRows(false);
        } else if (item.id == locationRow) {
            presentFragment(new BypassLocationPreferencesEntry());
        } else if (item.id == routeRow) {
            presentFragment(new BypassRoutePreferencesEntry());
        } else if (item.id == proxyRow) {
            presentFragment(new ProxyListActivity());
        } else if (item.id == disableProxyOnVpnRow) {
            FGBypassConfig.setDisableProxyOnVpn(!FGBypassConfig.disableProxyOnVpn);
            SettingsHelper.updateCheckState(view, FGBypassConfig.disableProxyOnVpn);

            controller.reevaluateForVpnToggle();
        } else if (item.id == suspendOnVpnRow) {
            FGBypassConfig.setSuspendOnVpn(!FGBypassConfig.suspendOnVpn);
            SettingsHelper.updateCheckState(view, FGBypassConfig.suspendOnVpn);
            controller.reevaluateForVpnToggle();
            updateRows(false);
        }
    }
}

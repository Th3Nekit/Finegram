package org.telegram.ui.Stories.recorder;

import static org.telegram.messenger.LocaleController.getString;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.DialogObject;
import org.telegram.messenger.FileLog;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.R;
import org.telegram.messenger.UserConfig;
import org.telegram.messenger.Utilities;
import org.telegram.tgnet.AbstractSerializedData;
import org.telegram.tgnet.ConnectionsManager;
import org.telegram.tgnet.OutputSerializedData;
import org.telegram.tgnet.TLObject;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.AlertsCreator;
import org.telegram.ui.Components.PermissionRequest;
import org.telegram.ui.LaunchActivity;
import org.telegram.ui.Stories.DarkThemeResourceProvider;

import java.util.Calendar;
import java.util.Date;
import java.util.List;
import java.util.TimeZone;

public class Weather {

    public static boolean isDefaultCelsius() {
        final String timezone = TimeZone.getDefault().getID();
        return !(
            timezone.startsWith("US/") ||
            "America/Nassau".equals(timezone) ||
            "America/Belize".equals(timezone) ||
            "America/Cayman".equals(timezone) ||
            "Pacific/Palau".equals(timezone)
        );
    }

    public static class State extends TLObject {
        public double lat, lng;

        public String emoji;
        public float temperature;

        public String getEmoji() {

            return emoji;
        }

        public String getTemperature() {
            return getTemperature(isDefaultCelsius());
        }

        public String getTemperature(boolean celsius) {
            if (celsius) {
                return (int) Math.round(temperature) + "°C";
            } else {
                return (int) Math.round((this.temperature * 9.0 / 5.0) + 32) + "°F";
            }
        }

        public static Weather.State TLdeserialize(AbstractSerializedData stream) {
            Weather.State state = new Weather.State();
            state.lat = stream.readDouble(false);
            state.lng = stream.readDouble(false);

            state.emoji = stream.readString(false);
            state.temperature = stream.readFloat(false);
            return state;
        }

        @Override
        public void serializeToStream(OutputSerializedData stream) {
            stream.writeDouble(lat);
            stream.writeDouble(lng);

            stream.writeString(emoji);
            stream.writeFloat(temperature);
        }
    }

    public static void fetch(boolean withProgress, Utilities.Callback<State> whenFetched) {
        if (whenFetched == null) return;
        getUserLocation(withProgress, location -> {
            if (location == null) {
                whenFetched.run(null);
                return;
            }

            Activity activity = LaunchActivity.instance;
            if (activity == null) activity = AndroidUtilities.findActivity(ApplicationLoader.applicationContext);
            if (activity == null || activity.isFinishing()) {
                whenFetched.run(null);
                return;
            }

            final AlertDialog progressDialog = withProgress ? new AlertDialog(activity, AlertDialog.ALERT_TYPE_SPINNER, new DarkThemeResourceProvider()) : null;
            if (withProgress) progressDialog.showDelayed(200);
            Runnable cancel = fetch(location.getLatitude(), location.getLongitude(), weather -> {
                if (withProgress) {
                    progressDialog.dismissUnless(350);
                }
                whenFetched.run(weather);
            });
            if (withProgress && cancel != null) {
                progressDialog.setOnCancelListener(di -> cancel.run());
            }
        });
    }

    private static String cacheKey;
    private static State cacheValue;

    public static State getCached() {
        return cacheValue;
    }

    public static Runnable fetch(double lat, double lng, Utilities.Callback<State> whenFetched) {
        if (whenFetched == null) return null;

        final Date date = new Date();
        final Calendar calendar = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
        calendar.setTime(date);
        final long hours = calendar.getTimeInMillis() / 1_000L / 60L / 60L;
        final String key = Math.round(lat * 1000) + ":" + Math.round(lng * 1000) + "at" + hours;
        if (cacheValue != null && TextUtils.equals(cacheKey, key)) {
            whenFetched.run(cacheValue);
            return null;
        }

        final int[] currentReqId = new int[1];

        final MessagesController messagesController = MessagesController.getInstance(UserConfig.selectedAccount);
        final ConnectionsManager connectionsManager = ConnectionsManager.getInstance(UserConfig.selectedAccount);
        final String username = messagesController.weatherSearchUsername;

        final TLRPC.User[] bot = new TLRPC.User[] { messagesController.getUser(username) };
        Runnable request = () -> {
            TLRPC.TL_messages_getInlineBotResults req2 = new TLRPC.TL_messages_getInlineBotResults();
            req2.bot = messagesController.getInputUser(bot[0]);
            req2.query = "";
            req2.offset = "";
            req2.flags |= 1;
            req2.geo_point = new TLRPC.TL_inputGeoPoint();
            req2.geo_point.lat = lat;
            req2.geo_point._long = lng;
            req2.peer = new TLRPC.TL_inputPeerEmpty();

            currentReqId[0] = connectionsManager.sendRequest(req2, (res2, err2) -> AndroidUtilities.runOnUIThread(() -> {
                currentReqId[0] = 0;
                if (res2 instanceof TLRPC.messages_BotResults) {
                    TLRPC.messages_BotResults r = (TLRPC.messages_BotResults) res2;
                    if (!r.results.isEmpty()) {
                        TLRPC.BotInlineResult rr = r.results.get(0);
                        final String emoji = rr.title;
                        final float temp;
                        try {
                            temp = Float.parseFloat(rr.description);
                        } catch (Exception e) {
                            whenFetched.run(null);
                            return;
                        }
                        final State state = new State();
                        state.lat = lat;
                        state.lng = lng;
                        state.emoji = emoji;
                        state.temperature = temp;

                        cacheKey = key;
                        cacheValue = state;

                        whenFetched.run(state);
                        return;
                    }
                }
                whenFetched.run(null);
            }));
        };

        if (bot[0] == null) {
            TLRPC.TL_contacts_resolveUsername req = new TLRPC.TL_contacts_resolveUsername();
            req.username = username;
            currentReqId[0] = connectionsManager.sendRequest(req, (res, err) -> AndroidUtilities.runOnUIThread(() -> {
                currentReqId[0] = 0;
                if (res instanceof TLRPC.TL_contacts_resolvedPeer) {
                    TLRPC.TL_contacts_resolvedPeer r = (TLRPC.TL_contacts_resolvedPeer) res;
                    messagesController.putUsers(r.users, false);
                    messagesController.putChats(r.chats, false);
                    long uid = DialogObject.getPeerDialogId(r.peer);
                    bot[0] = messagesController.getUser(uid);
                    if (bot[0] != null) {
                        request.run();
                        return;
                    }
                }
                whenFetched.run(null);
            }));
        } else {
            request.run();
        }

        return () -> {
            if (currentReqId[0] != 0) {
                connectionsManager.cancelRequest(currentReqId[0], true);
                currentReqId[0] = 0;
            }
        };
    }

    @SuppressLint("MissingPermission")
    public static void getUserLocation(boolean withProgress, Utilities.Callback<Location> whenGot) {
        if (whenGot == null) return;

        PermissionRequest.ensureEitherPermission(R.raw.permission_request_location, R.string.PermissionNoLocationStory,
            new String[] { Manifest.permission.ACCESS_COARSE_LOCATION, Manifest.permission.ACCESS_FINE_LOCATION },
            new String[] { Manifest.permission.ACCESS_COARSE_LOCATION }, granted -> {

            if (!granted) {
                whenGot.run(null);
                return;
            }

            LocationManager lm = (LocationManager) ApplicationLoader.applicationContext.getSystemService(Context.LOCATION_SERVICE);
            List<String> providers = lm.getProviders(true);
            Location l = null;
            for (int i = providers.size() - 1; i >= 0; i--) {
                l = lm.getLastKnownLocation(providers.get(i));
                if (l != null) {
                    break;
                }
            }
            if (l == null && withProgress) {
                if (!lm.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
                    Context context = LaunchActivity.instance;
                    if (context == null) context = ApplicationLoader.applicationContext;
                    if (context != null) {
                        try {
                            final Context finalContext = context;
                            AlertDialog.Builder builder = new AlertDialog.Builder(context);
                            builder.setTopAnimation(R.raw.permission_request_location, AlertsCreator.PERMISSIONS_REQUEST_TOP_ICON_SIZE, false, Theme.getColor(Theme.key_dialogTopBackground));
                            builder.setMessage(getString(R.string.GpsDisabledAlertText));
                            builder.setPositiveButton(getString(R.string.Enable), (dialog, id) -> {
                                try {
                                    finalContext.startActivity(new Intent(android.provider.Settings.ACTION_LOCATION_SOURCE_SETTINGS));
                                } catch (Exception ignore) {
                                }
                            });
                            builder.setNegativeButton(getString(R.string.Cancel), null);
                            builder.show();
                        } catch (Exception e) {
                            FileLog.e(e);
                        }
                    }
                } else {
                    try {
                        final Utilities.Callback<Location>[] callback = new Utilities.Callback[] { whenGot };
                        final LocationListener[] listenerArr = new LocationListener[] { null };
                        final LocationListener listener = location -> {
                            if (listenerArr[0] != null) {
                                lm.removeUpdates(listenerArr[0]);
                                listenerArr[0] = null;
                            }
                            if (callback[0] != null) {
                                callback[0].run(location);
                                callback[0] = null;
                            }
                        };
                        listenerArr[0] = listener;
                        lm.requestLocationUpdates(LocationManager.GPS_PROVIDER, 1, 0, listener);
                    } catch (Exception e) {
                        FileLog.e(e);
                        whenGot.run(null);
                    }
                    return;
                }
            }
            whenGot.run(l);
        });
    }

}

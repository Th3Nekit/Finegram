/* Licensed under GNU GPL v. 2 or later. */            
                                              
  
                                                                                                                                                                     
                                                                                                                                                                    
                                                         
   

package com.th3nekit.finegram.cards;

import android.util.Log;

import org.json.JSONObject;
import org.telegram.messenger.AndroidUtilities;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Currency;
import java.util.Iterator;
import java.util.Locale;

public final class FGCardsRates {

    private static final String TAG = "FinegramCardRates";

    /**
     * Курсы монет и валют.
     *
     * У сторонний форк всё это отдаёт их собственный сервер одним ответом, адрес которого
     * лежит в закрытых настройках сборки. Своего сервера у нас нет, поэтому собираем тот
     * же ответ сами из двух открытых источников — формат снимка и всё, что читает его
     * дальше, остались как у автора.
     */
    private static final String COINS_URL =
            "https://api.coingecko.com/api/v3/simple/price"
                    + "?ids=bitcoin,the-open-network&vs_currencies=usd"
                    + "&include_24hr_change=true";

    private static final String FIAT_URL = "https://open.er-api.com/v6/latest/USD";
    private static final String FIAT_FALLBACK_URL =
            "https://cdn.jsdelivr.net/npm/@fawazahmed0/currency-api@latest/v1/currencies/usd.json";

    /** Наши имена монет к именам источника. */
    private static final String[][] COIN_IDS = {
            {"btc", "bitcoin"},
            {"ton", "the-open-network"},
    };

    private static final long MIN_REFETCH_MS = 60_000L;

    private static final class RateSnapshot {
        final JSONObject data;
        final long fiatFetchedAtMs;
        final long coinsFetchedAtMs;

        RateSnapshot(JSONObject data, long fiatFetchedAtMs, long coinsFetchedAtMs) {
            this.data = data;
            this.fiatFetchedAtMs = fiatFetchedAtMs;
            this.coinsFetchedAtMs = coinsFetchedAtMs;
        }
    }

    private static volatile RateSnapshot snapshot;

    private static final ArrayList<Runnable> pending = new ArrayList<>();
    private static final ArrayList<WeakReference<CallbackHandle>> weakPending = new ArrayList<>();
    private static boolean fetchingFiat;
    private static boolean fetchingCoins;

    public static final class CallbackHandle {
        private Runnable callback;
        private boolean canceled;
        private final boolean fiatOnly;

        private CallbackHandle(Runnable callback, boolean fiatOnly) {
            this.callback = callback;
            this.fiatOnly = fiatOnly;
        }

        public void cancel() {
            synchronized (pending) {
                canceled = true;
                callback = null;
                for (int i = weakPending.size() - 1; i >= 0; i--) {
                    CallbackHandle handle = weakPending.get(i).get();
                    if (handle == null || handle == this) weakPending.remove(i);
                }
            }
        }

        private void dispatch() {
            Runnable runnable;
            synchronized (pending) {
                if (canceled) return;
                canceled = true;
                runnable = callback;
                callback = null;
            }
            if (runnable != null) runnable.run();
        }
    }

    public static void prefetch() {
        fetch(false, null);
    }

    public static void fetch(final boolean force, final Runnable onDone) {
        fetchInternal(force, false, onDone, null);
    }

    public static CallbackHandle fetchWeak(final boolean force, final Runnable onDone) {
        return fetchWeak(force, false, onDone);
    }

    public static CallbackHandle fetchWeak(final boolean force, final boolean fiatOnly,
                                           final Runnable onDone) {
        CallbackHandle handle = onDone == null ? null : new CallbackHandle(onDone, fiatOnly);
        fetchInternal(force, fiatOnly, null, handle);
        return handle;
    }

    private static void fetchInternal(final boolean force, final boolean fiatOnly, final Runnable onDone,
                                      final CallbackHandle weakCallback) {
        final boolean fresh;
        final boolean startFiat;
        final boolean startCoins;
        synchronized (pending) {
            long now = System.currentTimeMillis();
            RateSnapshot current = snapshot;
            fresh = !force && isFresh(current, now, fiatOnly);
            if (!fresh) {
                if (onDone != null) pending.add(onDone);
                if (weakCallback != null) weakPending.add(new WeakReference<>(weakCallback));
            }
            startFiat = !fresh && !fetchingFiat && (force || !isFiatFresh(current, now));
            startCoins = !fresh && !fiatOnly && !fetchingCoins && (force || !isCoinsFresh(current, now));
            if (startFiat) fetchingFiat = true;
            if (startCoins) fetchingCoins = true;
        }
        if (fresh) {
            if (onDone != null) AndroidUtilities.runOnUIThread(onDone);
            if (weakCallback != null) postWeak(weakCallback);
            return;
        }
        if (startFiat) startFetch(true);
        if (startCoins) startFetch(false);
    }

    private static void startFetch(boolean fiat) {
        new Thread(() -> {
            JSONObject result = null;
            try {
                result = fiat ? fetchFiat() : fetchCoins();
            } catch (Throwable t) {
                Log.e(TAG, "fetch failed: " + t);
            } finally {
                finishFetch(fiat, result);
            }
        }, fiat ? "pill-fiat-rates" : "pill-coin-rates").start();
    }

    private static void finishFetch(boolean fiat, JSONObject result) {
        final ArrayList<Runnable> toRun = new ArrayList<>();
        final ArrayList<CallbackHandle> weakToRun = new ArrayList<>();
        synchronized (pending) {
            if (result != null) {
                try {
                    RateSnapshot previous = snapshot;
                    JSONObject data = previous == null ? new JSONObject() : new JSONObject(previous.data.toString());
                    String key = fiat ? "fiat" : "coins";
                    data.put(key, merge(data.optJSONObject(key), result));
                    long now = System.currentTimeMillis();
                    long fiatTime = previous == null ? 0 : previous.fiatFetchedAtMs;
                    long coinsTime = previous == null ? 0 : previous.coinsFetchedAtMs;
                    if (fiat) fiatTime = now;
                    else if (result.length() == COIN_IDS.length) coinsTime = now;
                    snapshot = new RateSnapshot(data, fiatTime, coinsTime);
                } catch (Throwable t) {
                    Log.e(TAG, "cache update failed: " + t);
                }
            }
            if (fiat) fetchingFiat = false;
            else fetchingCoins = false;
            if (!fetchingFiat && !fetchingCoins) {
                toRun.addAll(pending);
                pending.clear();
            }
            for (int i = weakPending.size() - 1; i >= 0; i--) {
                CallbackHandle handle = weakPending.get(i).get();
                if (handle == null || (!fetchingFiat && (handle.fiatOnly || !fetchingCoins))) {
                    weakPending.remove(i);
                    if (handle != null) weakToRun.add(handle);
                }
            }
        }
        for (Runnable runnable : toRun) AndroidUtilities.runOnUIThread(runnable);
        for (CallbackHandle handle : weakToRun) postWeak(handle);
    }

    private static JSONObject merge(JSONObject previous, JSONObject update) throws Exception {
        JSONObject merged = previous == null ? new JSONObject() : new JSONObject(previous.toString());
        Iterator<String> keys = update.keys();
        while (keys.hasNext()) {
            String key = keys.next();
            merged.put(key, update.get(key));
        }
        return merged;
    }

    private static void postWeak(CallbackHandle handle) {
        final WeakReference<CallbackHandle> ref = new WeakReference<>(handle);
        AndroidUtilities.runOnUIThread(() -> {
            CallbackHandle callback = ref.get();
            if (callback != null) callback.dispatch();
        });
    }

    private static boolean isFresh(RateSnapshot value, long now, boolean fiatOnly) {
        return isFiatFresh(value, now) && (fiatOnly || isCoinsFresh(value, now));
    }

    private static boolean isFiatFresh(RateSnapshot value, long now) {
        return value != null && isFreshTime(value.fiatFetchedAtMs, now);
    }

    private static boolean isCoinsFresh(RateSnapshot value, long now) {
        return value != null && isFreshTime(value.coinsFetchedAtMs, now);
    }

    private static boolean isFreshTime(long fetchedAt, long now) {
        return fetchedAt > 0 && now >= fetchedAt && now - fetchedAt < MIN_REFETCH_MS;
    }

    private static JSONObject fetchCoins() {
        String body = load(COINS_URL);
        if (body == null) {
            return null;
        }
        try {
            JSONObject source = new JSONObject(body);
            JSONObject coins = new JSONObject();
            for (String[] pair : COIN_IDS) {
                JSONObject entry = source.optJSONObject(pair[1]);
                if (entry == null) {
                    continue;
                }
                double usd = entry.optDouble("usd", Double.NaN);
                if (!Double.isFinite(usd) || usd <= 0) {
                    continue;
                }
                JSONObject coin = new JSONObject();
                coin.put("usd", usd);
                double change = entry.optDouble("usd_24h_change", Double.NaN);
                if (Double.isFinite(change)) {
                    coin.put("change_24h", change);
                }
                coins.put(pair[0], coin);
            }
            return coins.length() > 0 ? coins : null;
        } catch (Throwable t) {
            Log.e(TAG, "coins failed: " + t);
            return null;
        }
    }

    private static JSONObject fetchFiat() {
        JSONObject rates = fetchFiat(FIAT_URL, false);
        return rates != null ? rates : fetchFiat(FIAT_FALLBACK_URL, true);
    }

    private static JSONObject fetchFiat(String address, boolean fallback) {
        String body = load(address);
        if (body == null) return null;
        try {
            JSONObject source = new JSONObject(body);
            if (!fallback && (!"success".equals(source.optString("result"))
                    || !"USD".equals(source.optString("base_code")))) return null;
            JSONObject rates = source.optJSONObject(fallback ? "usd" : "rates");
            if (rates == null || rates.optDouble(fallback ? "usd" : "USD", Double.NaN) != 1.0) return null;
            JSONObject valid = new JSONObject();
            Iterator<String> keys = rates.keys();
            while (keys.hasNext()) {
                String key = keys.next();
                double value = rates.optDouble(key, Double.NaN);
                if (!Double.isFinite(value) || value <= 0) continue;
                String iso = key.toUpperCase(Locale.ROOT);
                try {
                    Currency.getInstance(iso);
                } catch (IllegalArgumentException ignored) {
                    continue;
                }
                valid.put(iso, value);
            }
            return valid.length() > 1 ? valid : null;
        } catch (Throwable t) {
            Log.e(TAG, "fiat failed: " + t);
            return null;
        }
    }

    private static String load(String address) {
        HttpURLConnection con = null;
        try {
            URL url = new URL(address);
            con = (HttpURLConnection) url.openConnection();
            con.setRequestMethod("GET");
            con.setConnectTimeout(10000);
            con.setReadTimeout(10000);
            if (con.getResponseCode() != 200) {
                Log.e(TAG, "http " + con.getResponseCode() + " " + address);
                return null;
            }
            StringBuilder sb = new StringBuilder();
            try (BufferedReader r = new BufferedReader(new InputStreamReader(con.getInputStream(), "UTF-8"))) {
                String line;
                while ((line = r.readLine()) != null) sb.append(line);
            }
            return sb.toString();
        } catch (Throwable t) {
            Log.e(TAG, "load failed: " + t);
            return null;
        } finally {
            if (con != null) con.disconnect();
        }
    }

    public static boolean hasCached() {
        return snapshot != null;
    }

    public static JSONObject cached() {
        RateSnapshot value = snapshot;
        if (value == null) return null;
        try {
            return new JSONObject(value.data.toString());
        } catch (Throwable ignored) {
            return null;
        }
    }

    public static double coinUsd(String coin) {
        return coinUsd(snapshot, coin);
    }

    private static double coinUsd(RateSnapshot value, String coin) {
        JSONObject s = value != null ? value.data : null;
        if (s == null || coin == null) return Double.NaN;
        JSONObject coins = s.optJSONObject("coins");
        if (coins == null) return Double.NaN;
        JSONObject c = coins.optJSONObject(coin.toLowerCase(java.util.Locale.ROOT));
        if (c == null) return Double.NaN;
        double price = c.optDouble("usd", Double.NaN);
        return Double.isFinite(price) && price > 0 ? price : Double.NaN;
    }

    public static double fiatRate(String ccy) {
        return fiatRate(snapshot, ccy);
    }

    private static double fiatRate(RateSnapshot value, String ccy) {
        if (ccy == null || ccy.equalsIgnoreCase("USD")) return 1.0;
        JSONObject s = value != null ? value.data : null;
        if (s == null) return Double.NaN;
        JSONObject fiat = s.optJSONObject("fiat");
        if (fiat == null) return Double.NaN;
        double r = fiat.optDouble(ccy.toUpperCase(java.util.Locale.ROOT), Double.NaN);
        return Double.isFinite(r) && r > 0 ? r : Double.NaN;
    }

    public static double coinInFiat(String coin, String ccy) {
        RateSnapshot value = snapshot;
        return coinUsd(value, coin) * fiatRate(value, ccy);
    }

    public static Double change24h(String coin) {
        return coinChange(coin, "change_24h");
    }

    public static Double change7d(String coin) {
        return coinChange(coin, "change_7d");
    }

    private static Double coinChange(String coin, String key) {
        RateSnapshot value = snapshot;
        JSONObject s = value != null ? value.data : null;
        if (s == null || coin == null) return null;
        JSONObject coins = s.optJSONObject("coins");
        if (coins == null) return null;
        JSONObject c = coins.optJSONObject(coin.toLowerCase());
        if (c == null || !c.has(key)) return null;
        double v = c.optDouble(key, Double.NaN);
        return Double.isNaN(v) ? null : v;
    }

    private FGCardsRates() {}
}

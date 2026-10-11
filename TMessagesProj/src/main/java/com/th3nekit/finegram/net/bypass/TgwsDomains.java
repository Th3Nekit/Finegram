/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Список доменов, через которые к Telegram можно пройти без нашего сервера.
 * Схема адресов и способ хранения списка взяты из tg-ws-proxy (MIT,
 * Copyright (c) 2026 Flowseal); сам код здесь свой.
 */

package com.th3nekit.finegram.net.bypass;

import android.content.SharedPreferences;

import org.telegram.messenger.Utilities;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

public final class TgwsDomains {

    private static final String[] BUILT_IN = {
            "virkgj.com",
            "vmmzovy.com",
            "mkuosckvso.com",
            "zaewayzmplad.com",
            "twdmbzcm.com",
            "awzwsldi.com",
            "clngqrflngqin.com",
            "tjacxbqtj.com",
            "bxaxtxmrw.com",
            "dmohrsgmohcrwb.com",
            "vwbmtmoi.com",
            "khgrre.com",
            "ulihssf.com",
            "tmhqsdqmfpmk.com",
            "xwuwoqbm.com",
            "orgcnunpj.com",
            "zhkuldz.com",
            "zypoljnslxa.com",
            "efabnxaowuzs.com",
            "zaftuzsftqdq.com",
    };

    private static final String MIRROR_URL = "https://th3web.com/finegram/ws-domains.txt";

    private static final String UPSTREAM_URL =
            "https://raw.githubusercontent.com/Flowseal/tg-ws-proxy/main/.github/cfproxy-domains.txt";

    private static final String KEY_LIST = "tgws_domains";
    private static final String KEY_FETCHED = "tgws_domains_at";

    private static final long REFRESH_INTERVAL_MS = 6L * 60 * 60 * 1000;

    private static final int FETCH_TIMEOUT_MS = 8_000;

    private static final int MIN_VALID = 3;

    private static final AtomicBoolean fetching = new AtomicBoolean(false);

    private static volatile List<String> cached;

    private TgwsDomains() {}

    public static List<String> ordered() {
        List<String> all = decoded();
        if (all.isEmpty()) return all;
        int base = Math.floorMod(DomainPool.installId().hashCode(), all.size());
        List<String> out = new ArrayList<>(all.size());
        for (int i = 0; i < all.size(); i++) {
            out.add(all.get((base + i) % all.size()));
        }
        return out;
    }

    public static String hostFor(String baseDomain, int dc) {
        return "kws" + DomainPool.effectiveDc(dc) + "." + baseDomain;
    }

    private static List<String> decoded() {
        List<String> list = cached;
        if (list != null) return list;
        synchronized (TgwsDomains.class) {
            if (cached != null) return cached;
            String stored = FGBypassConfig.prefs().getString(KEY_LIST, "");
            List<String> parsed = parse(stored);
            if (parsed.size() < MIN_VALID) {
                parsed = parse(join(BUILT_IN));
            }
            cached = parsed;
            return parsed;
        }
    }

    private static String join(String[] lines) {
        StringBuilder sb = new StringBuilder();
        for (String line : lines) {
            sb.append(line).append('\n');
        }
        return sb.toString();
    }

    private static List<String> parse(String body) {
        LinkedHashSet<String> out = new LinkedHashSet<>();
        if (body == null || body.isEmpty()) return new ArrayList<>(out);
        for (String raw : body.split("\n")) {
            String line = raw.trim();
            if (line.isEmpty() || line.charAt(0) == '#') continue;
            String domain = decode(line);
            if (domain != null) out.add(domain);
        }
        return new ArrayList<>(out);
    }

    static String decode(String source) {
        String line = source.trim().toLowerCase();
        if (!line.endsWith(".com")) {

            return line.endsWith(".co.uk") ? line : null;
        }
        String body = line.substring(0, line.length() - 4);
        int shift = 0;
        for (int i = 0; i < body.length(); i++) {
            if (Character.isLetter(body.charAt(i))) shift++;
        }
        StringBuilder out = new StringBuilder(body.length() + 6);
        for (int i = 0; i < body.length(); i++) {
            char c = body.charAt(i);
            if (c >= 'a' && c <= 'z') {
                out.append((char) (Math.floorMod(c - 'a' - shift, 26) + 'a'));
            } else {
                out.append(c);
            }
        }
        out.append(".co.uk");
        String decoded = out.toString();
        return decoded.length() > ".co.uk".length() ? decoded : null;
    }

    public static void refreshIfStale() {
        SharedPreferences prefs = FGBypassConfig.prefs();
        long last = prefs.getLong(KEY_FETCHED, 0);
        long now = System.currentTimeMillis();
        if (now - last < REFRESH_INTERVAL_MS && now >= last) return;
        if (!fetching.compareAndSet(false, true)) return;
        Utilities.globalQueue.postRunnable(() -> {
            try {
                String body = fetch(MIRROR_URL);
                if (parse(body).size() < MIN_VALID) {
                    body = fetch(UPSTREAM_URL);
                }
                List<String> parsed = parse(body);
                if (parsed.size() >= MIN_VALID) {
                    synchronized (TgwsDomains.class) {
                        cached = parsed;
                    }
                    prefs.edit().putString(KEY_LIST, body).putLong(KEY_FETCHED, now).apply();
                    WsBypassCore.dbg("tgws: обновлён список, доменов " + parsed.size());
                } else {

                    WsBypassCore.dbg("tgws: список не обновлён");
                }
            } catch (Throwable ignored) {
            } finally {
                fetching.set(false);
            }
        });
    }

    private static String fetch(String url) {
        HttpURLConnection con = null;
        try {
            con = (HttpURLConnection) new URL(url).openConnection();
            con.setConnectTimeout(FETCH_TIMEOUT_MS);
            con.setReadTimeout(FETCH_TIMEOUT_MS);
            con.setInstanceFollowRedirects(true);
            con.setRequestProperty("Accept", "text/plain");
            if (con.getResponseCode() != 200) return "";
            ByteArrayOutputStream buffer = new ByteArrayOutputStream();
            try (InputStream in = con.getInputStream()) {
                byte[] chunk = new byte[4096];
                int read;

                while ((read = in.read(chunk)) > 0 && buffer.size() < 64 * 1024) {
                    buffer.write(chunk, 0, read);
                }
            }
            return new String(buffer.toByteArray(), StandardCharsets.UTF_8);
        } catch (Throwable t) {
            return "";
        } finally {
            if (con != null) {
                try { con.disconnect(); } catch (Throwable ignored) {}
            }
        }
    }
}

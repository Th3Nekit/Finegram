/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.bypass;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

final class TgwsDns {

    private static final String[] RESOLVERS = {
            "https://cloudflare-dns.com/dns-query",
            "https://dns.google/resolve",
            "https://dns.alidns.com/resolve",
            "https://mozilla.cloudflare-dns.com/dns-query",
    };

    private static final int TIMEOUT_MS = 1_200;

    private static final long CACHE_TTL_MS = 5L * 60 * 1000;

    private static final Map<String, Object[]> cache = new ConcurrentHashMap<>();
    private static final ExecutorService executor = new ThreadPoolExecutor(2, 2, 30L,
            TimeUnit.SECONDS, new ArrayBlockingQueue<>(4), r -> {
                Thread t = new Thread(r, "tgws-doh");
                t.setDaemon(true);
                return t;
            }, new ThreadPoolExecutor.AbortPolicy());

    private TgwsDns() {}

    static String resolve(String domain, long deadlineNanos, RawWebSocket.ConnectPermit permit) {
        if (!current(deadlineNanos, permit)) return null;
        Object[] hit = cache.get(domain);
        if (hit != null && System.currentTimeMillis() < (Long) hit[1]) {
            return (String) hit[0];
        }
        for (String resolver : RESOLVERS) {
            if (!current(deadlineNanos, permit)) return null;
            Future<String> pending = null;
            String ip;
            try {
                pending = executor.submit(() -> ask(resolver, domain, deadlineNanos));
                while (true) {
                    if (!current(deadlineNanos, permit)) return null;
                    try {
                        long left = deadlineNanos - System.nanoTime();
                        ip = pending.get(Math.max(1L, Math.min(left, TimeUnit.MILLISECONDS.toNanos(100))),
                                TimeUnit.NANOSECONDS);
                        break;
                    } catch (TimeoutException ignored) {
                    }
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                return null;
            } catch (Exception e) {
                return null;
            } finally {
                if (pending != null && !pending.isDone()) pending.cancel(true);
            }
            if (ip != null) {
                if (!current(deadlineNanos, permit)) return null;
                cache.put(domain, new Object[]{ip, System.currentTimeMillis() + CACHE_TTL_MS});
                return ip;
            }
        }
        return null;
    }

    static void clearCache() {
        cache.clear();
    }

    private static boolean current(long deadlineNanos, RawWebSocket.ConnectPermit permit) {
        return !Thread.currentThread().isInterrupted() && System.nanoTime() < deadlineNanos
                && (permit == null || permit.isCurrent());
    }

    private static String ask(String resolver, String domain, long deadlineNanos) {
        HttpURLConnection con = null;
        try {
            String url = resolver + "?type=A&name="
                    + URLEncoder.encode(domain, StandardCharsets.UTF_8.name());
            con = (HttpURLConnection) new URL(url).openConnection();
            int timeout = (int) Math.max(1L, Math.min(TIMEOUT_MS,
                    TimeUnit.NANOSECONDS.toMillis(deadlineNanos - System.nanoTime())));
            con.setConnectTimeout(timeout);
            con.setReadTimeout(timeout);
            con.setRequestProperty("Accept", "application/dns-json");
            if (con.getResponseCode() != 200) return null;

            ByteArrayOutputStream buffer = new ByteArrayOutputStream();
            try (InputStream in = con.getInputStream()) {
                byte[] chunk = new byte[2048];
                int read;
                while (current(deadlineNanos, null) && (read = in.read(chunk)) > 0) {
                    if (buffer.size() + read > 32 * 1024) return null;
                    buffer.write(chunk, 0, read);
                }
            }
            if (!current(deadlineNanos, null)) return null;
            JSONObject answer = new JSONObject(new String(buffer.toByteArray(), StandardCharsets.UTF_8));
            if (!answer.has("Answer")) return null;
            JSONArray records = answer.getJSONArray("Answer");
            for (int i = 0; i < records.length(); i++) {
                JSONObject record = records.getJSONObject(i);

                if (record.optInt("type") != 1) continue;
                String data = record.optString("data", "");
                if (!data.isEmpty()) return data;
            }
            return null;
        } catch (Throwable t) {
            return null;
        } finally {
            if (con != null) {
                try { con.disconnect(); } catch (Throwable ignored) {}
            }
        }
    }
}

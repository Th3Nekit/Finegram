/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Запасной путь к Telegram в обход нашего сервера. Схема соединения повторяет
 * tg-ws-proxy (MIT, Copyright (c) 2026 Flowseal); код здесь свой.
 */

package com.th3nekit.finegram.net.bypass;

import java.io.IOException;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.TimeUnit;

public final class TgwsRoute {

    private static final Map<Integer, String> DIRECT_DC_IP;

    static {
        Map<Integer, String> d = new HashMap<>();
        d.put(1, "149.154.175.50");
        d.put(2, "149.154.167.51");
        d.put(3, "149.154.175.100");
        d.put(4, "149.154.167.91");
        d.put(5, "149.154.171.5");
        d.put(203, "91.105.192.100");
        DIRECT_DC_IP = Collections.unmodifiableMap(d);
    }

    private static final String WS_PATH = "/apiws";

    private static final long DIRECT_ATTEMPT_MS = 1_500L;
    private static final long CF_ATTEMPT_MS = 2_500L;

    private static final long DNS_RETRY_MIN_MS = 2_000L;

    private static final int MEASURE_DOMAIN_LIMIT = 3;
    private static final int MEASURE_TIMEOUT_MS = 3_000;

    private static final String DIRECT_KEY = "\u0000direct";
    private static final long DIRECT_COOLDOWN_MS = 5L * 60 * 1000;

    private static final long FAIL_COOLDOWN_MS = 30_000L;
    private static final long BUSY_COOLDOWN_MS = 45_000L;
    private static final long BUSY_COOLDOWN_MAX_MS = 300_000L;

    private static final Map<String, long[]> cooldown = new ConcurrentHashMap<>();

    private TgwsRoute() {}

    static RawWebSocket connect(int dc, long deadlineNanos,
                                RawWebSocket.ConnectPermit permit) throws IOException {
        TgwsDomains.refreshIfStale();
        return connectCloudflare(dc, deadlineNanos, permit, false);
    }

    static RawWebSocket connectLastResort(int dc, boolean isMedia, long deadlineNanos,
                                          RawWebSocket.ConnectPermit permit) throws IOException {
        if (inCooldown(DIRECT_KEY)) throw new IOException("direct route is in cooldown");
        return connectDirect(dc, isMedia, deadlineNanos, permit);
    }

    private static RawWebSocket connectDirect(int dc, boolean isMedia, long deadlineNanos,
                                              RawWebSocket.ConnectPermit permit) throws IOException {
        String ip = DIRECT_DC_IP.get(dc);
        if (ip == null) throw new IOException("no direct address for dc " + dc);

        IOException last = null;
        int attempted = 0;
        for (String sni : DomainPool.wsDomainsForDc(dc, isMedia)) {
            checkAlive(permit);
            if (System.nanoTime() >= deadlineNanos) break;
            attempted++;
            long attemptDeadline = Math.min(deadlineNanos,
                    System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(DIRECT_ATTEMPT_MS));
            WsBypassCore.dbg("tgws: прямо dc=" + dc + " -> " + sni + " (" + ip + ")");
            try {
                RawWebSocket ws = RawWebSocket.connectUntil(ip, sni, WS_PATH, null,
                        attemptDeadline, permit);
                cooldown.remove(DIRECT_KEY);
                WsBypassCore.dbg("tgws: прямо 101 OK через " + sni);
                return ws;
            } catch (IOException ex) {
                WsBypassCore.dbg("tgws: прямо " + sni + " -> " + ex.getMessage());
                last = ex;

                if (!(ex instanceof RawWebSocket.HandshakeException)) break;
            } catch (Throwable t) {
                last = new IOException(t);
                break;
            }
        }
        if (attempted > 0) pause(DIRECT_KEY, DIRECT_COOLDOWN_MS, 0);
        if (last != null) throw last;
        throw new IOException("direct route exhausted");
    }

    private static RawWebSocket connectCloudflare(int dc, long deadlineNanos,
                                                  RawWebSocket.ConnectPermit permit,
                                                  boolean ignoreCooldown) throws IOException {
        List<String> domains = TgwsDomains.ordered();
        if (domains.isEmpty()) throw new IOException("no cloudflare domains");

        IOException last = null;
        int attempted = 0;
        for (String base : domains) {
            checkAlive(permit);
            if (System.nanoTime() >= deadlineNanos) break;
            if (!ignoreCooldown && inCooldown(base)) continue;
            String host = TgwsDomains.hostFor(base, dc);
            long attemptDeadline = Math.min(deadlineNanos,
                    System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(CF_ATTEMPT_MS));
            attempted++;
            WsBypassCore.dbg("tgws: cloudflare dc=" + dc + " -> " + host);
            try {
                RawWebSocket ws = RawWebSocket.connectUntil(host, host, WS_PATH, null,
                        attemptDeadline, permit);
                cooldown.remove(base);
                WsBypassCore.dbg("tgws: cloudflare 101 OK через " + host);
                return ws;
            } catch (IOException ex) {
                int status = ex instanceof RawWebSocket.HandshakeException
                        ? ((RawWebSocket.HandshakeException) ex).statusCode : 0;
                WsBypassCore.dbg("tgws: cloudflare " + host + " -> " + ex.getMessage());

                RawWebSocket viaIp = status != 0 ? null : retryByAddress(host, deadlineNanos, permit);
                if (viaIp != null) {
                    cooldown.remove(base);
                    return viaIp;
                }
                recordFailure(base, status);
                last = ex;
            } catch (Throwable t) {
                recordFailure(base, 0);
                last = new IOException(t);
            }
        }
        if (attempted == 0) throw new IOException("cloudflare domains are in cooldown");
        if (last != null) throw last;
        throw new IOException("cloudflare route exhausted");
    }

    private static RawWebSocket retryByAddress(String host, long deadlineNanos,
                                               RawWebSocket.ConnectPermit permit) {
        long left = deadlineNanos - System.nanoTime();
        if (left < TimeUnit.MILLISECONDS.toNanos(DNS_RETRY_MIN_MS)) return null;
        long dnsDeadline = Math.min(deadlineNanos - TimeUnit.MILLISECONDS.toNanos(1_000),
                System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(1_000));
        String ip = TgwsDns.resolve(host, dnsDeadline, permit);
        if (ip == null || ip.isEmpty()) return null;
        long attemptDeadline = Math.min(deadlineNanos,
                System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(CF_ATTEMPT_MS));
        WsBypassCore.dbg("tgws: cloudflare " + host + " по адресу " + ip);
        try {
            checkAlive(permit);
            RawWebSocket ws = RawWebSocket.connectUntil(ip, host, WS_PATH, null,
                    attemptDeadline, permit);
            WsBypassCore.dbg("tgws: cloudflare 101 OK через " + host + " (" + ip + ")");
            return ws;
        } catch (Throwable t) {
            WsBypassCore.dbg("tgws: cloudflare " + host + " (" + ip + ") -> " + t.getMessage());
            return null;
        }
    }

    private static void checkAlive(RawWebSocket.ConnectPermit permit) throws IOException {
        if (permit != null && !permit.isCurrent()) {
            throw new IOException("tgws connect cancelled");
        }
    }

    private static boolean inCooldown(String domain) {
        long[] state = cooldown.get(domain);
        if (state == null) return false;
        if (System.nanoTime() >= state[0]) {

            state[0] = 0;
            return false;
        }
        return true;
    }

    private static void recordFailure(String domain, int status) {
        long[] state = cooldown.get(domain);
        long strikes = state == null ? 0 : state[1];
        if (status == 429) {
            strikes++;
            pause(domain, Math.min(BUSY_COOLDOWN_MAX_MS, BUSY_COOLDOWN_MS * strikes), strikes);
        } else {
            pause(domain, FAIL_COOLDOWN_MS, strikes);
        }
    }

    private static void pause(String key, long millis, long strikes) {
        cooldown.put(key, new long[]{
                System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(millis), strikes});
    }

    static void clearCooldowns() {
        cooldown.clear();
        TgwsDns.clearCache();
    }

    public static int measure(int dc) {
        final List<String> domains = TgwsDomains.ordered();
        int tried = 0;
        for (String base : domains) {
            if (tried++ >= MEASURE_DOMAIN_LIMIT) break;
            final String host = TgwsDomains.hostFor(base, dc);
            final long started = android.os.SystemClock.elapsedRealtime();
            java.net.Socket socket = new java.net.Socket();
            try {
                socket.connect(new java.net.InetSocketAddress(host, 443), MEASURE_TIMEOUT_MS);
                return (int) (android.os.SystemClock.elapsedRealtime() - started);
            } catch (Throwable ignored) {
            } finally {
                try { socket.close(); } catch (Throwable ignored) {}
            }
        }
        return -1;
    }
}

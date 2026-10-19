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

/**
 * Дорога к Telegram, для которой не нужен наш сервер.
 *
 * Telegram держит свои датацентры не только на голом MTProto: тот же поток он принимает
 * WebSocket-ом по адресу {@code /apiws} — так работает веб-версия. Значит, рукопожатие,
 * которое мы и так собираем для релея, можно отдать прямо туда.
 *
 * Отсюда две дороги.
 *
 * Прямая — в датацентр по его адресу, с именем {@code kwsN.web.telegram.org} в TLS. Быстрее
 * всего и ни от кого не зависит, но там, где адреса Telegram закрыты, не открывается вовсе.
 *
 * Через Cloudflare — на чужую площадку, которая пересылает тот же WebSocket в нужный
 * датацентр. Со стороны это обычный сайт за Cloudflare, поэтому проходит там, где прямая
 * дорога закрыта. Площадки чужие: они приходят и уходят, список обновляется отдельно.
 *
 * В обоих случаях наружу уходит тот же обфусцированный поток, что и к релею: ни ключей, ни
 * содержимого переписки промежуточная площадка не видит — она пересылает байты.
 */
public final class TgwsRoute {

    /** Адреса датацентров для прямой дороги. */
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

    /** Сколько ждать одну попытку: дальше выгоднее пробовать следующий адрес. */
    private static final long DIRECT_ATTEMPT_MS = 1_500L;
    private static final long CF_ATTEMPT_MS = 2_500L;

    /** Меньше этого остатка вторую попытку начинать незачем: не успеет. */
    private static final long DNS_RETRY_MIN_MS = 2_000L;

    /** Замер для экрана настроек: дальше третьей площадки ходить незачем. */
    private static final int MEASURE_DOMAIN_LIMIT = 3;
    private static final int MEASURE_TIMEOUT_MS = 3_000;

    /**
     * Ключ прямой дороги в списке пауз.
     *
     * Она тоже уходит в паузу после отказа, и это важнее, чем кажется: там, где адреса
     * Telegram закрыты, она не откроется никогда, а ждать её каждый раз — значит съедать
     * время, отведённое на весь запасной путь, и не оставлять его Cloudflare.
     */
    private static final String DIRECT_KEY = "\u0000direct";
    private static final long DIRECT_COOLDOWN_MS = 5L * 60 * 1000;

    /**
     * Паузы после отказа.
     *
     * Обычный отказ — чаще всего пропала связь у самого телефона, поэтому пауза короткая.
     * А вот 429 присылает сам Cloudflare, когда с площадки идёт слишком много: ломиться
     * туда снова бессмысленно и вредно, пауза растёт с каждым разом.
     */
    private static final long FAIL_COOLDOWN_MS = 30_000L;
    private static final long BUSY_COOLDOWN_MS = 45_000L;
    private static final long BUSY_COOLDOWN_MAX_MS = 300_000L;

    /** Домен -> {до какого времени не трогать, сколько раз подряд отказывал}. */
    private static final Map<String, long[]> cooldown = new ConcurrentHashMap<>();

    private TgwsRoute() {}

    /** Площадки Flowseal: дорога, которую человек выбирает в настройках. */
    static RawWebSocket connect(int dc, long deadlineNanos,
                                RawWebSocket.ConnectPermit permit) throws IOException {
        TgwsDomains.refreshIfStale();
        return connectCloudflare(dc, deadlineNanos, permit, false);
    }

    /**
     * Прямо в датацентр Telegram: последняя попытка, когда обе названные дороги закрыты.
     *
     * В настройках её нет нарочно. Это ничей не сервер и выбирать тут нечего: где адреса
     * Telegram открыты, она просто сработает, а где закрыты — не сработает никогда, и
     * тогда после первого отказа она уходит в долгую паузу и больше не мешает.
     */
    static RawWebSocket connectLastResort(int dc, boolean isMedia, long deadlineNanos,
                                          RawWebSocket.ConnectPermit permit) throws IOException {
        if (inCooldown(DIRECT_KEY)) throw new IOException("direct route is in cooldown");
        return connectDirect(dc, isMedia, deadlineNanos, permit);
    }

    /** Прямо в датацентр: адрес известен, имя в TLS — телеграмовское. */
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
                // Второе имя ведёт на тот же адрес: если до него не дошли по сети,
                // от смены имени ничего не изменится, и время уйдёт впустую.
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

    /** Через чужую площадку: номер датацентра — в поддомене. */
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
                // Ответ с кодом значит, что до площадки мы дошли: имя разобралось верно,
                // и спрашивать адрес заново не о чем.
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

    /**
     * Вторая попытка к той же площадке, но по адресу, полученному в обход провайдера.
     *
     * Возвращает null, если времени на это уже нет или адрес не узнали: для вызывающего
     * это значит «не вышло», и он идёт к следующей площадке.
     */
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
            // пауза вышла — даём домену ещё один шанс, но счётчик не обнуляем:
            // если он снова откажет, пауза продолжит расти с того же места
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

    /** Забыть все паузы: сеть сменилась, прежние отказы больше ни о чём не говорят. */
    static void clearCooldowns() {
        cooldown.clear();
        TgwsDns.clearCache();
    }

    /**
     * Отклик площадок в миллисекундах или -1, если ни одна не отозвалась.
     *
     * Меряется время до обычного соединения на 443 с первой отозвавшейся площадкой — той
     * же меркой, что и наш сервер, иначе числа на экране не с чем сравнивать. Паузы тут не
     * в счёт: человек спрашивает, работает ли дорога сейчас.
     *
     * Зовётся только из фонового потока.
     */
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

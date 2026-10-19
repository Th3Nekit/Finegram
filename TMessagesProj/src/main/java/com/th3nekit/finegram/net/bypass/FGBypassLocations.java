/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.bypass;

import android.content.SharedPreferences;
import android.text.TextUtils;


import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Через какую страну ходит обход.
 *
 * Встроенные локации живут в коде, свои — в настройках: их можно добавить, когда появится
 * отдельный сервер. Выбранная локация идёт первой, остальные остаются запасными — если
 * выбранная замолчит, клиент уйдёт на следующую, а не останется без связи.
 */
public final class FGBypassLocations {

    /** Одна локация: имя для человека, релей и сервер, выдающий пропуска. */
    public static final class Location {
        public final String id;
        public final String flag;
        public final String title;
        public final String relayHost;
        public final String authUrl;

        Location(String id, String flag, String title, String relayHost, String authUrl) {
            this.id = id;
            this.flag = flag;
            this.title = title;
            this.relayHost = relayHost;
            this.authUrl = authUrl;
        }

        /** Как показать в списке: флаг и название. */
        public String label() {
            return flag.isEmpty() ? title : flag + "  " + title;
        }
    }

    /** Клиент сам выбирает, откуда ходить. */
    public static final String AUTO = "auto";

    /**
     * Сервер пропусков: пусто — значит, подтверждать доступ не у кого и не нужно.
     *
     * Наш релей пускает всех: ходить он умеет только к Telegram, а скорость каждого
     * ограничена на его стороне — проверять, кто пришёл, незачем.
     */
    private static final String NO_AUTH = "";

    private static final Location[] BUILT_IN = {
            new Location("nl-1", "🇳🇱", "Нидерланды", "tgws.th3web.com", NO_AUTH),
    };

    private static final String KEY_SELECTED = "location";

    private static SharedPreferences prefs() {
        return FGBypassConfig.prefs();
    }

    /** Все локации: сначала встроенные, потом свои. */
    public static List<Location> all() {
        List<Location> out = new ArrayList<>(BUILT_IN.length + 2);
        Collections.addAll(out, BUILT_IN);
        return out;
    }

    public static String selectedId() {
        return prefs().getString(KEY_SELECTED, AUTO);
    }

    public static void select(String id) {
        prefs().edit().putString(KEY_SELECTED, id == null ? AUTO : id).apply();
    }

    /** Выбранная локация или null, если выбран автоматический подбор. */
    public static Location selected() {
        String id = selectedId();
        if (AUTO.equals(id)) return null;
        for (Location location : all()) {
            if (location.id.equals(id)) return location;
        }
        return null;
    }

    /** Сервер пропусков выбранной локации. Пусто — пропуск не нужен. */
    public static String authUrl() {
        Location location = selected();
        if (location != null) {
            return location.authUrl;
        }
        // при автоподборе пропуск нужен, только если его просит хоть одна локация
        for (Location candidate : all()) {
            if (!candidate.authUrl.isEmpty()) {
                return candidate.authUrl;
            }
        }
        return NO_AUTH;
    }

    /** Нужно ли вообще подтверждать доступ при нынешнем выборе. */
    public static boolean needsAuth() {
        return !authUrl().isEmpty();
    }

    /**
     * Порядок, в котором пробуются релеи.
     *
     * Выбранная локация идёт первой, дальше — остальные: если её сервер замолчит, связь
     * не пропадёт совсем.
     */
    public static String[] relayHosts() {
        List<Location> locations = all();
        List<String> out = new ArrayList<>(locations.size());
        Location chosen = selected();
        if (chosen != null) {
            out.add(chosen.relayHost);
        } else {
            // автоподбор: начинаем с самой отзывчивой из тех, что уже мерили
            List<Location> byPing = new ArrayList<>(locations);
            Collections.sort(byPing, (a, b) -> {
                int first = ping(a);
                int second = ping(b);
                // не мерили или не отозвалась — в конец, но перед вовсе неизвестными
                int weightA = first > 0 ? first : (first < 0 ? 100000 : 50000);
                int weightB = second > 0 ? second : (second < 0 ? 100000 : 50000);
                return Integer.compare(weightA, weightB);
            });
            locations = byPing;
        }
        for (Location location : locations) {
            if (!out.contains(location.relayHost)) {
                out.add(location.relayHost);
            }
        }
        return out.toArray(new String[0]);
    }

    // --- отклик и то, что сейчас работает ---

    /** Отклик локаций, мс. Живёт в памяти: за время между запусками он всё равно меняется. */
    private static final java.util.Map<String, Integer> PING = new java.util.concurrent.ConcurrentHashMap<>();

    /** Хост, через который в последний раз удалось соединиться. */
    private static volatile String connectedHost = "";

    public static void markConnected(String host) {
        connectedHost = host == null ? "" : host;
    }

    public static boolean isConnected(Location location) {
        return location != null && location.relayHost.equalsIgnoreCase(connectedHost);
    }

    /** Отклик локации в миллисекундах или 0, если ещё не мерили; -1 — не отозвалась. */
    public static int ping(Location location) {
        Integer value = PING.get(location.relayHost);
        return value == null ? 0 : value;
    }

    /**
     * Меряет отклик всех локаций разом и зовёт {@code whenDone} на потоке интерфейса.
     *
     * Меряется время до установки обычного соединения на 443 — до релея по-настоящему, а
     * не до ближайшего узла сети.
     */
    public static void measureAll(Runnable whenDone) {
        final List<Location> locations = all();
        final java.util.concurrent.atomic.AtomicInteger left =
                new java.util.concurrent.atomic.AtomicInteger(locations.size());
        for (Location location : locations) {
            final String host = location.relayHost;
            new Thread(() -> {
                PING.put(host, measure(host));
                if (left.decrementAndGet() == 0 && whenDone != null) {
                    org.telegram.messenger.AndroidUtilities.runOnUIThread(whenDone);
                }
            }, "fg-ping-" + host).start();
        }
        if (locations.isEmpty() && whenDone != null) {
            whenDone.run();
        }
    }

    private static int measure(String host) {
        java.net.Socket socket = new java.net.Socket();
        long started = android.os.SystemClock.elapsedRealtime();
        try {
            socket.connect(new java.net.InetSocketAddress(host, 443), 3000);
            return (int) (android.os.SystemClock.elapsedRealtime() - started);
        } catch (Throwable e) {
            return -1;
        } finally {
            try {
                socket.close();
            } catch (Throwable ignored) {
            }
        }
    }



    private FGBypassLocations() {
    }
}

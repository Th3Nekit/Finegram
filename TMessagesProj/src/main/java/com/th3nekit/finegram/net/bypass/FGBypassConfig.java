/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Обход блокировок: MTProto поверх WebSocket. Своя реализация на
 * наши серверы.
 */

package com.th3nekit.finegram.net.bypass;

import android.content.Context;
import android.content.SharedPreferences;

import org.telegram.messenger.ApplicationLoader;


public final class FGBypassConfig {

    public static final String PREFS_NAME = "finegram_bypass";

    private static volatile SharedPreferences cached;

    public static SharedPreferences prefs() {
        SharedPreferences p = cached;
        if (p == null) {
            synchronized (FGBypassConfig.class) {
                p = cached;
                if (p == null) {
                    p = ApplicationLoader.applicationContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
                    cached = p;
                }
            }
        }
        return p;
    }

    public static SharedPreferences.Editor editor() {
        return prefs().edit();
    }

    public static boolean enabled = loadEnabledWithSentinel();

    private static boolean loadEnabledWithSentinel() {
        SharedPreferences p = prefs();
        if (!p.getBoolean("configured", false)) {
            p.edit().putBoolean("configured", true).putBoolean("enabled", true).apply();
            return true;
        }
        return p.getBoolean("enabled", true);
    }

    public static void setEnabled(boolean v) {
        com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig.mutateRelayState(() -> {
            if (enabled == v) return false;
            enabled = v;
            return true;
        });
        editor().putBoolean("enabled", v).apply();
    }

    public static int localPort = prefs().getInt("local_port", 0);

    /**
     * Порты, которые обход когда-либо занимал.
     *
     * Нужны, чтобы убрать за собой: на каждый порт в списке прокси клиента заводится
     * своя запись, а порт меняется, когда прежний оказался занят. Без этого списка
     * старые записи остаются навсегда и выглядят как дубли. Чужие записи на том же
     * 127.0.0.1 — например плагина — сюда не попадают и не трогаются.
     */
    public static java.util.Set<String> usedLocalPorts() {
        final java.util.Set<String> stored = prefs().getStringSet("used_local_ports", null);
        return stored == null ? new java.util.HashSet<>() : new java.util.HashSet<>(stored);
    }

    /** Забывает порт: его записи уже убраны, и второй раз трогать их нельзя. */
    public static void forgetUsedLocalPort(int v) {
        final java.util.Set<String> used = usedLocalPorts();
        if (used.remove(String.valueOf(v))) {
            editor().putStringSet("used_local_ports", used).apply();
        }
    }

    public static void setLocalPort(int v) {
        localPort = v;
        final java.util.Set<String> used = usedLocalPorts();
        if (v > 0 && used.add(String.valueOf(v))) {
            // Больше дюжины держать незачем: это след, а не история.
            while (used.size() > 12) {
                used.remove(used.iterator().next());
            }
            editor().putStringSet("used_local_ports", used).apply();
        }
        editor().putInt("local_port", v).apply();
    }

    /**
     * Выключать обычный прокси клиента, пока работает VPN.
     *
     * Речь не о встроенном обходе — у него своё правило рядом. Это про прокси из
     * настроек Telegram: заворачивать его в VPN обычно незачем.
     */
    public static boolean disableProxyOnVpn = prefs().getBoolean("disable_proxy_on_vpn", false);

    public static void setDisableProxyOnVpn(boolean v) {
        disableProxyOnVpn = v;
        editor().putBoolean("disable_proxy_on_vpn", v).apply();
    }

    /** Был ли прокси включён до того, как мы его выключили из-за VPN. */
    public static boolean proxyWasOnBeforeVpn = prefs().getBoolean("proxy_was_on_before_vpn", false);

    public static void setProxyWasOnBeforeVpn(boolean v) {
        proxyWasOnBeforeVpn = v;
        editor().putBoolean("proxy_was_on_before_vpn", v).apply();
    }

    public static boolean suspendOnVpn = prefs().getBoolean("suspend_on_vpn", true);

    public static void setSuspendOnVpn(boolean v) {
        com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig.mutateRelayState(() -> {
            if (suspendOnVpn == v) return false;
            suspendOnVpn = v;
            return true;
        });
        editor().putBoolean("suspend_on_vpn", v).apply();
    }

    /**
     * Через что ходит обход.
     *
     * Дорог две, и выбор между ними — единственное, что здесь решает человек. По
     * умолчанию наш сервер: замеры полного обмена с датацентром дают у него медиану около
     * 600 мс против полутора секунд у площадок, и так у всех восьми проверенных. Площадки
     * общие и бесплатные, за них держится весь чужой проект, поэтому ровного времени от
     * них ждать неоткуда. Ценны они другим — тем, что живут без нас.
     *
     * Выбор не запрещает вторую дорогу, а говорит, какую пробовать первой: если выбранная
     * замолчит, клиент сам уйдёт на другую, а не оставит человека без связи.
     */
    public static final int SOURCE_TGWS = 0;
    public static final int SOURCE_RELAY = 1;

    public static int source = loadSource();

    /**
     * До 0.1.54 выбор был из четырёх порядков и двух выключателей. Сводим его к двум
     * дорогам: у кого стояло «только наш сервер» или «сначала наш», тот получает наш.
     */
    private static int loadSource() {
        SharedPreferences p = prefs();
        if (p.contains("source")) {
            return p.getInt("source", SOURCE_RELAY) == SOURCE_RELAY ? SOURCE_RELAY : SOURCE_TGWS;
        }
        if (p.contains("route")) {
            final int old = p.getInt("route", 0);
            final int migrated = (old == 0 || old == 2) ? SOURCE_RELAY : SOURCE_TGWS;
            p.edit().putInt("source", migrated).remove("route")
                    .remove("tgws_direct").remove("tgws_cloudflare").apply();
            return migrated;
        }
        return SOURCE_RELAY;
    }

    public static void setSource(int v) {
        source = v == SOURCE_RELAY ? SOURCE_RELAY : SOURCE_TGWS;
        editor().putInt("source", source).apply();
    }

    public static boolean tgwsFirst() {
        return source == SOURCE_TGWS;
    }

    /**
     * Датацентр, через который ходили в прошлый раз.
     *
     * Нужен одному: на старте подготовить соединение до того, как оно понадобится. Номер
     * датацентра у человека не меняется годами, так что догадка почти всегда верна, а
     * ошибиться нестрашно — лишнее соединение просто протухнет в запасе.
     */
    public static int lastDc = prefs().getInt("last_dc", 0);

    public static void rememberDc(int dc) {
        if (dc <= 0 || dc == lastDc) return;
        lastDc = dc;
        editor().putInt("last_dc", dc).apply();
    }

    public static String mtprotoSecret = loadOrGenerateSecret();

    private static String loadOrGenerateSecret() {
        SharedPreferences p = prefs();
        String s = p.getString("mtproto_secret", "");
        if (MtprotoHandshake.validSecretHex(s) != null) {
            return MtprotoHandshake.validSecretHex(s);
        }
        String gen = MtprotoHandshake.generateSecretHex();
        p.edit().putString("mtproto_secret", gen).apply();
        return gen;
    }

    public static void setMtprotoSecret(String v) {
        String norm = MtprotoHandshake.validSecretHex(v);
        if (norm == null) {
            
            return;
        }
        mtprotoSecret = norm;
        editor().putString("mtproto_secret", norm).apply();
    }

    private FGBypassConfig() {}
}

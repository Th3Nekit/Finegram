/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.proxy;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.util.Base64;

import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.R;
import org.telegram.messenger.SharedConfig;

import com.th3nekit.finegram.net.bypass.FGBypassConfig;
import com.th3nekit.finegram.net.bypass.WsBypassCore;
import com.th3nekit.finegram.net.outbound.FGSubscriptionManager;
import org.telegram.proxy.ProxySettings;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Locale;
import java.util.regex.Pattern;

/**
 * Список прокси для плагинов: как записи называются и подписываются, и что с ними делать.
 *
 * Список зовёт подписи для каждой строки, а плагины перехватывают их — так они дают своим
 * соединениям человеческие имена вместо «адрес:порт». Плагины-менеджеры прокси работают
 * со списком через объект getInstance(): добавляют, удаляют, выбирают текущий и называют.
 * Имена храним отдельно от самих прокси, по «адрес:порт».
 */
public final class ProxyController {

    private static final ProxyController INSTANCE = new ProxyController();
    private static final String PREFS = "finegram_proxy_names";
    private static final Pattern IP_ADDRESS = Pattern.compile("(?:\\d{1,3}\\.){3}\\d{1,3}");

    public static ProxyController getInstance() {
        return INSTANCE;
    }

    /** Имя сервера подписки не зависит от адреса подключения. */
    public static String getDisplayName(SharedConfig.ProxyInfo info) {
        if (info == null) {
            return "";
        }
        if (isOwnBypass(info)) {
            return LocaleController.getString(R.string.FG_Bypass);
        }
        final String custom = INSTANCE.getName(info);
        if (info.settings.getType() == ProxySettings.Type.OUTBOUND) {
            String name = outboundName(info, custom);
            if (name == null) name = outboundName(info, FGSubscriptionManager.getInstance().serverName(info));
            if (name == null) name = outboundName(info, Uri.parse(info.settings.getSecret()).getFragment());
            return name == null ? getProxyTypeName(info) : name;
        }
        if (custom != null && !custom.isEmpty()) {
            return custom;
        }
        return info.settings.getAddress() + ":" + info.settings.getPort();
    }

    private static String outboundName(SharedConfig.ProxyInfo info, String title) {
        if (title == null) return null;
        String name = title.replaceAll("[\\p{Cntrl}]", "").trim();
        String lower = name.toLowerCase(Locale.ROOT);
        String host = info.settings.getAddress().toLowerCase(Locale.ROOT);
        String credentials = Uri.parse(info.settings.getSecret()).getUserInfo();
        if (name.isEmpty() || name.contains("://") || IP_ADDRESS.matcher(name).find()
                || !host.isEmpty() && lower.contains(host)
                || credentials != null && !credentials.isEmpty() && name.contains(credentials)) return null;
        return name.substring(0, name.offsetByCodePoints(0, Math.min(128, name.codePointCount(0, name.length()))));
    }

    public static boolean canShareProxy(SharedConfig.ProxyInfo info) {
        return info != null && info.settings.getType() != ProxySettings.Type.OUTBOUND;
    }

    /** Чем это соединение является: MTProto, SOCKS или наш обход. */
    public static String getProxyTypeName(SharedConfig.ProxyInfo info) {
        if (info == null) {
            return "";
        }
        if (isOwnBypass(info)) {
            return LocaleController.getString(R.string.FG_Bypass);
        }
        if (info.settings.getType() == org.telegram.proxy.ProxySettings.Type.OUTBOUND) {
            String link = info.settings.getSecret();
            return link != null && link.regionMatches(true, 0, "vless://", 0, 8) ? "VLESS" : "Hysteria2";
        }
        if (info.settings.getType() == org.telegram.proxy.ProxySettings.Type.WEB) {
            return "WebSocket";
        }
        if (info.settings.getSecret() != null && !info.settings.getSecret().isEmpty()) {
            return "MTProto";
        }
        return "SOCKS5";
    }

    /** Сохранение записи: плагины перехватывают, чтобы дописать в неё своё. */
    public static SharedConfig.ProxyInfo saveProxy(SharedConfig.ProxyInfo info,
                                                   boolean enable, boolean forCalls) {
        if (info == null) {
            return null;
        }
        SharedConfig.ProxyInfo added = SharedConfig.addProxy(info);
        if (enable) {
            SharedConfig.currentProxy = added != null ? added : info;
        }
        SharedConfig.saveProxyList();
        return added != null ? added : info;
    }

    private static boolean isOwnBypass(SharedConfig.ProxyInfo info) {
        return WsBypassCore.LOCAL_PROXY_HOST.equals(info.settings.getAddress())
                && info.settings.getPort() == FGBypassConfig.localPort;
    }

    // --- список ---

    public ArrayList<SharedConfig.ProxyInfo> getProxyList() {
        return SharedConfig.proxyList;
    }

    public SharedConfig.ProxyInfo getCurrentProxy() {
        return SharedConfig.currentProxy;
    }

    /** Сделать прокси текущим и сразу переподключиться через него. */
    public void setCurrentProxy(SharedConfig.ProxyInfo info) {
        SharedConfig.currentProxy = info;
        SharedConfig.saveProxyList();
        if (info != null) {
            org.telegram.tgnet.ConnectionsManager.setProxySettings(true, info.settings);
        }
        NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.proxySettingsChanged);
    }

    public SharedConfig.ProxyInfo addProxy(SharedConfig.ProxyInfo info) {
        if (info == null) {
            return null;
        }
        final SharedConfig.ProxyInfo added = SharedConfig.addProxy(info);
        NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.proxySettingsChanged);
        return added;
    }

    public void deleteProxy(SharedConfig.ProxyInfo info) {
        if (info == null) {
            return;
        }
        SharedConfig.deleteProxy(info);
        preferences().edit().remove(key(info)).apply();
        NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.proxySettingsChanged);
    }

    public void removeProxy(SharedConfig.ProxyInfo info) {
        deleteProxy(info);
    }

    public void saveProxyList() {
        SharedConfig.saveProxyList();
    }

    public void loadProxyList() {
        SharedConfig.loadProxyList();
    }

    public String buildShareLink(SharedConfig.ProxyInfo info) {
        return canShareProxy(info) ? info.getLink() : null;
    }

    public String buildShareLink(SharedConfig.ProxyInfo info, String ignored) {
        return buildShareLink(info);
    }

    // --- имена ---

    public String getName(SharedConfig.ProxyInfo info) {
        if (info == null) return null;
        String key = key(info);
        return preferences().contains(key) ? getName(key)
                : getName(info.settings.getAddress() + ":" + info.settings.getPort());
    }

    public String getName(String key) {
        if (key == null) {
            return null;
        }
        final String stored = preferences().getString(key, null);
        return stored == null ? null : decode(stored);
    }

    public void setName(SharedConfig.ProxyInfo info, String name) {
        if (info != null) {
            preferences().edit().putString(key(info), encode(name == null ? "" : name.trim())).apply();
            NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.proxySettingsChanged);
        }
    }

    public void setName(String key, String name) {
        if (key == null) {
            return;
        }
        if (name == null || name.isEmpty()) {
            preferences().edit().remove(key).apply();
        } else {
            preferences().edit().putString(key, encode(name)).apply();
        }
        NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.proxySettingsChanged);
    }

    private static String key(SharedConfig.ProxyInfo info) {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            String[] values = {info.settings.getType().name(), info.settings.getAddress(),
                    Integer.toString(info.settings.getPort()), info.settings.getUser(),
                    info.settings.getPassword(), info.settings.getSecret()};
            for (String value : values) {
                byte[] bytes = (value == null ? "" : value).getBytes(StandardCharsets.UTF_8);
                digest.update(Integer.toString(bytes.length).getBytes(StandardCharsets.UTF_8));
                digest.update((byte) ':');
                digest.update(bytes);
            }
            return "proxy:" + Base64.encodeToString(digest.digest(), Base64.NO_WRAP);
        } catch (java.security.NoSuchAlgorithmException e) {
            throw new IllegalStateException(e);
        }
    }

    private static SharedPreferences preferences() {
        return ApplicationLoader.applicationContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    /** Имена лежат в base64: в них бывают символы, которые ломают ключ-значение. */
    private static String encode(String value) {
        return Base64.encodeToString(value.getBytes(StandardCharsets.UTF_8), Base64.NO_WRAP);
    }

    private static String decode(String value) {
        try {
            return new String(Base64.decode(value, Base64.NO_WRAP), StandardCharsets.UTF_8);
        } catch (IllegalArgumentException e) {
            return value;
        }
    }

    /** Имена читаются по требованию; метод оставлен для тех, кто зовёт его перехватом. */
    private void load() {
    }

    private ProxyController() {
    }
}

/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.outbound;

import org.json.JSONArray;
import org.json.JSONObject;
import org.json.JSONTokener;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;

final class FGSubscriptionXray {
    static FGSubscriptionParser.Parsed parse(String text) throws FGSubscriptionError {
        try {
            checkDepth(text);
            JSONTokener tokener = new JSONTokener(text);
            Object root = tokener.nextValue();
            if (tokener.nextClean() != 0) fail(FGSubscriptionError.Code.INVALID_FORMAT);
            JSONArray configs = root instanceof JSONArray ? (JSONArray) root : new JSONArray().put(root);
            if (configs.length() > FGSubscriptionParser.MAX_SERVERS) fail(FGSubscriptionError.Code.TOO_MANY_SERVERS);
            LinkedHashMap<String, FGSubscriptionParser.Server> servers = new LinkedHashMap<>();
            int count = 0;
            int skipped = 0;
            for (int i = 0; i < configs.length(); i++) {
                JSONObject config = configs.getJSONObject(i);
                JSONArray outbounds = config.optJSONArray("outbounds");
                if (outbounds == null) {
                    if (!config.has("protocol")) fail(FGSubscriptionError.Code.UNSUPPORTED_FORMAT);
                    outbounds = new JSONArray().put(config);
                }
                if (outbounds.length() > FGSubscriptionParser.MAX_SERVERS) fail(FGSubscriptionError.Code.TOO_MANY_SERVERS);
                for (int j = 0; j < outbounds.length(); j++) {
                    JSONObject outbound = outbounds.getJSONObject(j);
                    String protocol = string(outbound, "protocol", "").toLowerCase(Locale.ROOT);
                    if (Arrays.asList("freedom", "blackhole", "dns").contains(protocol)) continue;
                    String title = string(config, "remarks", string(outbound, "tag", ""));
                    FGSubscriptionError.Code rejection = FGSubscriptionParser.rejectionName(title);
                    if (rejection != null) fail(rejection);
                    if (!Arrays.asList("vless", "hysteria", "hysteria2", "hy2").contains(protocol)) {
                        if (!Arrays.asList("vmess", "trojan", "shadowsocks", "socks", "http", "wireguard").contains(protocol)) fail(FGSubscriptionError.Code.UNSUPPORTED_FORMAT);
                        if (++count > FGSubscriptionParser.MAX_SERVERS) fail(FGSubscriptionError.Code.TOO_MANY_SERVERS);
                        skipped++;
                        continue;
                    }
                    for (String link : links(outbound, title)) {
                        if (++count > FGSubscriptionParser.MAX_SERVERS) fail(FGSubscriptionError.Code.TOO_MANY_SERVERS);
                        FGSubscriptionParser.Server server = FGSubscriptionParser.server(link);
                        servers.putIfAbsent(server.id, server);
                    }
                }
            }
            if (servers.isEmpty()) fail(skipped > 0 ? FGSubscriptionError.Code.UNSUPPORTED_PROTOCOL : FGSubscriptionError.Code.NO_SERVERS);
            return new FGSubscriptionParser.Parsed(new ArrayList<>(servers.values()), skipped, "");
        } catch (FGSubscriptionError error) {
            throw error;
        } catch (Exception ignored) {
            throw new FGSubscriptionError(FGSubscriptionError.Code.INVALID_FORMAT);
        }
    }

    private static List<String> links(JSONObject outbound, String title) throws Exception {
        keys(outbound, "protocol", "settings", "streamSettings", "tag", "remarks");
        String protocol = string(outbound, "protocol", "").toLowerCase(Locale.ROOT);
        JSONObject settings = outbound.getJSONObject("settings");
        JSONObject stream = object(outbound, "streamSettings");
        boolean vless = "vless".equals(protocol);
        String network = string(stream, "network", string(stream, "method", vless ? "tcp" : "hysteria"));
        LinkedHashMap<String, String> params = new LinkedHashMap<>();
        String security = string(stream, "security", vless ? "none" : "tls");
        String tlsKey = "reality".equals(security) ? "realitySettings" : "tlsSettings";
        String transportKey;
        switch (network) {
            case "ws": transportKey = "wsSettings"; break;
            case "httpupgrade": transportKey = "httpupgradeSettings"; break;
            case "grpc": transportKey = "grpcSettings"; break;
            case "http": case "h2": transportKey = "httpSettings"; break;
            case "tcp": transportKey = "tcpSettings"; break;
            case "raw": transportKey = "rawSettings"; break;
            case "hysteria": case "hysteria2": transportKey = "hysteriaSettings"; break;
            default: fail(FGSubscriptionError.Code.UNSUPPORTED_LINK); return null;
        }
        keys(stream, "network", "method", "security", tlsKey, transportKey);
        if (stream.has("method") && stream.has("network") && !stream.getString("method").equals(network)) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
        if (!Arrays.asList("none", "tls", "reality").contains(security) || (!vless && !"tls".equals(security))) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
        JSONObject tls = object(stream, tlsKey);
        if ("none".equals(security) && tls.length() != 0) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
        if (vless) params.put("security", security);
        keys(tls, "serverName", "allowInsecure", "alpn", "fingerprint", "publicKey", "password", "shortId");
        put(params, "sni", string(tls, "serverName", ""));
        if (tls.has("allowInsecure")) params.put("insecure", bool(tls, "allowInsecure") ? "1" : "0");
        if (tls.has("alpn")) put(params, "alpn", stringList(tls.get("alpn")));
        if (vless) {
            put(params, "fp", string(tls, "fingerprint", ""));
            if ("reality".equals(security)) {
                String key = string(tls, "publicKey", string(tls, "password", ""));
                if (tls.has("publicKey") && tls.has("password") && !tls.getString("password").equals(key)) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                put(params, "pbk", key);
                put(params, "sid", string(tls, "shortId", ""));
            } else if (tls.has("publicKey") || tls.has("password") || tls.has("shortId")) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
            params.put("type", network);
            transport(network, object(stream, transportKey), params);
        } else {
            if (tls.has("fingerprint") || tls.has("publicKey") || tls.has("password") || tls.has("shortId")) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
            if (!network.equals("hysteria") && !network.equals("hysteria2")) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
        }
        List<String> result = new ArrayList<>();
        if (vless) {
            keys(settings, "vnext", "address", "port", "id", "encryption", "flow");
            if (settings.has("vnext") && settings.length() != 1) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
            JSONArray destinations = settings.has("vnext") ? settings.getJSONArray("vnext") : new JSONArray().put(settings);
            if (destinations.length() == 0 || destinations.length() > FGSubscriptionParser.MAX_SERVERS) fail(FGSubscriptionError.Code.INVALID_LINK);
            for (int i = 0; i < destinations.length(); i++) {
                JSONObject destination = destinations.getJSONObject(i);
                if (settings.has("vnext")) keys(destination, "address", "port", "users");
                JSONArray users = destination.has("users") ? destination.getJSONArray("users") : new JSONArray().put(destination);
                if (users.length() == 0 || users.length() > FGSubscriptionParser.MAX_SERVERS) fail(FGSubscriptionError.Code.INVALID_LINK);
                for (int j = 0; j < users.length(); j++) {
                    if (result.size() >= FGSubscriptionParser.MAX_SERVERS) fail(FGSubscriptionError.Code.TOO_MANY_SERVERS);
                    JSONObject user = users.getJSONObject(j);
                    if (destination.has("users")) keys(user, "id", "encryption", "flow", "email", "level");
                    LinkedHashMap<String, String> query = new LinkedHashMap<>(params);
                    query.put("encryption", string(user, "encryption", "none"));
                    put(query, "flow", string(user, "flow", ""));
                    result.add(link("vless", string(user, "id", ""), destination, query, title));
                }
            }
        } else {
            keys(settings, "address", "port", "version", "sni", "allowInsecure");
            JSONObject hy = object(stream, "hysteriaSettings");
            keys(hy, "version", "auth", "sni", "obfs", "obfsParam");
            if (hy.has("version") && integer(hy, "version") != 2 || settings.has("version") && integer(settings, "version") != 2
                    || protocol.equals("hysteria") && !hy.has("version") && !settings.has("version")) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
            for (JSONObject candidate : Arrays.asList(settings, hy)) {
                String name = string(candidate, "sni", "");
                if (!name.isEmpty()) {
                    if (params.containsKey("sni") && !params.get("sni").equals(name)) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                    params.put("sni", name);
                }
            }
            if (settings.has("allowInsecure")) {
                String insecure = bool(settings, "allowInsecure") ? "1" : "0";
                if (params.containsKey("insecure") && !params.get("insecure").equals(insecure)) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                params.put("insecure", insecure);
            }
            if (hy.has("obfs") || hy.has("obfsParam")) {
                JSONObject obfs = object(hy, "obfs");
                keys(obfs, "type", "password");
                params.put("obfs", string(obfs, "type", "salamander"));
                params.put("obfs-password", string(obfs, "password", string(hy, "obfsParam", "")));
                if (obfs.has("password") && hy.has("obfsParam") && !hy.getString("obfsParam").equals(params.get("obfs-password"))) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
            }
            result.add(link("hy2", string(hy, "auth", ""), settings, params, title));
        }
        return result;
    }

    private static void transport(String network, JSONObject object, LinkedHashMap<String, String> params) throws Exception {
        switch (network) {
            case "tcp": case "raw":
                keys(object, "header");
                JSONObject header = object(object, "header");
                keys(header, "type");
                if (!"none".equals(string(header, "type", "none"))) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                break;
            case "ws":
                keys(object, "path", "host", "headers");
                JSONObject headers = object(object, "headers");
                keys(headers, "Host", "host");
                String host = string(headers, "Host", string(headers, "host", string(object, "host", "")));
                if (headers.has("Host") && headers.has("host") && !headers.getString("host").equals(host)
                        || object.has("host") && !object.getString("host").equals(host)) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                put(params, "host", host);
                put(params, "path", string(object, "path", "/"));
                break;
            case "httpupgrade":
                keys(object, "path", "host");
                put(params, "host", string(object, "host", ""));
                put(params, "path", string(object, "path", "/"));
                break;
            case "http": case "h2":
                keys(object, "path", "host");
                if (object.has("host")) put(params, "host", stringList(object.get("host")));
                put(params, "path", string(object, "path", "/"));
                break;
            case "grpc":
                keys(object, "serviceName", "multiMode");
                if (object.has("multiMode") && bool(object, "multiMode")) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                put(params, "serviceName", string(object, "serviceName", ""));
                break;
            default: fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
        }
    }

    private static String link(String scheme, String auth, JSONObject endpoint, LinkedHashMap<String, String> params, String title) throws Exception {
        String host = string(endpoint, "address", "");
        if (host.startsWith("[") || host.matches(".*[\\s/?#@].*")) fail(FGSubscriptionError.Code.INVALID_LINK);
        StringBuilder result = new StringBuilder(scheme).append("://").append(encode(auth)).append('@')
                .append(host.contains(":") ? "[" + host + "]" : host).append(':').append(integer(endpoint, "port")).append("/?");
        for (String key : params.keySet()) result.append(encode(key)).append('=').append(encode(params.get(key))).append('&');
        return result.append('#').append(encode(title)).toString();
    }

    private static JSONObject object(JSONObject parent, String key) throws Exception {
        return parent.has(key) ? parent.getJSONObject(key) : new JSONObject();
    }

    private static String string(JSONObject object, String key, String fallback) throws Exception {
        if (!object.has(key)) return fallback;
        Object value = object.get(key);
        if (!(value instanceof String)) fail(FGSubscriptionError.Code.INVALID_FORMAT);
        return (String) value;
    }

    private static boolean bool(JSONObject object, String key) throws Exception {
        Object value = object.get(key);
        if (!(value instanceof Boolean)) fail(FGSubscriptionError.Code.INVALID_FORMAT);
        return Boolean.TRUE.equals(value);
    }

    private static int integer(JSONObject object, String key) throws Exception {
        Object value = object.get(key);
        if (!(value instanceof Integer) && !(value instanceof Long)) fail(FGSubscriptionError.Code.INVALID_FORMAT);
        long number = ((Number) value).longValue();
        if (number < Integer.MIN_VALUE || number > Integer.MAX_VALUE) fail(FGSubscriptionError.Code.INVALID_FORMAT);
        return (int) number;
    }

    private static String stringList(Object value) throws Exception {
        if (value instanceof String) return (String) value;
        if (!(value instanceof JSONArray)) fail(FGSubscriptionError.Code.INVALID_FORMAT);
        JSONArray array = (JSONArray) value;
        StringBuilder result = new StringBuilder();
        for (int i = 0; i < array.length(); i++) {
            Object item = array.get(i);
            if (!(item instanceof String) || ((String) item).contains(",") || ((String) item).isEmpty()) fail(FGSubscriptionError.Code.INVALID_FORMAT);
            if (i > 0) result.append(',');
            result.append(item);
        }
        return result.toString();
    }

    private static void keys(JSONObject object, String... allowed) throws FGSubscriptionError {
        List<String> names = Arrays.asList(allowed);
        Iterator<String> iterator = object.keys();
        while (iterator.hasNext()) if (!names.contains(iterator.next())) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
    }

    private static void put(LinkedHashMap<String, String> params, String key, String value) {
        if (!value.isEmpty()) params.put(key, value);
    }

    private static String encode(String value) throws Exception {
        return URLEncoder.encode(value, "UTF-8").replace("+", "%20");
    }

    static void checkDepth(String text) throws FGSubscriptionError {
        int depth = 0;
        boolean quoted = false;
        boolean escaped = false;
        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i);
            if (escaped) { escaped = false; continue; }
            if (quoted && c == '\\') { escaped = true; continue; }
            if (c == '"') quoted = !quoted;
            if (quoted) continue;
            if (c == '[' || c == '{') if (++depth > 32) fail(FGSubscriptionError.Code.INVALID_FORMAT);
            if (c == ']' || c == '}') if (--depth < 0) fail(FGSubscriptionError.Code.INVALID_FORMAT);
        }
        if (depth != 0 || quoted) fail(FGSubscriptionError.Code.INVALID_FORMAT);
    }

    private static void fail(FGSubscriptionError.Code code) throws FGSubscriptionError {
        FGSubscriptionParser.fail(code);
    }
}

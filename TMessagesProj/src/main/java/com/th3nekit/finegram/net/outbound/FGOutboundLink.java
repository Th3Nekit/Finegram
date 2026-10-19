/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.outbound;

import android.net.Uri;
import android.text.TextUtils;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.Locale;

public final class FGOutboundLink {

    public static final class Parsed {
        public final String kind;
        public final String host;
        public final int port;
        public final String title;

        public final JSONObject outbound;

        Parsed(String kind, String host, int port, String title, JSONObject outbound) {
            this.kind = kind;
            this.host = host;
            this.port = port;
            this.title = title;
            this.outbound = outbound;
        }
    }

    public static final String KIND_VLESS = "vless";
    public static final String KIND_HYSTERIA2 = "hysteria2";

    private FGOutboundLink() {
    }

    public static boolean looksLikeLink(String text) {
        if (text == null) return false;
        final String s = text.trim().toLowerCase(Locale.ROOT);
        return s.startsWith("vless://") || s.startsWith("hy2://") || s.startsWith("hysteria2://");
    }

    public static Parsed parse(String link) {
        if (!looksLikeLink(link)) return null;
        try {
            final String raw = link.trim();
            final Uri uri = Uri.parse(raw);
            final String scheme = lower(uri.getScheme());
            String host = uri.getHost();
            if (host != null && host.startsWith("[") && host.endsWith("]")) {
                host = host.substring(1, host.length() - 1);
            }
            final int port = uri.getPort() == -1 ? 443 : uri.getPort();
            if (TextUtils.isEmpty(host) || port <= 0 || port > 65535) return null;

            final String userInfo = uri.getUserInfo();
            if (TextUtils.isEmpty(userInfo)) return null;

            final String title = TextUtils.isEmpty(uri.getFragment())
                    ? host : uri.getFragment();

            if ("vless".equals(scheme)) {
                return new Parsed(KIND_VLESS, host, port, title, vless(uri, host, port, userInfo));
            }
            return new Parsed(KIND_HYSTERIA2, host, port, title, hysteria2(uri, host, port, userInfo));
        } catch (Throwable ignored) {
            return null;
        }
    }

    private static JSONObject vless(Uri uri, String host, int port, String uuid) throws Exception {
        final String encryption = param(uri, "encryption");
        if (encryption != null && !"none".equals(encryption)) {
            throw new IllegalArgumentException("unsupported encryption");
        }
        final String security = lower(param(uri, "security"));
        if (!TextUtils.isEmpty(security) && !"none".equals(security)
                && !"tls".equals(security) && !"reality".equals(security)) {
            throw new IllegalArgumentException("unsupported security");
        }
        final JSONObject out = new JSONObject();
        out.put("type", "vless");
        out.put("tag", "out");
        out.put("server", host);
        out.put("server_port", port);
        out.put("uuid", uuid);

        final String flow = param(uri, "flow");
        if (!TextUtils.isEmpty(flow)) out.put("flow", flow);

        final boolean reality = "reality".equals(security);
        if (reality || "tls".equals(security)) {
            final JSONObject tls = new JSONObject();
            tls.put("enabled", true);
            String sni = firstOf(uri, "sni", "peer");
            if (TextUtils.isEmpty(sni)) sni = splitCommaValues(param(uri, "host")).optString(0);
            tls.put("server_name", TextUtils.isEmpty(sni) ? host : sni);
            if (truthy(firstOf(uri, "allowInsecure", "insecure"))) {
                tls.put("insecure", true);
            }
            final String alpn = param(uri, "alpn");
            if (!TextUtils.isEmpty(alpn)) tls.put("alpn", splitCommaValues(alpn));

            final JSONObject utls = new JSONObject();
            utls.put("enabled", true);
            final String fp = param(uri, "fp");
            utls.put("fingerprint", TextUtils.isEmpty(fp) ? "chrome" : fp);
            tls.put("utls", utls);

            if (reality) {
                final JSONObject r = new JSONObject();
                r.put("enabled", true);
                r.put("public_key", param(uri, "pbk"));
                final String sid = param(uri, "sid");
                if (!TextUtils.isEmpty(sid)) r.put("short_id", sid);
                tls.put("reality", r);
            }
            out.put("tls", tls);
        }

        final JSONObject transport = transport(uri, host);
        if (transport != null) out.put("transport", transport);
        return out;
    }

    private static JSONObject transport(Uri uri, String host) throws Exception {
        final String type = lower(param(uri, "type"));
        if (TextUtils.isEmpty(type) || "tcp".equals(type) || "raw".equals(type)) {
            final String header = lower(param(uri, "headerType"));
            if (!TextUtils.isEmpty(header) && !"none".equals(header)) {
                throw new IllegalArgumentException("unsupported TCP header");
            }
            return null;
        }

        final JSONObject t = new JSONObject();
        if ("ws".equals(type) || "httpupgrade".equals(type)) {
            t.put("type", type);
            final String path = param(uri, "path");
            t.put("path", TextUtils.isEmpty(path) ? "/" : path);
            if ("ws".equals(type)) applyWebSocketEarlyData(t);
            final String hostHeader = firstOf(uri, "host", "sni");
            if (!TextUtils.isEmpty(hostHeader)) {
                if ("httpupgrade".equals(type)) {
                    t.put("host", hostHeader);
                } else {
                    final JSONObject headers = new JSONObject();
                    headers.put("Host", hostHeader);
                    t.put("headers", headers);
                }
            }
            return t;
        }
        if ("http".equals(type) || "h2".equals(type)) {
            t.put("type", "http");
            final String path = param(uri, "path");
            t.put("path", TextUtils.isEmpty(path) ? "/" : path);
            final String hostHeader = param(uri, "host");
            if (!TextUtils.isEmpty(hostHeader)) t.put("host", splitCommaValues(hostHeader));
            return t;
        }
        if ("grpc".equals(type)) {
            t.put("type", "grpc");
            final String service = firstOf(uri, "serviceName", "path");
            if (!TextUtils.isEmpty(service)) t.put("service_name", service);
            return t;
        }
        throw new IllegalArgumentException("unsupported transport");
    }

    private static void applyWebSocketEarlyData(JSONObject transport) throws Exception {
        final String path = transport.getString("path");
        final int fragment = path.indexOf('#');
        final int end = fragment < 0 ? path.length() : fragment;
        final int query = path.indexOf('?');
        if (query < 0 || query >= end) return;
        StringBuilder retained = new StringBuilder();
        boolean found = false;
        boolean retainedPart = false;
        for (String part : path.substring(query + 1, end).split("&", -1)) {
            final int equals = part.indexOf('=');
            final String name = Uri.decode(equals < 0 ? part : part.substring(0, equals));
            if (!"ed".equals(name)) {
                if (retainedPart) retained.append('&');
                retained.append(part);
                retainedPart = true;
                continue;
            }
            if (found || equals < 0) throw new IllegalArgumentException("invalid early data");
            final String value = Uri.decode(part.substring(equals + 1));
            if (!value.matches("[0-9]+")) throw new IllegalArgumentException("invalid early data");
            final long size = Long.parseLong(value);
            if (size > 0xffffffffL) throw new IllegalArgumentException("invalid early data");
            transport.put("max_early_data", size);
            transport.put("early_data_header_name", "Sec-WebSocket-Protocol");
            found = true;
        }
        if (found) {
            transport.put("path", path.substring(0, query)
                    + (retained.length() == 0 ? "" : "?" + retained)
                    + path.substring(end));
        }
    }

    private static JSONObject hysteria2(Uri uri, String host, int port, String password) throws Exception {
        final JSONObject out = new JSONObject();
        out.put("type", "hysteria2");
        out.put("tag", "out");
        out.put("server", host);
        out.put("server_port", port);
        out.put("password", password);

        final JSONObject tls = new JSONObject();
        tls.put("enabled", true);
        final String sni = firstOf(uri, "sni", "peer");
        tls.put("server_name", TextUtils.isEmpty(sni) ? host : sni);
        if (truthy(firstOf(uri, "insecure", "allowInsecure"))) {
            tls.put("insecure", true);
        }
        final String alpn = param(uri, "alpn");
        if (!TextUtils.isEmpty(alpn)) tls.put("alpn", splitCommaValues(alpn));
        out.put("tls", tls);

        final String obfs = param(uri, "obfs");
        if ("salamander".equalsIgnoreCase(obfs)) {
            final JSONObject o = new JSONObject();
            o.put("type", "salamander");
            o.put("password", param(uri, "obfs-password"));
            out.put("obfs", o);
        }
        return out;
    }

    public static String config(Parsed parsed, FGOutboundCredentials credentials) {
        if (parsed == null || credentials == null || credentials.port <= 0) return "";
        try {
            final JSONObject root = new JSONObject();

            final JSONObject log = new JSONObject();
            log.put("level", "warn");
            log.put("disabled", true);
            root.put("log", log);

            final JSONObject in = new JSONObject();
            in.put("type", "socks");
            in.put("tag", "in");
            in.put("listen", "127.0.0.1");
            in.put("listen_port", credentials.port);
            final JSONObject user = new JSONObject();
            user.put("username", credentials.username);
            user.put("password", credentials.password);
            in.put("users", new JSONArray().put(user));
            root.put("inbounds", new JSONArray().put(in));

            root.put("outbounds", new JSONArray().put(parsed.outbound));
            JSONObject resolver = new JSONObject();
            resolver.put("type", "local");
            resolver.put("tag", "system");
            root.put("dns", new JSONObject().put("servers", new JSONArray().put(resolver)));
            root.put("route", new JSONObject().put("default_domain_resolver", "system"));
            return root.toString();
        } catch (Throwable t) {
            return "";
        }
    }

    private static String param(Uri uri, String name) {
        try {
            return uri.getQueryParameter(name);
        } catch (Throwable t) {
            return null;
        }
    }

    private static String firstOf(Uri uri, String... names) {
        for (String name : names) {
            final String value = param(uri, name);
            if (!TextUtils.isEmpty(value)) return value;
        }
        return null;
    }

    private static boolean truthy(String value) {
        return "1".equals(value) || "true".equalsIgnoreCase(value);
    }

    private static String lower(String value) {
        return value == null ? "" : value.toLowerCase(Locale.ROOT);
    }

    private static JSONArray splitCommaValues(String value) {
        final JSONArray array = new JSONArray();
        if (TextUtils.isEmpty(value)) return array;
        for (String part : value.split(",")) {
            final String trimmed = part.trim();
            if (!trimmed.isEmpty()) array.put(trimmed);
        }
        return array;
    }

}

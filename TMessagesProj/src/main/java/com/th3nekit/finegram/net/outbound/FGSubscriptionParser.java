/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.outbound;

import android.net.Uri;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.nio.ByteBuffer;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import android.util.Base64;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.zip.GZIPInputStream;

public final class FGSubscriptionParser {
    public static final int MAX_BODY_BYTES = 1024 * 1024;
    public static final int MAX_SERVERS = 256;
    public static final int MAX_LINK_LENGTH = 8192;
    private static final Set<String> VLESS_PARAMS = new HashSet<>(Arrays.asList(
            "encryption", "security", "sni", "peer", "allowInsecure", "insecure", "alpn",
            "fp", "pbk", "sid", "type", "headerType", "host", "path", "serviceName", "flow"));
    private static final Set<String> HY_PARAMS = new HashSet<>(Arrays.asList(
            "sni", "peer", "allowInsecure", "insecure", "alpn", "obfs", "obfs-password"));
    private static final Set<String> OTHER_SCHEMES = new HashSet<>(Arrays.asList(
            "ss", "ssr", "vmess", "trojan", "tuic", "hysteria", "wg", "wireguard",
            "socks", "socks5", "http", "https", "v2rayn", "hysteria2+realm", "realm"));

    public static final class Server {
        public final String id;
        public final String link;
        public final String kind;
        public final String host;
        public final int port;
        public final String title;
        final FGOutboundLink.Parsed parsed;

        private Server(String link, FGOutboundLink.Parsed parsed) throws Exception {
            this.link = link;
            this.parsed = parsed;
            id = digest(canonical(parsed.outbound));
            kind = parsed.kind;
            host = parsed.host;
            port = parsed.port;
            title = parsed.title;
        }
    }

    public static final class Parsed {
        public final List<Server> servers;
        public final int skipped;
        public final String profileTitle;

        Parsed(List<Server> servers, int skipped, String profileTitle) {
            this.servers = Collections.unmodifiableList(new ArrayList<>(servers));
            this.skipped = skipped;
            this.profileTitle = profileTitle;
        }
    }

    private FGSubscriptionParser() {
    }

    public static Parsed parse(byte[] bytes) throws FGSubscriptionError {
        if (bytes == null || bytes.length > MAX_BODY_BYTES) fail(FGSubscriptionError.Code.BODY_TOO_LARGE);
        byte[] current = bytes;
        String text;
        for (int depth = 0; ; depth++) {
            int start = 0;
            while (start < current.length) {
                if (current[start] == ' ' || current[start] == '\r' || current[start] == '\n' || current[start] == '\t') start++;
                else if (current.length - start >= 3 && current[start] == (byte) 0xef
                        && current[start + 1] == (byte) 0xbb && current[start + 2] == (byte) 0xbf) start += 3;
                else break;
            }
            byte[] value = Arrays.copyOfRange(current, start, current.length);
            String prefix = new String(value, 0, Math.min(16, value.length), StandardCharsets.ISO_8859_1).toLowerCase(Locale.ROOT);
            boolean compressed = value.length > 1 && value[0] == (byte) 0x1f && value[1] == (byte) 0x8b;
            if (depth >= 4 && (prefix.startsWith("base64:") || prefix.startsWith("gzip:") || compressed)) fail(FGSubscriptionError.Code.INVALID_FORMAT);
            if (prefix.startsWith("base64:gzip:") || prefix.startsWith("gzip:base64:")) {
                current = gunzip(base64(utf8(Arrays.copyOfRange(value, 12, value.length))));
            } else if (prefix.startsWith("base64:")) {
                current = base64(utf8(Arrays.copyOfRange(value, 7, value.length)));
            } else if (prefix.startsWith("gzip:")) {
                current = gunzip(Arrays.copyOfRange(value, 5, value.length));
            } else if (compressed) {
                current = gunzip(value);
            } else {
                text = utf8(value).replaceFirst("^\ufeff", "").trim();
                if (text.isEmpty()) fail(FGSubscriptionError.Code.NO_SERVERS);
                FGSubscriptionError.Code rejection = rejection(text);
                if (rejection != null) fail(rejection);
                if (text.startsWith("{") || text.startsWith("[")) return FGSubscriptionXray.parse(text);
                String lower = text.toLowerCase(Locale.ROOT);
                if (lower.startsWith("<") || lower.startsWith("proxies:") || lower.startsWith("proxy-providers:")
                        || lower.startsWith("---") || lower.startsWith("mixed-port:")) fail(FGSubscriptionError.Code.UNSUPPORTED_FORMAT);
                if (text.contains("://") || text.startsWith("#")) break;
                if (depth >= 4) fail(FGSubscriptionError.Code.INVALID_FORMAT);
                current = base64(text);
            }
        }
        LinkedHashMap<String, Server> servers = new LinkedHashMap<>();
        int skipped = 0;
        boolean unsupportedTransport = false;
        int count = 0;
        String title = "";
        String[] lines = text.split("\\r?\\n|\\r", -1);
        if (lines.length > 2048) fail(FGSubscriptionError.Code.TOO_MANY_SERVERS);
        for (String raw : lines) {
            String line = raw.trim();
            if (line.isEmpty()) continue;
            if (line.toLowerCase(Locale.ROOT).startsWith("#profile-title:")) {
                title = displayTitle(line.substring(15));
                continue;
            }
            if (line.startsWith("#") || line.startsWith("//")) continue;
            if (++count > MAX_SERVERS) fail(FGSubscriptionError.Code.TOO_MANY_SERVERS);
            int separator = line.indexOf("://");
            if (separator <= 0) fail(FGSubscriptionError.Code.INVALID_FORMAT);
            String scheme = line.substring(0, separator).toLowerCase(Locale.ROOT);
            if (OTHER_SCHEMES.contains(scheme)) {
                String fragment = Uri.parse(line).getFragment();
                FGSubscriptionError.Code rejection = fragment == null ? null : rejectionName(fragment);
                if (rejection != null) fail(rejection);
                skipped++;
                continue;
            }
            if (!FGOutboundLink.looksLikeLink(line)) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
            try {
                Server server = server(line);
                servers.putIfAbsent(server.id, server);
            } catch (FGSubscriptionError error) {
                if (error.code != FGSubscriptionError.Code.UNSUPPORTED_LINK) throw error;
                unsupportedTransport = true;
                skipped++;
            }
        }
        if (servers.isEmpty()) fail(unsupportedTransport ? FGSubscriptionError.Code.UNSUPPORTED_LINK
                : skipped > 0 ? FGSubscriptionError.Code.UNSUPPORTED_PROTOCOL : FGSubscriptionError.Code.NO_SERVERS);
        return new Parsed(new ArrayList<>(servers.values()), skipped, title);
    }

    static Server server(String link) throws FGSubscriptionError {
        try {
            if (link.length() > MAX_LINK_LENGTH) fail(FGSubscriptionError.Code.INVALID_LINK);
            URI strict = new URI(link);
            Uri uri = Uri.parse(link);
            boolean vless = "vless".equalsIgnoreCase(uri.getScheme());
            String authority = strict.getRawAuthority();
            if (authority == null || authority.indexOf('@') <= 0 || authority.indexOf('@') != authority.lastIndexOf('@')) fail(FGSubscriptionError.Code.INVALID_LINK);
            String endpoint = authority.substring(authority.indexOf('@') + 1);
            int colon = endpoint.startsWith("[") ? endpoint.indexOf(':', endpoint.indexOf(']')) : endpoint.indexOf(':');
            if (endpoint.startsWith("[") && endpoint.indexOf(']') < 0) fail(FGSubscriptionError.Code.INVALID_LINK);
            if (colon >= 0) {
                String port = endpoint.substring(colon + 1);
                if (port.contains(",") || port.contains("-")) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                if (!port.matches("[0-9]{1,5}") || Integer.parseInt(port) < 1 || Integer.parseInt(port) > 65535) fail(FGSubscriptionError.Code.INVALID_LINK);
            }
            if (strict.getRawPath() != null && !strict.getRawPath().isEmpty() && !"/".equals(strict.getRawPath())) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
            Set<String> allowed = vless ? VLESS_PARAMS : HY_PARAMS;
            for (String key : uri.getQueryParameterNames()) {
                if (!allowed.contains(key)) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                if (uri.getQueryParameters(key).size() != 1) fail(FGSubscriptionError.Code.INVALID_LINK);
            }
            for (String key : Arrays.asList("insecure", "allowInsecure")) {
                String value = uri.getQueryParameter(key);
                if (value != null && !value.matches("(?i)0|1|true|false")) fail(FGSubscriptionError.Code.INVALID_LINK);
            }
            String sni = uri.getQueryParameter("sni"), peer = uri.getQueryParameter("peer");
            if (sni != null && peer != null && !sni.equals(peer)) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
            String insecure = uri.getQueryParameter("insecure"), allowInsecure = uri.getQueryParameter("allowInsecure");
            if (insecure != null && allowInsecure != null && truthy(insecure) != truthy(allowInsecure)) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
            if (vless) {
                if (uri.getUserInfo() == null || !uri.getUserInfo().matches("(?i)[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}")) fail(FGSubscriptionError.Code.INVALID_LINK);
                String flow = uri.getQueryParameter("flow");
                if (flow != null && !flow.isEmpty() && !"xtls-rprx-vision".equals(flow)) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                String security = uri.getQueryParameter("security");
                if (!"reality".equalsIgnoreCase(security) && (nonempty(uri, "pbk") || nonempty(uri, "sid"))) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                String type = uri.getQueryParameter("type");
                if (type != null && !Arrays.asList("", "tcp", "raw", "ws", "httpupgrade", "http", "h2", "grpc")
                        .contains(type.toLowerCase(Locale.ROOT))) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                boolean tcp = type == null || type.isEmpty() || "tcp".equalsIgnoreCase(type) || "raw".equalsIgnoreCase(type);
                boolean grpc = "grpc".equalsIgnoreCase(type);
                boolean tls = "tls".equalsIgnoreCase(security) || "reality".equalsIgnoreCase(security);
                if (!grpc && nonempty(uri, "serviceName") || tcp && nonempty(uri, "path")) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                if (nonempty(uri, "headerType") && !"none".equalsIgnoreCase(uri.getQueryParameter("headerType"))) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                if (grpc && nonempty(uri, "serviceName") && nonempty(uri, "path")
                        && !uri.getQueryParameter("serviceName").equals(uri.getQueryParameter("path"))) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                if (!tls && (nonempty(uri, "fp") || nonempty(uri, "alpn") || nonempty(uri, "peer")
                        || truthy(insecure) || truthy(allowInsecure))) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                boolean hostTransport = Arrays.asList("ws", "httpupgrade", "http", "h2").contains(type == null ? "" : type.toLowerCase(Locale.ROOT));
                if (!tls && !hostTransport && (nonempty(uri, "host") || nonempty(uri, "sni"))) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                if (!tls && nonempty(uri, "sni") && (nonempty(uri, "host") || "http".equalsIgnoreCase(type) || "h2".equalsIgnoreCase(type))) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                if ("reality".equalsIgnoreCase(uri.getQueryParameter("security"))) {
                    String key = uri.getQueryParameter("pbk");
                    if (key == null || base64(key).length != 32) fail(FGSubscriptionError.Code.INVALID_LINK);
                    String sid = uri.getQueryParameter("sid");
                    if (sid != null && (!sid.matches("(?i)[0-9a-f]{0,16}") || sid.length() % 2 != 0)) fail(FGSubscriptionError.Code.INVALID_LINK);
                }
            } else {
                String obfs = uri.getQueryParameter("obfs");
                if (obfs != null && !obfs.isEmpty() && !"salamander".equalsIgnoreCase(obfs)) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
                if (obfs != null && !obfs.isEmpty() && (uri.getQueryParameter("obfs-password") == null || uri.getQueryParameter("obfs-password").isEmpty())) fail(FGSubscriptionError.Code.INVALID_LINK);
                if ((obfs == null || obfs.isEmpty()) && nonempty(uri, "obfs-password")) fail(FGSubscriptionError.Code.UNSUPPORTED_LINK);
            }
            FGOutboundLink.Parsed parsed = FGOutboundLink.parse(link);
            if (parsed == null || parsed.host.length() > 253 || parsed.host.matches(".*[\\s\\p{Cntrl}].*")) fail(FGSubscriptionError.Code.INVALID_LINK);
            FGSubscriptionError.Code rejection = rejectionName(parsed.title);
            if (rejection != null) fail(rejection);
            return new Server(link, parsed);
        } catch (FGSubscriptionError error) {
            throw error;
        } catch (Exception ignored) {
            throw new FGSubscriptionError(FGSubscriptionError.Code.INVALID_LINK);
        }
    }

    private static boolean nonempty(Uri uri, String key) {
        String value = uri.getQueryParameter(key);
        return value != null && !value.isEmpty();
    }

    private static boolean truthy(String value) {
        return "1".equals(value) || "true".equalsIgnoreCase(value);
    }

    static String utf8(byte[] bytes) throws FGSubscriptionError {
        try {
            return StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
                    .onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes)).toString();
        } catch (Exception ignored) {
            throw new FGSubscriptionError(FGSubscriptionError.Code.INVALID_ENCODING);
        }
    }

    static byte[] base64(String text) throws FGSubscriptionError {
        String compact = text.replaceAll("[\\r\\n\\t ]", "");
        if (compact.length() > MAX_BODY_BYTES || !compact.matches("[A-Za-z0-9+/_-]*={0,2}")) fail(FGSubscriptionError.Code.INVALID_FORMAT);
        int padding = compact.endsWith("==") ? 2 : compact.endsWith("=") ? 1 : 0;
        int remainder = (compact.length() - padding) % 4;
        if (remainder == 1 || padding != 0 && (compact.length() % 4 != 0 || padding != (4 - remainder) % 4)) fail(FGSubscriptionError.Code.INVALID_FORMAT);
        try {
            byte[] bytes = Base64.decode(compact.replace('-', '+').replace('_', '/'), Base64.DEFAULT);
            if (bytes.length > MAX_BODY_BYTES) fail(FGSubscriptionError.Code.BODY_TOO_LARGE);
            return bytes;
        } catch (IllegalArgumentException ignored) {
            throw new FGSubscriptionError(FGSubscriptionError.Code.INVALID_FORMAT);
        }
    }

    static byte[] gunzip(byte[] bytes) throws FGSubscriptionError {
        try (InputStream input = new GZIPInputStream(new ByteArrayInputStream(bytes))) {
            return readBounded(input, MAX_BODY_BYTES);
        } catch (FGSubscriptionError error) {
            throw error;
        } catch (IOException ignored) {
            throw new FGSubscriptionError(FGSubscriptionError.Code.INVALID_FORMAT);
        }
    }

    static byte[] readBounded(InputStream input, int limit) throws IOException, FGSubscriptionError {
        ByteArrayOutputStream output = new ByteArrayOutputStream(Math.min(limit, 8192));
        byte[] buffer = new byte[8192];
        int count;
        while ((count = input.read(buffer, 0, Math.min(buffer.length, limit - output.size() + 1))) != -1) {
            if (output.size() + count > limit) fail(FGSubscriptionError.Code.BODY_TOO_LARGE);
            output.write(buffer, 0, count);
        }
        return output.toByteArray();
    }

    static String displayTitle(String value) throws FGSubscriptionError {
        String result = value.trim();
        if (result.toLowerCase(Locale.ROOT).startsWith("base64:")) result = utf8(base64(result.substring(7)));
        result = result.split("[\\r\\n|]", 2)[0].replaceAll("[\\p{Cntrl}]", "").trim();
        if (result.length() > 256) fail(FGSubscriptionError.Code.INVALID_FORMAT);
        return result;
    }

    static FGSubscriptionError.Code rejectionName(String value) {
        String name = value.trim().toLowerCase(Locale.ROOT).replaceAll("^[^\\p{L}\\p{N}]+|[^\\p{L}\\p{N}]+$", "").replaceAll("\\s+", " ");
        if (Arrays.asList("app not supported", "application not supported", "приложение не поддерживается").contains(name)) {
            return FGSubscriptionError.Code.APP_NOT_SUPPORTED;
        }
        return Arrays.asList("hwid required", "hwid is required", "missing hwid", "hwid not found").contains(name)
                ? FGSubscriptionError.Code.HWID_REQUIRED : null;
    }

    static FGSubscriptionError.Code rejection(String text) {
        FGSubscriptionError.Code code = rejectionName(text);
        if (code != null) return code;
        if (!text.trim().startsWith("{")) return null;
        try {
            FGSubscriptionXray.checkDepth(text);
            JSONObject object = new JSONObject(text);
            for (String key : Arrays.asList("error", "message", "detail")) {
                code = rejectionName(object.optString(key, ""));
                if (code != null) return code;
            }
        } catch (Exception ignored) {
        }
        return null;
    }

    static String digest(String text) {
        try {
            byte[] bytes = MessageDigest.getInstance("SHA-256").digest(text.getBytes(StandardCharsets.UTF_8));
            StringBuilder result = new StringBuilder(64);
            String hex = "0123456789abcdef";
            for (byte value : bytes) result.append(hex.charAt((value & 255) >>> 4)).append(hex.charAt(value & 15));
            return result.toString();
        } catch (Exception ignored) {
            throw new IllegalStateException("SHA256");
        }
    }

    private static String canonical(Object value) throws Exception {
        if (value instanceof JSONObject) {
            JSONObject object = (JSONObject) value;
            List<String> keys = new ArrayList<>();
            Iterator<String> iterator = object.keys();
            while (iterator.hasNext()) keys.add(iterator.next());
            Collections.sort(keys);
            StringBuilder result = new StringBuilder("{");
            for (String key : keys) result.append(JSONObject.quote(key)).append(':').append(canonical(object.get(key))).append(',');
            return result.append('}').toString();
        }
        if (value instanceof JSONArray) {
            JSONArray array = (JSONArray) value;
            StringBuilder result = new StringBuilder("[");
            for (int i = 0; i < array.length(); i++) result.append(canonical(array.get(i))).append(',');
            return result.append(']').toString();
        }
        return value instanceof String ? JSONObject.quote((String) value) : String.valueOf(value);
    }

    static void fail(FGSubscriptionError.Code code) throws FGSubscriptionError {
        throw new FGSubscriptionError(code);
    }
}

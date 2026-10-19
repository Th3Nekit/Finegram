/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.outbound;

import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.Proxy;
import java.net.URI;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.SynchronousQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

final class FGSubscriptionHttp {
    static final int TOTAL_MILLIS = 20000;
    static final int MAX_REDIRECTS = 3;
    private static final ThreadPoolExecutor NETWORK = new ThreadPoolExecutor(0, 2, 30, TimeUnit.SECONDS,
            new SynchronousQueue<>(), task -> {
                Thread thread = new Thread(task, "FGSubscriptionHTTP");
                thread.setDaemon(true);
                return thread;
            });

    static final class Identity {
        final FGSubscriptionOptions.IdentityMode mode;
        final String hwid;
        final String osVersion;
        final String model;
        final Locale locale;
        final String userAgent;
        final boolean customAgent;

        Identity(FGSubscriptionOptions.IdentityMode mode, String hwid, String osVersion, String model, Locale locale) {
            this(mode == FGSubscriptionOptions.IdentityMode.LEAN_EXPLICIT ? FGSubscriptionOptions.lean(hwid)
                    : mode == FGSubscriptionOptions.IdentityMode.OWN_DEVICE ? FGSubscriptionOptions.ownDevice()
                    : mode == FGSubscriptionOptions.IdentityMode.EXPLICIT ? FGSubscriptionOptions.explicit(hwid)
                    : FGSubscriptionOptions.none(), hwid, osVersion, model, locale);
        }

        Identity(FGSubscriptionOptions options, String hwid, String osVersion, String model, Locale locale) {
            this.mode = options.identityMode;
            this.hwid = hwid;
            this.osVersion = header(osVersion, 64);
            this.model = header(model, 128);
            this.locale = locale;
            this.userAgent = options.userAgent();
            this.customAgent = options.userAgentMode == FGSubscriptionOptions.UserAgentMode.CUSTOM;
        }

        private static String header(String input, int limit) {
            String value = input == null ? "" : input.replaceAll("[^\\x20-\\x7e]", "");
            return value.substring(0, Math.min(limit, value.length()));
        }

        void apply(HttpURLConnection connection, boolean origin) {
            boolean lean = origin && mode == FGSubscriptionOptions.IdentityMode.LEAN_EXPLICIT && "Lean/1.2.0".equals(userAgent);
            connection.setRequestProperty("User-Agent", !origin && customAgent ? "Finegram/Subscription" : userAgent);
            connection.setRequestProperty("Accept", "*/*");
            connection.setRequestProperty("Accept-Encoding", "gzip");
            connection.setRequestProperty("Accept-Language", header(locale.toLanguageTag(), 64));
            if (lean) {
                connection.setRequestProperty("x-client", "LEAN");
                connection.setRequestProperty("x-device-locale", header(locale.toString(), 64));
                connection.setRequestProperty("x-app-version", "1.2.0");
            }
            if (origin && mode != FGSubscriptionOptions.IdentityMode.NONE && !hwid.isEmpty()) {
                connection.setRequestProperty("x-hwid", hwid);
                connection.setRequestProperty("x-device-os", "Android");
                connection.setRequestProperty("x-ver-os", osVersion);
                connection.setRequestProperty("x-device-model", model);
            }
        }
    }

    static final class Response {
        final byte[] body;
        final Map<String, String> headers;

        Response(byte[] body, Map<String, String> headers) {
            this.body = body;
            this.headers = headers;
        }
    }

    static URL source(String url, String hwid) throws FGSubscriptionError {
        if (url == null || url.length() > 4096) fail(FGSubscriptionError.Code.INVALID_URL);
        boolean placeholder = url.contains("{hwid}") || url.contains("{HWID}") || url.matches("(?i).*%7bhwid%7d.*");
        if (placeholder && (hwid == null || hwid.isEmpty())) fail(FGSubscriptionError.Code.HWID_REQUIRED);
        String expanded = url.replace("{hwid}", hwid == null ? "" : hwid).replace("{HWID}", hwid == null ? "" : hwid)
                .replaceAll("(?i)%7bhwid%7d", hwid == null ? "" : hwid);
        return checked(expanded);
    }

    static URL checked(String raw) throws FGSubscriptionError {
        try {
            if (raw.length() > 4096 || raw.matches(".*[\\p{Cntrl}\\s].*")) fail(FGSubscriptionError.Code.INVALID_URL);
            URI uri = new URI(raw);
            if (!"https".equalsIgnoreCase(uri.getScheme())) fail(FGSubscriptionError.Code.INVALID_URL);
            if (uri.getHost() == null || uri.getHost().isEmpty() || uri.getRawUserInfo() != null
                    || uri.getPort() == 0 || uri.getPort() > 65535 || raw.contains("{") || raw.contains("}")) fail(FGSubscriptionError.Code.INVALID_URL);
            int fragment = raw.indexOf('#');
            return new URL(fragment < 0 ? raw : raw.substring(0, fragment));
        } catch (FGSubscriptionError error) {
            throw error;
        } catch (Exception ignored) {
            throw new FGSubscriptionError(FGSubscriptionError.Code.INVALID_URL);
        }
    }

    static URL redirect(URL from, String location) throws FGSubscriptionError {
        try {
            URL next = new URL(from, location);
            if ("https".equalsIgnoreCase(from.getProtocol()) && "http".equalsIgnoreCase(next.getProtocol())) fail(FGSubscriptionError.Code.HTTPS_DOWNGRADE);
            next = checked(next.toString());
            if (location.matches("(?i).*(?:\\{hwid\\}|%7bhwid%7d).*")) fail(FGSubscriptionError.Code.INVALID_URL);
            return next;
        } catch (FGSubscriptionError error) {
            throw error;
        } catch (Exception ignored) {
            throw new FGSubscriptionError(FGSubscriptionError.Code.INVALID_URL);
        }
    }

    private static boolean sameOrigin(URL first, URL second) {
        int firstPort = first.getPort() == -1 ? first.getDefaultPort() : first.getPort();
        int secondPort = second.getPort() == -1 ? second.getDefaultPort() : second.getPort();
        return first.getProtocol().equalsIgnoreCase(second.getProtocol())
                && first.getHost().equalsIgnoreCase(second.getHost()) && firstPort == secondPort;
    }

    static Response fetch(String url, Identity identity, long deadlineNanos) throws FGSubscriptionError {
        URL original = source(url, identity.hwid);
        AtomicReference<HttpURLConnection> active = new AtomicReference<>();
        Future<Response> future;
        try {
            future = NETWORK.submit(() -> download(original, identity, deadlineNanos, active));
        } catch (RejectedExecutionException ignored) {
            throw new FGSubscriptionError(FGSubscriptionError.Code.BUSY);
        }
        try {
            long remaining = deadlineNanos - System.nanoTime();
            if (remaining <= 0) fail(FGSubscriptionError.Code.TIMEOUT);
            return future.get(remaining, TimeUnit.NANOSECONDS);
        } catch (TimeoutException ignored) {
            throw new FGSubscriptionError(FGSubscriptionError.Code.TIMEOUT);
        } catch (InterruptedException ignored) {
            Thread.currentThread().interrupt();
            throw new FGSubscriptionError(FGSubscriptionError.Code.TIMEOUT);
        } catch (ExecutionException error) {
            if (error.getCause() instanceof FGSubscriptionError) throw (FGSubscriptionError) error.getCause();
            if (error.getCause() instanceof java.net.SocketTimeoutException) throw new FGSubscriptionError(FGSubscriptionError.Code.TIMEOUT);
            throw new FGSubscriptionError(FGSubscriptionError.Code.NETWORK);
        } finally {
            future.cancel(true);
            HttpURLConnection connection = active.getAndSet(null);
            if (connection != null) connection.disconnect();
        }
    }

    private static Response download(URL original, Identity identity, long deadline, AtomicReference<HttpURLConnection> active) throws Exception {
        URL current = original;
        boolean identityAllowed = true;
        for (int hops = 0; ; hops++) {
            int remaining = remaining(deadline);
            HttpURLConnection connection = (HttpURLConnection) current.openConnection(Proxy.NO_PROXY);
            active.set(connection);
            if (Thread.currentThread().isInterrupted()) fail(FGSubscriptionError.Code.TIMEOUT);
            try {
                connection.setInstanceFollowRedirects(false);
                connection.setConnectTimeout(Math.min(10000, remaining));
                connection.setReadTimeout(Math.min(10000, remaining));
                connection.setUseCaches(false);
                identity.apply(connection, identityAllowed);
                int status = connection.getResponseCode();
                Map<String, String> headers = headers(connection);
                if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
                    if (hops >= MAX_REDIRECTS) fail(FGSubscriptionError.Code.TOO_MANY_REDIRECTS);
                    String location = headers.get("location");
                    if (location == null || location.isEmpty()) fail(FGSubscriptionError.Code.HTTP_STATUS);
                    URL next = redirect(current, location);
                    identityAllowed = identityAllowed && sameOrigin(current, next);
                    current = next;
                    continue;
                }
                if (status != 200) {
                    InputStream input = connection.getErrorStream();
                    if (input != null) {
                        try (InputStream error = input) {
                            byte[] bytes = new byte[4096];
                            int used = 0;
                            int count;
                            while (used < bytes.length && (count = error.read(bytes, used, bytes.length - used)) > 0) {
                                remaining(deadline);
                                used += count;
                            }
                            FGSubscriptionError.Code rejection = FGSubscriptionParser.rejection(new String(bytes, 0, used, StandardCharsets.UTF_8));
                            if (rejection != null) throw new FGSubscriptionError(rejection, status);
                        }
                    }
                    throw new FGSubscriptionError(FGSubscriptionError.Code.HTTP_STATUS, status);
                }
                String contentType = headers.getOrDefault("content-type", "").toLowerCase(Locale.ROOT);
                if (contentType.contains("html") || contentType.contains("yaml")) fail(FGSubscriptionError.Code.UNSUPPORTED_FORMAT);
                long size = connection.getContentLengthLong();
                if (size > FGSubscriptionParser.MAX_BODY_BYTES) fail(FGSubscriptionError.Code.BODY_TOO_LARGE);
                byte[] bytes;
                try (InputStream input = connection.getInputStream()) {
                    bytes = FGSubscriptionParser.readBounded(input, FGSubscriptionParser.MAX_BODY_BYTES);
                }
                remaining(deadline);
                String encoding = headers.getOrDefault("content-encoding", "").trim();
                if ("gzip".equalsIgnoreCase(encoding)) bytes = FGSubscriptionParser.gunzip(bytes);
                else if (!encoding.isEmpty() && !"identity".equalsIgnoreCase(encoding)) fail(FGSubscriptionError.Code.UNSUPPORTED_FORMAT);
                return new Response(bytes, headers);
            } finally {
                active.compareAndSet(connection, null);
                connection.disconnect();
            }
        }
    }

    private static int remaining(long deadline) throws FGSubscriptionError {
        long value = TimeUnit.NANOSECONDS.toMillis(deadline - System.nanoTime());
        if (value <= 0) fail(FGSubscriptionError.Code.TIMEOUT);
        return (int) Math.min(Integer.MAX_VALUE, value);
    }

    private static Map<String, String> headers(HttpURLConnection connection) throws FGSubscriptionError {
        Map<String, String> result = new HashMap<>();
        int size = 0;
        int count = 0;
        for (Map.Entry<String, List<String>> entry : connection.getHeaderFields().entrySet()) {
            if (++count > 64) fail(FGSubscriptionError.Code.LIMIT);
            if (entry.getKey() == null) continue;
            size += entry.getKey().length();
            for (String value : entry.getValue()) size += value.length();
            if (size > 16384) fail(FGSubscriptionError.Code.LIMIT);
            if (!entry.getValue().isEmpty()) result.put(entry.getKey().toLowerCase(Locale.ROOT), entry.getValue().get(0));
        }
        return result;
    }

    private static void fail(FGSubscriptionError.Code code) throws FGSubscriptionError {
        FGSubscriptionParser.fail(code);
    }
}

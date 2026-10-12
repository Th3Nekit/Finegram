/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Обход блокировок: MTProto поверх WebSocket. Своя реализация на
 * наши серверы.
 */

package com.th3nekit.finegram.net.bypass;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.InetAddress;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.util.Base64;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

import javax.net.ssl.HostnameVerifier;
import javax.net.ssl.SSLPeerUnverifiedException;
import javax.net.ssl.SSLSession;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;

public final class RawWebSocket {

    interface ConnectPermit {
        boolean isCurrent();
    }

    private static final int OPCODE_CONTINUATION = 0x0;
    private static final int OPCODE_TEXT   = 0x1;
    private static final int OPCODE_BINARY = 0x2;
    private static final int OPCODE_CLOSE  = 0x8;
    private static final int OPCODE_PING   = 0x9;
    private static final int OPCODE_PONG   = 0xA;

    private long rxFrames = 0, rxBytes = 0, rxFragments = 0;
    private long maxFrame = 0;

    private static final int DEFAULT_RCVBUF = 256 * 1024;
    private static final int DEFAULT_SNDBUF = 512 * 1024;
    private static final int MAX_HTTP_HEADER_LINE_BYTES = 8 * 1024;
    private static final int MAX_HTTP_HEADER_BYTES = 64 * 1024;
    private static final long MAX_WS_FRAME_BYTES = 16L * 1024L * 1024L;
    private static final long MAX_WS_MESSAGE_BYTES = 16L * 1024L * 1024L;
    private static final String WS_ACCEPT_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    private static final int IDLE_READ_TIMEOUT_MS = 70 * 1000;

    private static final SecureRandom RNG = new SecureRandom();
    private static final int WRITE_TIMEOUT_MS = 40_000;
    private static final ScheduledThreadPoolExecutor WRITE_DEADLINES = createWriteDeadlines();

    private static ScheduledThreadPoolExecutor createWriteDeadlines() {
        ScheduledThreadPoolExecutor executor = new ScheduledThreadPoolExecutor(1, r -> {
            Thread thread = new Thread(r, "wsbypass-write-deadline");
            thread.setDaemon(true);
            return thread;
        });
        executor.setRemoveOnCancelPolicy(true);
        return executor;
    }

    private static final ExecutorService DNS_EXECUTOR = new ThreadPoolExecutor(
            2, 2, 30L, TimeUnit.SECONDS,
            new ArrayBlockingQueue<>(8),
            r -> {
                Thread t = new Thread(r, "wsbypass-dns");
                t.setDaemon(true);
                return t;
            },
            new ThreadPoolExecutor.AbortPolicy());

    private static final java.util.concurrent.ConcurrentHashMap<String, CachedAddresses> DNS_CACHE =
            new java.util.concurrent.ConcurrentHashMap<>();
    private static final long DNS_STALE_MAX_MS = 6 * 60 * 60 * 1000L;
    private static final int DNS_STALE_WAIT_MS = 600;
    private static final int MIN_ADDRESS_BUDGET_MS = 500;

    private static final class CachedAddresses {
        final InetAddress[] addresses;
        final long savedAtMs;

        CachedAddresses(InetAddress[] addresses) {
            this.addresses = addresses;
            this.savedAtMs = android.os.SystemClock.elapsedRealtime();
        }

        boolean usable() {
            return android.os.SystemClock.elapsedRealtime() - savedAtMs < DNS_STALE_MAX_MS;
        }
    }

    private final Socket transport;
    private final Socket sock;
    private final InputStream reader;
    private final OutputStream writer;
    private final int timeoutSec;
    private final Object lock = new Object();
    private final AtomicBoolean closed = new AtomicBoolean(false);
    private volatile long lastIoMs = System.currentTimeMillis();

    private volatile long lastPongMs = System.currentTimeMillis();

    private volatile long lastPongToken;

    private volatile long lastInboundMs = System.currentTimeMillis();
    private boolean fragmentedMessageOpen;
    private long fragmentedMessageBytes;

    private RawWebSocket(Socket transport, Socket sock, InputStream reader, OutputStream writer, int timeoutSec) {
        this.transport = transport;
        this.sock = sock;
        this.reader = reader;
        this.writer = writer;
        this.timeoutSec = timeoutSec;
    }

    public static RawWebSocket connect(String connectHost, String sniHost, int timeoutSec) throws IOException {
        return connect(connectHost, sniHost, "/apiws", null, timeoutSec);
    }

    public static RawWebSocket connect(String connectHost, String sniHost, String path, int timeoutSec) throws IOException {
        return connect(connectHost, sniHost, path, null, timeoutSec);
    }

    public static RawWebSocket connect(String connectHost, String sniHost, String path,
                                       java.util.Map<String, String> extraHeaders, int timeoutSec) throws IOException {
        long timeoutMs = Math.max(1_000L, Math.min(60_000L, (long) timeoutSec * 1000L));
        return connectUntil(connectHost, sniHost, path, extraHeaders,
                System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(timeoutMs));
    }

    static RawWebSocket connectUntil(String connectHost, String sniHost, String path,
                                     Map<String, String> extraHeaders, long deadlineNanos) throws IOException {
        return connectUntil(connectHost, sniHost, path, extraHeaders, deadlineNanos, null);
    }

    static RawWebSocket connectUntil(String connectHost, String sniHost, String path,
                                     Map<String, String> extraHeaders, long deadlineNanos,
                                     ConnectPermit permit) throws IOException {
        checkPermit(permit);
        String sni = sniHost == null ? "" : sniHost.trim();
        if (connectHost == null || connectHost.trim().isEmpty() || sni.isEmpty()) {
            throw new IOException("empty websocket host");
        }
        String requestPath = path == null || path.isEmpty() ? "/apiws" : path;
        if (!requestPath.startsWith("/") || containsCrLf(requestPath) || requestPath.indexOf(' ') >= 0) {
            throw new IOException("invalid websocket path");
        }

        Socket raw = null;
        SSLSocket wrapped = null;
        BufferedInputStream in = null;
        boolean success = false;
        try {
            raw = openSocket(connectHost.trim(), deadlineNanos, permit);
            checkPermit(permit);
            wrapped = wrapTls(raw, sni, deadlineNanos, permit);
            checkPermit(permit);
            wrapped.setSoTimeout(remainingMillis(deadlineNanos));

            byte[] keyBytes = new byte[16];
            RNG.nextBytes(keyBytes);
            String key = Base64.getEncoder().encodeToString(keyBytes);

            StringBuilder req = new StringBuilder(256);
            req.append("GET ").append(requestPath).append(" HTTP/1.1\r\n");
            req.append("Host: ").append(sni).append("\r\n");
            req.append("Upgrade: websocket\r\n");
            req.append("Connection: Upgrade\r\n");
            req.append("Sec-WebSocket-Key: ").append(key).append("\r\n");
            req.append("Sec-WebSocket-Version: 13\r\n");
            req.append("Sec-WebSocket-Protocol: binary\r\n");
            req.append("Origin: https://web.telegram.org\r\n");
            req.append("User-Agent: Mozilla/5.0 (Linux; Android 13)\r\n");
            if (extraHeaders != null) {
                for (Map.Entry<String, String> e : extraHeaders.entrySet()) {
                    if (!validExtraHeader(e.getKey(), e.getValue())) continue;
                    req.append(e.getKey()).append(": ").append(e.getValue()).append("\r\n");
                }
            }
            req.append("\r\n");

            OutputStream out = wrapped.getOutputStream();
            wrapped.setSoTimeout(remainingMillis(deadlineNanos));
            out.write(req.toString().getBytes(StandardCharsets.UTF_8));
            out.flush();

            in = new BufferedInputStream(wrapped.getInputStream(), 8192);
            int[] headerBytes = new int[1];
            String statusLine;
            Map<String, String> headers = new HashMap<>();
            statusLine = readLine(in, headerBytes);
            while (true) {
                String line = readLine(in, headerBytes);
                if (line == null) throw new IOException("unexpected eof in websocket handshake");
                if (line.isEmpty()) break;
                int colon = line.indexOf(':');
                if (colon > 0) {
                    String k = line.substring(0, colon).trim().toLowerCase(Locale.ROOT);
                    String v = line.substring(colon + 1).trim();
                    String previous = headers.get(k);
                    headers.put(k, previous == null ? v : previous + "," + v);
                }
            }
            int code = parseStatusCode(statusLine);
            if (code != 101) {
                String loc = headers.get("location");
                throw new HandshakeException(statusLine == null || statusLine.isEmpty()
                        ? "bad websocket handshake" : statusLine, code, loc);
            }
            if (!containsToken(headers.get("upgrade"), "websocket")
                    || !containsToken(headers.get("connection"), "upgrade")) {
                throw new HandshakeException("invalid websocket upgrade headers", code, null);
            }
            String expectedAccept = websocketAccept(key);
            if (!expectedAccept.equals(headers.get("sec-websocket-accept"))) {
                throw new HandshakeException("invalid Sec-WebSocket-Accept", code, null);
            }
            String protocol = headers.get("sec-websocket-protocol");
            if (protocol != null && !"binary".equalsIgnoreCase(protocol.trim())) {
                throw new HandshakeException("unexpected websocket subprotocol", code, null);
            }
            if (headers.containsKey("sec-websocket-extensions")) {
                throw new HandshakeException("unsupported websocket extension", code, null);
            }
            checkPermit(permit);
            wrapped.setSoTimeout(IDLE_READ_TIMEOUT_MS);
            int timeoutSec = Math.max(1, (int) Math.min(Integer.MAX_VALUE,
                    TimeUnit.NANOSECONDS.toSeconds(Math.max(1L, deadlineNanos - System.nanoTime()))));
            RawWebSocket result = new RawWebSocket(raw, wrapped, in, out, timeoutSec);
            success = true;
            return result;
        } finally {
            if (!success) {
                closeQuietly(raw);
                closeQuietly(in);
                closeQuietly(wrapped);
            }
        }
    }

    private static Socket openSocket(String connectHost, long deadlineNanos,
                                     ConnectPermit permit) throws IOException {
        InetAddress[] addresses = preferIpv4(resolveUntil(connectHost, deadlineNanos, permit));
        IOException last = null;
        for (int i = 0; i < addresses.length; i++) {
            InetAddress address = addresses[i];
            checkPermit(permit);

            int remaining = remainingMillis(deadlineNanos);
            int left = addresses.length - i;
            int budget = left <= 1 ? remaining : Math.min(remaining, Math.max(MIN_ADDRESS_BUDGET_MS, remaining / left));
            Socket s = new Socket();
            try {
                try { s.setTcpNoDelay(true); } catch (Exception ignored) {}
                try {
                    s.setReceiveBufferSize(DEFAULT_RCVBUF);
                    s.setSendBufferSize(DEFAULT_SNDBUF);
                } catch (Exception ignored) {}
                try { s.setKeepAlive(true); } catch (Exception ignored) {}
                s.connect(new InetSocketAddress(address, 443), budget);
                checkPermit(permit);
                s.setSoTimeout(remainingMillis(deadlineNanos));
                return s;
            } catch (IOException e) {
                last = e;
                closeQuietly(s);
                if (System.nanoTime() >= deadlineNanos) break;
            }
        }
        if (last != null) throw last;
        throw new IOException("no address for " + connectHost);
    }

    private static SSLSocket wrapTls(Socket raw, String sni, long deadlineNanos,
                                     ConnectPermit permit) throws IOException {
        checkPermit(permit);
        SSLSocketFactory factory = (SSLSocketFactory) SSLSocketFactory.getDefault();
        SSLSocket ssl;
        try {
            ssl = (SSLSocket) factory.createSocket(raw, sni, 443, true);
        } catch (IOException | RuntimeException e) {
            closeQuietly(raw);
            throw e;
        }
        ssl.setUseClientMode(true);

        ssl.setSoTimeout(remainingMillis(deadlineNanos));
        try {
            ssl.startHandshake();
            checkPermit(permit);
        } catch (IOException e) {
            closeQuietly(raw);
            closeQuietly(ssl);
            throw e;
        }
        HostnameVerifier hv = javax.net.ssl.HttpsURLConnection.getDefaultHostnameVerifier();
        SSLSession session = ssl.getSession();
        if (!hv.verify(sni, session)) {
            closeQuietly(raw);
            closeQuietly(ssl);
            throw new SSLPeerUnverifiedException("hostname verification failed for " + sni);
        }
        return ssl;
    }

    public void send(byte[] payload) throws IOException {
        writeFrame(buildFrame(OPCODE_BINARY, payload, true), WRITE_TIMEOUT_MS);
    }

    public void sendBatch(List<byte[]> parts) throws IOException {
        try {
                if (parts == null || parts.isEmpty()) return;
                long totalLong = 0L;
                byte[][] frames = new byte[parts.size()][];
                for (int i = 0; i < parts.size(); i++) {
                    frames[i] = buildFrame(OPCODE_BINARY, parts.get(i), true);
                    totalLong += frames[i].length;
                    if (totalLong > Integer.MAX_VALUE) throw new IOException("websocket batch too large");
                }
                int total = (int) totalLong;
                byte[] joined = new byte[total];
                int off = 0;
                for (byte[] f : frames) {
                    System.arraycopy(f, 0, joined, off, f.length);
                    off += f.length;
                }
                writeFrame(joined, WRITE_TIMEOUT_MS);
        } catch (IOException e) {

            close();
            throw e;
        }
    }

    public byte[] recv() throws IOException {
        try {
            return recvInternal(0L, 0L);
        } catch (IOException e) {
            close();
            throw e;
        }
    }

    private byte[] recvInternal(long expectedPong, long deadlineNanos) throws IOException {
        while (true) {
            if (expectedPong != 0L) {
                long left = deadlineNanos - System.nanoTime();
                if (left <= 0L) throw new SocketTimeoutException("pong deadline");
                sock.setSoTimeout((int) Math.max(1L, TimeUnit.NANOSECONDS.toMillis(left)));
            }
            int[] header = new int[2];
            readExact(2, header, deadlineNanos);
            boolean fin = (header[0] & 0x80) != 0;
            int opcode = header[0] & 0x0F;
            if ((header[0] & 0x70) != 0) throw new IOException("unsupported ws RSV bits");
            boolean masked = (header[1] & 0x80) != 0;
            if (masked) throw new IOException("masked server websocket frame");
            long length = header[1] & 0x7F;
            if (length == 126) {
                byte[] ext = new byte[2];
                readExactBytes(ext, deadlineNanos);
                length = ((ext[0] & 0xFFL) << 8) | (ext[1] & 0xFFL);
            } else if (length == 127) {
                byte[] ext = new byte[8];
                readExactBytes(ext, deadlineNanos);
                if ((ext[0] & 0x80) != 0) throw new IOException("invalid ws frame length");
                length = 0;
                for (int i = 0; i < 8; i++) {
                    length = (length << 8) | (ext[i] & 0xFFL);
                }
            }
            byte[] maskKey = null;
            if (masked) {
                maskKey = new byte[4];
                readExactBytes(maskKey, deadlineNanos);
            }
            if (length < 0 || length > MAX_WS_FRAME_BYTES) {
                throw new IOException("ws frame too large: " + length);
            }
            boolean control = opcode == OPCODE_CLOSE || opcode == OPCODE_PING || opcode == OPCODE_PONG;
            if (!control && opcode != OPCODE_CONTINUATION && opcode != OPCODE_TEXT && opcode != OPCODE_BINARY) {
                throw new IOException("unsupported ws opcode: " + opcode);
            }
            if (control && (!fin || length > 125L)) {
                throw new IOException("invalid ws control frame");
            }
            long messageBytes = length;
            if (opcode == OPCODE_CONTINUATION) {
                if (!fragmentedMessageOpen) {
                    throw new IOException("unexpected ws continuation");
                }
                if (length > MAX_WS_MESSAGE_BYTES - fragmentedMessageBytes) {
                    throw new IOException("ws message too large");
                }
                messageBytes = fragmentedMessageBytes + length;
            } else if (opcode == OPCODE_TEXT || opcode == OPCODE_BINARY) {
                if (fragmentedMessageOpen) {
                    throw new IOException("interleaved ws data frame");
                }
                if (length > MAX_WS_MESSAGE_BYTES) {
                    throw new IOException("ws message too large");
                }
            }
            byte[] payload = new byte[(int) length];
            readExactBytes(payload, deadlineNanos);
            if (masked && payload.length > 0) {
                xorMask(payload, maskKey);
            }
            lastIoMs = System.currentTimeMillis();
            lastInboundMs = lastIoMs;
            if (opcode == OPCODE_CLOSE) {
                close();
                return null;
            }
            if (opcode == OPCODE_PING) {
                writeControl(OPCODE_PONG, payload);
                continue;
            }
            if (opcode == OPCODE_PONG) {
                lastPongMs = System.currentTimeMillis();
                lastPongToken = tokenOf(payload);
                if (expectedPong != 0L && lastPongToken == expectedPong) {
                    return new byte[0];
                }
                continue;
            }

            if (opcode == OPCODE_CONTINUATION || opcode == OPCODE_TEXT || opcode == OPCODE_BINARY) {
                if (fin) {
                    fragmentedMessageOpen = false;
                    fragmentedMessageBytes = 0L;
                } else {
                    fragmentedMessageOpen = true;
                    fragmentedMessageBytes = messageBytes;
                }
                rxFrames++; rxBytes += payload.length;
                if (payload.length > maxFrame) maxFrame = payload.length;
                boolean fragment = (opcode == OPCODE_CONTINUATION) || !fin;
                if (fragment) rxFragments++;
                if (WsBypassCore.DEBUG) {
                    if (fragment) {
                        WsBypassCore.dbg("recv FRAGMENT: op=" + opcode + " fin=" + fin + " len=" + payload.length
                                + " (frame#" + rxFrames + ", fragments=" + rxFragments + ", maxFrame=" + maxFrame + "B)"
                                + "  <-- WAS being dropped; now relayed");
                    } else if (rxFrames <= 4 || (rxFrames % 200) == 0) {
                        WsBypassCore.dbg("recv: frame#" + rxFrames + " op=" + opcode + " len=" + payload.length
                                + " totalRx=" + rxBytes + "B maxFrame=" + maxFrame + "B frags=" + rxFragments);
                    }
                }
                return payload;
            }
            if (WsBypassCore.DEBUG) WsBypassCore.dbg("recv: UNKNOWN opcode=" + opcode + " fin=" + fin + " len=" + payload.length);
        }
    }

    public boolean ping(byte[] payload) {
        return ping(payload, 15_000);
    }

    public boolean ping(byte[] payload, int timeoutMs) {
        try {
            writeFrame(buildFrame(OPCODE_PING, payload, true), timeoutMs);
            return true;
        } catch (Exception e) {
            try { close(); } catch (Exception ignored) {}
            return false;
        }
    }

    private static long tokenOf(byte[] payload) {
        if (payload == null || payload.length != 8) {
            return 0L;
        }
        long value = 0L;
        for (int i = 0; i < 8; i++) {
            value = (value << 8) | (payload[i] & 0xFFL);
        }
        return value;
    }

    public boolean alive(int timeoutMs) {
        if (closed.get()) {
            return false;
        }
        long token = System.nanoTime();
        if (token == 0L) {
            token = 1L;
        }
        final byte[] mark = new byte[8];
        for (int i = 7; i >= 0; i--) {
            mark[i] = (byte) (token >>> ((7 - i) * 8));
        }
        long deadline = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(Math.max(1, timeoutMs));
        if (!ping(mark, timeoutMs)) {
            return false;
        }
        try {
            final byte[] answer = recvInternal(token, deadline);
            boolean healthy = answer != null && answer.length == 0 && lastPongToken == token;
            if (!healthy) close();
            return healthy;
        } catch (Exception e) {
            close();
            return false;
        } finally {
            try {
                sock.setSoTimeout(IDLE_READ_TIMEOUT_MS);
            } catch (Exception ignored) {
            }
        }
    }

    public long sinceHeard() {
        return System.currentTimeMillis() - Math.max(lastPongMs, lastInboundMs);
    }

    public long sincePong() {
        return System.currentTimeMillis() - lastPongMs;
    }

    public void markPongNow() {
        lastPongMs = System.currentTimeMillis();
    }

    public double lastIoAge() {
        return (System.currentTimeMillis() - lastIoMs) / 1000.0;
    }

    public int getReadTimeoutSec() {
        return timeoutSec;
    }

    public boolean isClosed() {
        return closed.get();
    }

    public void close() {

        if (!closed.compareAndSet(false, true)) return;
        closeQuietly(transport);
        closeQuietly(sock);
        closeQuietly(reader);
        closeQuietly(writer);
    }

    private void writeControl(int opcode, byte[] payload) throws IOException {
        writeFrame(buildFrame(opcode, payload, true), WRITE_TIMEOUT_MS);
    }

    private void writeFrame(byte[] frame, int timeoutMs) throws IOException {
        if (closed.get()) throw new IOException("websocket closed");
        AtomicBoolean pending = new AtomicBoolean(true);
        ScheduledFuture<?> expiry = WRITE_DEADLINES.schedule(() -> {
            if (pending.compareAndSet(true, false)) close();
        }, Math.max(1, timeoutMs), TimeUnit.MILLISECONDS);
        try {
            synchronized (lock) {
                if (closed.get()) throw new IOException("websocket closed");
                writer.write(frame);
                writer.flush();
                if (!pending.compareAndSet(true, false) || closed.get()) {
                    throw new SocketTimeoutException("websocket write deadline");
                }
                lastIoMs = System.currentTimeMillis();
            }
        } catch (IOException e) {
            close();
            throw e;
        } finally {
            pending.set(false);
            expiry.cancel(false);
        }
    }

    static byte[] buildFrame(int opcode, byte[] payloadIn, boolean mask) {
        byte[] body = payloadIn == null ? new byte[0] : payloadIn;
        int length = body.length;
        if (length > MAX_WS_FRAME_BYTES) {
            throw new IllegalArgumentException("ws frame too large: " + length);
        }
        if ((opcode == OPCODE_CLOSE || opcode == OPCODE_PING || opcode == OPCODE_PONG) && length > 125) {
            throw new IllegalArgumentException("ws control frame too large");
        }
        int headerLen = 2;
        if (length >= 126 && length <= 0xFFFF) headerLen += 2;
        else if (length > 0xFFFF) headerLen += 8;
        if (mask) headerLen += 4;
        byte[] out = new byte[headerLen + length];
        int pos = 0;
        out[pos++] = (byte) (0x80 | (opcode & 0x0F));
        int maskBit = mask ? 0x80 : 0x00;
        if (length < 126) {
            out[pos++] = (byte) (maskBit | length);
        } else if (length <= 0xFFFF) {
            out[pos++] = (byte) (maskBit | 126);
            out[pos++] = (byte) ((length >>> 8) & 0xFF);
            out[pos++] = (byte) (length & 0xFF);
        } else {
            out[pos++] = (byte) (maskBit | 127);
            long ll = length & 0xFFFFFFFFL;
            for (int i = 7; i >= 0; i--) {
                out[pos++] = (byte) ((ll >>> (i * 8)) & 0xFF);
            }
        }
        if (mask) {
            byte[] mk = new byte[4];
            RNG.nextBytes(mk);
            out[pos++] = mk[0]; out[pos++] = mk[1]; out[pos++] = mk[2]; out[pos++] = mk[3];
            for (int i = 0; i < length; i++) {
                out[pos + i] = (byte) (body[i] ^ mk[i & 3]);
            }
        } else {
            System.arraycopy(body, 0, out, pos, length);
        }
        return out;
    }

    private static void xorMask(byte[] payload, byte[] mk) {
        for (int i = 0; i < payload.length; i++) {
            payload[i] = (byte) (payload[i] ^ mk[i & 3]);
        }
    }

    private void readExact(int n, int[] outAsUnsigned, long deadlineNanos) throws IOException {
        byte[] tmp = new byte[n];
        readExactBytes(tmp, deadlineNanos);
        for (int i = 0; i < n; i++) outAsUnsigned[i] = tmp[i] & 0xFF;
    }

    private void readExactBytes(byte[] buf, long deadlineNanos) throws IOException {
        int off = 0;
        while (off < buf.length) {
            if (deadlineNanos != 0L) {
                long left = deadlineNanos - System.nanoTime();
                if (left <= 0L) throw new SocketTimeoutException("pong deadline");
                sock.setSoTimeout((int) Math.max(1L, TimeUnit.NANOSECONDS.toMillis(left)));
            }
            int r = reader.read(buf, off, buf.length - off);
            if (r < 0) throw new IOException("unexpected eof");
            if (r == 0) continue;
            lastIoMs = System.currentTimeMillis();
            lastInboundMs = lastIoMs;
            if (deadlineNanos != 0L && System.nanoTime() >= deadlineNanos) {
                throw new SocketTimeoutException("pong deadline");
            }
            off += r;
        }
    }

    public static final class HandshakeException extends IOException {
        public final int statusCode;
        public final String location;
        HandshakeException(String message, int statusCode, String location) {
            super(location == null || location.isEmpty() ? message : message + " -> " + location);
            this.statusCode = statusCode;
            this.location = location;
        }
        public boolean isRedirect() {
            return statusCode == 301 || statusCode == 302 || statusCode == 303
                    || statusCode == 307 || statusCode == 308;
        }
    }

    private static InetAddress[] preferIpv4(InetAddress[] addresses) {
        InetAddress[] sorted = addresses.clone();
        java.util.Arrays.sort(sorted, (a, b) -> Boolean.compare(
                !(a instanceof java.net.Inet4Address), !(b instanceof java.net.Inet4Address)));
        return sorted;
    }

    private static InetAddress[] resolveUntil(String host, long deadlineNanos,
                                              ConnectPermit permit) throws IOException {
        CachedAddresses cached = DNS_CACHE.get(host);
        if (cached != null && !cached.usable()) {
            DNS_CACHE.remove(host, cached);
            cached = null;
        }
        final Future<InetAddress[]> future;
        try {
            future = DNS_EXECUTOR.submit(() -> {
                InetAddress[] found = InetAddress.getAllByName(host);
                if (found != null && found.length > 0) {
                    DNS_CACHE.put(host, new CachedAddresses(found));
                }
                return found;
            });
        } catch (java.util.concurrent.RejectedExecutionException saturated) {
            if (cached != null) return cached.addresses;
            throw new IOException("DNS resolver saturated for " + host, saturated);
        }
        final long staleAt = cached == null ? Long.MAX_VALUE
                : System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(DNS_STALE_WAIT_MS);
        boolean keepLookup = false;
        try {
            while (true) {
                checkPermit(permit);
                int remaining = remainingMillis(deadlineNanos);
                if (cached != null) {
                    long toStale = staleAt - System.nanoTime();
                    if (toStale <= 0) {
                        keepLookup = true;
                        return cached.addresses;
                    }
                    remaining = (int) Math.min(remaining, Math.max(1L, TimeUnit.NANOSECONDS.toMillis(toStale)));
                }
                int slice = permit == null ? remaining : Math.min(remaining, 100);
                try {
                    InetAddress[] result = future.get(slice, TimeUnit.MILLISECONDS);
                    checkPermit(permit);
                    if (result == null || result.length == 0) {
                        throw new IOException("no address for " + host);
                    }
                    return result;
                } catch (java.util.concurrent.TimeoutException e) {
                    if (cached == null && slice >= remaining) {
                        throw new SocketTimeoutException("DNS timeout for " + host);
                    }
                }
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new IOException("DNS interrupted for " + host, e);
        } catch (java.util.concurrent.ExecutionException e) {
            if (cached != null) return cached.addresses;
            Throwable cause = e.getCause();
            if (cause instanceof IOException) throw (IOException) cause;
            throw new IOException("DNS failed for " + host, cause);
        } finally {
            if (!keepLookup) future.cancel(true);
        }
    }

    private static void checkPermit(ConnectPermit permit) throws IOException {
        if (permit != null && !permit.isCurrent()) {
            throw new IOException("websocket connect cancelled");
        }
    }

    private static int remainingMillis(long deadlineNanos) throws SocketTimeoutException {
        long remaining = deadlineNanos - System.nanoTime();
        if (remaining <= 0) throw new SocketTimeoutException("websocket deadline exceeded");
        return (int) Math.max(1L, Math.min(Integer.MAX_VALUE,
                TimeUnit.NANOSECONDS.toMillis(remaining) + 1L));
    }

    private static int parseStatusCode(String statusLine) {
        if (statusLine == null) return 0;
        String[] parts = statusLine.trim().split(" +", 3);
        if (parts.length < 2 || !"HTTP/1.1".equalsIgnoreCase(parts[0])) return 0;
        try { return Integer.parseInt(parts[1]); } catch (NumberFormatException e) { return 0; }
    }

    private static boolean containsToken(String value, String wanted) {
        if (value == null) return false;
        for (String token : value.split(",")) {
            if (wanted.equalsIgnoreCase(token.trim())) return true;
        }
        return false;
    }

    private static String websocketAccept(String key) throws IOException {
        try {
            byte[] digest = MessageDigest.getInstance("SHA-1")
                    .digest((key + WS_ACCEPT_GUID).getBytes(StandardCharsets.US_ASCII));
            return Base64.getEncoder().encodeToString(digest);
        } catch (Exception e) {
            throw new IOException("websocket accept hash failed", e);
        }
    }

    private static boolean containsCrLf(String value) {
        return value.indexOf('\r') >= 0 || value.indexOf('\n') >= 0;
    }

    private static boolean validExtraHeader(String name, String value) {
        if (name == null || value == null || name.isEmpty() || containsCrLf(value)) return false;
        for (int i = 0; i < name.length(); i++) {
            char c = name.charAt(i);
            if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z')
                    && !(c >= '0' && c <= '9') && c != '-') return false;
        }
        String lower = name.toLowerCase(Locale.ROOT);
        return !"host".equals(lower) && !"connection".equals(lower) && !"upgrade".equals(lower)
                && !lower.startsWith("sec-websocket-");
    }

    private static String readLine(InputStream in, int[] totalBytes) throws IOException {

        java.io.ByteArrayOutputStream baos = new java.io.ByteArrayOutputStream(128);
        while (true) {
            int b = in.read();
            if (b < 0) {
                if (baos.size() == 0) return null;
                break;
            }
            totalBytes[0]++;
            if (totalBytes[0] > MAX_HTTP_HEADER_BYTES) {
                throw new IOException("websocket handshake headers too large");
            }
            if (b == '\n') {
                byte[] arr = baos.toByteArray();
                int len = arr.length;
                if (len > 0 && arr[len - 1] == '\r') len--;
                return new String(arr, 0, len, StandardCharsets.UTF_8);
            }
            if (baos.size() >= MAX_HTTP_HEADER_LINE_BYTES) {
                throw new IOException("websocket handshake header line too large");
            }
            baos.write(b);
        }
        return new String(baos.toByteArray(), StandardCharsets.UTF_8);
    }

    private static void closeQuietly(java.io.Closeable c) {
        if (c == null) return;
        try { c.close(); } catch (Exception ignored) {}
    }

    private static void closeQuietly(Socket s) {
        if (s == null) return;
        try { s.close(); } catch (Exception ignored) {}
    }
}

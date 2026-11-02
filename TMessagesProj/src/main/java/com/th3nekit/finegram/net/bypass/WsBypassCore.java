/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.bypass;

import org.telegram.messenger.FileLog;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

public final class WsBypassCore {

    public static final String LOCAL_PROXY_HOST = "127.0.0.1";

    private volatile long lastBridgeOkAtMs = 0L;
    public long getLastBridgeOkAtMs() { return lastBridgeOkAtMs; }
    private final Object bridgeStateLock = new Object();
    private int activeBridges;
    private long bridgeGeneration;
    public boolean hasActiveBridge() {
        synchronized (bridgeStateLock) {
            return activeBridges > 0;
        }
    }
    private long getBridgeGeneration() {
        synchronized (bridgeStateLock) {
            return bridgeGeneration;
        }
    }
    private boolean isBridgeGenerationCurrent(long generation) {
        synchronized (bridgeStateLock) {
            return generation == bridgeGeneration;
        }
    }
    private void markBridgeOk(long generation) {
        synchronized (bridgeStateLock) {
            if (generation == bridgeGeneration && activeBridges > 0) {
                lastBridgeOkAtMs = System.currentTimeMillis();
            }
        }
    }
    private boolean markBridgeStarted(long generation) {
        synchronized (bridgeStateLock) {
            if (generation != bridgeGeneration) {
                return false;
            }
            if (activeBridges++ == 0) {
                lastBridgeOkAtMs = 0L;
            }
            return true;
        }
    }
    private void markBridgeStopped(long generation) {
        synchronized (bridgeStateLock) {
            if (generation != bridgeGeneration) {
                return;
            }
            if (activeBridges > 0) {
                activeBridges--;
            }
            if (activeBridges == 0) {
                lastBridgeOkAtMs = 0L;
            }
        }
    }
    private void invalidateBridgeGeneration() {
        synchronized (bridgeStateLock) {
            bridgeGeneration++;
            activeBridges = 0;
            lastBridgeOkAtMs = 0L;
        }
    }

    static volatile boolean DEBUG = org.telegram.messenger.BuildVars.LOGS_ENABLED;   
    static final java.util.concurrent.atomic.AtomicInteger CONN_SEQ = new java.util.concurrent.atomic.AtomicInteger();

    static void decodeMtproto(int connId, String dir, byte[] plain) {
        if (!DEBUG || plain == null || plain.length < 4) return;
        try {
            StringBuilder hex = new StringBuilder();
            for (int i = 0; i < Math.min(40, plain.length); i++) hex.append(String.format("%02x", plain[i] & 0xFF));
            
            long flen = (plain[0] & 0xFFL) | ((plain[1] & 0xFFL) << 8) | ((plain[2] & 0xFFL) << 16) | ((plain[3] & 0xFFL) << 24);
            String interp;
            if (flen == 4 && plain.length >= 8) {
                int err = (plain[4] & 0xFF) | ((plain[5] & 0xFF) << 8) | ((plain[6] & 0xFF) << 16) | ((plain[7] & 0xFF) << 24);
                interp = "TRANSPORT ERROR code=" + err;
            } else if (plain.length >= 28) {
                boolean authZero = true;
                for (int i = 4; i < 12; i++) if (plain[i] != 0) { authZero = false; break; }
                if (authZero) {
                    long cons = (plain[24] & 0xFFL) | ((plain[25] & 0xFFL) << 8) | ((plain[26] & 0xFFL) << 16) | ((plain[27] & 0xFFL) << 24);
                    String name = cons == 0x05162463L ? "resPQ" : cons == 0xd0e8075cL ? "server_DH_params_ok"
                            : cons == 0x79cb045dL ? "server_DH_params_fail" : cons == 0x3bcbf734L ? "dh_gen_ok"
                            : cons == 0xbe7e8ef1L ? "req_pq_multi" : cons == 0xd712e4beL ? "req_DH_params"
                            : cons == 0xf5045f1fL ? "set_client_DH_params" : String.format("0x%08x", cons);
                    interp = "UNENCRYPTED auth msg: " + name + " (flen=" + flen + ")";
                } else {
                    interp = "ENCRYPTED (auth_key set, flen=" + flen + ") — session active";
                }
            } else {
                interp = "flen=" + flen + " short";
            }
            dbg("conn#" + connId + " " + dir + " mtproto: " + interp + " | hex=" + hex);
        } catch (Throwable ignored) {}
    }

    static volatile boolean SPLIT_UP = false;
    static void dbg(String msg) {
        if (!DEBUG) return;
        try { android.util.Log.i("NMWSBYPASS", msg); } catch (Throwable ignored) {}
        try { FileLog.d("wsbypass: " + msg); } catch (Throwable ignored) {}
    }

    private static final double WS_BLACKLIST_TTL_SEC = 420.0;
    private static final long WS_ROUTE_DEADLINE_MS = 9_000L;
    private static final long KEEPALIVE_IDLE_MS = 25_000L;

    private static final long KEEPALIVE_DEAD_MS = 40_000L;
    private static final long STALL_PROBE_MS = 3_500L;
    private static final long STALL_PONG_MS = 2_500L;
    private static final int STALL_PING_WRITE_MS = 5_000;
    private static final long BULK_WINDOW_MS = 1_000L;
    private static final long BRIDGE_TICK_MS = 200L;
    private static final long BULK_BYTES = 32L * 1024L;
    private static final long BULK_HOLD_MS = 8_000L;
    private static final long BULK_HOLD_MAX_MS = 30_000L;
    private static final long SLOW_UPLINK_BYTES_PER_SEC = 16L * 1024L;

    private static final long WS_RELAY_BUDGET_MS = 3_500L;
    private static final long WS_TGWS_BUDGET_MS = 4_000L;
    private static final long ROUTE_HEDGE_MS = 900L;
    private static final long RELAY_HOST_MIN_BUDGET_MS = 2_500L;
    private static final long RELAY_BACKOFF_BASE_MS = 5_000L;
    private static final long RELAY_BACKOFF_MAX_MS = 60_000L;
    private static final long WS_POOL_KEEPER_INTERVAL_SEC = 30L;

    private static final int SOCK_RCVBUF = 1024 * 1024;
    private static final int SOCK_SNDBUF = 1024 * 1024;
    private static final int RECV_CHUNK = 256 * 1024;
    private static final int UPLOAD_FIRST_FRAME_BYTES = 16 * 1024;
    private static final int FIRST_FRAME_PEEK_MS = 150;
    private static final byte[] NO_DATA = new byte[0];
    private static final int ACCEPT_TIMEOUT_MS = 1_000;
    private static final int HANDSHAKE_READ_TIMEOUT_MS = 10_000;
    private static final int LISTEN_BACKLOG = 64;

    private static final int MAX_HANDLER_THREADS = 512;

    private static volatile WsBypassCore instance;

    public static WsBypassCore getInstance() {
        WsBypassCore local = instance;
        if (local == null) {
            synchronized (WsBypassCore.class) {
                local = instance;
                if (local == null) {
                    local = new WsBypassCore();
                    instance = local;
                }
            }
        }
        return local;
    }

    private final Object lifecycleLock = new Object();
    private volatile boolean running;
    private volatile int port;
    private volatile ServerSocket listener;
    private volatile Thread acceptThread;
    private volatile ExecutorService handlerPool;
    private volatile ExecutorService warmPool;
    private volatile ScheduledExecutorService keeperPool;
    private volatile ScheduledFuture<?> keeperTask;
    private volatile byte[] secretBytes = new byte[0];
    private volatile String secretHex = "";

    private final Object cfgLock = new Object();
    private final Map<Long, Long> failUntilMs = new HashMap<>();
    private final Map<Long, Integer> failCount = new HashMap<>();
    private final Map<Long, Long> blacklistUntilMs = new HashMap<>();
    private final Set<Long> blacklist = new HashSet<>();
    private final Map<Long, String> wsDomainPref = new HashMap<>();
    private final Map<String, Long> relayFailUntilMs = new HashMap<>();
    private final Map<String, Integer> relayFailCount = new HashMap<>();
    private final Map<String, Long> relayHealthVersions = new HashMap<>();
    private final Map<RawWebSocket, RelayRoute> relayRoutes = new java.util.WeakHashMap<>();

    private static final class RelayRoute {
        final String host;
        final long generation;
        final long healthVersion;

        RelayRoute(String host, long generation, long healthVersion) {
            this.host = host;
            this.generation = generation;
            this.healthVersion = healthVersion;
        }
    }

    private final Object trackedLock = new Object();
    private final Set<Object> tracked = new HashSet<>();

    private final WebSocketPool wsPool = new WebSocketPool();

    public boolean isRunning() {
        return running;
    }

    public int getPort() {
        return port;
    }

    public static boolean ownsProxy(String address, int localPort) {
        WsBypassCore core = getInstance();
        return core.running && localPort != 0 && localPort == core.port && LOCAL_PROXY_HOST.equals(address);
    }

    public void measurePing(int dc, java.util.function.LongConsumer done) {
        new Thread(() -> {
            long rtt = -1;
            try {
                rtt = wsPool.measure(dc);
            } catch (Throwable t) {
                FileLog.e(t);
            }
            done.accept(rtt);
        }, "fg-bypass-ping").start();
    }

    public boolean isAcceptThreadAlive() {
        if (!running) return false;
        if (listener == null) return false;
        Thread at = acceptThread;
        return at != null && at.isAlive();
    }

    public String getSecretHex() {
        return secretHex;
    }

    public synchronized String start(int desiredPort, String secretHexIn) {
        synchronized (lifecycleLock) {
            try {
                stopLocked();

                String sec = MtprotoHandshake.validSecretHex(secretHexIn);
                if (sec == null || sec.isEmpty()) {
                    sec = MtprotoHandshake.generateSecretHex();
                }
                byte[] secBytes = MtprotoHandshake.toBytes16(sec);
                if (secBytes == null) {
                    return "invalid secret";
                }

                int bindPort = desiredPort;
                if (bindPort <= 0 || bindPort > 65535) {
                    bindPort = 0;
                }

                ServerSocket srv = new ServerSocket();
                try {
                    srv.setReuseAddress(true);
                } catch (Throwable ignored) {}
                try {
                    srv.setReceiveBufferSize(SOCK_RCVBUF);
                } catch (Throwable ignored) {}
                try {
                    srv.bind(new InetSocketAddress(InetAddress.getByName(LOCAL_PROXY_HOST), bindPort), LISTEN_BACKLOG);
                } catch (IOException ex) {
                    
                    if (bindPort != 0) {
                        try { srv.close(); } catch (Throwable ignored) {}
                        srv = new ServerSocket();
                        try { srv.setReuseAddress(true); } catch (Throwable ignored2) {}
                        try { srv.setReceiveBufferSize(SOCK_RCVBUF); } catch (Throwable ignored2) {}
                        try {
                            srv.bind(new InetSocketAddress(InetAddress.getByName(LOCAL_PROXY_HOST), 0), LISTEN_BACKLOG);
                        } catch (IOException ex2) {
                            try { srv.close(); } catch (Throwable ignored2) {}
                            return ex2.getMessage() == null ? "bind failed" : ex2.getMessage();
                        }
                    } else {
                        try { srv.close(); } catch (Throwable ignored) {}
                        return ex.getMessage() == null ? "bind failed" : ex.getMessage();
                    }
                }
                srv.setSoTimeout(ACCEPT_TIMEOUT_MS);

                this.listener = srv;
                this.port = srv.getLocalPort();
                this.secretHex = sec;
                this.secretBytes = secBytes;

                synchronized (cfgLock) {
                    failUntilMs.clear();
                    failCount.clear();
                    blacklist.clear();
                    blacklistUntilMs.clear();
                }

                ThreadPoolExecutor pool = new ThreadPoolExecutor(
                        0, MAX_HANDLER_THREADS,
                        60L, TimeUnit.SECONDS,
                        new java.util.concurrent.SynchronousQueue<Runnable>(),
                        new ThreadFactory() {
                            @Override
                            public Thread newThread(Runnable r) {
                                Thread t = new Thread(r, "wsbypass-handler");
                                t.setDaemon(true);
                                return t;
                            }
                        },
                        new ThreadPoolExecutor.AbortPolicy());
                this.handlerPool = pool;
                this.warmPool = new ThreadPoolExecutor(2, 2, 30L, TimeUnit.SECONDS,
                        new java.util.concurrent.ArrayBlockingQueue<>(16), r -> {
                            Thread t = new Thread(r, "wsbypass-warm");
                            t.setDaemon(true);
                            return t;
                        }, new ThreadPoolExecutor.AbortPolicy());

                this.keeperPool = Executors.newSingleThreadScheduledExecutor(new ThreadFactory() {
                    @Override
                    public Thread newThread(Runnable r) {
                        Thread t = new Thread(r, "wsbypass-keeper");
                        t.setDaemon(true);
                        return t;
                    }
                });
                this.keeperTask = keeperPool.scheduleAtFixedRate(new Runnable() {
                    @Override
                    public void run() {
                        try {
                            wsPool.healthScan();
                        } catch (Throwable t) {
                            FileLog.e(t);
                        }
                    }
                }, WS_POOL_KEEPER_INTERVAL_SEC, WS_POOL_KEEPER_INTERVAL_SEC, TimeUnit.SECONDS);

                this.running = true;
                final long generation = getBridgeGeneration();
                Thread accept = new Thread(new Runnable() {
                    @Override
                    public void run() {
                        acceptLoop(generation);
                    }
                }, "wsbypass-accept");
                accept.setDaemon(true);
                this.acceptThread = accept;
                accept.start();

                final int lastDc = FGBypassConfig.lastDc;
                if (POOL_ENABLED && lastDc > 0) {
                    warmUp(lastDc, false, generation);
                }

                FileLog.d("WsBypassCore started on " + LOCAL_PROXY_HOST + ":" + this.port);
                return "";
            } catch (Throwable t) {
                FileLog.e("WsBypassCore.start failed", t);
                try { stopLocked(); } catch (Throwable ignored) {}
                return t.getMessage() == null ? "start failed" : t.getMessage();
            }
        }
    }

    public void stop() {
        synchronized (lifecycleLock) {
            stopLocked();
        }
    }

    public void wakeUp() {
        if (!running) {
            return;
        }
        resetResilienceState();
        try {
            wsPool.healthScan();
        } catch (Throwable ignored) {
        }
    }

    public void onNetworkChanged() {
        if (!running) {
            return;
        }
        resetResilienceState();
        wsPool.clear();
        final int lastDc = FGBypassConfig.lastDc;
        if (POOL_ENABLED && lastDc > 0) {
            warmUp(lastDc, false, getBridgeGeneration());
        }
    }

    public void resetResilienceState() {
        synchronized (cfgLock) {
            failUntilMs.clear();
            failCount.clear();
            blacklist.clear();
            blacklistUntilMs.clear();
            relayFailUntilMs.clear();
            relayFailCount.clear();
        }
        TgwsRoute.clearCooldowns();
    }

    private void stopLocked() {
        running = false;
        invalidateBridgeGeneration();
        ServerSocket s = listener;
        listener = null;
        if (s != null) {
            try { s.close(); } catch (Throwable ignored) {}
        }
        List<Object> retired = detachTracked();
        retired.addAll(wsPool.detachAll());

        ScheduledFuture<?> kt = keeperTask;
        keeperTask = null;
        if (kt != null) kt.cancel(false);

        ScheduledExecutorService ksp = keeperPool;
        keeperPool = null;
        if (ksp != null) ksp.shutdownNow();

        ExecutorService hp = handlerPool;
        handlerPool = null;
        if (hp != null) hp.shutdownNow();
        ExecutorService wp = warmPool;
        warmPool = null;
        if (wp != null) wp.shutdownNow();
        warming.clear();

        Thread at = acceptThread;
        acceptThread = null;
        WsSocketCleanup.close(retired, at);
    }

    private void acceptLoop(long generation) {
        try {
            while (running && isBridgeGenerationCurrent(generation)) {
                ServerSocket srv = listener;
                if (srv == null) break;
                
                try {
                    Socket conn;
                    try {
                        conn = srv.accept();
                    } catch (SocketTimeoutException to) {
                        continue;
                    } catch (IOException ex) {
                        if (!running) break;
                        continue;
                    }
                    try {
                        try { conn.setTcpNoDelay(true); } catch (Throwable ignored) {}
                        try {
                            conn.setReceiveBufferSize(SOCK_RCVBUF);
                            conn.setSendBufferSize(SOCK_SNDBUF);
                        } catch (Throwable ignored) {}
                    } catch (Throwable ignored) {}

                    dbg("accept: tgnet connected to local proxy from " + conn.getRemoteSocketAddress());
                    if (!trackIfGenerationCurrent(conn, generation)) {
                        closeQuietly(conn);
                        continue;
                    }
                    final Socket fc = conn;
                    ExecutorService pool = handlerPool;
                    if (pool == null) {
                        untrack(fc);
                        closeQuietly(fc);
                        break;
                    }
                    try {
                        pool.submit(new Runnable() {
                            @Override
                            public void run() {
                                handleClient(fc, generation);
                            }
                        });
                    } catch (Throwable t) {
                        untrack(fc);
                        closeQuietly(fc);
                    }
                } catch (Throwable iter) {
                    FileLog.e("WsBypassCore.acceptLoop iteration error", iter);
                    
                    try { Thread.sleep(50); } catch (InterruptedException ie) { Thread.currentThread().interrupt(); break; }
                }
            }
        } finally {
            
            if (Thread.currentThread() == acceptThread) {
                running = false;
                FileLog.d("WsBypassCore.acceptLoop exited; running cleared");
            }
        }
    }

    private void handleClient(Socket conn, long generation) {
        try {
            if (!isBridgeGenerationCurrent(generation)) {
                return;
            }
            final byte[] connectionSecret = secretBytes;
            conn.setSoTimeout(HANDSHAKE_READ_TIMEOUT_MS);
            byte[] initPacket = recvExact(conn, MtprotoHandshake.HANDSHAKE_LEN);
            conn.setSoTimeout(0);
            if (!isBridgeGenerationCurrent(generation)) {
                return;
            }

            MtprotoHandshake.HandshakeResult hr = MtprotoHandshake.tryHandshake(initPacket, connectionSecret);
            if (hr == null) { dbg("handshake: FAILED to parse tgnet init (" + initPacket.length + "B) — secret mismatch?"); return; }

            int dc = hr.dcId;
            boolean isMedia = hr.isMedia;
            FGBypassConfig.rememberDc(dc);
            dbg("handshake OK: dc=" + dc + " media=" + isMedia + " kind=" + hr.kind
                    + " protoTag=" + org.telegram.messenger.Utilities.bytesToHex(hr.protoTag));
            int relayDcIdx = isMedia ? -dc : dc;
            byte[] relayInit = MtprotoHandshake.generateRelayInit(hr.protoTag, relayDcIdx);
            CryptoCtx ctx = CryptoCtx.build(hr.decPrekeyIv, connectionSecret, relayInit);
            if (ctx == null) return;
            if (!isBridgeGenerationCurrent(generation)) return;

            long dcKey = poolKey(dc, isMedia);
            final long routeDeadline = System.nanoTime()
                    + TimeUnit.MILLISECONDS.toNanos(WS_ROUTE_DEADLINE_MS);

            byte[] firstPlain = NO_DATA;
            final boolean mixed = FGBypassConfig.mixedRouting();
            boolean upload = hr.kind == MtprotoHandshake.KIND_UPLOAD;
            boolean fileTransfer = isMedia || upload || hr.kind == MtprotoHandshake.KIND_DOWNLOAD;
            if (mixed && hr.kind == MtprotoHandshake.KIND_UNKNOWN && !isMedia) {
                firstPlain = peekFirstPlain(conn, ctx);
                upload = firstFrameLength(firstPlain, hr.protoTag) >= UPLOAD_FIRST_FRAME_BYTES;
                fileTransfer = upload;
            }
            if (mixed && fileTransfer) {
                dbg("mixed: " + (upload ? "отправка" : "загрузка") + " файла (dc=" + dc + ")");
            }
            final boolean preferTgws = preferTgws(fileTransfer);
            final boolean usePool = !mixed || isMedia || !fileTransfer;
            RawWebSocket ws = openRoute(dc, isMedia, routeDeadline, generation, preferTgws, usePool);
            if (ws == null) {
                dbg("route: ни одна дорога не открылась (dc=" + dc + ")");
                return;
            }

            if (!trackIfGenerationCurrent(ws, generation)) {
                try { ws.close(); } catch (Throwable ignored) {}
                return;
            }
            if (!runIfBridgeGenerationCurrent(generation, () -> failClear(dcKey))) {
                try { ws.close(); } catch (Throwable ignored) {}
                untrack(ws);
                return;
            }
            try {
                if (!isBridgeGenerationCurrent(generation)) {
                    try { ws.close(); } catch (Throwable ignored) {}
                    untrack(ws);
                    return;
                }
                try {
                    ws.send(relayInit);
                } catch (IOException ex) {
                    recordRelayFailure(ws, generation, ex);
                    throw ex;
                }
                MsgSplitter splitter = null;
                if (SPLIT_UP) {
                    try {
                        splitter = new MsgSplitter(relayInit, hr.protoInt);
                    } catch (Throwable t) {
                        splitter = null;
                    }
                }
                dbg("bridge: started (dc=" + dc + "), relayInit " + relayInit.length + "B sent, splitter=" + (splitter != null));
                bridgeWs(conn, ws, ctx, splitter, generation, firstPlain, upload);
            } catch (Throwable t) {
                try { ws.close(); } catch (Throwable ignored) {}
                untrack(ws);
            }
        } catch (Throwable t) {
            
        } finally {
            closeQuietly(conn);
            untrack(conn);
        }
    }

    private static boolean preferTgws(boolean fileTransfer) {
        return FGBypassConfig.mixedRouting() ? fileTransfer : FGBypassConfig.tgwsFirst();
    }

    private byte[] peekFirstPlain(Socket conn, CryptoCtx ctx) throws IOException {
        conn.setSoTimeout(FIRST_FRAME_PEEK_MS);
        try {
            final byte[] buf = new byte[RECV_CHUNK];
            final int n = conn.getInputStream().read(buf);
            if (n < 0) {
                throw new java.io.EOFException("client closed before the first frame");
            }
            if (n == 0) {
                return NO_DATA;
            }
            return cipherUpdate(ctx.cltDec, Arrays.copyOf(buf, n));
        } catch (java.net.SocketTimeoutException timeout) {
            return NO_DATA;
        } finally {
            conn.setSoTimeout(0);
        }
    }

    private static int firstFrameLength(byte[] plain, byte[] protoTag) {
        if (plain == null || plain.length == 0) {
            return 0;
        }
        if (Arrays.equals(protoTag, MtprotoHandshake.PROTO_TAG_ABRIDGED)) {
            final int head = plain[0] & 0x7F;
            if (head < 0x7F) {
                return head * 4;
            }
            if (plain.length < 4) {
                return 0;
            }
            return ((plain[1] & 0xFF) | (plain[2] & 0xFF) << 8 | (plain[3] & 0xFF) << 16) * 4;
        }
        if (plain.length < 4) {
            return 0;
        }
        return (plain[0] & 0xFF) | (plain[1] & 0xFF) << 8 | (plain[2] & 0xFF) << 16 | (plain[3] & 0x7F) << 24;
    }

    private static void sendUp(RawWebSocket ws, MsgSplitter splitter, byte[] data,
                               java.util.concurrent.atomic.AtomicLong upSent) throws IOException {
        if (data == null || data.length == 0) {
            return;
        }
        if (splitter != null) {
            final List<byte[]> parts = splitter.split(data);
            if (parts == null || parts.isEmpty()) {
                return;
            }
            if (parts.size() > 1) {
                ws.sendBatch(parts);
            } else {
                ws.send(parts.get(0));
            }
            for (byte[] p : parts) {
                upSent.addAndGet(p.length);
            }
        } else {
            ws.send(data);
            upSent.addAndGet(data.length);
        }
    }

    private RawWebSocket openRoute(int dc, boolean isMedia, long routeDeadline, long generation,
                                   boolean preferTgws, boolean usePool) {
        if (POOL_ENABLED && usePool) {
            RawWebSocket warm;
            while ((warm = wsPool.get(dc, isMedia)) != null) {
                if (isRouteUsable(warm, generation) && !TgwsRoute.isPaused(warm.routeTag())) {
                    dbg("route: из тёплого запаса (dc=" + dc + ")");
                    warmUp(dc, isMedia, generation);
                    return warm;
                }
                try { warm.close(); } catch (Throwable ignored) {}
            }
        }

        RawWebSocket ws = raceRoutes(dc, isMedia, routeDeadline, generation, preferTgws);
        if (ws == null) {
            ws = tryDirect(dc, isMedia, routeDeadline, generation);
        }
        if (POOL_ENABLED && ws != null) {
            warmUp(dc, isMedia, generation);
        }
        return ws;
    }

    private RawWebSocket raceRoutes(int dc, boolean isMedia, long routeDeadline, long generation,
                                    boolean tgwsFirst) {
        final ExecutorService executor = handlerPool;
        if (executor == null) {
            RawWebSocket ws = tgwsFirst
                    ? tryTgws(dc, routeDeadline, generation, false)
                    : tryRelay(dc, isMedia, routeDeadline, generation, false);
            if (ws == null) {
                ws = tgwsFirst
                        ? tryRelay(dc, isMedia, routeDeadline, generation, true)
                        : tryTgws(dc, routeDeadline, generation, true);
            }
            return ws;
        }
        final RouteRace race = new RouteRace();
        final Runnable relay = () -> race.deliver(tryRelay(dc, isMedia, routeDeadline, generation, true),
                dc, isMedia, generation);
        final Runnable tgws = () -> race.deliver(tryTgws(dc, routeDeadline, generation, true),
                dc, isMedia, generation);
        if (!race.start(executor, tgwsFirst ? tgws : relay)) {
            return null;
        }
        final long hedgeAt = Math.min(routeDeadline,
                System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(ROUTE_HEDGE_MS));
        RawWebSocket ws = race.await(hedgeAt, 1);
        if (ws != null) {
            return ws;
        }
        if (isBridgeGenerationCurrent(generation) && System.nanoTime() < routeDeadline) {
            race.start(executor, tgwsFirst ? relay : tgws);
        }
        return race.finish(routeDeadline);
    }

    private final class RouteRace {
        private final Object lock = new Object();
        private RawWebSocket winner;
        private int started;
        private int finished;
        private boolean abandoned;

        boolean start(ExecutorService executor, Runnable route) {
            synchronized (lock) {
                started++;
            }
            try {
                executor.execute(route);
                return true;
            } catch (Throwable t) {
                synchronized (lock) {
                    started--;
                    lock.notifyAll();
                }
                return false;
            }
        }

        void deliver(RawWebSocket ws, int dc, boolean isMedia, long generation) {
            synchronized (lock) {
                finished++;
                if (ws != null && winner == null && !abandoned) {
                    winner = ws;
                    ws = null;
                }
                lock.notifyAll();
            }
            if (ws != null) {
                keepSpare(ws, dc, isMedia, generation);
            }
        }

        RawWebSocket await(long untilNanos, int enoughFinished) {
            synchronized (lock) {
                while (winner == null && finished < enoughFinished && finished < started) {
                    long left = untilNanos - System.nanoTime();
                    if (left <= 0) break;
                    try {
                        TimeUnit.NANOSECONDS.timedWait(lock, left);
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                        break;
                    }
                }
                return winner;
            }
        }

        RawWebSocket finish(long untilNanos) {
            synchronized (lock) {
                while (winner == null && finished < started) {
                    long left = untilNanos - System.nanoTime();
                    if (left <= 0) break;
                    try {
                        TimeUnit.NANOSECONDS.timedWait(lock, left);
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                        break;
                    }
                }
                if (winner == null) {
                    abandoned = true;
                }
                return winner;
            }
        }
    }

    private void keepSpare(RawWebSocket ws, int dc, boolean isMedia, long generation) {
        if (POOL_ENABLED && !FGBypassConfig.mixedRouting() && runIfBridgeGenerationCurrent(generation, () -> {
            if (isRouteUsable(ws, generation) && wsPool.hasRoom(dc, isMedia, WARM_TARGET)) {
                wsPool.put(dc, isMedia, ws);
            } else {
                try { ws.close(); } catch (Throwable ignored) {}
            }
        })) {
            return;
        }
        try { ws.close(); } catch (Throwable ignored) {}
    }

    private static final boolean POOL_ENABLED = true;

    private final java.util.Set<String> warming = java.util.concurrent.ConcurrentHashMap.newKeySet();

    private static final long WARM_BUDGET_MS = 15_000L;

    private static final int WARM_TARGET = 3;

    private void warmUp(int dc, boolean isMedia, long generation) {
        final String key = generation + ":" + poolKey(dc, isMedia);
        ExecutorService executor = warmPool;
        if (executor == null || !running || !isBridgeGenerationCurrent(generation)) return;
        if (!warming.add(key)) return;
        try {
            executor.execute(() -> {
                try {
                    for (int attempt = 0; attempt < WARM_TARGET; attempt++) {
                        if (!wsPool.hasRoom(dc, isMedia, WARM_TARGET)) break;
                        if (!isBridgeGenerationCurrent(generation) || !running) return;
                        final long deadline = System.nanoTime()
                                + TimeUnit.MILLISECONDS.toNanos(WARM_BUDGET_MS);
                        final RawWebSocket ws = openRouteOnce(dc, isMedia, deadline, generation);
                        if (ws == null) return;
                        if (!isBridgeGenerationCurrent(generation)) {
                            try { ws.close(); } catch (Throwable ignored) {}
                            return;
                        }
                        if (!runIfBridgeGenerationCurrent(generation, () -> {
                            if (isRouteUsable(ws, generation)) {
                                wsPool.put(dc, isMedia, ws);
                            } else {
                                try { ws.close(); } catch (Throwable ignored) {}
                            }
                        })) {
                            try { ws.close(); } catch (Throwable ignored) {}
                            return;
                        }
                        dbg("прогрев: в запасе " + wsPool.size(dc, isMedia) + " (dc=" + dc + ")");
                    }
                } catch (Throwable t) {
                    dbg("прогрев не удался (dc=" + dc + "): " + t.getMessage());
                } finally {
                    warming.remove(key);
                }
            });
        } catch (java.util.concurrent.RejectedExecutionException ignored) {
            warming.remove(key);
        }
    }

    private RawWebSocket openRouteOnce(int dc, boolean isMedia, long deadline, long generation) {
        final boolean tgwsFirst = preferTgws(isMedia);
        RawWebSocket ws = tgwsFirst
                ? tryTgws(dc, deadline, generation, true)
                : tryRelay(dc, isMedia, deadline, generation, true);
        if (ws == null) {
            ws = tgwsFirst
                    ? tryRelay(dc, isMedia, deadline, generation, true)
                    : tryTgws(dc, deadline, generation, true);
        }
        return ws;
    }

    private RawWebSocket tryRelay(int dc, boolean isMedia, long routeDeadline,
                                  long generation, boolean last) {
        long deadline = last ? routeDeadline : Math.min(routeDeadline,
                System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(WS_RELAY_BUDGET_MS));
        try {
            RawWebSocket ws = connectWsCf(dc, isMedia, deadline, generation);
            dbg("route: наш сервер OK (dc=" + dc + ")");
            return ws;
        } catch (Throwable ex) {
            dbg("route: наш сервер не ответил (dc=" + dc + "): "
                    + ex.getClass().getSimpleName() + ": " + ex.getMessage());
            return null;
        }
    }

    private RawWebSocket tryTgws(int dc, long routeDeadline, long generation, boolean last) {
        long deadline = last ? routeDeadline : Math.min(routeDeadline,
                System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(WS_TGWS_BUDGET_MS));
        try {
            RawWebSocket ws = TgwsRoute.connect(dc, deadline,
                    () -> isBridgeGenerationCurrent(generation));
            dbg("route: площадки Flowseal OK (dc=" + dc + ")");
            return ws;
        } catch (Throwable ex) {
            dbg("route: площадки Flowseal закрыты (dc=" + dc + "): " + ex.getMessage());
            return null;
        }
    }

    private RawWebSocket tryDirect(int dc, boolean isMedia, long routeDeadline, long generation) {
        if (System.nanoTime() >= routeDeadline) return null;
        try {
            RawWebSocket ws = TgwsRoute.connectLastResort(dc, isMedia, routeDeadline,
                    () -> isBridgeGenerationCurrent(generation));
            dbg("route: прямо к Telegram OK (dc=" + dc + ")");
            return ws;
        } catch (Throwable ex) {
            dbg("route: прямо к Telegram закрыто (dc=" + dc + "): " + ex.getMessage());
            return null;
        }
    }

    private RawWebSocket connectWsCf(int dc, boolean isMedia, long deadlineNanos,
                                     long generation) throws IOException {
        IOException last = null;
        
        final String path = DomainPool.relayPathForDc(dc);
        
        java.util.Map<String, String> headers = new java.util.HashMap<>();
        final int authAccount = org.telegram.messenger.UserConfig.selectedAccount;
        WsRelayAuth.Credential authCredential = null;
        try { headers.put("X-Install", DomainPool.installId()); } catch (Throwable ignored) {}
        try {
            authCredential = WsRelayAuth.getCached(authAccount);
            if (authCredential != null) headers.put("X-Cred", authCredential.header());
        } catch (Throwable ignored) {}
        int attempted = 0;
        java.util.List<String> hosts = DomainPool.relayHostsForDc(dc);
        for (int index = 0; index < hosts.size(); index++) {
            final String host = hosts.get(index);
            if (!isBridgeGenerationCurrent(generation)) {
                throw new IOException("relay connect cancelled");
            }
            if (System.nanoTime() >= deadlineNanos) break;
            if (isRelayInBackoff(host)) continue;
            final long healthVersion = relayHealthVersion(host);
            long remainingMs = TimeUnit.NANOSECONDS.toMillis(deadlineNanos - System.nanoTime());
            long hostBudgetMs = Math.max(RELAY_HOST_MIN_BUDGET_MS, remainingMs / (hosts.size() - index));
            long attemptDeadline = Math.min(deadlineNanos,
                    System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(hostBudgetMs));
            attempted++;
            dbg("connectWsCf: dc=" + dc + " -> " + host + path + " cred=" + headers.containsKey("X-Cred"));
            try {
                RawWebSocket ws = RawWebSocket.connectUntil(host, host, path, headers,
                        attemptDeadline, () -> isBridgeGenerationCurrent(generation));
                if (!isBridgeGenerationCurrent(generation)) {
                    try { ws.close(); } catch (Throwable ignored) {}
                    throw new IOException("relay connect cancelled");
                }
                if (!rememberRelayRoute(ws, host, generation, healthVersion)) {
                    try { ws.close(); } catch (Throwable ignored) {}
                    if (!isBridgeGenerationCurrent(generation)) {
                        throw new IOException("relay connect cancelled");
                    }
                    continue;
                }
                dbg("connectWsCf: 101 OK via " + host + path);
                return ws;
            } catch (IOException ex) {
                if (!isBridgeGenerationCurrent(generation)) {
                    throw new IOException("relay connect cancelled", ex);
                }
                dbg("connectWsCf: " + host + " -> " + ex.getMessage());
                int status = ex instanceof RawWebSocket.HandshakeException
                        ? ((RawWebSocket.HandshakeException) ex).statusCode : 0;
                final boolean offline = isLocalNetworkError(ex);
                if (!runIfBridgeGenerationCurrent(generation, () -> {
                    synchronized (cfgLock) {
                        if (relayHealthVersion(host) == healthVersion
                                && status != 401 && status != 403 && !offline) {
                            relayFailRecord(host);
                        }
                    }
                })) {
                    throw new IOException("relay connect cancelled", ex);
                }
                if (headers.containsKey("X-Cred") && (status == 401 || status == 403)) {
                    final WsRelayAuth.Credential rejectedCredential = authCredential;
                    runIfBridgeGenerationCurrent(generation,
                            () -> WsRelayAuth.invalidateCredential(authAccount, rejectedCredential));
                    headers.remove("X-Cred");
                    authCredential = null;
                }
                last = ex;
            } catch (Throwable t) {
                if (!isBridgeGenerationCurrent(generation)) {
                    throw new IOException("relay connect cancelled", t);
                }
                dbg("connectWsCf: " + host + " -> " + t.getClass().getSimpleName() + ": " + t.getMessage());
                last = new IOException(t);
            }
        }
        if (attempted == 0) throw new IOException("relay hosts are in backoff");
        if (last != null) throw last;
        throw new IOException("cf websocket unavailable");
    }




    private void bridgeWs(final Socket client, final RawWebSocket ws, final CryptoCtx ctx,
                          final MsgSplitter splitter, final long generation, final byte[] firstPlain,
                          final boolean upload) {
        if (!markBridgeStarted(generation)) {
            try { ws.close(); } catch (Throwable ignored) {}
            closeQuietly(client);
            untrack(ws);
            return;
        }
        try {
        final AtomicBoolean done = new AtomicBoolean(false);
        final AtomicBoolean clientEnded = new AtomicBoolean(false);
        final long[] upBytes = {0};   
        final java.util.concurrent.atomic.AtomicLong upSent = new java.util.concurrent.atomic.AtomicLong();
        final long[] downBytes = {0};
        final int connId = CONN_SEQ.incrementAndGet();
        final boolean[] loggedUp = {false};
        final boolean[] loggedDown = {false};
        Thread up = new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    InputStream in = client.getInputStream();
                    byte[] buf = new byte[RECV_CHUNK];
                    if (firstPlain != null && firstPlain.length > 0) {
                        upBytes[0] += firstPlain.length;
                        try {
                            sendUp(ws, splitter, cipherUpdate(ctx.tgEnc, firstPlain), upSent);
                        } catch (IOException ex) {
                            if (!done.get() && !clientEnded.get()) recordRelayFailure(ws, generation, ex);
                            throw ex;
                        }
                    }
                    while (!done.get()) {
                        int n;
                        try {
                            n = in.read(buf);
                        } catch (IOException ex) {
                            clientEnded.set(true);
                            throw ex;
                        }
                        if (n <= 0) {
                            clientEnded.set(true);
                            dbg("bridge up: client(tgnet) EOF n=" + n + " after up=" + upBytes[0] + "B down=" + downBytes[0] + "B");
                            if (splitter != null) {
                                try {
                                    List<byte[]> tail = splitter.flush();
                                    for (byte[] p : tail) {
                                        try { ws.send(p); } catch (Throwable t) { break; }
                                    }
                                } catch (Throwable ignored) {}
                            }
                            break;
                        }
                        upBytes[0] += n;
                        byte[] chunk = (n == buf.length) ? buf : Arrays.copyOf(buf, n);
                        byte[] plain = cipherUpdate(ctx.cltDec, chunk);
                        if (!loggedUp[0]) { loggedUp[0] = true; decodeMtproto(connId, "UP(client->DC)", plain); }
                        byte[] data = cipherUpdate(ctx.tgEnc, plain);
                        if (data == null || data.length == 0) continue;
                        try {
                            if (splitter != null) {
                                List<byte[]> parts = splitter.split(data);
                                if (parts == null || parts.isEmpty()) continue;
                                if (parts.size() > 1) {
                                
                                    ws.sendBatch(parts);
                                } else {
                                    ws.send(parts.get(0));
                                }
                                for (byte[] p : parts) upSent.addAndGet(p.length);
                            } else {
                                ws.send(data);
                                upSent.addAndGet(data.length);
                            }
                        } catch (IOException ex) {
                            if (!done.get() && !clientEnded.get()) recordRelayFailure(ws, generation, ex);
                            throw ex;
                        }
                    }
                } catch (Throwable t) { dbg("bridge up-thread end: " + t.getClass().getSimpleName() + ": " + t.getMessage()); }
                done.set(true);
                synchronized (done) { done.notifyAll(); }
            }
        }, "wsbypass-ws-up");
        up.setDaemon(true);

        Thread down = new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    OutputStream out = client.getOutputStream();
                    byte[] plainBuffer = new byte[64 * 1024 + 32];
                    byte[] clientBuffer = new byte[64 * 1024 + 32];
                    while (!done.get()) {
                        byte[] payload;
                        try {
                            payload = ws.recv();
                        } catch (IOException ex) {
                            if (!done.get() && !clientEnded.get()) recordRelayFailure(ws, generation, ex);
                            throw ex;
                        }
                        if (payload == null) {
                            if (!done.get() && !clientEnded.get()) recordRelayFailure(ws, generation, null);
                            dbg("bridge down: ws.recv returned null (CLOSE)");
                            break;
                        }
                        if (payload.length == 0) continue;
                        downBytes[0] += payload.length;
                        plainBuffer = ensureCipherBuffer(ctx.tgDec, payload.length, plainBuffer);
                        int plainLength = cipherUpdateInto(
                                ctx.tgDec, payload, payload.length, plainBuffer);
                        if (!loggedDown[0]) {
                            loggedDown[0] = true;
                            if (DEBUG) {
                                decodeMtproto(connId, "DOWN(DC->client)",
                                        Arrays.copyOf(plainBuffer, plainLength));
                            }
                        }
                        clientBuffer = ensureCipherBuffer(ctx.cltEnc, plainLength, clientBuffer);
                        int outputLength = cipherUpdateInto(
                                ctx.cltEnc, plainBuffer, plainLength, clientBuffer);
                        if (outputLength > 0) {
                            try {
                                out.write(clientBuffer, 0, outputLength);
                                out.flush();
                            } catch (IOException ex) {
                                clientEnded.set(true);
                                throw ex;
                            }
                            markBridgeOk(generation);
                            markRelayProgress(ws, generation);
                        }
                    }
                } catch (Throwable t) { dbg("bridge down-thread end: " + t.getClass().getSimpleName() + ": " + t.getMessage()); }
                done.set(true);
                synchronized (done) { done.notifyAll(); }
            }
        }, "wsbypass-ws-down");
        down.setDaemon(true);

        up.start();
        down.start();
        
        final long startMs = System.currentTimeMillis();
        long nextLog = startMs + 2000;
        ws.markPongNow();
        long nextPing = startMs + KEEPALIVE_IDLE_MS;
        long stallProbeAt = 0;
        long bulkWindowAt = startMs;
        long bulkWindowBytes = 0;
        long bulkUntil = 0;
        try {
            while (!done.get() && (up.isAlive() || down.isAlive())) {
                synchronized (done) {
                    if (!done.get()) done.wait(BRIDGE_TICK_MS);
                }

                final long now = System.currentTimeMillis();
                final long heard = ws.sinceHeard();
                final long sent = ws.sinceSent();
                if (now - bulkWindowAt >= BULK_WINDOW_MS) {
                    final long sentNow = upSent.get();
                    final long burst = sentNow - bulkWindowBytes;
                    if (burst >= BULK_BYTES) {
                        final long drainMs = burst * 1000L / SLOW_UPLINK_BYTES_PER_SEC;
                        bulkUntil = Math.max(bulkUntil, now + Math.min(BULK_HOLD_MAX_MS, BULK_HOLD_MS + drainMs));
                    }
                    bulkWindowBytes = sentNow;
                    bulkWindowAt = now;
                }
                if (upload || now < bulkUntil) {
                    stallProbeAt = 0;
                } else if (stallProbeAt != 0) {
                    if (heard < now - stallProbeAt) {
                        stallProbeAt = 0;
                    } else if (now - stallProbeAt >= STALL_PONG_MS) {
                        dbg("bridge: downstream stalled " + heard + "ms after a send, reconnecting");
                        if (!done.get() && !clientEnded.get()) {
                            recordRelayFailure(ws, generation, null);
                            TgwsRoute.penalizeStalled(ws.routeTag());
                        }
                        break;
                    }
                } else if (heard >= STALL_PROBE_MS && sent < heard) {
                    if (!ws.ping(null, STALL_PING_WRITE_MS)) {
                        if (!done.get() && !clientEnded.get()) recordRelayFailure(ws, generation, null);
                        break;
                    }
                    stallProbeAt = System.currentTimeMillis();
                }
                final long silence = Math.min(heard, sent);
                if (silence >= KEEPALIVE_DEAD_MS) {
                    if (!done.get() && !clientEnded.get()) recordRelayFailure(ws, generation, null);
                    dbg("bridge: silence " + (silence / 1000) + "s, dropping");
                    break;
                }
                if (now >= nextPing && silence >= KEEPALIVE_IDLE_MS) {
                    if (!ws.ping(null, (int) Math.max(2_000L, KEEPALIVE_DEAD_MS - silence))) {
                        if (!done.get() && !clientEnded.get()) recordRelayFailure(ws, generation, null);
                        break;
                    }
                    nextPing = now + KEEPALIVE_IDLE_MS;
                }
                if (DEBUG && System.currentTimeMillis() >= nextLog) {
                    int pend = splitter == null ? 0 : splitter.pendingBytes();
                    dbg("bridge ALIVE dur=" + ((System.currentTimeMillis() - startMs) / 1000.0)
                            + "s upRead=" + upBytes[0] + "B upSent=" + upSent.get() + "B splitPend=" + pend
                            + "B down=" + downBytes[0] + "B"
                            + (upBytes[0] - upSent.get() > 1024 ? "  <-- UP STUCK (read>>sent)" : ""));
                    nextLog += 2000;
                }
            }
        } catch (InterruptedException ie) { Thread.currentThread().interrupt(); }
        done.set(true);
        dbg("bridge CLOSED dur=" + ((System.currentTimeMillis() - startMs) / 1000.0)
                + "s upRead=" + upBytes[0] + "B upSent=" + upSent.get() + "B down=" + downBytes[0] + "B"
                + (downBytes[0] == 0 ? "  <-- NO DATA FROM DC" : "")
                + (upBytes[0] - upSent.get() > 1024 ? "  <-- UP UNSENT (splitter held bytes)" : ""));
        try { ws.close(); } catch (Throwable ignored) {}
        closeQuietly(client);
        try { up.join(500); } catch (InterruptedException ignored) { Thread.currentThread().interrupt(); }
        try { down.join(500); } catch (InterruptedException ignored) { Thread.currentThread().interrupt(); }
        untrack(ws);
        untrack(client);
        } finally {
            markBridgeStopped(generation);
        }
    }

    private static long poolKey(int dc, boolean isMedia) {
        return ((long) dc << 1) | (isMedia ? 1L : 0L);
    }



    private void failClear(long dcKey) {
        synchronized (cfgLock) {
            failUntilMs.remove(dcKey);
            failCount.remove(dcKey);
        }
    }



    private long relayHealthVersion(String host) {
        synchronized (cfgLock) {
            Long version = relayHealthVersions.get(host);
            return version == null ? 0L : version;
        }
    }

    private boolean rememberRelayRoute(RawWebSocket ws, String host, long generation, long healthVersion) {
        synchronized (bridgeStateLock) {
            if (generation != bridgeGeneration) return false;
            synchronized (cfgLock) {
                if (relayHealthVersion(host) != healthVersion || isRelayInBackoff(host)) return false;
                relayRoutes.put(ws, new RelayRoute(host, generation, healthVersion));
                return true;
            }
        }
    }

    private boolean isRouteUsable(RawWebSocket ws, long generation) {
        synchronized (bridgeStateLock) {
            if (generation != bridgeGeneration) return false;
            synchronized (cfgLock) {
                RelayRoute route = relayRoutes.get(ws);
                return route == null || route.generation == generation
                        && route.healthVersion == relayHealthVersion(route.host)
                        && !isRelayInBackoff(route.host);
            }
        }
    }

    private void markRelayProgress(RawWebSocket ws, long generation) {
        runIfBridgeGenerationCurrent(generation, () -> {
            synchronized (cfgLock) {
                RelayRoute route = relayRoutes.get(ws);
                if (route != null && route.generation == generation
                        && route.healthVersion == relayHealthVersion(route.host)) {
                    relayFailClear(route.host);
                    FGBypassLocations.markConnected(route.host);
                }
            }
        });
    }

    private void recordRelayFailure(RawWebSocket ws, long generation, Throwable error) {
        if (isLocalNetworkError(error)) return;
        runIfBridgeGenerationCurrent(generation, () -> {
            synchronized (cfgLock) {
                RelayRoute route = relayRoutes.get(ws);
                if (route != null && route.generation == generation
                        && route.healthVersion == relayHealthVersion(route.host)) {
                    relayFailRecord(route.host);
                }
            }
        });
    }

    private boolean isRelayInBackoff(String host) {
        synchronized (cfgLock) {
            Long until = relayFailUntilMs.get(host);
            if (until == null) return false;
            if (nowElapsedMs() >= until) {
                relayFailUntilMs.remove(host);
                return false;
            }
            return true;
        }
    }

    private void relayFailRecord(String host) {
        synchronized (cfgLock) {
            relayHealthVersions.put(host, relayHealthVersion(host) + 1L);
            int failures = relayFailCount.containsKey(host) ? relayFailCount.get(host) + 1 : 1;
            relayFailCount.put(host, failures);
            int exp = Math.min(4, Math.max(0, failures - 1));
            long base = Math.min(RELAY_BACKOFF_MAX_MS, RELAY_BACKOFF_BASE_MS * (1L << exp));
            long jittered = (long) (base
                    * java.util.concurrent.ThreadLocalRandom.current().nextDouble(0.85, 1.16));
            relayFailUntilMs.put(host, nowElapsedMs() + jittered);
        }
    }

    private static boolean isLocalNetworkError(Throwable error) {
        for (Throwable t = error; t != null; t = t.getCause()) {
            if (t instanceof java.net.UnknownHostException || t instanceof java.net.NoRouteToHostException) {
                return true;
            }
            String message = t.getMessage();
            if (message != null && (message.contains("ENETUNREACH") || message.contains("EHOSTUNREACH")
                    || message.contains("Network is unreachable"))) {
                return true;
            }
            if (t.getCause() == t) {
                break;
            }
        }
        return false;
    }

    private void relayFailClear(String host) {
        synchronized (cfgLock) {
            relayFailUntilMs.remove(host);
            relayFailCount.remove(host);
        }
    }

    private static long nowElapsedMs() {
        return TimeUnit.NANOSECONDS.toMillis(System.nanoTime());
    }


    private boolean trackIfGenerationCurrent(Object closer, long generation) {
        if (closer == null) return false;
        synchronized (bridgeStateLock) {
            if (generation != bridgeGeneration) return false;
            synchronized (trackedLock) {
                tracked.add(closer);
            }
            return true;
        }
    }

    private boolean runIfBridgeGenerationCurrent(long generation, Runnable action) {
        synchronized (bridgeStateLock) {
            if (generation != bridgeGeneration) return false;
            action.run();
            return true;
        }
    }

    private void untrack(Object closer) {
        if (closer == null) return;
        synchronized (trackedLock) {
            tracked.remove(closer);
        }
    }

    private List<Object> detachTracked() {
        List<Object> snapshot;
        synchronized (trackedLock) {
            snapshot = new ArrayList<>(tracked);
            tracked.clear();
        }
        return snapshot;
    }

    private static byte[] recvExact(Socket conn, int size) throws IOException {
        InputStream in = conn.getInputStream();
        byte[] out = new byte[size];
        int off = 0;
        while (off < size) {
            int r = in.read(out, off, size - off);
            if (r < 0) throw new IOException("unexpected eof");
            off += r;
        }
        return out;
    }

    private static byte[] cipherUpdate(javax.crypto.Cipher cipher, byte[] data) {
        if (cipher == null || data == null || data.length == 0) return new byte[0];
        try {
            byte[] out = cipher.update(data);
            return out == null ? new byte[0] : out;
        } catch (Throwable t) {
            return new byte[0];
        }
    }

    private static byte[] ensureCipherBuffer(javax.crypto.Cipher cipher, int inputLength,
                                             byte[] current) {
        if (cipher == null) {
            return current;
        }
        int required = Math.max(inputLength + 32, cipher.getOutputSize(inputLength));
        return current.length >= required ? current : new byte[required];
    }

    private static int cipherUpdateInto(javax.crypto.Cipher cipher, byte[] input, int inputLength,
                                        byte[] output) throws javax.crypto.ShortBufferException {
        if (cipher == null || input == null || inputLength <= 0) {
            return 0;
        }
        return cipher.update(input, 0, inputLength, output, 0);
    }

    private static void shutdownWrite(Socket s) {
        if (s == null) return;
        try { s.shutdownOutput(); } catch (Throwable ignored) {}
    }

    private static void closeQuietly(Socket s) {
        if (s == null) return;
        try { s.close(); } catch (Throwable ignored) {}
    }

    private static void lowerPriority() {
        try {
            int target = Math.max(Thread.MIN_PRIORITY, Thread.NORM_PRIORITY - 2);
            Thread.currentThread().setPriority(target);
        } catch (Throwable ignored) {}
    }

    public static boolean isWsBypassPluginFile(File file) {
        if (file == null || !file.exists() || !file.isFile()) return false;
        try (java.io.BufferedReader r = new java.io.BufferedReader(new java.io.FileReader(file))) {
            String line;
            int lines = 0;
            while ((line = r.readLine()) != null && lines < 30) {
                lines++;
                String trimmed = line.trim();
                if (trimmed.startsWith("__id__")) {
                    int eq = trimmed.indexOf('=');
                    if (eq < 0) continue;
                    String rhs = trimmed.substring(eq + 1).trim();
                    if (rhs.length() < 2) continue;
                    char q = rhs.charAt(0);
                    if (q != '"' && q != '\'') continue;
                    int end = rhs.indexOf(q, 1);
                    if (end < 0) continue;
                    String id = rhs.substring(1, end);
                    return "wsbypass".equals(id);
                }
            }
        } catch (Throwable t) {
            FileLog.e("обход: isWsBypassPluginFile failed", t);
        }
        return false;
    }
}

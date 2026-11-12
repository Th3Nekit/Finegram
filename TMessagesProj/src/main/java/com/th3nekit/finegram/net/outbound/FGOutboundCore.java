/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Uses sing-box (GPL-3.0, Copyright the sing-box authors, https://github.com/SagerNet/sing-box).
 * Runs Xray-core (MPL-2.0, Copyright the Xray authors, https://github.com/XTLS/Xray-core).
 */

package com.th3nekit.finegram.net.outbound;

import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.AndroidUtilities;

import java.io.File;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.IntConsumer;

import com.th3nekit.finegram.core.FinegramLogger;

import io.nekohasekai.libbox.CommandServer;
import io.nekohasekai.libbox.CommandServerHandler;
import io.nekohasekai.libbox.Libbox;
import io.nekohasekai.libbox.OverrideOptions;
import io.nekohasekai.libbox.SetupOptions;
import io.nekohasekai.libbox.SystemProxyStatus;

public final class FGOutboundCore {

    private static final ExecutorService worker = Executors.newSingleThreadExecutor(runnable -> {
        Thread thread = new Thread(runnable, "FGOutboundCore");
        thread.setDaemon(true);
        return thread;
    });
    private static final AtomicLong requests = new AtomicLong();
    private static final Object supportLock = new Object();
    private static final Object setupLock = new Object();

    private static volatile FGOutboundCredentials credentials;
    private static final ConcurrentHashMap<Integer, FGOutboundCredentials> probeCredentials = new ConcurrentHashMap<>();

    private static volatile CommandServer server;
    private static volatile FGXrayProcess xray;

    private static volatile String runningLink;

    private static volatile long readyRequest;
    private static IntConsumer runningCallback;

    private static boolean setupDone;

    private FGOutboundCore() {
    }

    private static volatile int supported;

    public static boolean isSupported() {
        int known = supported;
        if (known != 0) return known > 0;
        synchronized (supportLock) {
            if (supported == 0) {
                try {
                    Libbox.goVersion();
                    supported = 1;
                } catch (Throwable t) {
                    supported = -1;
                    FinegramLogger.d("FGOutbound", () -> "ядра в этой сборке нет");
                }
            }
            return supported > 0;
        }
    }

    public static int currentPort() {
        FGOutboundCredentials current = currentCredentials();
        return current != null ? current.port : 0;
    }

    public static FGOutboundCredentials currentCredentials() {
        long request = readyRequest;
        FGOutboundCredentials current = credentials;
        return request == requests.get() && request == readyRequest ? current : null;
    }

    public static FGOutboundCredentials probeCredentials(int localPort) {
        return probeCredentials.get(localPort);
    }

    public static boolean isRunning() {
        return currentPort() != 0;
    }

    public static void start(String link, IntConsumer onReady) {
        final long request = requests.incrementAndGet();
        worker.execute(() -> startOnWorker(link, request, onReady));
    }

    private static void startOnWorker(String link, long request, IntConsumer onReady) {
        if (request != requests.get()) return;
        if ((server != null || xray != null && xray.isAlive()) && link != null && link.equals(runningLink)) {
            readyRequest = request;
            runningCallback = onReady;
            notifyReady(request, onReady, credentials.port);
            return;
        }
        stopOnWorker();
        if (request != requests.get()) return;

        if (!isSupported()) {
            FinegramLogger.e("FGOutbound", () -> "ядра в этой сборке нет");
            notifyReady(request, onReady, 0);
            return;
        }

        final FGOutboundLink.Parsed parsed = FGOutboundLink.parse(link);
        if (parsed == null) {
            FinegramLogger.e("FGOutbound", () -> "ссылка не разобралась");
            notifyReady(request, onReady, 0);
            return;
        }
        if (parsed.xray != null) {
            startXrayOnWorker(link, parsed, request, onReady);
            return;
        }
        CommandServer started = null;
        try {
            setup();
            final int free = freePort();
            if (free == 0) {
                notifyReady(request, onReady, 0);
                return;
            }

            final FGOutboundCredentials startedCredentials = FGOutboundCredentials.create(free);
            final String config = FGOutboundLink.config(parsed, startedCredentials);
            if (config.isEmpty()) {
                notifyReady(request, onReady, 0);
                return;
            }
            Libbox.checkConfig(config);
            if (request != requests.get()) return;

            final Handler handler = new Handler(true);
            started = new CommandServer(handler, new FGOutboundPlatform());
            handler.owner = started;
            started.start();
            if (request != requests.get()) {
                close(started);
                return;
            }
            started.startOrReloadService(config, new OverrideOptions());
            if (request != requests.get()) {
                close(started);
                return;
            }

            server = started;
            credentials = startedCredentials;
            runningLink = link;
            runningCallback = onReady;
            readyRequest = request;
            FinegramLogger.d("FGOutbound", () ->
                    "ядро поднято: " + parsed.kind + ", SOCKS на " + free);
            notifyReady(request, onReady, free);
        } catch (Throwable t) {
            FinegramLogger.e("FGOutbound", () -> "ядро не поднялось: " + t.getClass().getSimpleName());
            if (server == started) {
                stopOnWorker();
            } else {
                close(started);
            }
            notifyReady(request, onReady, 0);
        }
    }

    private static void startXrayOnWorker(String link, FGOutboundLink.Parsed parsed, long request, IntConsumer onReady) {
        FGXrayProcess started = null;
        try {
            final int free = freePort();
            if (free == 0) {
                notifyReady(request, onReady, 0);
                return;
            }
            final FGOutboundCredentials startedCredentials = FGOutboundCredentials.create(free);
            started = FGXrayProcess.start(parsed.xray, startedCredentials, FGOutboundCore::onXrayExit,
                    () -> request != requests.get());
            if (request != requests.get()) {
                started.close();
                return;
            }
            xray = started;
            credentials = startedCredentials;
            runningLink = link;
            runningCallback = onReady;
            readyRequest = request;
            FinegramLogger.d("FGOutbound", () -> "xray поднят: XHTTP, SOCKS на " + free);
            notifyReady(request, onReady, free);
        } catch (Throwable t) {
            FinegramLogger.e("FGOutbound", () -> "xray не поднялся: " + t.getClass().getSimpleName());
            if (started != null) {
                if (xray == started) {
                    stopOnWorker();
                } else {
                    started.close();
                }
            }
            notifyReady(request, onReady, 0);
        }
    }

    private static void onXrayExit(FGXrayProcess exited) {
        worker.execute(() -> {
            if (xray != exited) return;
            final long request = readyRequest;
            final IntConsumer callback = runningCallback;
            stopOnWorker();
            notifyReady(request, callback, 0);
        });
    }

    public static void stop() {
        final long request = requests.incrementAndGet();
        worker.execute(() -> {
            if (request == requests.get()) stopOnWorker();
        });
    }

    private static void notifyReady(long request, IntConsumer callback, int localPort) {
        if (callback == null || request != requests.get()) return;
        AndroidUtilities.runOnUIThread(() -> {
            if (request == requests.get() && (localPort == 0 || currentPort() == localPort)) {
                callback.accept(localPort);
            }
        });
    }

    private static void stopOnWorker() {
        final CommandServer running = server;
        final FGXrayProcess runningXray = xray;
        server = null;
        xray = null;
        credentials = null;
        runningLink = null;
        runningCallback = null;
        close(running);
        if (runningXray != null) runningXray.close();
    }

    private static void close(CommandServer running) {
        if (running == null) return;
        try {
            running.closeService();
        } catch (Throwable ignored) {
        }
        try {
            running.close();
        } catch (Throwable t) {
            FinegramLogger.e("FGOutbound", () -> "ядро не остановилось: " + t.getClass().getSimpleName());
        }
    }

    public static final class Probe implements AutoCloseable {
        public final int port;
        public final FGOutboundCredentials credentials;
        private final CommandServer server;
        private final FGXrayProcess xray;
        private final AtomicBoolean closed = new AtomicBoolean();

        private Probe(FGOutboundCredentials credentials, CommandServer server) {
            this(credentials, server, null);
        }

        private Probe(FGOutboundCredentials credentials, CommandServer server, FGXrayProcess xray) {
            this.credentials = credentials;
            this.port = credentials.port;
            this.server = server;
            this.xray = xray;
        }

        @Override
        public synchronized void close() {
            if (!closed.compareAndSet(false, true)) return;
            probeCredentials.remove(port, credentials);
            if (xray != null) xray.close();
            if (server == null) return;
            try {
                server.closeService();
            } catch (Throwable ignored) {
            }
            try {
                server.close();
            } catch (Throwable ignored) {
            }
        }

        private synchronized boolean register() {
            if (closed.get() || Thread.currentThread().isInterrupted() || xray != null && !xray.isAlive()) return false;
            probeCredentials.put(port, credentials);
            return true;
        }
    }

    public static Probe startProbe(String link) {
        if (Thread.currentThread().isInterrupted()) return null;
        if (!isSupported()) return null;
        final FGOutboundLink.Parsed parsed = FGOutboundLink.parse(link);
        if (parsed == null) return null;
        if (parsed.xray != null) return startXrayProbe(parsed);
        Probe probe = null;
        try {
            setup();
            final int free = freePort();
            if (free == 0) return null;
            final FGOutboundCredentials startedCredentials = FGOutboundCredentials.create(free);
            final String config = FGOutboundLink.config(parsed, startedCredentials);
            if (config.isEmpty()) return null;
            Libbox.checkConfig(config);
            final CommandServer started = new CommandServer(new Handler(false), new FGOutboundPlatform());
            probe = new Probe(startedCredentials, started);
            started.startOrReloadService(config, new OverrideOptions());
            if (!probe.register()) {
                probe.close();
                return null;
            }
            return probe;
        } catch (Throwable ignored) {
            if (probe != null) probe.close();
            return null;
        }
    }

    private static Probe startXrayProbe(FGOutboundLink.Parsed parsed) {
        Probe probe = null;
        try {
            final int free = freePort();
            if (free == 0) return null;
            final FGOutboundCredentials startedCredentials = FGOutboundCredentials.create(free);
            final AtomicReference<Probe> owner = new AtomicReference<>();
            final FGXrayProcess started = FGXrayProcess.start(parsed.xray, startedCredentials, exited -> {
                final Probe running = owner.get();
                if (running != null) running.close();
            });
            probe = new Probe(startedCredentials, null, started);
            owner.set(probe);
            if (!probe.register()) {
                probe.close();
                return null;
            }
            return probe;
        } catch (Throwable t) {
            if (probe != null) probe.close();
            FinegramLogger.e("FGOutbound", () -> "проверка xray не поднялась: " + t.getClass().getSimpleName());
            return null;
        }
    }

    private static void setup() throws Exception {
        synchronized (setupLock) {
            if (setupDone) return;
            final File base = new File(ApplicationLoader.getFilesDirFixed(), "outbound");
            final File temp = new File(ApplicationLoader.applicationContext.getCacheDir(), "outbound");
            base.mkdirs();
            temp.mkdirs();

            final SetupOptions options = new SetupOptions();
            options.setBasePath(base.getAbsolutePath());
            options.setWorkingPath(base.getAbsolutePath());
            options.setTempPath(temp.getAbsolutePath());
            Libbox.setup(options);
            setupDone = true;
        }
    }

    private static int freePort() {
        try (ServerSocket socket = new ServerSocket()) {
            socket.bind(new InetSocketAddress("127.0.0.1", 0));
            return socket.getLocalPort();
        } catch (Throwable t) {
            FinegramLogger.e("FGOutbound", () -> "не нашёлся свободный порт", t);
            return 0;
        }
    }

    private static final class Handler implements CommandServerHandler {

        private final boolean active;
        private CommandServer owner;

        Handler(boolean active) {
            this.active = active;
        }

        @Override
        public void serviceReload() {
        }

        @Override
        public void serviceStop() {
            if (!active) return;
            worker.execute(() -> {
                if (server != owner) return;
                final long request = readyRequest;
                final IntConsumer callback = runningCallback;
                stopOnWorker();
                notifyReady(request, callback, 0);
            });
        }

        @Override
        public SystemProxyStatus getSystemProxyStatus() {
            final SystemProxyStatus status = new SystemProxyStatus();
            status.setAvailable(false);
            status.setEnabled(false);
            return status;
        }

        @Override
        public void setSystemProxyEnabled(boolean enabled) {
        }

        @Override
        public int connectSSHAgent() throws Exception {
            throw new UnsupportedOperationException("ssh agent is not used");
        }

        @Override
        public void triggerNativeCrash() {
        }

        @Override
        public void writeDebugMessage(String message) {
        }
    }
}

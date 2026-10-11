/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Ядро — sing-box (GPL-3.0, Copyright the sing-box authors,
 * https://github.com/SagerNet/sing-box).
 */

package com.th3nekit.finegram.net.outbound;

import org.telegram.messenger.ApplicationLoader;

import java.io.File;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.util.concurrent.atomic.AtomicBoolean;

import com.th3nekit.finegram.core.FinegramLogger;

import io.nekohasekai.libbox.CommandServer;
import io.nekohasekai.libbox.CommandServerHandler;
import io.nekohasekai.libbox.Libbox;
import io.nekohasekai.libbox.OverrideOptions;
import io.nekohasekai.libbox.SetupOptions;
import io.nekohasekai.libbox.SystemProxyStatus;

public final class FGOutboundCore {

    private static volatile int port;

    private static volatile CommandServer server;

    private static volatile String runningLink;

    private static boolean setupDone;

    private FGOutboundCore() {
    }

    private static volatile int supported;

    public static boolean isSupported() {
        int known = supported;
        if (known != 0) return known > 0;
        synchronized (FGOutboundCore.class) {
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

    public static synchronized int currentPort() {
        return port;
    }

    public static synchronized boolean isRunning() {
        return server != null;
    }

    public static synchronized int start(String link) {
        if (server != null && link != null && link.equals(runningLink)) {
            return port;
        }
        stop();

        if (!isSupported()) {
            FinegramLogger.e("FGOutbound", () -> "ядра в этой сборке нет");
            return 0;
        }

        final FGOutboundLink.Parsed parsed = FGOutboundLink.parse(link);
        if (parsed == null) {
            FinegramLogger.e("FGOutbound", () -> "ссылка не разобралась");
            return 0;
        }
        try {
            setup();
            final int free = freePort();
            if (free == 0) return 0;

            final String config = FGOutboundLink.config(parsed, free);
            if (config.isEmpty()) return 0;

            final CommandServer started = new CommandServer(new Handler(), new FGOutboundPlatform());
            server = started;
            started.start();
            started.startOrReloadService(config, new OverrideOptions());

            port = free;
            runningLink = link;
            FinegramLogger.d("FGOutbound", () ->
                    "ядро поднято: " + parsed.kind + " " + parsed.host + ", SOCKS на " + free);
            return free;
        } catch (Throwable t) {
            FinegramLogger.e("FGOutbound", () -> "ядро не поднялось", t);
            stop();
            return 0;
        }
    }

    public static synchronized void stop() {
        final CommandServer running = server;
        server = null;
        port = 0;
        runningLink = null;
        if (running == null) return;
        try {
            running.closeService();
        } catch (Throwable ignored) {
        }
        try {
            running.close();
        } catch (Throwable t) {
            FinegramLogger.e("FGOutbound", () -> "ядро не остановилось", t);
        }
    }

    public static final class Probe implements AutoCloseable {
        public final int port;
        private final CommandServer server;
        private final AtomicBoolean closed = new AtomicBoolean();

        private Probe(int port, CommandServer server) {
            this.port = port;
            this.server = server;
        }

        @Override
        public void close() {
            if (!closed.compareAndSet(false, true)) return;
            try {
                server.closeService();
            } catch (Throwable ignored) {
            }
            try {
                server.close();
            } catch (Throwable ignored) {
            }
        }
    }

    public static Probe startProbe(String link) {
        if (!isSupported()) return null;
        final FGOutboundLink.Parsed parsed = FGOutboundLink.parse(link);
        if (parsed == null) return null;
        Probe probe = null;
        try {
            setup();
            final int free = freePort();
            if (free == 0) return null;
            final String config = FGOutboundLink.config(parsed, free);
            if (config.isEmpty()) return null;
            final CommandServer started = new CommandServer(new Handler(false), new FGOutboundPlatform());
            probe = new Probe(free, started);
            started.startOrReloadService(config, new OverrideOptions());
            return probe;
        } catch (Throwable ignored) {
            if (probe != null) probe.close();
            return null;
        }
    }

    private static synchronized void setup() throws Exception {
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

        Handler() {
            this(true);
        }

        Handler(boolean active) {
            this.active = active;
        }

        @Override
        public void serviceReload() {
        }

        @Override
        public void serviceStop() {
            if (active) stop();
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
            FinegramLogger.d("FGOutbound", () -> message);
        }
    }
}

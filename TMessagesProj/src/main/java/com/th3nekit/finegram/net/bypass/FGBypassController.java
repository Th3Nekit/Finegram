/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Обход блокировок: MTProto поверх WebSocket. Своя реализация на
 * наши серверы.
 */

package com.th3nekit.finegram.net.bypass;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.FileLog;
import org.telegram.messenger.Utilities;

import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

public final class FGBypassController {

    public static final String STATE_OFF = "off";
    public static final String STATE_STARTING = "starting";
    public static final String STATE_RUNNING = "running";
    public static final String STATE_FAILED = "failed";
    public static final String STATE_VPN = "vpn";

    private static final long WATCHDOG_INTERVAL_SEC = 30L;

    private static volatile FGBypassController instance;

    public static FGBypassController getInstance() {
        FGBypassController local = instance;
        if (local == null) {
            synchronized (FGBypassController.class) {
                local = instance;
                if (local == null) {
                    local = new FGBypassController();
                    instance = local;
                }
            }
        }
        return local;
    }

    private final Object lifecycleLock = new Object();
    private final AtomicBoolean starting = new AtomicBoolean(false);
    private final AtomicBoolean resumeCheckPending = new AtomicBoolean(false);

    private long nextStartToken;
    private long activeStartToken;
    private long watchdogGeneration;
    private volatile boolean running;
    private volatile boolean lastStartFailed;
    private volatile String lastError = "";
    private volatile int currentPort;
    private volatile String currentSecret = "";
    private volatile Runnable settingsReloader;
    private volatile ScheduledExecutorService watchdogPool;
    private volatile ScheduledFuture<?> watchdogTask;

    private FGBypassController() {
    }

    public void setSettingsReloader(Runnable reloader) {
        this.settingsReloader = reloader;
    }

    public boolean isRunning() {
        return running;
    }

    public boolean isStarting() {
        return starting.get();
    }

    public String getLastError() {
        return lastError == null ? "" : lastError;
    }

    public boolean blockedByVpn() {
        return com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig
                .isSuspendOnVpnEnabled() && FGVpnDetector.isVpnActive();
    }

    private boolean blockedByVpnFresh() {
        return com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig
                .isSuspendOnVpnEnabled() && FGVpnDetector.isVpnActiveFresh();
    }

    private boolean enforceVpnSuspensionFresh() {
        if (!blockedByVpnFresh()) return false;
        onVpnStateChanged(true);
        return true;
    }

    public void reevaluateForVpnToggle() {
        onVpnStateChanged(FGVpnDetector.isVpnActiveFresh());
    }

    public String getConnectionState() {
        if (!com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig
                .isDataBypassEnabled()) return STATE_OFF;
        if (blockedByVpn()) return STATE_VPN;
        if (running) {

            try {
                WsBypassCore core = WsBypassCore.getInstance();
                if (!core.isRunning() || !core.isAcceptThreadAlive()) return STATE_FAILED;
                if (!core.hasActiveBridge() || core.getLastBridgeOkAtMs() == 0L) return STATE_STARTING;
            } catch (Throwable ignored) {}
            return STATE_RUNNING;
        }
        if (starting.get()) return STATE_STARTING;
        if (lastStartFailed) return STATE_FAILED;
        return STATE_OFF;
    }

    public void ensureStarted() {
        if (!com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig
                .isDataBypassEnabled() || enforceVpnSuspensionFresh()) return;
        if (running || starting.get()) return;
        startAsync();
    }

    public void scheduleStartupRetries() {
        scheduleStartupRetry(0);
    }

    private static final long[] STARTUP_RETRY_DELAYS = {2_000L, 5_000L, 10_000L, 20_000L, 40_000L};

    private void scheduleStartupRetry(int attempt) {
        if (attempt >= STARTUP_RETRY_DELAYS.length) {
            return;
        }
        org.telegram.messenger.AndroidUtilities.runOnUIThread(() -> {
            if (!com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig.isDataBypassEnabled()) {
                return;
            }
            if (blockedByVpn()) {

                return;
            }
            if (!running && !starting.get()) {
                ensureStarted();
            }
            if (!running) {
                scheduleStartupRetry(attempt + 1);
            }
        }, STARTUP_RETRY_DELAYS[attempt]);
    }

    public void ensureStartedSync() {
        WsBypassCore.dbg("ensureStartedSync: вызван, включён="
                + com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig.isDataBypassEnabled());
        if (!com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig
                .isDataBypassEnabled() || enforceVpnSuspensionFresh()) return;
        long token = claimStart();
        WsBypassCore.dbg("ensureStartedSync: token=" + token);
        if (token == 0L) return;
        try {
            startSync(token);
        } catch (Throwable t) {
            FileLog.e("FGBypassController.ensureStartedSync", t);
            releaseStart(token);
        }
    }

    public void setEnabled(boolean v) {
        long token = 0L;
        synchronized (lifecycleLock) {

            FGBypassConfig.setEnabled(v);
            if (v) {
                if (blockedByVpnFresh()) {
                    suspendForVpn();
                } else {
                    token = claimStartLocked();
                }
            } else {
                stop();
            }
            notifyReloader();
        }
        if (token != 0L) launchStartThread(token);
    }

    public void onVpnStateChanged(boolean vpnActive) {
        applyProxyPolicyForVpn(vpnActive);

        boolean suspend = vpnActive
                && com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig
                .isSuspendOnVpnEnabled();
        if (suspend) {
            suspendForVpn();
        } else {
            resumeAfterVpn();
        }
    }

    private void applyProxyPolicyForVpn(boolean vpnActive) {
        if (!FGBypassConfig.disableProxyOnVpn) {
            return;
        }
        try {
            if (vpnActive) {
                if (!org.telegram.messenger.SharedConfig.isProxyEnabled()) {
                    return;
                }

                if (ProxyApplier.isLocalProxyActive(
                        WsBypassCore.LOCAL_PROXY_HOST, currentPort, currentSecret)) {
                    return;
                }
                FGBypassConfig.setProxyWasOnBeforeVpn(true);
                setClientProxyEnabled(false);
            } else if (FGBypassConfig.proxyWasOnBeforeVpn) {
                FGBypassConfig.setProxyWasOnBeforeVpn(false);
                setClientProxyEnabled(true);
            }
        } catch (Throwable t) {
            FileLog.e(t);
        }
    }

    private static void setClientProxyEnabled(boolean enabled) {
        final org.telegram.messenger.SharedConfig.ProxyInfo current =
                org.telegram.messenger.SharedConfig.currentProxy;
        if (current == null) {

            if (enabled) return;
            org.telegram.messenger.MessagesController.getGlobalMainSettings().edit()
                    .putBoolean("proxy_enabled", false).apply();
            org.telegram.messenger.NotificationCenter.getGlobalInstance().postNotificationName(
                    org.telegram.messenger.NotificationCenter.proxySettingsChanged);
            return;
        }
        org.telegram.messenger.MessagesController.getGlobalMainSettings().edit()
                .putBoolean("proxy_enabled", enabled).apply();
        org.telegram.tgnet.ConnectionsManager.setProxySettings(enabled, current.settings);
        org.telegram.messenger.NotificationCenter.getGlobalInstance().postNotificationName(
                org.telegram.messenger.NotificationCenter.proxySettingsChanged);
    }

    private void suspendForVpn() {
        synchronized (lifecycleLock) {

            if (!blockedByVpn()) return;
            invalidateStartLocked();
            cancelRelayAuthConnections();
            cancelWatchdogLocked();
            try { WsBypassCore.getInstance().stop(); } catch (Throwable ignored) {}

            try { ProxyApplier.suspendForVpn(WsBypassCore.LOCAL_PROXY_HOST); } catch (Throwable ignored) {}
            try { ProxyApplier.removeLocalFromList(WsBypassCore.LOCAL_PROXY_HOST); } catch (Throwable ignored) {}
            running = false;
            currentPort = 0;
            currentSecret = "";
            notifyReloader();
        }
    }

    private void resumeAfterVpn() {
        long token = 0L;
        synchronized (lifecycleLock) {

            if (blockedByVpnFresh()) return;
            try { ProxyApplier.restoreForVpn(); } catch (Throwable ignored) {}
            if (com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig
                    .isDataBypassEnabled()) {
                token = claimStartLocked();
            }
            notifyReloader();
        }
        if (token != 0L) launchStartThread(token);
    }

    public void stop() {
        synchronized (lifecycleLock) {
            invalidateStartLocked();
            cancelRelayAuthConnections();
            cancelWatchdogLocked();
            try {
                WsBypassCore.getInstance().stop();
            } catch (Throwable t) {
                FileLog.e("FGBypassController.stop core error", t);
            }
            try {
                ProxyApplier.apply(0, false, "", WsBypassCore.LOCAL_PROXY_HOST);
            } catch (Throwable t) {
                FileLog.e("FGBypassController.stop proxy error", t);
            }
            try {
                ProxyApplier.removeLocalFromList(WsBypassCore.LOCAL_PROXY_HOST);
            } catch (Throwable ignored) {}
            running = false;
            currentPort = 0;
            currentSecret = "";
            notifyReloader();
        }
    }

    public void onAppResume() {
        final boolean dataEnabled = com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig
                .isDataBypassEnabled();
        final boolean suspendOnVpn = com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig
                .isSuspendOnVpnEnabled();
        if (!dataEnabled && !suspendOnVpn) {
            return;
        }
        if (!resumeCheckPending.compareAndSet(false, true)) {
            return;
        }
        Utilities.globalQueue.postRunnable(() -> {
            try {

                FGVpnDetector.recheckNow();

                WsBypassCore.getInstance().wakeUp();
                WsBypassCore.getInstance().resetResilienceState();
                if (com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig
                        .isDataBypassEnabled() && !enforceVpnSuspensionFresh()
                        && !running && !starting.get()) {
                    startAsync();
                }
            } catch (Throwable ignored) {
            } finally {
                resumeCheckPending.set(false);
                notifyReloader();
            }
        });
    }

    private void startAsync() {
        long token = claimStart();
        if (token == 0L) return;
        launchStartThread(token);
    }

    private long claimStart() {
        synchronized (lifecycleLock) {
            return claimStartLocked();
        }
    }

    private long claimStartLocked() {
        if (running || activeStartToken != 0L) return 0L;
        long token = ++nextStartToken;
        if (token == 0L) token = ++nextStartToken;
        activeStartToken = token;
        starting.set(true);
        return token;
    }

    private void invalidateStartLocked() {
        activeStartToken = 0L;
        starting.set(false);
    }

    private void releaseStart(long token) {
        synchronized (lifecycleLock) {
            releaseStartLocked(token);
        }
    }

    private void releaseStartLocked(long token) {
        if (activeStartToken != token) return;
        activeStartToken = 0L;
        starting.set(false);
    }

    private void launchStartThread(final long token) {
        notifyReloader();
        Thread t = new Thread(new Runnable() {
            @Override
            public void run() {
                startSync(token);
            }
        }, "wsbypass-start");
        t.setDaemon(true);
        try {
            t.start();
        } catch (Throwable startFailure) {
            synchronized (lifecycleLock) {
                releaseStartLocked(token);
                lastError = String.valueOf(startFailure.getMessage());
                lastStartFailed = true;
                if (com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig
                        .isDataBypassEnabled() && !blockedByVpn()) {
                    ensureWatchdogLocked();
                }
            }
            FileLog.e("FGBypassController start thread failed", startFailure);
            notifyReloader();
        }
    }

    private void startSync(long token) {
        synchronized (lifecycleLock) {
            if (activeStartToken != token) return;
            try {

                if (!com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig
                        .isDataBypassEnabled()) {
                    WsBypassCore.dbg("startSync: обход выключен в настройках");
                    return;
                }
                if (blockedByVpnFresh()) {
                    WsBypassCore.dbg("startSync: спим под VPN");
                    suspendForVpn();
                    return;
                }
                int desiredPort = FGBypassConfig.localPort;
                String secret = FGBypassConfig.mtprotoSecret;
                if (secret == null || secret.isEmpty()) {
                    secret = MtprotoHandshake.generateSecretHex();
                    FGBypassConfig.setMtprotoSecret(secret);
                }

                WsBypassCore core = WsBypassCore.getInstance();
                WsBypassCore.dbg("startSync: поднимаю ядро, порт " + desiredPort);
                String err = core.start(desiredPort, secret);
                WsBypassCore.dbg("startSync: ядро ответило " + (err == null || err.isEmpty() ? "ок" : err));
                if (err != null && !err.isEmpty()) {
                    lastError = err;
                    lastStartFailed = true;
                    running = false;
                    ensureWatchdogLocked();
                    return;
                }

                int boundPort = core.getPort();
                currentPort = boundPort;
                currentSecret = core.getSecretHex();
                if (boundPort > 0 && boundPort != desiredPort) {
                    FGBypassConfig.setLocalPort(boundPort);
                }

                boolean proxyApplied;
                try {
                    proxyApplied = ProxyApplier.apply(
                            boundPort, true, currentSecret, WsBypassCore.LOCAL_PROXY_HOST);
                    WsBypassCore.dbg("startSync: прокси прописан = " + proxyApplied
                            + ", порт " + boundPort);
                } catch (Throwable t) {
                    FileLog.e("FGBypassController.startSync proxy apply error", t);
                    lastError = String.valueOf(t.getMessage());
                    lastStartFailed = true;
                    running = false;
                    try { core.stop(); } catch (Throwable ignored) {}
                    currentPort = 0;
                    currentSecret = "";
                    ensureWatchdogLocked();
                    return;
                }
                if (!proxyApplied) {
                    if (blockedByVpnFresh()) {

                        lastStartFailed = false;
                        lastError = "";
                        suspendForVpn();
                        return;
                    }
                    lastError = "proxy apply failed";
                    lastStartFailed = true;
                    running = false;
                    try { core.stop(); } catch (Throwable ignored) {}
                    try { ProxyApplier.apply(0, false, "", WsBypassCore.LOCAL_PROXY_HOST); } catch (Throwable ignored) {}
                    try { ProxyApplier.removeLocalFromList(WsBypassCore.LOCAL_PROXY_HOST); } catch (Throwable ignored) {}
                    currentPort = 0;
                    currentSecret = "";
                    ensureWatchdogLocked();
                    return;
                }

                running = true;
                lastStartFailed = false;
                lastError = "";
                try {
                    int account = org.telegram.messenger.UserConfig.selectedAccount;
                    WsRelayAuth.prefetchAsync(account);
                    if (com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig.isVoipBypassEnabled()) {
                        com.th3nekit.finegram.net.bypass.voip.VoipRelayAuth.prefetchAsync(account);
                    }
                } catch (Throwable ignored) {}
                ensureWatchdogLocked();
                FileLog.d("FGBypassController: started on 127.0.0.1:" + boundPort);
            } catch (Throwable t) {
                FileLog.e("FGBypassController.startSync error", t);
                if (blockedByVpnFresh()) {
                    lastStartFailed = false;
                    lastError = "";
                    suspendForVpn();
                } else {
                    lastError = String.valueOf(t.getMessage());
                    lastStartFailed = true;
                    running = false;
                    try { WsBypassCore.getInstance().stop(); } catch (Throwable ignored) {}
                    try {
                        ProxyApplier.apply(0, false, "", WsBypassCore.LOCAL_PROXY_HOST);
                    } catch (Throwable ignored) {}
                    try {
                        ProxyApplier.removeLocalFromList(WsBypassCore.LOCAL_PROXY_HOST);
                    } catch (Throwable ignored) {}
                    currentPort = 0;
                    currentSecret = "";
                    ensureWatchdogLocked();
                }
            } finally {
                releaseStartLocked(token);
                notifyReloader();
            }
        }
    }

    private void startWatchdogLocked() {
        cancelWatchdogLocked();
        final long generation = ++watchdogGeneration;
        ScheduledExecutorService pool = Executors.newSingleThreadScheduledExecutor(new ThreadFactory() {
            @Override
            public Thread newThread(Runnable r) {
                Thread t = new Thread(r, "wsbypass-watchdog");
                t.setDaemon(true);
                return t;
            }
        });
        watchdogPool = pool;
        try {
            watchdogTask = pool.scheduleAtFixedRate(new Runnable() {
                @Override
                public void run() {
                    try {
                        watchdogTick(generation);
                    } catch (Throwable t) {
                        FileLog.e("FGBypassController watchdog tick error", t);
                    }
                }
            }, WATCHDOG_INTERVAL_SEC, WATCHDOG_INTERVAL_SEC, TimeUnit.SECONDS);
        } catch (Throwable scheduleFailure) {
            watchdogTask = null;
            watchdogPool = null;
            pool.shutdownNow();
            FileLog.e("FGBypassController watchdog start failed", scheduleFailure);
        }
    }

    private void watchdogTick(long generation) {
        synchronized (lifecycleLock) {
            if (generation != watchdogGeneration || watchdogTask == null) return;
            if (!com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig
                    .isDataBypassEnabled()) return;

            if (blockedByVpnFresh()) {

                suspendForVpn();
                return;
            }
            if (activeStartToken != 0L) return;

            if (!running) {
                FileLog.d("FGBypassController watchdog: enabled but not running, restarting");
                restartCoreLocked();
                return;
            }

            boolean coreAlive;
            try {
                WsBypassCore core = WsBypassCore.getInstance();
                coreAlive = core.isRunning() && core.isAcceptThreadAlive();
            } catch (Throwable t) {
                coreAlive = false;
            }
            if (!coreAlive) {
                FileLog.d("FGBypassController watchdog: core dead while running, restarting");
                restartCoreLocked();
                return;
            }

            int p = currentPort;
            if (p <= 0) return;
            if (!ProxyApplier.isLocalProxyActive(
                    WsBypassCore.LOCAL_PROXY_HOST, p, currentSecret)) {
                FileLog.d("FGBypassController watchdog: proxy state diverged, re-applying");
                if (!ProxyApplier.apply(
                        p, true, currentSecret, WsBypassCore.LOCAL_PROXY_HOST)) {
                    restartCoreLocked();
                }
            }
        }
    }

    private void restartCoreLocked() {
        if (!com.th3nekit.finegram.net.bypass.voip.VoipBypassConfig
                .isDataBypassEnabled() || activeStartToken != 0L) {
            return;
        }
        if (blockedByVpnFresh()) {
            suspendForVpn();
            return;
        }
        try {
            running = false;
            try { WsBypassCore.getInstance().stop(); } catch (Throwable ignored) {}
            long token = claimStartLocked();
            if (token != 0L) startSync(token);
        } catch (Throwable t) {
            FileLog.e("FGBypassController.restartCore error", t);
        }
    }

    private void cancelWatchdogLocked() {
        watchdogGeneration++;
        ScheduledFuture<?> task = watchdogTask;
        watchdogTask = null;
        if (task != null) task.cancel(false);
        ScheduledExecutorService pool = watchdogPool;
        watchdogPool = null;
        if (pool != null) pool.shutdownNow();
    }

    private static void cancelRelayAuthConnections() {
        WsRelayAuth.cancelPendingAuth();
        com.th3nekit.finegram.net.bypass.voip.VoipRelayAuth.cancelPendingAuth();
    }

    private void ensureWatchdogLocked() {
        ScheduledFuture<?> task = watchdogTask;
        if (task == null || task.isCancelled() || task.isDone()) {
            startWatchdogLocked();
        }
    }

    private void notifyReloader() {
        final Runnable r = settingsReloader;
        if (r == null) return;
        try {
            AndroidUtilities.runOnUIThread(r);
        } catch (Throwable ignored) {}
    }
}

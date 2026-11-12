/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Runs Xray-core (MPL-2.0, Copyright the Xray authors, https://github.com/XTLS/Xray-core).
 */

package com.th3nekit.finegram.net.outbound;

import android.os.Environment;

import org.json.JSONArray;
import org.json.JSONObject;
import org.telegram.messenger.ApplicationLoader;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.CancellationException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

import com.th3nekit.finegram.core.FinegramLogger;

final class FGXrayProcess implements AutoCloseable {

    private static final String BINARY = "libxray.so";
    private static final long READY_TIMEOUT_MS = 8_000L;
    private static final Object sweepLock = new Object();
    private static boolean swept;
    private static volatile int available;

    private final Process process;
    private final File config;
    private final AtomicBoolean closed = new AtomicBoolean();

    private FGXrayProcess(Process process, File config) {
        this.process = process;
        this.config = config;
    }

    static File binary() {
        try {
            final String dir = ApplicationLoader.applicationContext.getApplicationInfo().nativeLibraryDir;
            if (dir == null) return null;
            if (android.os.Build.VERSION.SDK_INT < 26) {
                final File systemBin = new File(Environment.getRootDirectory(), "bin");
                if (!new File(systemBin, "toybox").canExecute() || !new File(systemBin, "sh").canExecute()) return null;
            }
            final File file = new File(dir, BINARY);
            return file.isFile() && file.canExecute() ? file : null;
        } catch (Throwable t) {
            return null;
        }
    }

    static boolean isAvailable() {
        int known = available;
        if (known == 0) {
            known = binary() != null ? 1 : -1;
            available = known;
        }
        return known > 0;
    }

    static String config(JSONObject outbound, FGOutboundCredentials credentials) throws Exception {
        final JSONObject root = new JSONObject();
        root.put("log", new JSONObject().put("loglevel", "warning").put("access", "none").put("dnsLog", false));

        final JSONObject account = new JSONObject();
        account.put("user", credentials.username);
        account.put("pass", credentials.password);
        final JSONObject settings = new JSONObject();
        settings.put("auth", "password");
        settings.put("accounts", new JSONArray().put(account));
        settings.put("udp", false);
        settings.put("ip", "127.0.0.1");
        final JSONObject in = new JSONObject();
        in.put("tag", "in");
        in.put("listen", "127.0.0.1");
        in.put("port", credentials.port);
        in.put("protocol", "socks");
        in.put("settings", settings);
        in.put("sniffing", new JSONObject().put("enabled", false));
        root.put("inbounds", new JSONArray().put(in));
        root.put("outbounds", new JSONArray().put(new JSONObject(outbound.toString())));
        return root.toString();
    }

    static FGXrayProcess start(JSONObject outbound, FGOutboundCredentials credentials, Consumer<FGXrayProcess> onExit) throws Exception {
        return start(outbound, credentials, onExit, () -> false);
    }

    static FGXrayProcess start(JSONObject outbound, FGOutboundCredentials credentials, Consumer<FGXrayProcess> onExit,
                               BooleanSupplier cancelled) throws Exception {
        checkCancelled(cancelled);
        final File binary = binary();
        if (binary == null) throw new IllegalStateException("xray is missing");
        sweepOrphans();
        final File dir = new File(ApplicationLoader.applicationContext.getCacheDir(), "outbound");
        if (!dir.isDirectory() && !dir.mkdirs()) throw new IllegalStateException("xray cache is unavailable");
        final File config = File.createTempFile("xray-", ".json", dir);
        FGXrayProcess handle = null;
        boolean ready = false;
        try {
            if (!config.setReadable(false, false) || !config.setReadable(true, true)) {
                throw new IllegalStateException("xray config permissions failed");
            }
            try (OutputStream output = new FileOutputStream(config)) {
                output.write(config(outbound, credentials).getBytes(StandardCharsets.UTF_8));
            }
            checkCancelled(cancelled);
            final ProcessBuilder builder;
            if (android.os.Build.VERSION.SDK_INT < 26) {
                final File systemBin = new File(Environment.getRootDirectory(), "bin");
                builder = new ProcessBuilder(new File(systemBin, "toybox").getAbsolutePath(), "setsid",
                        new File(systemBin, "sh").getAbsolutePath(), "-c",
                        "set +m; trap 'kill -KILL 0' HUP INT TERM; \"$@\" </dev/null & wait \"$!\"; exit $?",
                        "xray", binary.getAbsolutePath(), "run", "-c", config.getAbsolutePath());
            } else {
                builder = new ProcessBuilder(binary.getAbsolutePath(), "run", "-c", config.getAbsolutePath());
            }
            builder.directory(dir);
            builder.redirectErrorStream(true);
            builder.environment().put("XRAY_LOCATION_ASSET", dir.getAbsolutePath());
            handle = new FGXrayProcess(builder.start(), config);
            handle.drain(onExit);
            final long deadline = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(READY_TIMEOUT_MS);
            while (System.nanoTime() < deadline) {
                checkCancelled(cancelled);
                if (!handle.isAlive()) break;
                if (speaksSocks(credentials) && handle.isAlive()) {
                    checkCancelled(cancelled);
                    ready = true;
                    return handle;
                }
                Thread.sleep(80);
            }
            throw new IllegalStateException("xray did not open its authenticated port");
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw e;
        } finally {
            config.delete();
            if (!ready && handle != null) handle.close();
        }
    }

    private static void checkCancelled(BooleanSupplier cancelled) throws InterruptedException {
        if (Thread.currentThread().isInterrupted()) {
            throw new InterruptedException("xray startup cancelled");
        }
        if (cancelled.getAsBoolean()) throw new CancellationException("xray startup cancelled");
    }

    boolean isAlive() {
        try {
            process.exitValue();
            return false;
        } catch (IllegalThreadStateException alive) {
            return true;
        }
    }

    @Override
    public void close() {
        if (!closed.compareAndSet(false, true)) return;
        try {
            process.destroy();
            if (android.os.Build.VERSION.SDK_INT >= 26) {
                try {
                    if (!process.waitFor(250, TimeUnit.MILLISECONDS)) process.destroyForcibly();
                } catch (InterruptedException e) {
                    process.destroyForcibly();
                    Thread.currentThread().interrupt();
                }
            } else {
                final long deadline = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(250);
                while (isAlive() && System.nanoTime() < deadline) {
                    try {
                        Thread.sleep(10);
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                        break;
                    }
                }
            }
        } catch (Throwable ignored) {
        }
        try {
            process.getOutputStream().close();
        } catch (Throwable ignored) {
        }
        try {
            process.getInputStream().close();
        } catch (Throwable ignored) {
        }
        config.delete();
    }

    private void drain(Consumer<FGXrayProcess> onExit) {
        final Thread thread = new Thread(() -> {
            try (InputStream input = process.getInputStream()) {
                final byte[] buffer = new byte[4096];
                while (input.read(buffer) != -1) {
                }
            } catch (Throwable ignored) {
            }
            final int exitCode;
            try {
                exitCode = process.waitFor();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                close();
                return;
            }
            if (closed.get()) return;
            FinegramLogger.e("FGOutbound", () -> "xray завершился: код " + exitCode);
            close();
            if (onExit != null) onExit.accept(this);
        }, "FGXray");
        thread.setDaemon(true);
        thread.start();
    }

    private static boolean speaksSocks(FGOutboundCredentials credentials) {
        try (Socket socket = new Socket()) {
            socket.connect(new InetSocketAddress("127.0.0.1", credentials.port), 300);
            socket.setSoTimeout(500);
            final OutputStream output = socket.getOutputStream();
            output.write(new byte[]{5, 1, 2});
            final InputStream input = socket.getInputStream();
            if (input.read() != 5 || input.read() != 2) return false;
            final byte[] username = credentials.username.getBytes(StandardCharsets.UTF_8);
            final byte[] password = credentials.password.getBytes(StandardCharsets.UTF_8);
            output.write(1);
            output.write(username.length);
            output.write(username);
            output.write(password.length);
            output.write(password);
            output.flush();
            return input.read() == 1 && input.read() == 0;
        } catch (Throwable t) {
            return false;
        }
    }

    private static void sweepOrphans() {
        synchronized (sweepLock) {
            if (swept) return;
            swept = true;
            try {
                final File[] entries = new File("/proc").listFiles();
                if (entries == null) return;
                final int self = android.os.Process.myPid();
                final int uid = android.os.Process.myUid();
                for (File entry : entries) {
                    final int pid;
                    try {
                        pid = Integer.parseInt(entry.getName());
                    } catch (NumberFormatException e) {
                        continue;
                    }
                    if (pid == self) continue;
                    final String argv0 = argv0(new File(entry, "cmdline"));
                    if (argv0 == null || !BINARY.equals(new File(argv0).getName())) continue;
                    try {
                        if (android.system.Os.stat(entry.getPath()).st_uid != uid) continue;
                        android.os.Process.killProcess(pid);
                    } catch (Throwable ignored) {
                    }
                }
            } catch (Throwable ignored) {
            }
        }
    }

    private static String argv0(File cmdline) {
        try (InputStream input = new java.io.FileInputStream(cmdline)) {
            final byte[] buffer = new byte[512];
            final int read = input.read(buffer);
            if (read <= 0) return null;
            int end = 0;
            while (end < read && buffer[end] != 0) end++;
            return new String(buffer, 0, end, StandardCharsets.UTF_8);
        } catch (Throwable t) {
            return null;
        }
    }
}

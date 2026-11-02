/*
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.bypass;

import java.io.Closeable;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

final class WsSocketCleanup {
    private static final ExecutorService worker = Executors.newFixedThreadPool(2, task -> {
        Thread thread = new Thread(task, "wsbypass-close");
        thread.setDaemon(true);
        return thread;
    });

    static void close(List<?> sockets, Thread acceptThread) {
        if (sockets.isEmpty() && acceptThread == null) return;
        List<?> retired = new ArrayList<>(sockets);
        worker.execute(() -> {
            for (Object socket : retired) {
                try {
                    if (socket instanceof RawWebSocket) ((RawWebSocket) socket).close();
                    else if (socket instanceof Closeable) ((Closeable) socket).close();
                } catch (Throwable ignored) {
                }
            }
            if (acceptThread != null && acceptThread != Thread.currentThread()) {
                try {
                    acceptThread.join(1500);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }
        });
    }

    private WsSocketCleanup() {
    }
}

package org.telegram.proxy;

import com.th3nekit.finegram.net.outbound.FGOutboundCore;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.tgnet.RequestTimeDelegate;

import java.util.ArrayDeque;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class OutboundProxyConnectionTester {
    private static final OutboundProxyConnectionTester INSTANCE = new OutboundProxyConnectionTester();
    private final ArrayDeque<Request> queue = new ArrayDeque<>();
    private final ExecutorService worker = Executors.newSingleThreadExecutor(runnable -> {
        Thread thread = new Thread(runnable, "OutboundChecks");
        thread.setDaemon(true);
        return thread;
    });
    private Request current;

    public interface TestImplementation {
        void doCheck(int port, RequestTimeDelegate delegate);
    }

    private static final class Request {
        final String link;
        final RequestTimeDelegate delegate;
        final TestImplementation test;
        FGOutboundCore.Probe probe;
        Runnable timeout;

        Request(String link, RequestTimeDelegate delegate, TestImplementation test) {
            this.link = link;
            this.delegate = delegate;
            this.test = test;
        }
    }

    private OutboundProxyConnectionTester() {
    }

    public static OutboundProxyConnectionTester getInstance() {
        return INSTANCE;
    }

    public void checkProxy(ProxySettings settings, RequestTimeDelegate delegate, TestImplementation test) {
        if (delegate == null) return;
        AndroidUtilities.runOnUIThread(() -> {
            if (settings == null || test == null || !settings.isValid()) {
                delegate.run(-1);
                return;
            }
            queue.addLast(new Request(settings.getSecret(), delegate, test));
            processNext();
        });
    }

    private void processNext() {
        if (current != null) return;
        Request request = queue.pollFirst();
        if (request == null) return;
        current = request;
        request.timeout = () -> finish(request, -1);
        AndroidUtilities.runOnUIThread(request.timeout, 30_000);
        worker.execute(() -> {
            FGOutboundCore.Probe probe = FGOutboundCore.startProbe(request.link);
            AndroidUtilities.runOnUIThread(() -> {
                if (current != request) {
                    if (probe != null) worker.execute(probe::close);
                    return;
                }
                request.probe = probe;
                if (probe == null) {
                    finish(request, -1);
                    return;
                }
                try {
                    request.test.doCheck(probe.port,
                            time -> AndroidUtilities.runOnUIThread(() -> finish(request, time)));
                } catch (Throwable ignored) {
                    finish(request, -1);
                }
            });
        });
    }

    private void finish(Request request, long time) {
        if (current != request) return;
        AndroidUtilities.cancelRunOnUIThread(request.timeout);
        current = null;
        FGOutboundCore.Probe probe = request.probe;
        request.probe = null;
        worker.execute(() -> {
            if (probe != null) probe.close();
            AndroidUtilities.runOnUIThread(() -> {
                try {
                    request.delegate.run(time);
                } finally {
                    processNext();
                }
            });
        });
    }
}

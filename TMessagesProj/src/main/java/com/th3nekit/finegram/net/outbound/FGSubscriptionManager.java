/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.outbound;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.provider.Settings;
import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.SharedConfig;
import org.telegram.proxy.ProxySettings;
import org.telegram.tgnet.ConnectionsManager;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

public final class FGSubscriptionManager {
    public interface Callback {
        void onComplete(Result result);
    }

    public static final class Result {
        public final boolean success;
        public final FGSubscriptionError.Code error;
        public final int httpStatus;
        public final String subscriptionId;
        public final List<FGSubscription> subscriptions;
        public final int added;
        public final int removed;
        public final boolean selectionCleared;

        private Result(String id, List<FGSubscription> sources, Change change, FGSubscriptionError failure) {
            success = failure == null;
            error = failure == null ? null : failure.code;
            httpStatus = failure == null ? 0 : failure.httpStatus;
            subscriptionId = id;
            subscriptions = sources;
            added = change.added;
            removed = change.removed;
            selectionCleared = change.cleared;
        }
    }

    private static final FGSubscriptionManager INSTANCE = new FGSubscriptionManager();
    private final ThreadPoolExecutor worker = new ThreadPoolExecutor(1, 1, 0, TimeUnit.MILLISECONDS,
            new ArrayBlockingQueue<>(8), task -> {
                Thread thread = new Thread(task, "FGSubscriptions");
                thread.setDaemon(true);
                return thread;
            });
    private volatile List<FGSubscription> cached = Collections.emptyList();
    private volatile Map<String, String> cachedOwned = Collections.emptyMap();
    private volatile Map<ProxySettings, String> cachedServerIds = Collections.emptyMap();
    private volatile Map<ProxySettings, String> cachedServerNames = Collections.emptyMap();
    private FGSubscriptionState state;
    private SharedPreferences preferences;
    private Change appliedChange;

    private FGSubscriptionManager() {
    }

    public static FGSubscriptionManager getInstance() {
        return INSTANCE;
    }

    public List<FGSubscription> list() {
        return cached;
    }

    public String serverName(SharedConfig.ProxyInfo proxy) {
        return proxy == null ? null : cachedServerNames.get(proxy.settings);
    }

    public boolean isManaged(SharedConfig.ProxyInfo proxy) {
        if (proxy == null) return false;
        synchronized (SharedConfig.getProxyListSync()) {
            return SharedConfig.proxyList.contains(proxy) && cachedOwned.containsKey(rowKey(proxy.settings));
        }
    }

    public List<SharedConfig.ProxyInfo> servers(String id) {
        Set<String> members = new HashSet<>();
        for (FGSubscription source : cached) {
            if (source.id.equals(id)) for (FGSubscriptionParser.Server server : source.members) members.add(server.id);
        }
        if (members.isEmpty()) return Collections.emptyList();
        List<SharedConfig.ProxyInfo> result = new ArrayList<>();
        synchronized (SharedConfig.getProxyListSync()) {
            Map<ProxySettings, String> index = cachedServerIds;
            for (SharedConfig.ProxyInfo proxy : SharedConfig.proxyList) {
                if (members.contains(index.get(proxy.settings))) result.add(proxy);
            }
        }
        return Collections.unmodifiableList(result);
    }

    public void load(Callback callback) {
        submit(null, () -> new Change(), callback);
    }

    public void add(String name, String url, FGSubscriptionOptions options, Callback callback) {
        String id = UUID.randomUUID().toString();
        submit(id, () -> {
            if (state.sources.size() >= FGSubscriptionState.MAX_SUBSCRIPTIONS) throw error(FGSubscriptionError.Code.LIMIT);
            FGSubscription source = download(id, name, url, options, null);
            return replace(state.replace(id, source));
        }, callback);
    }

    public void refresh(String id, Callback callback) {
        submit(id, () -> {
            FGSubscription old = state.find(id);
            return updateSource(old, old.name, old.url, old.options);
        }, callback);
    }

    public void update(String id, String name, String url, FGSubscriptionOptions options, Callback callback) {
        submit(id, () -> updateSource(state.find(id), name, url, options), callback);
    }

    public void delete(String id, Callback callback) {
        submit(id, () -> {
            state.find(id);
            return replace(state.replace(id, null));
        }, callback);
    }

    public void remove(String id, Callback callback) {
        delete(id, callback);
    }

    public void retainManually(SharedConfig.ProxyInfo proxy, Callback callback) {
        submit(null, () -> {
            String key = onUi(() -> {
                synchronized (SharedConfig.getProxyListSync()) {
                    if (proxy == null || !SharedConfig.proxyList.contains(proxy)) throw error(FGSubscriptionError.Code.NOT_FOUND);
                    proxy.applyLegacyEdits();
                    return rowKey(proxy.settings);
                }
            });
            Map<String, String> owned = new LinkedHashMap<>(state.owned);
            if (key != null) owned.remove(key);
            save(new FGSubscriptionState(state.sources, owned, Collections.emptyList(), Collections.emptySet()));
            return new Change();
        }, callback);
    }

    private Change updateSource(FGSubscription old, String name, String url, FGSubscriptionOptions options) throws Exception {
        FGSubscription source;
        try {
            source = download(old.id, name, url, options, old);
        } catch (FGSubscriptionError failure) {
            if (failure.code != FGSubscriptionError.Code.STORAGE) {
                save(new FGSubscriptionState(state.replace(old.id, old.failed(failure.code, System.currentTimeMillis())),
                        state.owned, Collections.emptyList(), Collections.emptySet()));
            }
            throw failure;
        }
        return replace(state.replace(old.id, source));
    }

    private FGSubscription download(String id, String name, String url, FGSubscriptionOptions options, FGSubscription old) throws Exception {
        if (name == null) name = "";
        name = name.trim();
        if (name.length() > 128 || name.matches("(?s).*[\\p{Cntrl}].*")) throw error(FGSubscriptionError.Code.INVALID_FORMAT);
        if (url == null) throw error(FGSubscriptionError.Code.INVALID_URL);
        url = url.trim();
        if (options == null) options = FGSubscriptionOptions.none();
        options.validate();
        FGSubscriptionHttp.Identity identity = identity(options);
        FGSubscriptionHttp.source(url, identity.hwid);
        long attempt = System.currentTimeMillis();
        long deadline = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(FGSubscriptionHttp.TOTAL_MILLIS);
        Fetched fetched = null;
        try {
            fetched = fetch(url, identity, deadline);
        } catch (FGSubscriptionError failure) {
            if (failure.code != FGSubscriptionError.Code.NO_SERVERS) throw failure;
        }
        if (fetched == null || shrunk(fetched, old)) {
            Fetched retry = fetch(url, identity, deadline);
            if (fetched == null || retry.parsed.servers.size() > fetched.parsed.servers.size()) fetched = retry;
        }
        if (shrunk(fetched, old)) throw error(FGSubscriptionError.Code.SUSPICIOUS_SHRINK);
        FGSubscriptionMetadata metadata = fetched.metadata;
        if (old != null) metadata = metadata.merge(old.metadata);
        if (name.isEmpty()) name = metadata.profileTitle.substring(0, Math.min(128, metadata.profileTitle.length()));
        return new FGSubscription(id, name, url, options, metadata, fetched.parsed.servers,
                fetched.parsed.skipped, System.currentTimeMillis(), attempt, null);
    }

    private static boolean shrunk(Fetched fetched, FGSubscription old) {
        return old != null && fetched.parsed.servers.size() * 3 <= old.serverCount;
    }

    private static Fetched fetch(String url, FGSubscriptionHttp.Identity identity, long deadline) throws FGSubscriptionError {
        FGSubscriptionHttp.Response response = FGSubscriptionHttp.fetch(url, identity, deadline);
        FGSubscriptionParser.Parsed parsed = FGSubscriptionParser.parse(response.body);
        return new Fetched(parsed, FGSubscriptionMetadata.parse(response.headers, parsed.profileTitle));
    }

    private FGSubscriptionHttp.Identity identity(FGSubscriptionOptions options) throws FGSubscriptionError {
        String hwid = options.leanHwid;
        if (options.identityMode == FGSubscriptionOptions.IdentityMode.OWN_DEVICE) {
            String androidId = null;
            try {
                androidId = Settings.Secure.getString(ApplicationLoader.applicationContext.getContentResolver(), Settings.Secure.ANDROID_ID);
            } catch (Exception ignored) {
            }
            hwid = FGSubscriptionDevice.id(preferences, androidId);
        }
        return new FGSubscriptionHttp.Identity(options, hwid, Build.VERSION.RELEASE,
                Build.MANUFACTURER + " " + Build.MODEL, Locale.getDefault());
    }

    private void submit(String id, Callable<Change> action, Callback callback) {
        try {
            worker.execute(() -> {
                Change change = new Change();
                FGSubscriptionError failure = null;
                try {
                    change.add(ensureLoaded());
                    change.add(action.call());
                } catch (FGSubscriptionError error) {
                    failure = error;
                } catch (Exception ignored) {
                    failure = error(FGSubscriptionError.Code.STORAGE);
                }
                if (appliedChange != null) {
                    change.add(appliedChange);
                    appliedChange = null;
                }
                Result result = new Result(id, cached, change, failure);
                if (callback != null) AndroidUtilities.runOnUIThread(() -> callback.onComplete(result));
            });
        } catch (RejectedExecutionException ignored) {
            Result result = new Result(id, cached, new Change(), error(FGSubscriptionError.Code.BUSY));
            if (callback != null) AndroidUtilities.runOnUIThread(() -> callback.onComplete(result));
        }
    }

    private Change ensureLoaded() throws Exception {
        if (preferences == null) preferences = ApplicationLoader.applicationContext.getSharedPreferences("finegram_outbound_subscriptions", Context.MODE_PRIVATE);
        if (state == null) {
            FGSubscriptionState restored = FGSubscriptionState.decode(preferences.getString("state", ""));
            onUi(() -> {
                synchronized (SharedConfig.getProxyListSync()) { SharedConfig.loadProxyList(); }
                return null;
            });
            state = restored;
            cached = restored.sources;
            cachedOwned = restored.owned;
        }
        indexRows(snapshot());
        if (!state.pendingAdd.isEmpty() || !state.pendingRemove.isEmpty()) return finish(true);
        return new Change();
    }

    private List<ProxySettings> snapshot() throws Exception {
        return onUi(() -> {
            List<ProxySettings> rows = new ArrayList<>();
            synchronized (SharedConfig.getProxyListSync()) {
                for (SharedConfig.ProxyInfo proxy : SharedConfig.proxyList) {
                    proxy.applyLegacyEdits();
                    if (proxy.settings.getType() == ProxySettings.Type.OUTBOUND) rows.add(proxy.settings);
                }
            }
            return rows;
        });
    }

    private List<FGSubscriptionState.Row> indexRows(List<ProxySettings> snapshot) {
        List<FGSubscriptionState.Row> rows = new ArrayList<>();
        Map<ProxySettings, String> index = new LinkedHashMap<>();
        for (ProxySettings settings : snapshot) {
            String serverId = null;
            try {
                FGSubscriptionParser.Server server = FGSubscriptionParser.server(settings.getSecret());
                if (server.host.equals(settings.getAddress()) && server.port == settings.getPort()) serverId = server.id;
            } catch (FGSubscriptionError ignored) {
            }
            if (serverId != null) index.put(settings, serverId);
            rows.add(new FGSubscriptionState.Row(rowKey(settings), serverId));
        }
        cacheServers(index, state.sources);
        return rows;
    }

    private void cacheServers(Map<ProxySettings, String> index, List<FGSubscription> sources) {
        Map<String, String> titles = new LinkedHashMap<>();
        for (FGSubscription source : sources) {
            for (FGSubscriptionParser.Server server : source.members) titles.putIfAbsent(server.id, server.title);
        }
        Map<ProxySettings, String> names = new LinkedHashMap<>();
        for (Map.Entry<ProxySettings, String> entry : index.entrySet()) {
            String title = titles.get(entry.getValue());
            if (title != null) names.put(entry.getKey(), title);
        }
        cachedServerNames = Collections.unmodifiableMap(names);
        cachedServerIds = Collections.unmodifiableMap(index);
    }

    private Change replace(List<FGSubscription> sources) throws Exception {
        List<FGSubscriptionState.Row> rows = indexRows(snapshot());
        save(FGSubscriptionState.reconcile(state, sources, rows));
        return finish(false);
    }

    private Change finish(boolean recovering) throws Exception {
        FGSubscriptionState journal = state;
        Change change = onUi(() -> {
            Change result = new Change();
            appliedChange = result;
            result.owned.putAll(journal.owned);
            Map<ProxySettings, String> serverIndex = new LinkedHashMap<>(cachedServerIds);
            synchronized (SharedConfig.getProxyListSync()) {
                Set<String> present = new HashSet<>();
                for (int index = SharedConfig.proxyList.size() - 1; index >= 0; index--) {
                    SharedConfig.ProxyInfo proxy = SharedConfig.proxyList.get(index);
                    proxy.applyLegacyEdits();
                    String key = rowKey(proxy.settings);
                    if (journal.pendingRemove.contains(key)) {
                        serverIndex.remove(proxy.settings);
                        SharedConfig.proxyList.remove(index);
                        result.removed++;
                    } else present.add(key);
                }
                for (FGSubscriptionParser.Server server : journal.pendingAdd) {
                    String key = FGSubscriptionState.rowKey(server);
                    ProxySettings settings = ProxySettings.builder().setType(ProxySettings.Type.OUTBOUND)
                            .setAddress(server.host).setPort(server.port).setSecret(server.link).build();
                    serverIndex.put(settings, server.id);
                    if (!present.add(key)) {
                        if (!recovering) result.owned.remove(key);
                        continue;
                    }
                    SharedConfig.proxyList.add(new SharedConfig.ProxyInfo(settings));
                    result.added++;
                }
                cacheServers(serverIndex, journal.sources);
                if (SharedConfig.currentProxy != null) {
                    SharedConfig.currentProxy.applyLegacyEdits();
                    if (journal.pendingRemove.contains(rowKey(SharedConfig.currentProxy.settings))) {
                        SharedConfig.currentProxy = null;
                        SharedPreferences.Editor editor = MessagesController.getGlobalMainSettings().edit();
                        ProxySettings.EMPTY.toSharedPreferences(editor);
                        editor.putBoolean("proxy_enabled", false).putBoolean("proxy_enabled_calls", false).apply();
                        ConnectionsManager.setProxySettings(false, (ProxySettings) null);
                        result.cleared = true;
                    }
                }
                if (result.added != 0 || result.removed != 0 || result.cleared) {
                    SharedConfig.markProxyListChanged();
                    SharedConfig.saveProxyList();
                }
            }
            if (result.added != 0 || result.removed != 0 || result.cleared) {
                NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.proxySettingsChanged);
            }
            return result;
        });
        if (!MessagesController.getGlobalMainSettings().edit().commit()) throw error(FGSubscriptionError.Code.STORAGE);
        save(new FGSubscriptionState(journal.sources, change.owned, Collections.emptyList(), Collections.emptySet()));
        appliedChange = null;
        return change;
    }

    private void save(FGSubscriptionState next) throws FGSubscriptionError {
        String encoded = next.encode();
        String previous = preferences.getString("state", "");
        if (!preferences.edit().putString("state", encoded).commit()) {
            preferences.edit().putString("state", previous).commit();
            throw error(FGSubscriptionError.Code.STORAGE);
        }
        state = next;
        cached = next.sources;
        cachedOwned = next.owned;
    }

    private static String rowKey(ProxySettings settings) {
        if (settings.getType() != ProxySettings.Type.OUTBOUND) return null;
        return FGSubscriptionState.rowKey(settings.getAddress(), settings.getPort(), settings.getSecret());
    }

    private static <T> T onUi(Callable<T> action) throws Exception {
        FutureTask<T> task = new FutureTask<>(action);
        AndroidUtilities.runOnUIThread(task);
        try {
            return task.get();
        } catch (ExecutionException failure) {
            if (failure.getCause() instanceof Exception) throw (Exception) failure.getCause();
            throw error(FGSubscriptionError.Code.STORAGE);
        }
    }

    private static FGSubscriptionError error(FGSubscriptionError.Code code) {
        return new FGSubscriptionError(code);
    }

    private static final class Fetched {
        final FGSubscriptionParser.Parsed parsed;
        final FGSubscriptionMetadata metadata;

        Fetched(FGSubscriptionParser.Parsed parsed, FGSubscriptionMetadata metadata) {
            this.parsed = parsed;
            this.metadata = metadata;
        }
    }

    private static final class Change {
        int added;
        int removed;
        boolean cleared;
        final Map<String, String> owned = new LinkedHashMap<>();

        void add(Change other) {
            added += other.added;
            removed += other.removed;
            cleared |= other.cleared;
        }
    }
}

/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.outbound;

import org.json.JSONArray;
import org.json.JSONObject;
import org.json.JSONTokener;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

final class FGSubscriptionState {
    static final int MAX_SUBSCRIPTIONS = 32;
    static final int MAX_MEMBERS = 1024;
    static final int MAX_STATE_CHARS = 4 * 1024 * 1024;
    final List<FGSubscription> sources;
    final Map<String, String> owned;
    final List<FGSubscriptionParser.Server> pendingAdd;
    final Set<String> pendingRemove;

    FGSubscriptionState(List<FGSubscription> sources, Map<String, String> owned,
                        List<FGSubscriptionParser.Server> add, Set<String> remove) {
        this.sources = Collections.unmodifiableList(new ArrayList<>(sources));
        this.owned = Collections.unmodifiableMap(new LinkedHashMap<>(owned));
        pendingAdd = Collections.unmodifiableList(new ArrayList<>(add));
        pendingRemove = Collections.unmodifiableSet(new LinkedHashSet<>(remove));
    }

    static FGSubscriptionState empty() {
        return new FGSubscriptionState(Collections.emptyList(), Collections.emptyMap(), Collections.emptyList(), Collections.emptySet());
    }

    FGSubscriptionState clean() {
        return new FGSubscriptionState(sources, owned, Collections.emptyList(), Collections.emptySet());
    }

    FGSubscription find(String id) throws FGSubscriptionError {
        for (FGSubscription source : sources) if (source.id.equals(id)) return source;
        throw new FGSubscriptionError(FGSubscriptionError.Code.NOT_FOUND);
    }

    List<FGSubscription> replace(String id, FGSubscription value) {
        List<FGSubscription> result = new ArrayList<>();
        boolean found = false;
        for (FGSubscription source : sources) {
            if (source.id.equals(id)) {
                found = true;
                if (value != null) result.add(value);
            } else result.add(source);
        }
        if (!found && value != null) result.add(value);
        return result;
    }

    static final class Row {
        final String key;
        final String serverId;

        Row(String key, String serverId) {
            this.key = key;
            this.serverId = serverId;
        }
    }

    static String rowKey(String host, int port, String link) {
        return FGSubscriptionParser.digest(host.length() + ":" + host + ":" + port + ":" + link.length() + ":" + link);
    }

    static String rowKey(FGSubscriptionParser.Server server) {
        return rowKey(server.host, server.port, server.link);
    }

    static FGSubscriptionState reconcile(FGSubscriptionState old, List<FGSubscription> sources, List<Row> rows) throws FGSubscriptionError {
        if (sources.size() > MAX_SUBSCRIPTIONS) throw new FGSubscriptionError(FGSubscriptionError.Code.LIMIT);
        LinkedHashMap<String, FGSubscriptionParser.Server> wanted = new LinkedHashMap<>();
        int count = 0;
        for (FGSubscription source : sources) {
            count += source.serverCount;
            for (FGSubscriptionParser.Server server : source.members) wanted.putIfAbsent(server.id, server);
        }
        if (count > MAX_MEMBERS) throw new FGSubscriptionError(FGSubscriptionError.Code.LIMIT);
        LinkedHashMap<String, String> owners = new LinkedHashMap<>();
        Set<String> remove = new LinkedHashSet<>();
        Set<String> present = new HashSet<>();
        for (Row row : rows) {
            if (row.serverId != null) present.add(row.serverId);
            if (row.serverId != null && row.serverId.equals(old.owned.get(row.key))) {
                if (wanted.containsKey(row.serverId)) owners.put(row.key, row.serverId);
                else remove.add(row.key);
            }
        }
        List<FGSubscriptionParser.Server> add = new ArrayList<>();
        for (FGSubscriptionParser.Server server : wanted.values()) {
            if (present.contains(server.id)) continue;
            add.add(server);
            owners.put(rowKey(server), server.id);
        }
        return new FGSubscriptionState(sources, owners, add, remove);
    }

    String encode() throws FGSubscriptionError {
        try {
            JSONObject root = new JSONObject().put("schema", 1);
            JSONArray subscriptions = new JSONArray();
            int count = 0;
            for (FGSubscription source : sources) {
                count += source.serverCount;
                FGSubscriptionMetadata meta = source.metadata;
                JSONObject object = new JSONObject().put("id", source.id).put("name", source.name).put("url", source.url)
                        .put("mode", source.options.identityMode.name()).put("hwid", source.options.leanHwid)
                        .put("ua_mode", source.options.userAgentMode.name()).put("ua_custom", source.options.customUserAgent)
                        .put("updated", source.lastUpdated).put("attempt", source.lastAttempt).put("skipped", source.skipped)
                        .put("error", source.lastError == null ? "" : source.lastError.name())
                        .put("title", meta.profileTitle).put("userinfo", meta.hasUserInfo)
                        .put("upload", meta.upload).put("download", meta.download).put("total", meta.total).put("expire", meta.expire);
                JSONArray links = new JSONArray();
                for (FGSubscriptionParser.Server server : source.members) links.put(server.link);
                subscriptions.put(object.put("links", links));
            }
            if (sources.size() > MAX_SUBSCRIPTIONS || count > MAX_MEMBERS || owned.size() > MAX_MEMBERS) throw new FGSubscriptionError(FGSubscriptionError.Code.LIMIT);
            JSONObject owners = new JSONObject();
            for (Map.Entry<String, String> owner : owned.entrySet()) owners.put(owner.getKey(), owner.getValue());
            JSONArray add = new JSONArray();
            for (FGSubscriptionParser.Server server : pendingAdd) add.put(server.link);
            JSONArray remove = new JSONArray();
            for (String key : pendingRemove) remove.put(key);
            String result = root.put("subscriptions", subscriptions).put("owned", owners).put("add", add).put("remove", remove).toString();
            if (result.length() > MAX_STATE_CHARS) throw new FGSubscriptionError(FGSubscriptionError.Code.LIMIT);
            return result;
        } catch (FGSubscriptionError error) {
            throw error;
        } catch (Exception ignored) {
            throw new FGSubscriptionError(FGSubscriptionError.Code.STORAGE);
        }
    }

    static FGSubscriptionState decode(String encoded) throws FGSubscriptionError {
        if (encoded == null || encoded.isEmpty()) return empty();
        try {
            if (encoded.length() > MAX_STATE_CHARS) throw new IllegalArgumentException();
            FGSubscriptionXray.checkDepth(encoded);
            JSONTokener reader = new JSONTokener(encoded);
            Object decoded = reader.nextValue();
            if (!(decoded instanceof JSONObject) || reader.nextClean() != 0) throw new IllegalArgumentException();
            JSONObject root = (JSONObject) decoded;
            if (!Integer.valueOf(1).equals(root.get("schema"))) throw new IllegalArgumentException();
            JSONArray array = root.getJSONArray("subscriptions");
            if (array.length() > MAX_SUBSCRIPTIONS) throw new IllegalArgumentException();
            List<FGSubscription> sources = new ArrayList<>();
            Set<String> ids = new HashSet<>();
            Set<String> wanted = new HashSet<>();
            int count = 0;
            for (int i = 0; i < array.length(); i++) {
                JSONObject object = array.getJSONObject(i);
                String id = object.getString("id");
                if (!id.matches("[0-9a-f-]{36}") || !ids.add(id)) throw new IllegalArgumentException();
                FGSubscriptionOptions.IdentityMode mode = FGSubscriptionOptions.IdentityMode.valueOf(object.getString("mode"));
                FGSubscriptionOptions options = mode == FGSubscriptionOptions.IdentityMode.LEAN_EXPLICIT ? FGSubscriptionOptions.lean(object.getString("hwid"))
                        : mode == FGSubscriptionOptions.IdentityMode.EXPLICIT ? FGSubscriptionOptions.explicit(object.getString("hwid"))
                        : mode == FGSubscriptionOptions.IdentityMode.OWN_DEVICE ? FGSubscriptionOptions.ownDevice() : FGSubscriptionOptions.none();
                if (object.has("ua_mode") || object.has("ua_custom")) {
                    options = options.withUserAgent(FGSubscriptionOptions.UserAgentMode.valueOf(object.getString("ua_mode")), object.getString("ua_custom"));
                }
                options.validate();
                String name = object.getString("name");
                String url = object.getString("url");
                if (name.length() > 128 || url.length() > 4096) throw new IllegalArgumentException();
                List<FGSubscriptionParser.Server> members = readLinks(object.getJSONArray("links"), FGSubscriptionParser.MAX_SERVERS);
                if (members.isEmpty()) throw new IllegalArgumentException();
                count += members.size();
                if (count > MAX_MEMBERS) throw new IllegalArgumentException();
                for (FGSubscriptionParser.Server server : members) wanted.add(server.id);
                String error = object.getString("error");
                String title = object.getString("title");
                if (title.length() > 256) throw new IllegalArgumentException();
                FGSubscriptionMetadata metadata = new FGSubscriptionMetadata(title, object.getBoolean("userinfo"),
                        object.getLong("upload"), object.getLong("download"), object.getLong("total"), object.getLong("expire"));
                sources.add(new FGSubscription(id, name, url, options, metadata, members, object.getInt("skipped"),
                        object.getLong("updated"), object.getLong("attempt"), error.isEmpty() ? null : FGSubscriptionError.Code.valueOf(error)));
            }
            Map<String, String> owners = new LinkedHashMap<>();
            JSONObject owned = root.getJSONObject("owned");
            if (owned.length() > MAX_MEMBERS) throw new IllegalArgumentException();
            Iterator<String> keys = owned.keys();
            while (keys.hasNext()) {
                String key = keys.next();
                String value = owned.getString(key);
                if (!key.matches("[0-9a-f]{64}") || !wanted.contains(value)) throw new IllegalArgumentException();
                owners.put(key, value);
            }
            List<FGSubscriptionParser.Server> add = readLinks(root.getJSONArray("add"), MAX_MEMBERS);
            for (FGSubscriptionParser.Server server : add) if (!server.id.equals(owners.get(rowKey(server)))) throw new IllegalArgumentException();
            Set<String> remove = new LinkedHashSet<>();
            JSONArray removed = root.getJSONArray("remove");
            if (removed.length() > MAX_MEMBERS) throw new IllegalArgumentException();
            for (int i = 0; i < removed.length(); i++) {
                String key = removed.getString(i);
                if (!key.matches("[0-9a-f]{64}") || owners.containsKey(key)) throw new IllegalArgumentException();
                remove.add(key);
            }
            return new FGSubscriptionState(sources, owners, add, remove);
        } catch (Exception ignored) {
            throw new FGSubscriptionError(FGSubscriptionError.Code.STORAGE);
        }
    }

    private static List<FGSubscriptionParser.Server> readLinks(JSONArray links, int limit) throws Exception {
        if (links.length() > limit) throw new IllegalArgumentException();
        List<FGSubscriptionParser.Server> result = new ArrayList<>();
        Set<String> ids = new HashSet<>();
        for (int i = 0; i < links.length(); i++) {
            FGSubscriptionParser.Server server = FGSubscriptionParser.server(links.getString(i));
            if (!ids.add(server.id)) throw new IllegalArgumentException();
            result.add(server);
        }
        return result;
    }
}

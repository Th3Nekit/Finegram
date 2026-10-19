/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.updater;

import android.app.Activity;
import android.content.SharedPreferences;
import android.text.TextUtils;

import org.json.JSONArray;
import org.json.JSONObject;
import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.Utilities;
import org.telegram.ui.ActionBar.Theme;

import java.io.File;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.MessageDigest;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import com.th3nekit.finegram.core.configs.FinegramCoreConfig;
import com.th3nekit.finegram.core.helpers.FGResourcesHelper;

/**
 * Обновления клиента: опись на сервере проекта и загрузка файла оттуда же.
 *
 * Раньше обновления искались в канале: клиент перебирал последние сообщения, находил в них
 * json с пометкой и тянул APK вложением. Это работает, только пока в канале лежит такой
 * пост, и проверить это со стороны клиента нельзя. Здесь то же самое лежит одним файлом на
 * сервере проекта, рядом с описью плагинов, — его видно в браузере и можно поправить, не
 * трогая канал. Поиск по каналу остался запасным путём.
 *
 * Опись:
 * <pre>
 * {
 *   "release": {
 *     "version": "0.1.51",
 *     "version_code": 7141,
 *     "build_flavor": "standalone",
 *     "release_date": "26.09.2026",
 *     "can_not_skip": false,
 *     "changelog": "— первая строка\n— вторая",
 *     "builds": {
 *       "arm64-v8a":   { "url": "https://…/arm64.apk",     "size": 80913187, "sha256": "…" },
 *       "armeabi-v7a": { "url": "https://…/arm32.apk",     "size": 73655107, "sha256": "…" },
 *       "universal":   { "url": "https://…/universal.apk", "size": 105113665, "sha256": "…" }
 *     }
 *   },
 *   "beta": { … то же самое … }
 * }
 * </pre>
 * Ключи release и beta необязательны: опись без них считается выпуском.
 */
public final class FGClientUpdates {

    /** Опись обновлений: лежит рядом с описью плагинов. */
    public static final String MANIFEST_URL = "https://th3web.com/finegram/update.json";

    private static final int CONNECT_TIMEOUT_MS = 12_000;
    private static final int READ_TIMEOUT_MS = 20_000;
    private static final int MAX_MANIFEST_BYTES = 1024 * 1024;

    /** Сколько ждать между проверками: чаще незачем, опись меняется редко. */
    public static final long CHECK_INTERVAL_MS = 60L * 60 * 1000;

    private FGClientUpdates() {
    }

    /** Файл сборки под конкретную архитектуру. */
    public static final class Build {
        public final String abi;
        public final String url;
        public final long size;
        public final String sha256;

        Build(String abi, String url, long size, String sha256) {
            this.abi = abi;
            this.url = url;
            this.size = size;
            this.sha256 = sha256;
        }
    }

    /** Обновление целиком, как его описал сервер. */
    public static final class Update {
        public final String version;
        public final int versionCode;
        public final String flavor;
        public final String date;
        public final boolean canNotSkip;
        public final String changelog;
        public final ArrayList<Build> builds;

        Update(String version, int versionCode, String flavor, String date,
               boolean canNotSkip, String changelog, ArrayList<Build> builds) {
            this.version = version;
            this.versionCode = versionCode;
            this.flavor = flavor;
            this.date = date;
            this.canNotSkip = canNotSkip;
            this.changelog = changelog;
            this.builds = builds;
        }

        /**
         * Файл под это устройство.
         *
         * Архитектуры перебираем в том порядке, в котором их предпочитает система, и лишь
         * потом берём сборку на все архитектуры: она тяжелее вдвое.
         */
        public Build preferred() {
            for (String abi : android.os.Build.SUPPORTED_ABIS) {
                for (Build build : builds) {
                    if (build.abi.equals(abi)) return build;
                }
            }
            for (Build build : builds) {
                if ("universal".equals(build.abi)) return build;
            }
            return builds.isEmpty() ? null : builds.get(0);
        }
    }

    public interface Callback {
        void onResult(Update update, String error);
    }

    private static Update pending;
    private static boolean pendingLoaded;

    static SharedPreferences preferences() {
        return ApplicationLoader.applicationContext.getSharedPreferences("finegram_update_engine", 0);
    }

    public static synchronized Update getPending() {
        if (ApplicationLoader.applicationContext == null) return null;
        if (!pendingLoaded) {
            pending = deserialize(preferences().getString("pending", null));
            pendingLoaded = true;
        }
        if (pending != null && !isAvailable(pending)) {
            rememberChangelog(pending.version, pending.versionCode, pending.changelog);
            storePending(null);
            pending = null;
        }
        return pending;
    }

    public static synchronized void setPending(Update update) {
        if (update != null && !isAvailable(update)) update = null;
        if (storePending(update)) {
            pending = update;
            pendingLoaded = true;
        }
    }

    private static boolean storePending(Update update) {
        SharedPreferences.Editor editor = preferences().edit();
        if (update == null) editor.remove("pending");
        else editor.putString("pending", serialize(update));
        return editor.commit();
    }

    static boolean isAvailable(Update update) {
        return update != null && FGUpdatePolicy.isNewRelease(FGResourcesHelper.getFinegramVersion(),
                Utilities.parseInt(FGResourcesHelper.getCodeVersion()), update.version, update.versionCode);
    }

    static String serialize(Update update) {
        try {
            JSONObject json = new JSONObject();
            json.put("version", update.version);
            json.put("code", update.versionCode);
            json.put("flavor", update.flavor);
            json.put("date", update.date);
            json.put("required", update.canNotSkip);
            json.put("changelog", update.changelog);
            JSONArray builds = new JSONArray();
            for (Build build : update.builds) {
                JSONObject item = new JSONObject();
                item.put("abi", build.abi);
                item.put("url", build.url);
                item.put("size", build.size);
                item.put("hash", build.sha256);
                builds.put(item);
            }
            json.put("builds", builds);
            return json.toString();
        } catch (Exception error) {
            throw new IllegalArgumentException("Invalid update metadata");
        }
    }

    static Update deserialize(String text) {
        if (text == null || text.length() > 256 * 1024) return null;
        try {
            JSONObject json = new JSONObject(text);
            JSONArray items = json.getJSONArray("builds");
            ArrayList<Build> builds = new ArrayList<>();
            for (int i = 0; i < items.length(); i++) {
                JSONObject item = items.getJSONObject(i);
                builds.add(new Build(item.getString("abi"), item.getString("url"),
                        item.getLong("size"), emptyToNull(item.optString("hash", ""))));
            }
            return new Update(json.getString("version"), json.getInt("code"), json.optString("flavor", ""),
                    json.optString("date", ""), json.optBoolean("required", false),
                    json.optString("changelog", ""), builds);
        } catch (Exception error) {
            return null;
        }
    }

    public static String getInstalledChangelog() {
        final String cached = BaseRemoteHelper.preferences.getString(installedChangelogKey(), null);
        return TextUtils.isEmpty(cached)
                ? FGInstalledChangelog.forVersion(FGResourcesHelper.getFinegramVersion())
                : cached;
    }

    public static final class Changelog {
        public final String version;
        public final String text;
        private final int code;

        private Changelog(String version, int code, String text) {
            this.version = version;
            this.code = code;
            this.text = text;
        }
    }

    public static ArrayList<Changelog> getChangelogHistory() {
        HashMap<String, Changelog> entries = new HashMap<>();
        for (String version : FGInstalledChangelog.versions()) {
            addChangelog(entries, version, 0, FGInstalledChangelog.forVersion(version));
        }
        final String prefix = "installed_changelog_";
        for (Map.Entry<String, ?> item : BaseRemoteHelper.preferences.getAll().entrySet()) {
            String key = item.getKey();
            if (!key.startsWith(prefix) || !(item.getValue() instanceof String)) continue;
            int separator = key.lastIndexOf('_');
            if (separator <= prefix.length()) continue;
            try {
                addChangelog(entries, key.substring(prefix.length(), separator),
                        Integer.parseInt(key.substring(separator + 1)), (String) item.getValue());
            } catch (NumberFormatException ignored) {
            }
        }
        Update saved = deserialize(preferences().getString("pending", null));
        if (saved != null) addChangelog(entries, saved.version, saved.versionCode, saved.changelog);
        ArrayList<Changelog> result = new ArrayList<>(entries.values());
        result.sort((left, right) -> FGUpdatePolicy.compareVersions(right.version, left.version));
        return result;
    }

    private static void addChangelog(HashMap<String, Changelog> entries, String version, int code, String text) {
        if (!validChangelog(version, code, text)) return;
        int comparison = FGUpdatePolicy.compareVersions(version, FGResourcesHelper.getFinegramVersion());
        if (comparison > 0 || comparison == 0 && code > Utilities.parseInt(FGResourcesHelper.getCodeVersion())) return;
        Changelog previous = entries.get(version);
        if (previous == null || code >= previous.code) entries.put(version, new Changelog(version, code, text));
    }

    private static boolean validChangelog(String version, int code, String text) {
        return version != null && version.length() <= 64 && version.matches("[0-9]+(?:\\.[0-9]+){1,3}")
                && code >= 0 && text != null && !text.trim().isEmpty() && text.length() <= 128 * 1024;
    }

    private static void rememberChangelog(String version, int code, String text) {
        if (code > 0 && validChangelog(version, code, text)) {
            BaseRemoteHelper.preferences.edit().putString("installed_changelog_" + version + "_" + code, text).apply();
        }
    }

    private static void rememberChangelog(JSONObject branch) {
        if (branch != null) rememberChangelog(branch.optString("version", ""),
                branch.optInt("version_code", 0), changelogOf(branch));
    }

    private static String installedChangelogKey() {
        return "installed_changelog_" + FGResourcesHelper.getFinegramVersion() + "_" + FGResourcesHelper.getCodeVersion();
    }

    public static void refreshInstalledChangelog(Runnable callback) {
        if (!TextUtils.isEmpty(getInstalledChangelog())) {
            AndroidUtilities.runOnUIThread(callback);
            return;
        }
        Utilities.globalQueue.postRunnable(() -> {
            try {
                readManifest();
            } catch (Exception ignored) {
            }
            AndroidUtilities.runOnUIThread(callback);
        });
    }

    private static JSONObject readManifest() throws Exception {
        final JSONObject root = new JSONObject(readText(MANIFEST_URL));
        final JSONObject release = root.optJSONObject("release");
        final JSONObject beta = root.optJSONObject("beta");
        rememberChangelog(release);
        rememberChangelog(beta);
        if (!root.has("release") && !root.has("beta")) rememberChangelog(root);
        if (FinegramCoreConfig.INSTANCE.getInstallBetas() && cacheInstalledChangelog(beta)) return root;
        if (cacheInstalledChangelog(release) || cacheInstalledChangelog(beta)) return root;
        if (!root.has("release") && !root.has("beta")) cacheInstalledChangelog(root);
        return root;
    }

    private static boolean cacheInstalledChangelog(JSONObject branch) {
        if (branch == null || !FGResourcesHelper.getFinegramVersion().equals(branch.optString("version", ""))) return false;
        final int code = branch.optInt("version_code", 0);
        if (code > 0 && code != Utilities.parseInt(FGResourcesHelper.getCodeVersion())) return false;
        final String changelog = changelogOf(branch).trim();
        if (TextUtils.isEmpty(changelog)) return false;
        BaseRemoteHelper.preferences.edit().putString(installedChangelogKey(), changelog).apply();
        return true;
    }

    /**
     * Спрашивает сервер и отвечает в потоке интерфейса.
     *
     * Если обновления нет, приходит null без ошибки; ошибку отдаём отдельно, чтобы
     * «нет обновлений» и «сервер недоступен» не выглядели одинаково.
     */
    public static void check(Callback callback) {
        Utilities.globalQueue.postRunnable(() -> {
            Update update = null;
            String error = null;
            try {
                final JSONObject root = readManifest();
                final JSONObject branch = pickBranch(root);
                if (branch != null) {
                    update = parse(branch);
                }
                setPending(update);
            } catch (Throwable e) {
                error = ApplicationLoader.applicationContext.getString(org.telegram.messenger.R.string.FGUpdateErrorNetwork);
            }
            final Update result = update;
            final String resultError = error;
            AndroidUtilities.runOnUIThread(() -> {
                callback.onResult(result, resultError);
            });
        });
    }

    /** Ветка описи: бета или выпуск. Описи без веток считаем выпуском. */
    private static JSONObject pickBranch(JSONObject root) {
        final boolean betas = FinegramCoreConfig.INSTANCE.getInstallBetas();
        if (betas && root.has("beta")) {
            final JSONObject beta = root.optJSONObject("beta");
            if (beta != null && isNewer(beta)) return beta;
        }
        if (root.has("release")) {
            final JSONObject release = root.optJSONObject("release");
            return release != null && isNewer(release) ? release : null;
        }
        return isNewer(root) ? root : null;
    }

    /** Новее ли то, что предлагает сервер. Сравниваем и по имени, и по номеру сборки. */
    private static boolean isNewer(JSONObject branch) {
        return FGUpdatePolicy.isNewRelease(FGResourcesHelper.getFinegramVersion(),
                Utilities.parseInt(FGResourcesHelper.getCodeVersion()), branch.optString("version", ""),
                branch.optInt("version_code", 0));
    }

    private static Update parse(JSONObject branch) {
        final ArrayList<Build> builds = new ArrayList<>();
        final JSONObject files = branch.optJSONObject("builds");
        if (files != null) {
            for (Iterator<String> it = files.keys(); it.hasNext(); ) {
                final String abi = it.next();
                final JSONObject file = files.optJSONObject(abi);
                if (file == null) continue;
                final String url = file.optString("url", "");
                if (TextUtils.isEmpty(url)) continue;
                builds.add(new Build(abi, url, file.optLong("size", 0), emptyToNull(file.optString("sha256", ""))));
            }
        }
        return new Update(
                branch.optString("version", ""),
                branch.optInt("version_code", 0),
                branch.optString("build_flavor", ""),
                branch.optString("release_date", ""),
                branch.optBoolean("can_not_skip", false),
                changelogOf(branch),
                builds
        );
    }

    /** Список изменений принимаем и строкой, и списком строк: так опись удобнее писать руками. */
    private static String changelogOf(JSONObject branch) {
        final JSONArray lines = branch.optJSONArray("changelog");
        if (lines != null) {
            final StringBuilder text = new StringBuilder();
            for (int i = 0; i < lines.length(); i++) {
                if (i > 0) text.append('\n');
                text.append(lines.optString(i, ""));
            }
            return text.toString();
        }
        return branch.optString("changelog", "");
    }

    private static String emptyToNull(String value) {
        return TextUtils.isEmpty(value) ? null : value;
    }

    static File directory() {
        return new File(ApplicationLoader.applicationContext.getFilesDir(), "finegram-updates");
    }

    static String identity(Update update) {
        try {
            Build build = update.preferred();
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            String[] values = { update.version, Integer.toString(update.versionCode), update.flavor,
                    build == null ? "" : build.abi, build == null ? "" : build.url,
                    build == null ? "" : Long.toString(build.size), build == null ? "" : build.sha256 };
            for (String value : values) {
                byte[] bytes = (value == null ? "" : value).getBytes(StandardCharsets.UTF_8);
                digest.update(Integer.toString(bytes.length).getBytes(StandardCharsets.US_ASCII));
                digest.update((byte) ':');
                digest.update(bytes);
            }
            return FGUpdateTransfer.hex(digest.digest());
        } catch (Exception error) {
            throw new IllegalArgumentException("Invalid update identity");
        }
    }

    public static File fileFor(Update update) {
        return new File(directory(), "fg-update-" + identity(update) + ".apk");
    }

    public static File readyFile(Update update) {
        return FGUpdateDownload.verifiedFile(update);
    }

    public static void install(File apk, Activity activity, Theme.ResourcesProvider resourcesProvider) {
        FGUpdateDownload.install(apk, activity, resourcesProvider);
    }

    // --- сеть ---

    private static String readText(String url) throws Exception {
        HttpURLConnection connection = null;
        try {
            connection = open(url);
            String length = connection.getHeaderField("Content-Length");
            if (length != null) {
                long declared = Long.parseLong(length.trim());
                if (declared < 0 || declared > MAX_MANIFEST_BYTES) throw new IOException("Invalid manifest size");
            }
            String encoding = connection.getHeaderField("Content-Encoding");
            if (encoding != null && !"identity".equalsIgnoreCase(encoding)) throw new IOException("Encoded manifest response");
            try (InputStream in = connection.getInputStream(); ByteArrayOutputStream bytes = new ByteArrayOutputStream()) {
                final byte[] buffer = new byte[8192];
                int read;
                while ((read = in.read(buffer)) != -1) {
                    if (read > MAX_MANIFEST_BYTES - bytes.size()) throw new IOException("Manifest exceeds size limit");
                    bytes.write(buffer, 0, read);
                }
                return new String(bytes.toByteArray(), StandardCharsets.UTF_8);
            }
        } finally {
            if (connection != null) connection.disconnect();
        }
    }

    private static HttpURLConnection open(String url) throws Exception {
        URL target = new URL(url);
        for (int redirects = 0; redirects <= 5; redirects++) {
            if (!"https".equals(target.getProtocol())) throw new IOException("Invalid manifest protocol");
            HttpURLConnection connection = (HttpURLConnection) target.openConnection();
            connection.setConnectTimeout(CONNECT_TIMEOUT_MS);
            connection.setReadTimeout(READ_TIMEOUT_MS);
            connection.setInstanceFollowRedirects(false);
            connection.setRequestProperty("Accept-Encoding", "identity");
            connection.setRequestProperty("User-Agent", "Finegram/" + FGResourcesHelper.getFinegramVersion()
                    + " (Android " + android.os.Build.VERSION.RELEASE + ")");
            try {
                int code = connection.getResponseCode();
                if (code >= 200 && code <= 299) return connection;
                if (code != 301 && code != 302 && code != 303 && code != 307 && code != 308) {
                    throw new IOException("Manifest HTTP error");
                }
                String location = connection.getHeaderField("Location");
                if (location == null || redirects == 5) throw new IOException("Invalid manifest redirect");
                target = new URL(target, location);
            } catch (Exception error) {
                connection.disconnect();
                throw error;
            }
            connection.disconnect();
        }
        throw new IOException("Too many manifest redirects");
    }

}

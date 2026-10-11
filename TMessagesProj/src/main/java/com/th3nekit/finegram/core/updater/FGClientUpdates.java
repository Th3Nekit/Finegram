/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.updater;

import android.app.Activity;
import android.text.TextUtils;

import org.json.JSONArray;
import org.json.JSONObject;
import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.FileLoader;
import org.telegram.messenger.Utilities;
import org.telegram.ui.ActionBar.Theme;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.concurrent.atomic.AtomicBoolean;

import com.th3nekit.finegram.core.FinegramLogger;
import com.th3nekit.finegram.core.configs.FinegramCoreConfig;
import com.th3nekit.finegram.core.helpers.FGResourcesHelper;

public final class FGClientUpdates {

    public static final String MANIFEST_URL = "https://th3web.com/finegram/update.json";

    private static final int CONNECT_TIMEOUT_MS = 12_000;
    private static final int READ_TIMEOUT_MS = 20_000;

    public static final long CHECK_INTERVAL_MS = 60L * 60 * 1000;

    private FGClientUpdates() {
    }

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

    public interface ProgressCallback {
        void onProgress(long done, long total);
    }

    public interface DownloadCallback {
        void onDone(File file, String error);
    }

    private static volatile Update pending;

    public static Update getPending() {
        return pending;
    }

    public static void setPending(Update update) {
        pending = update;
    }

    public static void check(Callback callback) {
        Utilities.globalQueue.postRunnable(() -> {
            Update update = null;
            String error = null;
            try {
                final JSONObject root = new JSONObject(readText(MANIFEST_URL));
                final JSONObject branch = pickBranch(root);
                if (branch != null) {
                    update = parse(branch);
                }
            } catch (Throwable e) {
                error = e.getMessage() == null ? e.getClass().getSimpleName() : e.getMessage();
                FinegramLogger.e("FGUpdates", () -> "опись обновлений не прочиталась", e);
            }
            final Update result = update;
            final String resultError = error;
            AndroidUtilities.runOnUIThread(() -> {
                if (result != null) {
                    pending = result;
                }
                callback.onResult(result, resultError);
            });
        });
    }

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

    private static boolean isNewer(JSONObject branch) {
        final String version = branch.optString("version", "");
        if (TextUtils.isEmpty(version)) return false;
        if (UpdateHelper.isNew(FGResourcesHelper.getFinegramVersion(), version)) return true;
        final int code = branch.optInt("version_code", 0);
        return code > 0 && UpdateHelper.isNew(FGResourcesHelper.getCodeVersion(), String.valueOf(code));
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

    public static File fileFor(Update update) {
        final File dir = FileLoader.getDirectory(FileLoader.MEDIA_DIR_CACHE);
        return new File(dir, "Finegram-" + update.version + "-" + abiOf(update) + ".apk");
    }

    private static String abiOf(Update update) {
        final Build build = update.preferred();
        return build == null ? "unknown" : build.abi;
    }

    public static File readyFile(Update update) {
        final Build build = update.preferred();
        if (build == null) return null;
        final File file = fileFor(update);
        if (!file.exists()) return null;
        if (build.size > 0 && file.length() != build.size) return null;
        return file;
    }

    public static AtomicBoolean download(Update update, ProgressCallback progress, DownloadCallback done) {
        final AtomicBoolean cancelled = new AtomicBoolean(false);
        final Build build = update.preferred();
        if (build == null) {
            AndroidUtilities.runOnUIThread(() -> done.onDone(null, "нет файла для этого устройства"));
            return cancelled;
        }
        Utilities.globalQueue.postRunnable(() -> {
            File result = null;
            String error = null;
            final File target = fileFor(update);
            final File part = new File(target.getAbsolutePath() + ".part");
            HttpURLConnection connection = null;
            try {
                connection = open(build.url);
                final long total = build.size > 0 ? build.size : connection.getContentLength();
                final MessageDigest digest = build.sha256 == null ? null : MessageDigest.getInstance("SHA-256");
                long done1 = 0;
                long lastReported = -1;
                try (InputStream in = connection.getInputStream();
                     OutputStream out = new FileOutputStream(part)) {
                    final byte[] buffer = new byte[64 * 1024];
                    int read;
                    while ((read = in.read(buffer)) > 0) {
                        if (cancelled.get()) throw new InterruptedException("отменено");
                        out.write(buffer, 0, read);
                        if (digest != null) digest.update(buffer, 0, read);
                        done1 += read;

                        final long step = Math.max(total / 200, 64 * 1024);
                        if (done1 - lastReported >= step) {
                            lastReported = done1;
                            final long reported = done1;
                            AndroidUtilities.runOnUIThread(() -> progress.onProgress(reported, total));
                        }
                    }
                }
                if (digest != null) {
                    final String actual = hex(digest.digest());
                    if (!actual.equalsIgnoreCase(build.sha256)) {
                        throw new IllegalStateException("файл повреждён при передаче");
                    }
                }

                target.delete();
                if (!part.renameTo(target)) {
                    throw new IllegalStateException("не записать файл");
                }
                result = target;
            } catch (Throwable e) {

                part.delete();
                if (!cancelled.get()) {
                    error = e.getMessage() == null ? e.getClass().getSimpleName() : e.getMessage();
                    FinegramLogger.e("FGUpdates", () -> "обновление не скачалось", e);
                }
            } finally {
                if (connection != null) connection.disconnect();
            }
            final File file = result;
            final String resultError = error;
            AndroidUtilities.runOnUIThread(() -> {
                if (cancelled.get()) return;
                done.onDone(file, resultError);
            });
        });
        return cancelled;
    }

    public static void install(File apk, Activity activity, Theme.ResourcesProvider resourcesProvider) {
        if (apk == null || activity == null) return;
        AndroidUtilities.openForView(apk, apk.getName(), "application/vnd.android.package-archive",
                activity, resourcesProvider, false);
    }

    private static String readText(String url) throws Exception {
        HttpURLConnection connection = null;
        try {
            connection = open(url);
            try (InputStream in = connection.getInputStream()) {
                final StringBuilder text = new StringBuilder();
                final byte[] buffer = new byte[8192];
                int read;
                while ((read = in.read(buffer)) > 0) {
                    text.append(new String(buffer, 0, read, "UTF-8"));
                }
                return text.toString();
            }
        } finally {
            if (connection != null) connection.disconnect();
        }
    }

    private static HttpURLConnection open(String url) throws Exception {
        final HttpURLConnection connection = (HttpURLConnection) new URL(url).openConnection();
        connection.setConnectTimeout(CONNECT_TIMEOUT_MS);
        connection.setReadTimeout(READ_TIMEOUT_MS);
        connection.setInstanceFollowRedirects(true);
        connection.setRequestProperty("User-Agent", "Finegram/" + FGResourcesHelper.getFinegramVersion()
                + " (Android " + android.os.Build.VERSION.RELEASE + ")");
        connection.connect();
        final int code = connection.getResponseCode();
        if (code < 200 || code > 299) {
            throw new IllegalStateException("сервер ответил " + code);
        }
        return connection;
    }

    private static String hex(byte[] bytes) {
        final StringBuilder text = new StringBuilder(bytes.length * 2);
        for (byte b : bytes) {
            text.append(Character.forDigit((b >> 4) & 0xf, 16));
            text.append(Character.forDigit(b & 0xf, 16));
        }
        return text.toString();
    }
}

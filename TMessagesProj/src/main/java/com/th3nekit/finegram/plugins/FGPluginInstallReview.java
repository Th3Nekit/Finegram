/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.plugins;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;

import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;
import java.util.function.BooleanSupplier;
import java.util.zip.ZipFile;

public final class FGPluginInstallReview implements AutoCloseable {
    private static final int MAX_SIZE = 32 * 1024 * 1024;
    private final Set<File> files = new HashSet<>();
    private volatile boolean closed;

    public static final class Snapshot implements AutoCloseable {
        public final File file;
        public final JSONObject report;
        private final FGPluginInstallReview owner;

        private Snapshot(FGPluginInstallReview owner, File file, JSONObject report) {
            this.owner = owner;
            this.file = file;
            this.report = report;
        }

        public File takeFile() {
            return owner.take(file);
        }

        @Override
        public void close() {
            owner.discard(file);
        }
    }

    public Snapshot inspect(Context context, String path, BooleanSupplier cancelled) {
        File file = null;
        boolean complete = false;
        try {
            check(cancelled);
            if (context == null || path == null || path.isEmpty()) throw new IOException();
            Uri uri = Uri.parse(path);
            boolean content = "content".equalsIgnoreCase(uri.getScheme());
            File source = content ? null : new File(path);
            if (!content && !source.isAbsolute()) throw new IOException();
            String name = content ? name(context, uri) : source.getName();
            File cache = context.getCacheDir();
            if (cache == null) throw new IOException();
            File dir = new File(cache, "plugin-review");
            if (!dir.isDirectory() && !dir.mkdirs()) throw new IOException();
            file = File.createTempFile("install-", ".tmp", dir);
            if (!add(file)) throw new IOException();
            try (InputStream input = content ? context.getContentResolver().openInputStream(uri)
                    : new FileInputStream(source);
                 FileOutputStream output = new FileOutputStream(file)) {
                if (input == null) throw new IOException();
                byte[] buffer = new byte[8192];
                int total = 0;
                int count;
                while ((count = input.read(buffer)) != -1) {
                    check(cancelled);
                    if (count > MAX_SIZE - total) throw new IOException();
                    output.write(buffer, 0, count);
                    total += count;
                }
            }
            check(cancelled);
            String suffix = suffix(file, name);
            synchronized (this) {
                check(cancelled);
                File snapshot = new File(dir, file.getName() + suffix);
                if (snapshot.exists() || !file.renameTo(snapshot)) throw new IOException();
                files.remove(file);
                file = snapshot;
                files.add(file);
            }
            JSONObject report = FGPluginSecurity.inspect(file, FGPluginSecuritySettings.checkOnInstall());
            check(cancelled);
            if (report == null) report = new JSONObject();
            complete = true;
            return new Snapshot(this, file, report);
        } catch (Exception e) {
            return new Snapshot(this, null, new JSONObject());
        } finally {
            if (!complete && file != null) {
                synchronized (this) {
                    files.remove(file);
                    file.delete();
                }
            }
        }
    }

    private void check(BooleanSupplier cancelled) throws IOException {
        if (closed || cancelled.getAsBoolean()) throw new IOException();
    }

    private synchronized boolean add(File file) {
        if (closed) return false;
        files.add(file);
        return true;
    }

    private synchronized File take(File file) {
        return !closed && file != null && files.remove(file) ? file : null;
    }

    private synchronized void discard(File file) {
        if (file != null && files.remove(file)) file.delete();
    }

    @Override
    public synchronized void close() {
        closed = true;
        for (File file : files) file.delete();
        files.clear();
    }

    private static String name(Context context, Uri uri) {
        try (Cursor cursor = context.getContentResolver().query(uri,
                new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int column = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (column >= 0) {
                    String value = cursor.getString(column);
                    if (value != null && !value.isEmpty()) return value;
                }
            }
        } catch (Exception e) {
        }
        return uri.getLastPathSegment();
    }

    private static String suffix(File file, String name) throws IOException {
        String named = ".plugin";
        if (name != null) {
            String lower = name.toLowerCase(Locale.ROOT);
            for (String suffix : new String[]{".plugin", ".eaf", ".dex", ".apk", ".jar", ".zip"}) {
                if (lower.endsWith(suffix)) {
                    named = suffix;
                    break;
                }
            }
        }
        byte[] header = new byte[4];
        int count;
        try (FileInputStream input = new FileInputStream(file)) {
            count = input.read(header);
        }
        if (count == 4 && header[0] == 'd' && header[1] == 'e' && header[2] == 'x' && header[3] == '\n') {
            return ".dex";
        }
        if (count == 4 && header[0] == 'P' && header[1] == 'K'
                && ((header[2] == 3 && header[3] == 4) || (header[2] == 5 && header[3] == 6)
                || (header[2] == 7 && header[3] == 8))) {
            try (ZipFile zip = new ZipFile(file)) {
                if (named.equals(".jar") || named.equals(".zip") || named.equals(".apk") || named.equals(".eaf")) {
                    return named;
                }
                if (zip.getEntry("AndroidManifest.xml") != null || zip.getEntry("classes.dex") != null) {
                    return ".apk";
                }
            }
            return ".eaf";
        }
        return named;
    }
}

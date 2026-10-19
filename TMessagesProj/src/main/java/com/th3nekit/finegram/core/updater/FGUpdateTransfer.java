/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.updater;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.MessageDigest;
import java.util.Properties;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

final class FGUpdateTransfer {
    static final long MAX_BYTES = 1024L * 1024 * 1024;
    private static final Pattern RANGE = Pattern.compile("bytes ([0-9]+)-([0-9]+)/([0-9]+)");
    private static final Pattern UNSATISFIED = Pattern.compile("bytes \\*/([0-9]+)");
    private final File part;
    private final File metadata;
    private final String url;
    private final long expected;

    interface Progress {
        void onProgress(long downloaded, long total);
    }

    static final class Cancellation {
        final AtomicBoolean cancelled = new AtomicBoolean();
        private volatile HttpURLConnection connection;

        boolean isCancelled() {
            return cancelled.get();
        }

        void cancel() {
            cancelled.set(true);
            HttpURLConnection active = connection;
            if (active != null) active.disconnect();
        }

        void check() throws IOException {
            if (isCancelled()) throw new IOException("Paused");
        }
    }

    FGUpdateTransfer(File part, String url, long expected) {
        this.part = part;
        this.metadata = new File(part.getPath() + ".http");
        this.url = url;
        this.expected = expected;
    }

    long download(Cancellation cancellation, Progress progress) throws Exception {
        if (expected < 0 || expected > MAX_BYTES) throw new IOException("Invalid update size");
        cancellation.check();
        Properties saved;
        try {
            saved = FGUpdateFiles.read(metadata);
        } catch (IOException | IllegalArgumentException error) {
            saved = new Properties();
        }
        long offset = part.isFile() ? part.length() : 0;
        String previousTag = strongTag(saved.getProperty("etag"));
        long previousTotal = number(saved.getProperty("total"), -1);
        boolean resume = offset > 0 && offset <= MAX_BYTES && previousTag != null
                && (previousTotal < 0 || offset <= previousTotal)
                && (expected == 0 || offset <= expected);
        HttpURLConnection connection = open(cancellation, resume ? offset : 0, resume ? previousTag : null);
        try {
            cancellation.check();
            int code = connection.getResponseCode();
            String tag = strongTag(connection.getHeaderField("ETag"));
            if (code == 416) {
                Matcher range = UNSATISFIED.matcher(header(connection, "Content-Range"));
                if (!resume || !range.matches() || !previousTag.equals(tag)) throw new IOException("Invalid range response");
                long total = positive(range.group(1));
                if (offset != total || previousTotal != total || expected > 0 && total != expected) {
                    throw new IOException("Incomplete update range");
                }
                progress.onProgress(offset, total);
                return total;
            }
            if (code != 200 && code != 206) throw new IOException("HTTP " + code);
            String encoding = header(connection, "Content-Encoding");
            if (!encoding.isEmpty() && !"identity".equalsIgnoreCase(encoding)) throw new IOException("Encoded update response");
            long length = contentLength(connection);
            long total;
            if (code == 206) {
                Matcher range = RANGE.matcher(header(connection, "Content-Range"));
                if (!resume || !range.matches() || !previousTag.equals(tag)) throw new IOException("Invalid partial response");
                long start = positive(range.group(1));
                long end = positive(range.group(2));
                total = positive(range.group(3));
                if (start != offset || end < start || end != total - 1 || total > MAX_BYTES
                        || previousTotal >= 0 && previousTotal != total
                        || length >= 0 && length != end - start + 1) throw new IOException("Invalid partial response length");
            } else {
                if (!header(connection, "Content-Range").isEmpty()) throw new IOException("Unexpected content range");
                total = length;
                offset = 0;
            }
            if (total > MAX_BYTES || expected > 0 && total >= 0 && total != expected) throw new IOException("Unexpected update size");
            if (total < 0 && expected > 0) total = expected;
            File directory = part.getParentFile();
            if (!directory.isDirectory() && !directory.mkdirs()) throw new IOException("Cannot create update directory");
            cancellation.check();
            try (InputStream in = connection.getInputStream();
                 FileOutputStream out = new FileOutputStream(part, code == 206)) {
                Properties next = new Properties();
                if (tag != null) next.setProperty("etag", tag);
                next.setProperty("total", Long.toString(total));
                FGUpdateFiles.write(metadata, next);
                long downloaded = offset;
                long syncAt = System.nanoTime();
                progress.onProgress(downloaded, total);
                byte[] buffer = new byte[64 * 1024];
                try {
                    while (true) {
                        cancellation.check();
                        int read = in.read(buffer);
                        cancellation.check();
                        if (read == -1) break;
                        if (read == 0) continue;
                        long limit = total >= 0 ? total : MAX_BYTES;
                        if (read > limit - downloaded) throw new IOException("Update exceeds declared size");
                        out.write(buffer, 0, read);
                        downloaded += read;
                        progress.onProgress(downloaded, total);
                        if (System.nanoTime() - syncAt >= 1_000_000_000L) {
                            out.getFD().sync();
                            syncAt = System.nanoTime();
                        }
                    }
                    if (downloaded == 0 || total >= 0 && downloaded != total) throw new IOException("Incomplete update body");
                    if (total < 0) {
                        total = downloaded;
                        next.setProperty("total", Long.toString(total));
                        FGUpdateFiles.write(metadata, next);
                    }
                    cancellation.check();
                    progress.onProgress(downloaded, total);
                    return total;
                } finally {
                    out.getFD().sync();
                }
            }
        } finally {
            cancellation.connection = null;
            connection.disconnect();
        }
    }

    private HttpURLConnection open(Cancellation cancellation, long offset, String etag) throws Exception {
        URL target = new URL(url);
        for (int redirects = 0; redirects <= 5; redirects++) {
            cancellation.check();
            String protocol = target.getProtocol();
            if (!"https".equals(protocol)) throw new IOException("Invalid update protocol");
            HttpURLConnection connection = (HttpURLConnection) target.openConnection();
            cancellation.connection = connection;
            connection.setConnectTimeout(12_000);
            connection.setReadTimeout(20_000);
            connection.setInstanceFollowRedirects(false);
            connection.setRequestProperty("Accept-Encoding", "identity");
            connection.setRequestProperty("User-Agent", "Finegram-Updater");
            if (etag != null) {
                connection.setRequestProperty("Range", "bytes=" + offset + "-");
                connection.setRequestProperty("If-Range", etag);
            }
            try {
                int code = connection.getResponseCode();
                if (code != 301 && code != 302 && code != 303 && code != 307 && code != 308) return connection;
                String location = connection.getHeaderField("Location");
                if (location == null) throw new IOException("Invalid update redirect");
                URL next = new URL(target, location);
                if ("https".equals(protocol) && !"https".equals(next.getProtocol())) throw new IOException("Insecure update redirect");
                target = next;
            } catch (Exception error) {
                connection.disconnect();
                cancellation.connection = null;
                throw error;
            }
            connection.disconnect();
            cancellation.connection = null;
        }
        throw new IOException("Too many update redirects");
    }

    static void verifyHash(File file, String sha256, Cancellation cancellation) throws Exception {
        cancellation.check();
        if (sha256 == null) return;
        if (!sha256.matches("[a-fA-F0-9]{64}")) throw new IOException("Invalid SHA-256 metadata");
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        try (FileInputStream in = new FileInputStream(file)) {
            byte[] buffer = new byte[64 * 1024];
            int count;
            while ((count = in.read(buffer)) != -1) {
                cancellation.check();
                digest.update(buffer, 0, count);
            }
        }
        if (!hex(digest.digest()).equalsIgnoreCase(sha256)) throw new IOException("Update checksum mismatch");
        cancellation.check();
    }

    static String hex(byte[] bytes) {
        StringBuilder text = new StringBuilder(bytes.length * 2);
        for (byte b : bytes) {
            text.append(Character.forDigit((b >> 4) & 15, 16));
            text.append(Character.forDigit(b & 15, 16));
        }
        return text.toString();
    }

    private static long contentLength(HttpURLConnection connection) throws IOException {
        String value = connection.getHeaderField("Content-Length");
        return value == null ? -1 : positive(value);
    }

    private static long positive(String value) throws IOException {
        try {
            if (value == null || !value.matches("[0-9]+")) throw new NumberFormatException();
            long result = Long.parseLong(value);
            if (result < 0) throw new NumberFormatException();
            return result;
        } catch (NumberFormatException error) {
            throw new IOException("Invalid response length");
        }
    }

    private static long number(String value, long fallback) {
        try {
            return value == null ? fallback : Long.parseLong(value);
        } catch (NumberFormatException error) {
            return fallback;
        }
    }

    private static String header(HttpURLConnection connection, String name) {
        String value = connection.getHeaderField(name);
        return value == null ? "" : value.trim();
    }

    private static String strongTag(String tag) {
        if (tag == null || tag.length() < 2 || tag.length() > 1024 || tag.charAt(0) != '"'
                || tag.charAt(tag.length() - 1) != '"') return null;
        for (int i = 1; i < tag.length() - 1; i++) {
            char c = tag.charAt(i);
            if (c < 33 || c == '"' || c == 127) return null;
        }
        return tag;
    }
}

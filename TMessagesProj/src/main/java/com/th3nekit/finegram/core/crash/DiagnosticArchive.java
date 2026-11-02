package com.th3nekit.finegram.core.crash;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.Comparator;
import java.util.function.BooleanSupplier;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

final class DiagnosticArchive {
    private static final long MAX_AGE_MS = 24 * 60 * 60 * 1000L;
    private static final String PREFIX = "finegram-diagnostics-";

    static synchronized File create(File directory, String snapshot, String events, String logcat,
                                    BooleanSupplier cancelled) throws IOException {
        checkCancelled(cancelled);
        if (!directory.isDirectory() && !directory.mkdirs()) throw new IOException("Cannot create diagnostic cache");
        rotate(directory, 2);
        File pending = File.createTempFile(PREFIX, ".part", directory);
        File result = new File(directory, pending.getName().replace(".part", ".zip"));
        boolean finished = false;
        try {
            try (ZipOutputStream zip = new ZipOutputStream(new FileOutputStream(pending))) {
                entry(zip, "README.txt", "Finegram camera diagnostics\n"
                        + "No databases, account files, subscription settings, old logs or message history are included.\n"
                        + "Camera events and process camera logcat are bounded. No recorded audio, video or images are included.\n"
                        + "Logcat is bounded, filtered to camera/media diagnostics and restricted to this app PID.\n"
                        + "Android may hide camera service telemetry. This is not the full system log.\n"
                        + "Sensitive fields are filtered before storage and upload. Temporary camera reports can be disabled in About Finegram.\n", cancelled);
                entry(zip, "device-camera.txt", snapshot, cancelled);
                entry(zip, "camera-events.txt", events, cancelled);
                entry(zip, "process-camera-logcat.txt", logcat, cancelled);
            }
            checkCancelled(cancelled);
            if (!pending.renameTo(result)) throw new IOException("Cannot finish diagnostic archive");
            finished = true;
            return result;
        } finally {
            if (!finished) {
                pending.delete();
                result.delete();
            }
        }
    }

    private static void entry(ZipOutputStream zip, String name, String value, BooleanSupplier cancelled) throws IOException {
        checkCancelled(cancelled);
        StringBuilder safe = new StringBuilder();
        String bounded = value == null ? "Unavailable\n" : value;
        if (bounded.length() > 300000) {
            int firstCompleteLine = bounded.indexOf('\n', bounded.length() - 300000);
            bounded = firstCompleteLine < 0 ? "[oversized section omitted]\n" : bounded.substring(firstCompleteLine + 1);
        }
        for (String line : bounded.split("\\n")) {
            if (safe.length() + line.length() > 260000) break;
            safe.append("process-camera-logcat.txt".equals(name)
                    ? DiagnosticLogcat.cleanLine(line) : DiagnosticText.clean(line)).append('\n');
        }
        zip.putNextEntry(new ZipEntry(name));
        zip.write(safe.toString().getBytes(StandardCharsets.UTF_8));
        zip.closeEntry();
    }

    private static void checkCancelled(BooleanSupplier cancelled) throws InterruptedIOException {
        if (cancelled.getAsBoolean()) throw new InterruptedIOException("Diagnostic export cancelled");
    }

    private static void rotate(File directory, int keep) throws IOException {
        File[] files = directory.listFiles((dir, name) -> name.matches("finegram-diagnostics-[A-Za-z0-9_-]+\\.(?:zip|part)"));
        if (files == null) return;
        Arrays.sort(files, Comparator.comparingLong(File::lastModified).reversed());
        int retained = 0;
        long now = System.currentTimeMillis();
        for (File file : files) {
            if (!file.isFile()) continue;
            if (file.getName().endsWith(".part") || now - file.lastModified() > MAX_AGE_MS || retained++ >= keep) {
                if (!file.delete()) throw new IOException("Cannot rotate diagnostic cache");
            }
        }
    }

    private DiagnosticArchive() {}
}

package com.th3nekit.finegram.core.crash;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.UUID;

final class CameraReportQueue {
    private static final long MAX_AGE = 48 * 60 * 60 * 1000L;
    private static final int MAX_REPORTS = 6;
    private final File directory;

    CameraReportQueue(File directory) {
        this.directory = directory;
    }

    synchronized File add(File source, long now) throws IOException {
        if (!source.isFile() || source.length() == 0 || source.length() > 1024 * 1024) {
            source.delete();
            throw new IOException("Invalid camera report size");
        }
        if (!directory.isDirectory() && !directory.mkdirs()) throw new IOException("Cannot create camera outbox");
        File target = new File(directory, UUID.randomUUID() + ".zip");
        if (!source.renameTo(target)) throw new IOException("Cannot commit camera report");
        if (!target.setLastModified(now)) {
            target.delete();
            throw new IOException("Cannot date camera report");
        }
        pending(now);
        return target;
    }

    synchronized List<File> pending(long now) {
        File[] files = directory.listFiles();
        ArrayList<File> reports = new ArrayList<>();
        if (files == null) return reports;
        Arrays.sort(files, Comparator.comparingLong(File::lastModified).reversed());
        for (File file : files) {
            if (!file.isFile()) continue;
            boolean report = validName(file.getName());
            if (!report || file.length() == 0 || file.length() > 1024 * 1024
                    || now - file.lastModified() > MAX_AGE || file.lastModified() - now > 60 * 60 * 1000L
                    || reports.size() >= MAX_REPORTS) {
                file.delete();
            } else {
                reports.add(file);
            }
        }
        return reports;
    }

    synchronized void remove(File file) throws IOException {
        if (validName(file.getName())
                && directory.getCanonicalFile().equals(file.getCanonicalFile().getParentFile())) file.delete();
    }

    private static boolean validName(String name) {
        return name.matches("[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89ab][a-f0-9]{3}-[a-f0-9]{12}\\.zip");
    }

    synchronized void clear() {
        File[] files = directory.listFiles();
        if (files == null) return;
        for (File file : files) if (file.isFile()) file.delete();
    }
}

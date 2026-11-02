/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.updater;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Properties;

final class FGUpdateFiles {
    private FGUpdateFiles() {
    }

    static synchronized Properties read(File file) throws IOException {
        File backup = new File(file.getPath() + ".bak");
        File source = file.isFile() ? file : backup;
        Properties result = new Properties();
        if (source.isFile()) {
            if (source.length() > 64 * 1024) throw new IOException("Invalid transfer metadata");
            try (FileInputStream in = new FileInputStream(source)) {
                result.load(in);
            }
        }
        return result;
    }

    static synchronized void write(File file, Properties properties) throws IOException {
        File directory = file.getParentFile();
        if (!directory.isDirectory() && !directory.mkdirs()) throw new IOException("Cannot create update directory");
        File temp = new File(file.getPath() + ".tmp");
        File backup = new File(file.getPath() + ".bak");
        try (FileOutputStream out = new FileOutputStream(temp)) {
            properties.store(out, null);
            out.getFD().sync();
        }
        if (file.exists()) {
            if (backup.exists() && !backup.delete()) throw new IOException("Cannot replace transfer metadata");
            if (!file.renameTo(backup)) throw new IOException("Cannot preserve transfer metadata");
        }
        if (!temp.renameTo(file)) throw new IOException("Cannot save transfer metadata");
        backup.delete();
    }

    static void clear(File file) {
        file.delete();
        new File(file.getPath() + ".bak").delete();
        new File(file.getPath() + ".tmp").delete();
    }
}

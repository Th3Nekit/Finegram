package com.th3nekit.finegram.core.ui.folders;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.net.Uri;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.ImageLoader;
import org.telegram.messenger.UserConfig;
import org.telegram.messenger.Utilities;

import java.io.File;
import java.io.FileOutputStream;
import java.util.UUID;
import java.util.function.Consumer;

public final class FolderCoverStore {
    public static final int AUTO = 0;
    public static final int GEOMETRY = 1;
    public static final int LANDSCAPE = 2;
    public static final int IMAGE = 3;

    public static final class Cover {
        public final int style;
        public final int seed;
        public final String image;

        Cover(int style, int seed, String image) {
            this.style = style;
            this.seed = seed;
            this.image = image;
        }

        public boolean sameAs(Cover other) {
            return other != null && style == other.style && seed == other.seed && image.equals(other.image);
        }
    }

    private static SharedPreferences preferences() {
        return ApplicationLoader.applicationContext.getSharedPreferences("finegram_folder_covers", Context.MODE_PRIVATE);
    }

    private static String key(int account, int folder) {
        return UserConfig.getInstance(account).getClientUserId() + "_" + folder;
    }

    private static File directory() {
        return new File(ApplicationLoader.applicationContext.getFilesDir(), "folder_covers");
    }

    public static Cover get(int account, int folder) {
        String key = key(account, folder);
        int style = preferences().getInt(key + "_style", AUTO);
        String image = preferences().getString(key + "_image", "");
        if (!image.matches("[a-f0-9-]+\\.jpg") || !new File(directory(), image).isFile()) {
            image = "";
        }
        if (style == IMAGE && image.isEmpty()) style = AUTO;
        return new Cover(style, folder, image.isEmpty() ? "" : new File(directory(), image).getAbsolutePath());
    }

    public static void setStyle(int account, int folder, int style) {
        preferences().edit().putInt(key(account, folder) + "_style", Math.max(AUTO, Math.min(IMAGE, style))).apply();
    }

    public static void reset(int account, int folder) {
        String key = key(account, folder);
        String image = preferences().getString(key + "_image", "");
        preferences().edit().remove(key + "_style").remove(key + "_image").apply();
        if (image.matches("[a-f0-9-]+\\.jpg")) new File(directory(), image).delete();
    }

    public static void importImage(int account, int folder, Uri uri, Consumer<Boolean> callback) {
        final String key = key(account, folder);
        Utilities.globalQueue.postRunnable(() -> {
            File output = null;
            Bitmap bitmap = null;
            boolean success = false;
            try {
                bitmap = ImageLoader.loadBitmap(null, uri, 960, 480, true);
                if (bitmap == null) throw new IllegalArgumentException();
                File dir = directory();
                if (!dir.isDirectory() && !dir.mkdirs()) throw new IllegalStateException();
                String name = UUID.randomUUID() + ".jpg";
                output = new File(dir, name);
                try (FileOutputStream stream = new FileOutputStream(output)) {
                    if (!bitmap.compress(Bitmap.CompressFormat.JPEG, 88, stream)) throw new IllegalStateException();
                }
                String previous = preferences().getString(key + "_image", "");
                success = preferences().edit().putString(key + "_image", name).putInt(key + "_style", IMAGE).commit();
                if (success && previous.matches("[a-f0-9-]+\\.jpg")) new File(dir, previous).delete();
            } catch (Exception | OutOfMemoryError ignored) {
            } finally {
                if (bitmap != null) bitmap.recycle();
                if (!success && output != null) output.delete();
            }
            boolean result = success;
            AndroidUtilities.runOnUIThread(() -> callback.accept(result));
        });
    }

    private FolderCoverStore() {}
}

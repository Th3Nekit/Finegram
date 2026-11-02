package org.telegram.localization;

import android.content.Context;
import android.util.SparseArray;
import android.util.SparseIntArray;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.telegram.tgnet.SerializedData;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;
import java.util.TreeMap;

public class Localization {
    private static SparseIntArray rawResBindings;

    public static final Localization EMPTY = new Localization();

    private final SparseArray<String> localizations;

    private Localization() {
        localizations = new SparseArray<>();
    }

    private Localization(Builder builder) {
        localizations = builder.localizations != null ?
            builder.localizations.clone() : new SparseArray<>();
    }

    public String getByResId(Context context, @StringRes int resId) {
        if (context == null || resId == 0) {
            return null;
        }

        if (rawResBindings == null) {
            rawResBindings = loadStringResBindings(context);
        }
        final int hash = rawResBindings.get(resId);
        if (hash == 0) {
            return null;
        }
        return get(hash);
    }

    public String getByResName(String resName) {
        if (resName != null) {
            return get(resName.hashCode());
        }
        return null;
    }

    public String getByResNameOrResId(Context context, String resName, @StringRes int resId) {
        String value = null;
        if (resName != null) {
            value = getByResName(resName);
        }
        if (value == null && resId != 0) {
            value = getByResId(context, resId);
        }
        return value;
    }

    public String get(int resNameHash) {
        return localizations.get(resNameHash);
    }

    public static class Builder {
        private SparseArray<String> localizations;

        public Builder addResLocalization(Context context, String localizationAssetPath) {
            try {
                localizations = load(context, localizationAssetPath, localizations);
            } catch (Exception ignored) {}
            return this;
        }

        public Builder addLocalization(HashMap<String, String> localization) {
            TreeMap<Integer, String> sorted = new TreeMap<>();
            for (Map.Entry<String, String> entry : localization.entrySet()) {
                sorted.put(entry.getKey().hashCode(), entry.getValue());
            }
            SparseArray<String> incoming = new SparseArray<>(sorted.size());
            for (Map.Entry<Integer, String> entry : sorted.entrySet()) {
                incoming.append(entry.getKey(), entry.getValue());
            }
            localizations = merge(localizations, incoming);
            return this;
        }

        public Builder addLocalization(Localization localization) {
            localizations = merge(localizations, localization.localizations);
            return this;
        }

        public Localization build() {
            return new Localization(this);
        }
    }

    private static SparseArray<String> merge(@Nullable SparseArray<String> previous, SparseArray<String> incoming) {
        if (previous == null) {
            return incoming.clone();
        }
        SparseArray<String> result = new SparseArray<>(previous.size() + incoming.size());
        int a = 0, b = 0;
        while (a < previous.size() || b < incoming.size()) {
            if (b == incoming.size() || a < previous.size() && previous.keyAt(a) < incoming.keyAt(b)) {
                result.append(previous.keyAt(a), previous.valueAt(a));
                a++;
            } else {
                if (a < previous.size() && previous.keyAt(a) == incoming.keyAt(b)) {
                    a++;
                }
                result.append(incoming.keyAt(b), incoming.valueAt(b));
                b++;
            }
        }
        return result;
    }

    private static SparseArray<String> load(Context context, String localizationAssetPath, @Nullable SparseArray<String> result) throws IOException {
        try (InputStream stream = new BufferedInputStream(
                context.getAssets().open(localizationAssetPath))) {
            final SerializedData data = new SerializedData(stream);
            final int count = data.readInt32(true);
            SparseArray<String> incoming = new SparseArray<>(count);
            for (int a = 0; a < count; a++) {
                final int hash = data.readInt32(true);
                final String value = data.readString(true);
                incoming.append(hash, value);
            }
            return merge(result, incoming);
        }
    }

    private static SparseIntArray loadStringResBindings(Context context) {
        try (InputStream stream = new BufferedInputStream(
                context.getResources().getAssets().open("string_resource_ids.bin"))) {
            final SerializedData data = new SerializedData(stream);
            final int count = data.readInt32(true);

            final SparseIntArray result = new SparseIntArray(count);
            for (int a = 0; a < count; a++) {
                final int resId = data.readInt32(true);
                final int resHash = data.readInt32(true);
                result.append(resId, resHash);
            }
            return result;
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
}

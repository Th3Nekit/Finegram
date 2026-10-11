/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.plugins;

import android.graphics.drawable.Drawable;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.DocumentObject;
import org.telegram.messenger.ImageLocation;
import org.telegram.messenger.ImageReceiver;
import org.telegram.messenger.MediaDataController;
import org.telegram.messenger.UserConfig;
import org.telegram.tgnet.ConnectionsManager;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.BackupImageView;

import java.io.File;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;

public final class FGPluginIcon {

    private FGPluginIcon() {
    }

    private static final int[] COLORS = {
            0xFFE17076, 0xFF7BC862, 0xFFE5CA77, 0xFF65AADD,
            0xFFA695E7, 0xFFEE7AAE, 0xFF6EC9CB
    };

    private static final int PARALLEL_REQUESTS = 3;

    private static final HashMap<String, TLRPC.TL_messages_stickerSet> READY = new HashMap<>();

    private static final HashMap<String, ArrayList<Runnable>> WAITING = new HashMap<>();

    private static final Set<String> RUNNING = new HashSet<>();

    private static final ArrayDeque<String> QUEUE = new ArrayDeque<>();

    private static final HashMap<String, Integer> TRIES = new HashMap<>();

    private static final int MAX_TRIES = 3;

    private static final long[] RETRY_DELAYS_MS = {3000, 15000, 45000};

    private static final Set<String> MISSING = new HashSet<>();

    private static long pausedUntil;
    private static boolean resumeScheduled;

    public static int colorFor(String id) {
        return FGPluginAvatar.background(id);
    }

    public static int accentFor(String id) {
        return FGPluginAvatar.foreground(id);
    }

    public static String letterFor(String name) {
        return name == null || name.isEmpty() ? "?" : name.substring(0, 1).toUpperCase();
    }

    public static String letterFor(String name, String icon) {
        return isEmoji(icon) ? icon.trim() : letterFor(name);
    }

    public static boolean isEmoji(String icon) {
        if (icon == null) {
            return false;
        }
        final String trimmed = icon.trim();
        if (trimmed.isEmpty() || trimmed.length() > 8 || trimmed.contains("/")) {
            return false;
        }
        for (int i = 0; i < trimmed.length(); i++) {
            final char c = trimmed.charAt(i);
            if (c < 128) {
                return false;
            }
        }
        return true;
    }

    public static void load(BackupImageView view, String icon, int sizeDp, Runnable onShown) {
        if (view == null || icon == null || icon.trim().isEmpty()) {
            return;
        }
        final String trimmed = icon.trim();
        view.setTag(trimmed);

        if (loadFromFile(view, trimmed, onShown) || loadFromResources(view, trimmed, onShown)) {
            return;
        }

        final int slash = trimmed.lastIndexOf('/');
        if (slash <= 0) {
            return;
        }
        final String setName = trimmed.substring(0, slash).trim();
        final int index;
        try {
            index = Integer.parseInt(trimmed.substring(slash + 1).trim());
        } catch (NumberFormatException e) {
            return;
        }
        if (setName.isEmpty() || index < 0 || MISSING.contains(setName.toLowerCase())) {
            return;
        }

        final TLRPC.TL_messages_stickerSet known = READY.get(setName.toLowerCase()) != null
                ? READY.get(setName.toLowerCase())
                : MediaDataController.getInstance(UserConfig.selectedAccount).getStickerSetByName(setName);
        if (known != null && known.documents != null && known.documents.size() > index) {
            READY.put(setName.toLowerCase(), known);
            show(view, known, index, sizeDp, onShown, trimmed);
            return;
        }
        request(setName, () -> {
            final TLRPC.TL_messages_stickerSet set = READY.get(setName.toLowerCase());
            if (set != null) {
                show(view, set, index, sizeDp, onShown, trimmed);
                return;
            }
            final String key = setName.toLowerCase();
            if (MISSING.contains(key)) {
                return;
            }

            final int tries = TRIES.getOrDefault(key, 0) + 1;
            TRIES.put(key, tries);
            if (tries < MAX_TRIES) {
                AndroidUtilities.runOnUIThread(() -> load(view, trimmed, sizeDp, onShown),
                        RETRY_DELAYS_MS[Math.min(tries - 1, RETRY_DELAYS_MS.length - 1)]);
            } else {
                FGPluginsController.note("значок " + trimmed + ": набор не отдался, попыток " + tries);
            }
        });
    }

    private static boolean loadFromFile(BackupImageView view, String icon, Runnable onShown) {
        if (icon.indexOf('.') < 0) {
            return false;
        }
        final File file = new File(icon);
        if (!file.isFile()) {
            return false;
        }
        view.setImage(ImageLocation.getForPath(file.getAbsolutePath()), null, (Drawable) null, null);
        view.setVisibility(BackupImageView.VISIBLE);
        if (onShown != null) {
            onShown.run();
        }
        return true;
    }

    private static boolean loadFromResources(BackupImageView view, String icon, Runnable onShown) {
        if (icon.contains("/") || icon.contains(".") || isEmoji(icon)) {
            return false;
        }
        final int id;
        try {
            id = ApplicationLoader.applicationContext.getResources()
                    .getIdentifier(icon, "drawable", ApplicationLoader.applicationContext.getPackageName());
        } catch (Throwable e) {
            return false;
        }
        if (id == 0) {
            return false;
        }
        final Drawable drawable = ApplicationLoader.applicationContext.getResources().getDrawable(id, null);
        if (drawable == null) {
            return false;
        }
        drawable.setColorFilter(new android.graphics.PorterDuffColorFilter(
                Theme.getColor(Theme.key_windowBackgroundWhiteBlackText),
                android.graphics.PorterDuff.Mode.SRC_IN));
        view.setImageDrawable(drawable);
        view.setVisibility(BackupImageView.VISIBLE);
        if (onShown != null) {
            onShown.run();
        }
        return true;
    }

    private static void request(String setName, Runnable onReady) {
        final String key = setName.toLowerCase();
        ArrayList<Runnable> waiting = WAITING.get(key);
        if (waiting == null) {
            waiting = new ArrayList<>();
            WAITING.put(key, waiting);
        }
        waiting.add(onReady);

        if (RUNNING.contains(key) || QUEUE.contains(key)) {
            return;
        }
        if (RUNNING.size() >= PARALLEL_REQUESTS || pausedUntil > android.os.SystemClock.elapsedRealtime()) {
            QUEUE.add(key);
            next();
            return;
        }
        fetch(key, setName);
    }

    private static void fetch(String key, String setName) {
        RUNNING.add(key);
        final int account = UserConfig.selectedAccount;
        final MediaDataController data = MediaDataController.getInstance(account);
        data.loadCachedStickerSet(setName, cached -> {
            if (cached != null && cached.documents != null && !cached.documents.isEmpty()) {
                data.storeTempStickerSet(cached);
                finish(key, cached);
                return;
            }
            final TLRPC.TL_inputStickerSetShortName input = new TLRPC.TL_inputStickerSetShortName();
            input.short_name = setName;
            final TLRPC.TL_messages_getStickerSet request = new TLRPC.TL_messages_getStickerSet();
            request.stickerset = input;
            ConnectionsManager.getInstance(account).sendRequest(request, (response, error) -> AndroidUtilities.runOnUIThread(() -> {
                if (response instanceof TLRPC.TL_messages_stickerSet) {
                    final TLRPC.TL_messages_stickerSet set = (TLRPC.TL_messages_stickerSet) response;
                    data.putStickerSet(set, false);
                    finish(key, set);
                    return;
                }
                final String text = error == null || error.text == null ? "" : error.text;
                if (text.startsWith("FLOOD_WAIT_")) {
                    long seconds = 30;
                    try {
                        seconds = Long.parseLong(text.substring("FLOOD_WAIT_".length()));
                    } catch (NumberFormatException ignore) {
                    }
                    pausedUntil = Math.max(pausedUntil, android.os.SystemClock.elapsedRealtime() + (seconds + 1) * 1000L);
                    RUNNING.remove(key);
                    QUEUE.addFirst(key);
                    FGPluginsController.note("значки: сервер просит подождать " + seconds + " с");
                    next();
                    return;
                }
                if (text.contains("STICKERSET_INVALID") || text.contains("SHORTNAME_INVALID")) {
                    MISSING.add(key);
                    FGPluginsController.note("значок: набора " + setName + " на сервере нет");
                }
                finish(key, null);
            }));
        });
    }

    private static void finish(String key, TLRPC.TL_messages_stickerSet set) {
        if (set != null) {
            READY.put(key, set);
        }
        RUNNING.remove(key);
        final ArrayList<Runnable> waiting = WAITING.remove(key);
        if (waiting != null) {
            for (Runnable action : waiting) {
                action.run();
            }
        }
        next();
    }

    private static void next() {
        final long wait = pausedUntil - android.os.SystemClock.elapsedRealtime();
        if (wait > 0) {
            if (!resumeScheduled && !QUEUE.isEmpty()) {
                resumeScheduled = true;
                AndroidUtilities.runOnUIThread(() -> {
                    resumeScheduled = false;
                    next();
                }, wait);
            }
            return;
        }
        while (RUNNING.size() < PARALLEL_REQUESTS && !QUEUE.isEmpty()) {
            final String key = QUEUE.poll();
            if (key != null && !RUNNING.contains(key)) {
                fetch(key, key);
            }
        }
    }

    private static void show(BackupImageView view, TLRPC.TL_messages_stickerSet set, int index,
                             int sizeDp, Runnable onShown, String tag) {
        if (!tag.equals(view.getTag())) {
            return;
        }
        if (set == null || set.documents == null || set.documents.size() <= index) {
            final int size = set == null || set.documents == null ? 0 : set.documents.size();
            FGPluginsController.note("значок " + tag + ": в наборе " + size + " картинок");
            return;
        }
        final TLRPC.Document document = set.documents.get(index);
        if (document == null) {
            return;
        }
        final Drawable thumb = DocumentObject.getSvgThumb(document,
                Theme.key_windowBackgroundWhiteGrayIcon, 0.2f, 1f, null);
        final String filter = sizeDp + "_" + sizeDp;
        view.setImage(ImageLocation.getForDocument(document), filter, thumb, 0, document);

        final ImageReceiver receiver = view.getImageReceiver();
        receiver.setAllowStartAnimation(true);
        receiver.setAllowStartLottieAnimation(true);
        receiver.setAllowDecodeSingleFrame(true);
        receiver.setAutoRepeat(1);
        receiver.setAutoRepeatCount(0);
        receiver.startAnimation();
        view.setVisibility(BackupImageView.VISIBLE);
        if (onShown != null) {
            onShown.run();
        }
    }
}

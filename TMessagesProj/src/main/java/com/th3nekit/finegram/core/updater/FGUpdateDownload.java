/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.updater;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.Theme;

import java.io.File;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.Comparator;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;
import java.util.concurrent.CopyOnWriteArraySet;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;

public final class FGUpdateDownload {
    public enum Phase { IDLE, DOWNLOADING, PAUSED, VERIFYING, READY, FAILED }

    public static final class State {
        public final FGClientUpdates.Update update;
        public final Phase phase;
        public final long downloaded;
        public final long total;
        public final File file;
        public final String error;
        final long generation;

        private State(FGClientUpdates.Update update, Phase phase, long downloaded, long total, File file, String error) {
            this(update, phase, downloaded, total, file, error, -1);
        }

        private State(FGClientUpdates.Update update, Phase phase, long downloaded, long total, File file, String error, long generation) {
            this.update = update;
            this.phase = phase;
            this.downloaded = downloaded;
            this.total = total;
            this.file = file;
            this.error = error;
            this.generation = generation;
        }
    }

    public interface Listener {
        void onStateChanged(State state);
    }

    private static final ExecutorService worker = Executors.newSingleThreadExecutor(r -> new Thread(r, "FGUpdateWorker"));
    private static final ExecutorService interrupter = Executors.newSingleThreadExecutor(r -> new Thread(r, "FGUpdateInterrupt"));
    private static final CopyOnWriteArraySet<Listener> listeners = new CopyOnWriteArraySet<>();
    private static final AtomicBoolean initialized = new AtomicBoolean();
    private static final AtomicLong request = new AtomicLong();
    private static volatile State state = new State(null, Phase.IDLE, 0, 0, null, null);
    private static volatile FGClientUpdates.Update requested;
    private static volatile FGUpdateTransfer.Cancellation cancellation;
    private static volatile Verified verified;

    private FGUpdateDownload() {
    }

    public static State getState() {
        initialize();
        return state;
    }

    public static synchronized void initialize() {
        if (ApplicationLoader.applicationContext == null || initialized.get()) return;
        FGClientUpdates.getPending();
        FGClientUpdates.Update saved = FGClientUpdates.deserialize(FGClientUpdates.preferences().getString("active", null));
        if (FGClientUpdates.isAvailable(saved)) {
            File target = candidateFor(saved);
            boolean candidate = target != null;
            requested = saved;
            state = new State(saved, candidate ? Phase.VERIFYING : Phase.PAUSED,
                    candidate ? target.length() : partFor(saved).length(),
                    candidate ? target.length() : totalFor(saved), null, null, 0);
        }
        initialized.set(true);
        worker.execute(FGUpdateDownload::restore);
    }

    public static void addListener(Listener listener) {
        if (listener == null) return;
        initialize();
        listeners.add(listener);
        AndroidUtilities.runOnUIThread(() -> {
            if (listeners.contains(listener)) listener.onStateChanged(state);
        });
    }

    public static void removeListener(Listener listener) {
        listeners.remove(listener);
    }

    public static boolean isSame(FGClientUpdates.Update left, FGClientUpdates.Update right) {
        return left != null && right != null && FGClientUpdates.identity(left).equals(FGClientUpdates.identity(right));
    }

    public static void start(FGClientUpdates.Update update) {
        if (update == null) return;
        initialize();
        FGClientUpdates.Update snapshot = FGClientUpdates.deserialize(FGClientUpdates.serialize(update));
        if (snapshot == null) return;
        if (isSame(state.update, snapshot) && (state.phase == Phase.DOWNLOADING || state.phase == Phase.VERIFYING)) return;
        long id = requestResume(snapshot);
        AndroidUtilities.runOnUIThread(() -> {
            if (!isCurrent(id)) return;
            Intent intent = new Intent(ApplicationLoader.applicationContext, FGUpdateService.class)
                    .setAction(FGUpdateService.ACTION_START)
                    .putExtra(FGUpdateService.EXTRA_UPDATE, FGClientUpdates.serialize(snapshot))
                    .putExtra(FGUpdateService.EXTRA_REQUEST, id);
            try {
                if (Build.VERSION.SDK_INT >= 26) ApplicationLoader.applicationContext.startForegroundService(intent);
                else ApplicationLoader.applicationContext.startService(intent);
            } catch (RuntimeException error) {
                foregroundDenied(snapshot, id);
            }
        });
    }

    public static void pause() {
        initialize();
        long id = request.incrementAndGet();
        interrupt();
        worker.execute(() -> {
            if (!isCurrent(id)) return;
            FGClientUpdates.Update update = requested != null ? requested : state.update;
            if (update == null || state.phase == Phase.READY && isSame(update, state.update)) return;
            publishSaved(update, Phase.PAUSED, totalFor(update), null, id);
        });
    }

    private static void interrupt() {
        FGUpdateTransfer.Cancellation active = cancellation;
        if (active != null) {
            active.cancelled.set(true);
            interrupter.execute(active::cancel);
        }
    }

    static boolean isCurrent(long id) {
        return id == request.get();
    }

    static synchronized long requestResume(FGClientUpdates.Update update) {
        initialize();
        long id = request.incrementAndGet();
        requested = update;
        interrupt();
        return id;
    }

    static void begin(FGClientUpdates.Update update, long id) {
        initialize();
        worker.execute(() -> run(update, id));
    }

    static void foregroundDenied(FGClientUpdates.Update update, long id) {
        worker.execute(() -> {
            if (isCurrent(id)) publishSaved(update, Phase.FAILED, totalFor(update), text(R.string.FGUpdateErrorForeground), id);
        });
        AndroidUtilities.runOnUIThread(() -> {
            if (isCurrent(id)) ApplicationLoader.applicationContext.stopService(
                    new Intent(ApplicationLoader.applicationContext, FGUpdateService.class));
        });
    }

    static void serviceStopped(long id) {
        if (isCurrent(id)) pause();
    }

    private static void run(FGClientUpdates.Update update, long id) {
        if (!isCurrent(id)) return;
        FGUpdateTransfer.Cancellation token = new FGUpdateTransfer.Cancellation();
        cancellation = token;
        if (!isCurrent(id)) {
            cancellation = null;
            return;
        }
        verified = null;
        File target = FGClientUpdates.fileFor(update);
        File part = partFor(update);
        long total = totalFor(update);
        try {
            if (!FGClientUpdates.isAvailable(update)) throw new Rejected(R.string.FGUpdateErrorVersion);
            FGClientUpdates.Build build = update.preferred();
            if (build == null || build.size < 0 || build.size > FGUpdateTransfer.MAX_BYTES
                    || build.sha256 != null && !build.sha256.matches("[a-fA-F0-9]{64}")) {
                throw new Rejected(R.string.FGUpdateErrorMetadata);
            }
            synchronized (FGClientUpdates.class) {
                FGClientUpdates.Update pending = FGClientUpdates.getPending();
                if (pending == null || FGUpdatePolicy.isNewRelease(pending.version, pending.versionCode,
                        update.version, update.versionCode)) {
                    FGClientUpdates.setPending(update);
                }
            }
            save(update, Phase.VERIFYING, total);
            if (target.isFile()) {
                emit(new State(update, Phase.VERIFYING, target.length(), target.length(), null, null), id);
                validate(target, update, token);
            } else {
                save(update, Phase.DOWNLOADING, total);
                emit(new State(update, Phase.DOWNLOADING, part.length(), total, null, null), id);
                final long[] last = {0};
                total = new FGUpdateTransfer(part, build.url, build.size).download(token, (done, size) -> {
                    long now = System.nanoTime();
                    if (isCurrent(id) && (now - last[0] >= 250_000_000L || done == size)) {
                        last[0] = now;
                        emit(new State(update, Phase.DOWNLOADING, done, Math.max(0, size), null, null), id);
                    }
                });
                token.check();
                if (!isCurrent(id)) return;
                save(update, Phase.VERIFYING, total);
                emit(new State(update, Phase.VERIFYING, part.length(), total, null, null), id);
                validate(part, update, token);
                token.check();
                if (!isCurrent(id)) return;
                if (!part.renameTo(target)) throw new StorageFailure();
                FGUpdateFiles.clear(new File(part.getPath() + ".http"));
            }
            token.check();
            if (!isCurrent(id)) return;
            verified = new Verified(update, target);
            save(update, Phase.READY, target.length());
            emit(new State(update, Phase.READY, target.length(), target.length(), target, null), id);
            cleanup(update);
        } catch (Exception error) {
            if (error instanceof Rejected) deletePayload(update);
            if (isCurrent(id)) {
                if (token.isCancelled()) publishSaved(update, Phase.PAUSED, totalFor(update), null, id);
                else publishSaved(update, Phase.FAILED, totalFor(update), diagnostic(error), id);
            }
        } finally {
            if (cancellation == token) cancellation = null;
        }
    }

    private static void restore() {
        FGClientUpdates.getPending();
        FGClientUpdates.Update update = FGClientUpdates.deserialize(FGClientUpdates.preferences().getString("active", null));
        if (update == null) {
            cleanup(null);
            return;
        }
        if (!FGClientUpdates.isAvailable(update)) {
            deletePayload(update);
            FGClientUpdates.preferences().edit().remove("active").remove("phase").remove("total").commit();
            cleanup(null);
            return;
        }
        if (request.get() != 0) {
            cleanup(update);
            return;
        }
        File target = FGClientUpdates.fileFor(update);
        File candidate = candidateFor(update);
        long id = 0;
        if (candidate != null) {
            FGUpdateTransfer.Cancellation token = new FGUpdateTransfer.Cancellation();
            cancellation = token;
            if (!isCurrent(id)) {
                cancellation = null;
                return;
            }
            try {
                emit(new State(update, Phase.VERIFYING, candidate.length(), candidate.length(), null, null), id);
                validate(candidate, update, token);
                if (isCurrent(id)) {
                    if (!candidate.equals(target)) {
                        if (!candidate.renameTo(target)) throw new StorageFailure();
                        FGUpdateFiles.clear(new File(candidate.getPath() + ".http"));
                    }
                    verified = new Verified(update, target);
                    save(update, Phase.READY, target.length());
                    emit(new State(update, Phase.READY, target.length(), target.length(), target, null), id);
                }
            } catch (Exception error) {
                if (error instanceof Rejected) deletePayload(update);
                if (isCurrent(id)) publishSaved(update, token.isCancelled() ? Phase.PAUSED : Phase.FAILED,
                        totalFor(update), token.isCancelled() ? null : diagnostic(error), id);
            } finally {
                if (cancellation == token) cancellation = null;
            }
        } else if (isCurrent(id)) {
            publishSaved(update, Phase.PAUSED, totalFor(update), null, id);
        }
        cleanup(update);
    }

    private static void publishSaved(FGClientUpdates.Update update, Phase phase, long total, String error, long id) {
        try {
            save(update, phase, total);
        } catch (IOException failure) {
            phase = Phase.FAILED;
            error = text(R.string.FGUpdateErrorStorage);
        }
        emit(new State(update, phase, partFor(update).length(), Math.max(0, total), null, error), id);
    }

    private static void save(FGClientUpdates.Update update, Phase phase, long total) throws IOException {
        if (!FGClientUpdates.preferences().edit().putString("active", FGClientUpdates.serialize(update))
                .putString("phase", phase.name()).putLong("total", Math.max(0, total)).commit()) {
            throw new StorageFailure();
        }
    }

    private static void emit(State next, long id) {
        State snapshot = new State(next.update, next.phase, next.downloaded, next.total, next.file, next.error, id);
        state = snapshot;
        AndroidUtilities.runOnUIThread(() -> {
            for (Listener listener : listeners) {
                try {
                    listener.onStateChanged(snapshot);
                } catch (RuntimeException ignored) {
                }
            }
        });
    }

    private static File partFor(FGClientUpdates.Update update) {
        return new File(FGClientUpdates.fileFor(update).getPath() + ".part");
    }

    private static File candidateFor(FGClientUpdates.Update update) {
        File target = FGClientUpdates.fileFor(update);
        if (target.isFile()) return target;
        File part = partFor(update);
        long total = totalFor(update);
        return total > 0 && part.isFile() && part.length() == total ? part : null;
    }

    private static long totalFor(FGClientUpdates.Update update) {
        FGClientUpdates.Build build = update.preferred();
        if (build != null && build.size > 0) return build.size;
        try {
            Properties metadata = FGUpdateFiles.read(new File(partFor(update).getPath() + ".http"));
            long total = Long.parseLong(metadata.getProperty("total", "0"));
            return total > 0 && total <= FGUpdateTransfer.MAX_BYTES ? total : 0;
        } catch (IOException | IllegalArgumentException error) {
            return 0;
        }
    }

    static File verifiedFile(FGClientUpdates.Update update) {
        Verified cached = verified;
        return cached != null && cached.matches(update) ? cached.file : null;
    }

    private static void validate(File file, FGClientUpdates.Update update, FGUpdateTransfer.Cancellation token) throws Exception {
        long length = file.length();
        long modified = file.lastModified();
        FGClientUpdates.Build build = update == null ? null : update.preferred();
        if (!file.isFile() || length <= 0 || length > FGUpdateTransfer.MAX_BYTES
                || build != null && build.size > 0 && build.size != length) throw new Rejected(R.string.FGUpdateErrorSize);
        try {
            FGUpdateTransfer.verifyHash(file, build == null ? null : build.sha256, token);
        } catch (IOException error) {
            if (token.isCancelled()) throw error;
            throw new Rejected(R.string.FGUpdateErrorHash);
        }
        token.check();
        PackageManager manager = ApplicationLoader.applicationContext.getPackageManager();
        String name = ApplicationLoader.applicationContext.getPackageName();
        int flags = Build.VERSION.SDK_INT >= 28 ? PackageManager.GET_SIGNING_CERTIFICATES : PackageManager.GET_SIGNATURES;
        PackageInfo installed;
        try {
            installed = manager.getPackageInfo(name, flags);
        } catch (Exception error) {
            throw new Rejected(R.string.FGUpdateErrorSignatureUnreadable);
        }
        if (installed == null || !name.equals(installed.packageName)) {
            throw new Rejected(R.string.FGUpdateErrorSignatureUnreadable);
        }
        PackageInfo archive;
        try {
            archive = manager.getPackageArchiveInfo(file.getAbsolutePath(), flags);
        } catch (RuntimeException error) {
            throw new Rejected(R.string.FGUpdateErrorPackage);
        }
        if (archive == null || !name.equals(archive.packageName)) throw new Rejected(R.string.FGUpdateErrorPackage);
        if (version(archive) <= version(installed)) throw new Rejected(R.string.FGUpdateErrorVersion);
        FGUpdateSignatures.Result signatures = FGUpdateSignatures.compare(manager, name, file.getAbsolutePath(), installed, archive);
        if (signatures == FGUpdateSignatures.Result.UNREADABLE) throw new Rejected(R.string.FGUpdateErrorSignatureUnreadable);
        if (signatures != FGUpdateSignatures.Result.MATCH) throw new Rejected(R.string.FGUpdateErrorSignature);
        token.check();
        if (file.length() != length || file.lastModified() != modified) throw new Rejected(R.string.FGUpdateErrorHash);
    }

    private static long version(PackageInfo info) {
        return Build.VERSION.SDK_INT >= 28 ? info.getLongVersionCode() : info.versionCode;
    }

    static void install(File file, Activity activity, Theme.ResourcesProvider resourcesProvider) {
        if (file == null || activity == null) return;
        initialize();
        WeakReference<Activity> reference = new WeakReference<>(activity);
        long id = request.get();
        worker.execute(() -> {
            if (!isCurrent(id)) return;
            FGClientUpdates.Update update = state.update;
            if (update == null || !FGClientUpdates.fileFor(update).equals(file)) {
                update = FGClientUpdates.getPending();
                if (update == null || !FGClientUpdates.fileFor(update).equals(file)) update = null;
            }
            try {
                validate(file, update, new FGUpdateTransfer.Cancellation());
                long length = file.length();
                long modified = file.lastModified();
                AndroidUtilities.runOnUIThread(() -> {
                    Activity owner = reference.get();
                    if (!isCurrent(id) || owner == null || owner.isFinishing() || owner.isDestroyed()
                            || file.length() != length || file.lastModified() != modified) return;
                    AndroidUtilities.openForView(file, file.getName(), "application/vnd.android.package-archive",
                            owner, resourcesProvider, false);
                });
            } catch (Exception error) {
                if (update != null) {
                    verified = null;
                    if (error instanceof Rejected) deletePayload(update);
                    if (isCurrent(id)) publishSaved(update, Phase.FAILED, totalFor(update), diagnostic(error), id);
                }
            }
        });
    }

    private static void deletePayload(FGClientUpdates.Update update) {
        verified = null;
        FGClientUpdates.fileFor(update).delete();
        File part = partFor(update);
        part.delete();
        FGUpdateFiles.clear(new File(part.getPath() + ".http"));
    }

    private static void cleanup(FGClientUpdates.Update active) {
        File[] files = FGClientUpdates.directory().listFiles(file -> file.isFile()
                && file.getName().matches("fg-update-[a-f0-9]{64}\\.apk(?:\\.part)?(?:\\.http)?(?:\\.bak|\\.tmp)?"));
        if (files == null) return;
        Map<String, ArrayList<File>> groups = new HashMap<>();
        for (File file : files) {
            String key = file.getName().substring(0, "fg-update-".length() + 64 + 4);
            groups.computeIfAbsent(key, ignored -> new ArrayList<>()).add(file);
        }
        ArrayList<ArrayList<File>> ordered = new ArrayList<>(groups.values());
        ordered.sort(Comparator.comparingLong((ArrayList<File> group) -> newest(group)).reversed());
        String keep = active == null ? null : FGClientUpdates.fileFor(active).getName();
        long now = System.currentTimeMillis();
        long retained = 0;
        int candidates = 0;
        for (ArrayList<File> group : ordered) {
            synchronized (FGUpdateDownload.class) {
                if (protectedGroup(group, keep)) continue;
            }
            candidates++;
            for (File file : group) retained += file.length();
            boolean installed = false;
            for (File file : group) {
                if (!file.getName().endsWith(".apk")) continue;
                try {
                    PackageManager manager = ApplicationLoader.applicationContext.getPackageManager();
                    String name = ApplicationLoader.applicationContext.getPackageName();
                    PackageInfo archive = manager.getPackageArchiveInfo(file.getPath(), 0);
                    installed = archive != null && name.equals(archive.packageName)
                            && version(archive) <= version(manager.getPackageInfo(name, 0));
                } catch (Exception ignored) {
                }
            }
            if (installed || now - newest(group) > 7L * 24 * 60 * 60 * 1000
                    || candidates > 2 || retained > 2 * FGUpdateTransfer.MAX_BYTES) {
                synchronized (FGUpdateDownload.class) {
                    if (!protectedGroup(group, keep)) for (File file : group) file.delete();
                }
            }
        }
    }

    private static boolean protectedGroup(ArrayList<File> group, String keep) {
        String name = group.get(0).getName();
        return keep != null && name.startsWith(keep) || requested != null
                && name.startsWith(FGClientUpdates.fileFor(requested).getName());
    }

    private static long newest(ArrayList<File> group) {
        long result = 0;
        for (File file : group) result = Math.max(result, file.lastModified());
        return result;
    }

    private static String diagnostic(Exception error) {
        return text(error instanceof Rejected ? ((Rejected) error).resource
                : error instanceof StorageFailure ? R.string.FGUpdateErrorStorage : R.string.FGUpdateErrorNetwork);
    }

    private static String text(int resource) {
        return ApplicationLoader.applicationContext.getString(resource);
    }

    private static final class Rejected extends IOException {
        final int resource;

        Rejected(int resource) {
            this.resource = resource;
        }
    }

    private static final class StorageFailure extends IOException {
    }

    private static final class Verified {
        final String identity;
        final File file;
        final long length;
        final long modified;

        Verified(FGClientUpdates.Update update, File file) {
            identity = FGClientUpdates.identity(update);
            this.file = file;
            length = file.length();
            modified = file.lastModified();
        }

        boolean matches(FGClientUpdates.Update update) {
            return update != null && identity.equals(FGClientUpdates.identity(update))
                    && file.isFile() && file.length() == length && file.lastModified() == modified;
        }
    }
}

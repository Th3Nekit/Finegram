package com.th3nekit.finegram.core.crash;

import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Process;
import android.util.AtomicFile;

import org.json.JSONException;
import org.json.JSONObject;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.BuildConfig;
import org.telegram.messenger.BuildVars;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.Map;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Consumer;

import okhttp3.Call;
import okhttp3.MediaType;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.RequestBody;
import okhttp3.Response;

public final class CameraLogUpload {
    private static final int JOB_ID = 0x464743;
    private static final String VERSION = BuildConfig.BUILD_VERSION_STRING_FINEGRAM;
    private static final int CODE = BuildVars.BUILD_VERSION;
    private static final String BUILD = VERSION + "-" + CODE;
    private static final String ENDPOINT = "https://spb.th3web.com/finegram/diagnostics/v1/camera";
    private static final ScheduledExecutorService WORKER = Executors.newSingleThreadScheduledExecutor(r -> new Thread(r, "camera-diagnostics"));
    private static final Object LOCK = new Object();
    private static final Object UPLOAD_LOCK = new Object();
    private static volatile boolean initialized;
    private static volatile boolean allowed;
    private static volatile long generation;
    private static volatile Call uploading;
    private static volatile AtomicBoolean uploadOwner;
    private static volatile Capture capture;
    private static CameraReportQueue outbox;
    private static File directory;
    private static ScheduledFuture<?> firstCheckpoint;
    private static ScheduledFuture<?> checkpoint;
    private static long lastIncident;

    private static final class Capture {
        final AtomicBoolean cancelled = new AtomicBoolean();
        final DiagnosticLogcat.Tail logcat = new DiagnosticLogcat.Tail(120000);
        volatile String status = "starting";
        Thread reader;
    }

    private static SharedPreferences preferences() {
        return ApplicationLoader.applicationContext.getSharedPreferences("camera_diagnostics", Context.MODE_PRIVATE);
    }

    private static Boolean userChoice() {
        SharedPreferences prefs = preferences();
        if (prefs.contains("enabled")) return prefs.getBoolean("enabled", false);
        for (Map.Entry<String, ?> entry : prefs.getAll().entrySet()) {
            if (entry.getKey().startsWith("enabled_") && Boolean.FALSE.equals(entry.getValue())) return false;
        }
        return null;
    }

    public static boolean isEnabled() {
        return CameraReportPolicy.enabled(CODE, userChoice());
    }

    public static boolean needsNotice() {
        return userChoice() == null && CameraReportPolicy.byDefault(CODE)
                && !preferences().getBoolean("notice_" + CODE, false);
    }

    public static void noticeShown() {
        initialize();
        synchronized (LOCK) {
            preferences().edit().putBoolean("notice_" + CODE, true).commit();
            allowed = isEnabled();
        }
    }

    public static void initialize() {
        synchronized (LOCK) {
            if (initialized) return;
            if (ApplicationLoader.applicationContext == null) return;
            directory = new File(ApplicationLoader.applicationContext.getNoBackupFilesDir(), "camera-reports");
            outbox = new CameraReportQueue(new File(directory, BUILD + "/outbox"));
            allowed = CameraReportPolicy.collect(CODE, userChoice(), !needsNotice());
            initialized = true;
        }
        WORKER.execute(() -> {
            File[] versions = directory.listFiles();
            if (versions != null) for (File old : versions) if (!old.getName().equals(BUILD)) deleteTree(old);
            if (allowed) recover();
            else if (!preferences().getBoolean("manual_pending_" + BUILD, false)) clearFiles();
            scheduleUpload();
        });
    }

    public static void setEnabled(boolean enabled) {
        initialize();
        synchronized (LOCK) {
            SharedPreferences.Editor choice = preferences().edit().putBoolean("enabled", enabled)
                    .putBoolean("notice_" + CODE, true);
            if (!enabled) choice.putBoolean("manual_pending_" + BUILD, false);
            choice.commit();
            allowed = enabled;
            if (!enabled) {
                generation++;
                Call call = uploading;
                if (call != null) call.cancel();
                stopCapture();
                JobScheduler scheduler = scheduler();
                if (scheduler != null) scheduler.cancel(JOB_ID);
                WORKER.execute(CameraLogUpload::clearFiles);
            }
        }
    }

    public static void cameraOpened(boolean resumed) {
        initialize();
        synchronized (LOCK) {
            if (!allowed || capture != null) return;
            Capture next = new Capture();
            capture = next;
            long token = generation;
            CameraDiagnostics.record("round.session.open", "resumed=" + resumed);
            next.reader = new Thread(() -> {
                while (!next.cancelled.get()) {
                    long started = System.nanoTime();
                    next.status = DiagnosticLogcat.read(
                            Arrays.asList("/system/bin/logcat", "-v", "threadtime", "--pid=" + Process.myPid(), "-T", "1"),
                            15 * 60 * 1000L, 8 * 1024 * 1024, next.cancelled::get,
                            line -> next.logcat.add(DiagnosticLogcat.cameraLine(line, Process.myPid())));
                    if (System.nanoTime() - started < TimeUnit.SECONDS.toNanos(2)) break;
                }
            }, "camera-logcat");
            next.reader.start();
            WORKER.execute(() -> save(next, token));
            firstCheckpoint = WORKER.schedule(() -> save(next, token), 1, TimeUnit.SECONDS);
            checkpoint = WORKER.scheduleWithFixedDelay(() -> save(next, token), 15, 15, TimeUnit.SECONDS);
        }
    }

    public static void cameraClosed() {
        synchronized (LOCK) {
            Capture previous = capture;
            if (previous == null) return;
            CameraDiagnostics.record("round.session.close", "completed=true");
            stopCapture();
            long token = generation;
            WORKER.execute(() -> {
                try { previous.reader.join(1000); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
                save(previous, token);
                if (allowed && generation == token) recover();
                scheduleUpload();
            });
        }
    }

    private static void stopCapture() {
        Capture previous = capture;
        capture = null;
        if (previous != null) previous.cancelled.set(true);
        if (firstCheckpoint != null) firstCheckpoint.cancel(false);
        firstCheckpoint = null;
        if (checkpoint != null) checkpoint.cancel(false);
        checkpoint = null;
    }

    private static File sessionFile() {
        return new File(directory, BUILD + "/session.json");
    }

    private static void save(Capture value, long token) {
        if (!allowed || generation != token) return;
        AtomicFile file = new AtomicFile(sessionFile());
        FileOutputStream stream = null;
        try {
            JSONObject json = new JSONObject();
            json.put("created", System.currentTimeMillis());
            json.put("device", DiagnosticText.clean(ManualDiagnostics.snapshot(ApplicationLoader.applicationContext)));
            json.put("events", CameraDiagnostics.snapshot());
            json.put("logcat", "Capture status: " + value.status + "\n" + value.logcat.snapshot());
            byte[] bytes = json.toString().getBytes(StandardCharsets.UTF_8);
            if (bytes.length > 1024 * 1024) return;
            stream = file.startWrite();
            stream.write(bytes);
            if (!allowed || generation != token) {
                file.failWrite(stream);
                file.delete();
            } else file.finishWrite(stream);
        } catch (Exception ignored) {
            if (stream != null) file.failWrite(stream);
        }
    }

    private static void recover() {
        AtomicFile file = new AtomicFile(sessionFile());
        final String device;
        final String events;
        final String logcat;
        try {
            File base = file.getBaseFile();
            if (!base.exists() || base.length() > 1024 * 1024) { file.delete(); return; }
            byte[] bytes = file.readFully();
            JSONObject json = new JSONObject(new String(bytes, StandardCharsets.UTF_8));
            long age = System.currentTimeMillis() - json.optLong("created");
            if (age < -60 * 60 * 1000L || age > 48 * 60 * 60 * 1000L) { file.delete(); return; }
            device = json.getString("device");
            events = json.getString("events");
            logcat = json.getString("logcat");
        } catch (IOException ignored) {
            return;
        } catch (JSONException ignored) {
            file.delete();
            return;
        }
        File archive = null;
        try {
            long token = generation;
            archive = DiagnosticArchive.create(new File(directory, BUILD + "/staging"), device, events, logcat,
                    () -> !allowed || generation != token);
            synchronized (LOCK) {
                if (allowed && generation == token) outbox.add(archive, System.currentTimeMillis());
                else archive.delete();
            }
            file.delete();
        } catch (Exception ignored) {
        } finally {
            if (archive != null) archive.delete();
        }
    }

    public static void sendNow(Consumer<Boolean> completed) {
        initialize();
        queueReport(true, completed);
    }

    public static void incident(String event, String state) {
        CameraDiagnostics.record(event, state);
        initialize();
        synchronized (LOCK) {
            long now = System.currentTimeMillis();
            if (!allowed || !initialized || now - lastIncident < 10 * 60 * 1000L) return;
            lastIncident = now;
        }
        queueReport(false, null);
    }

    private static void queueReport(boolean manual, Consumer<Boolean> completed) {
        long token = generation;
        WORKER.execute(() -> {
            boolean queued = false;
            File archive = null;
            try {
                DiagnosticLogcat.Tail tail = new DiagnosticLogcat.Tail(120000);
                String status = DiagnosticLogcat.read(Arrays.asList("/system/bin/logcat", "-d", "-v", "threadtime",
                        "--pid=" + Process.myPid(), "-t", "2000"), 2500, 384 * 1024, () -> generation != token,
                        line -> tail.add(DiagnosticLogcat.cameraLine(line, Process.myPid())));
                Capture current = capture;
                String logs = "Dump status: " + status + "\n" + tail.snapshot();
                if (current != null) logs += "\n" + current.status + "\n" + current.logcat.snapshot();
                archive = DiagnosticArchive.create(new File(directory, BUILD + "/staging"),
                        ManualDiagnostics.snapshot(ApplicationLoader.applicationContext), CameraDiagnostics.snapshot(),
                        logs, () -> generation != token);
                synchronized (LOCK) {
                    if (generation == token && (manual || allowed)) {
                        outbox.add(archive, System.currentTimeMillis());
                        if (manual) preferences().edit().putBoolean("manual_pending_" + BUILD, true).commit();
                        queued = true;
                    }
                }
                if (queued) scheduleUpload();
            } catch (Exception ignored) {
            } finally {
                if (!queued && archive != null) archive.delete();
                if (completed != null) completed.accept(queued);
            }
        });
    }

    private static JobScheduler scheduler() {
        return (JobScheduler) ApplicationLoader.applicationContext.getSystemService(Context.JOB_SCHEDULER_SERVICE);
    }

    private static void scheduleUpload() {
        if (outbox.pending(System.currentTimeMillis()).isEmpty()) return;
        JobScheduler scheduler = scheduler();
        if (scheduler == null) return;
        try {
            scheduler.schedule(new JobInfo.Builder(JOB_ID, new ComponentName(ApplicationLoader.applicationContext, CameraLogUploadJob.class))
                    .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                    .setBackoffCriteria(60_000, JobInfo.BACKOFF_POLICY_EXPONENTIAL)
                    .setPersisted(true).build());
        } catch (RuntimeException ignored) {
        }
    }

    static boolean upload(AtomicBoolean stopped) {
        synchronized (UPLOAD_LOCK) {
            return uploadSerial(stopped);
        }
    }

    private static boolean uploadSerial(AtomicBoolean stopped) {
        initialize();
        if (stopped.get()) return false;
        long token = generation;
        OkHttpClient client = new OkHttpClient.Builder().followRedirects(false).followSslRedirects(false)
                .retryOnConnectionFailure(false).connectTimeout(10, TimeUnit.SECONDS)
                .readTimeout(20, TimeUnit.SECONDS).writeTimeout(20, TimeUnit.SECONDS)
                .callTimeout(30, TimeUnit.SECONDS).build();
        try {
            for (File file : outbox.pending(System.currentTimeMillis())) {
                final Call call;
                final String id = file.getName().substring(0, 36);
                synchronized (LOCK) {
                    if (stopped.get() || generation != token || (!allowed && !preferences().getBoolean("manual_pending_" + BUILD, false))) return false;
                    Request request = new Request.Builder().url(ENDPOINT)
                            .header("X-Finegram-Report-Id", id).header("X-Finegram-Version", VERSION)
                            .header("User-Agent", "Finegram-Camera-Diagnostics/1")
                            .post(RequestBody.create(file, MediaType.get("application/zip"))).build();
                    call = client.newCall(request);
                    uploading = call;
                    uploadOwner = stopped;
                }
                try (Response response = call.execute()) {
                    if (stopped.get() || generation != token) return false;
                    int code = response.code();
                    if (code == 200 || code == 201) {
                        if (!receipt(response, id)) return true;
                        outbox.remove(file);
                    } else if (code == 400 && "invalid_version".equals(error(response))) return true;
                    else if (code >= 400 && code < 500 && code != 408 && code != 429) outbox.remove(file);
                    else return true;
                } catch (Exception ignored) {
                    return !stopped.get() && generation == token;
                } finally {
                    synchronized (LOCK) {
                        if (uploading == call) {
                            uploading = null;
                            uploadOwner = null;
                        }
                    }
                }
            }
            synchronized (LOCK) {
                if (!outbox.pending(System.currentTimeMillis()).isEmpty()) return true;
                preferences().edit().putBoolean("manual_pending_" + BUILD, false).apply();
                return false;
            }
        } finally {
            client.connectionPool().evictAll();
            client.dispatcher().executorService().shutdown();
        }
    }

    private static boolean receipt(Response response, String id) throws Exception {
        JSONObject json = smallJson(response);
        return json != null && id.equals(json.optString("id"))
                && ("stored".equals(json.optString("status")) || "duplicate".equals(json.optString("status")));
    }

    private static String error(Response response) {
        try {
            JSONObject json = smallJson(response);
            return json == null ? null : json.optString("error", null);
        } catch (Exception ignored) {
            return null;
        }
    }

    private static JSONObject smallJson(Response response) throws Exception {
        if (response.body() == null || response.body().contentLength() > 4096) return null;
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (InputStream stream = response.body().byteStream()) {
            byte[] buffer = new byte[512];
            int count;
            while ((count = stream.read(buffer)) != -1) {
                if (bytes.size() + count > 4096) return null;
                bytes.write(buffer, 0, count);
            }
        }
        return new JSONObject(new String(bytes.toByteArray(), StandardCharsets.UTF_8));
    }

    static void cancelUpload(AtomicBoolean owner) {
        synchronized (LOCK) {
            Call call = uploading;
            if (call != null && uploadOwner == owner) call.cancel();
        }
    }

    private static void clearFiles() {
        outbox.clear();
        new AtomicFile(sessionFile()).delete();
        deleteTree(new File(directory, BUILD + "/staging"));
    }

    private static void deleteTree(File file) {
        File[] children = file.listFiles();
        if (children != null) for (File child : children) deleteTree(child);
        file.delete();
    }

    private CameraLogUpload() {}
}

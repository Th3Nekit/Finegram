package com.th3nekit.finegram.core.crash;

import android.app.job.JobParameters;
import android.app.job.JobService;
import android.os.Handler;
import android.os.Looper;

import java.util.IdentityHashMap;
import java.util.concurrent.atomic.AtomicBoolean;

public final class CameraLogUploadJob extends JobService {
    private final IdentityHashMap<JobParameters, AtomicBoolean> running = new IdentityHashMap<>();
    private final Handler main = new Handler(Looper.getMainLooper());

    @Override
    public boolean onStartJob(JobParameters params) {
        AtomicBoolean stop = new AtomicBoolean();
        running.put(params, stop);
        new Thread(() -> {
            boolean retry = true;
            try {
                retry = CameraLogUpload.upload(stop);
            } catch (RuntimeException ignored) {
            }
            boolean reschedule = retry;
            main.post(() -> {
                if (running.get(params) != stop) return;
                running.remove(params);
                if (!stop.get()) jobFinished(params, reschedule);
            });
        }, "camera-report-upload").start();
        return true;
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        AtomicBoolean stop = running.remove(params);
        if (stop != null) {
            stop.set(true);
            CameraLogUpload.cancelUpload(stop);
        }
        return true;
    }

    @Override
    public void onDestroy() {
        for (AtomicBoolean stop : running.values()) {
            stop.set(true);
            CameraLogUpload.cancelUpload(stop);
        }
        running.clear();
        super.onDestroy();
    }
}

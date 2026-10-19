/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.updater;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.text.format.Formatter;

import androidx.core.app.NotificationCompat;

import org.telegram.messenger.R;
import org.telegram.ui.LaunchActivity;

public final class FGUpdateService extends Service implements FGUpdateDownload.Listener {
    static final String ACTION_START = "com.th3nekit.finegram.UPDATE_START";
    private static final String ACTION_PAUSE = "com.th3nekit.finegram.UPDATE_PAUSE";
    private static final String ACTION_RESUME = "com.th3nekit.finegram.UPDATE_RESUME";
    private static final String ACTION_DISMISS = "com.th3nekit.finegram.UPDATE_DISMISS";
    static final String EXTRA_UPDATE = "update";
    static final String EXTRA_REQUEST = "request";
    private static final String CHANNEL = "finegram_updates";
    private static final int NOTIFICATION = 0x464755;
    private FGClientUpdates.Update update;
    private long requestId = -1;
    private boolean stopping;
    private boolean dismissed;

    @Override
    public void onCreate() {
        super.onCreate();
        if (Build.VERSION.SDK_INT >= 26) {
            NotificationChannel channel = new NotificationChannel(CHANNEL,
                    getString(R.string.FGUpdateNotificationChannel), NotificationManager.IMPORTANCE_LOW);
            channel.setDescription(getString(R.string.FGUpdateNotificationChannelDescription));
            channel.setSound(null, null);
            getSystemService(NotificationManager.class).createNotificationChannel(channel);
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null) {
            stopSelf(startId);
            return START_NOT_STICKY;
        }
        String action = intent.getAction();
        if (ACTION_DISMISS.equals(action)) {
            dismissed = true;
            if (requestId < 0) {
                stopping = true;
                stopSelf(startId);
            }
            return START_NOT_STICKY;
        }
        if (ACTION_PAUSE.equals(action)) {
            pauseAndStop();
            return START_NOT_STICKY;
        }
        if (ACTION_RESUME.equals(action)) {
            FGUpdateDownload.State current = FGUpdateDownload.getState();
            FGClientUpdates.Update resume = FGClientUpdates.deserialize(intent.getStringExtra(EXTRA_UPDATE));
            if (resume == null) resume = current.update != null ? current.update : FGClientUpdates.getPending();
            if (resume != null) {
                update = resume;
                requestId = FGUpdateDownload.requestResume(resume);
                stopping = false;
                dismissed = false;
                try {
                    foreground(notification(current, FGUpdateDownload.Phase.PAUSED));
                    FGUpdateDownload.addListener(this);
                    FGUpdateDownload.begin(update, requestId);
                } catch (RuntimeException error) {
                    stopping = true;
                    FGUpdateDownload.foregroundDenied(update, requestId);
                    stopSelf(startId);
                }
            } else {
                stopSelf(startId);
            }
            return START_NOT_STICKY;
        }
        if (!ACTION_START.equals(action)) {
            stopSelf(startId);
            return START_NOT_STICKY;
        }
        long id = intent.getLongExtra(EXTRA_REQUEST, -1);
        FGClientUpdates.Update candidate = FGClientUpdates.deserialize(intent.getStringExtra(EXTRA_UPDATE));
        if (candidate == null || !FGUpdateDownload.isCurrent(id)) {
            if (requestId < 0) stopSelf(startId);
            return START_NOT_STICKY;
        }
        update = candidate;
        requestId = id;
        stopping = false;
        dismissed = false;
        try {
            foreground(notification(FGUpdateDownload.getState(), FGUpdateDownload.Phase.DOWNLOADING));
            FGUpdateDownload.addListener(this);
            FGUpdateDownload.begin(update, requestId);
        } catch (RuntimeException error) {
            stopping = true;
            FGUpdateDownload.foregroundDenied(update, requestId);
            stopSelf(startId);
        }
        return START_NOT_STICKY;
    }

    private void foreground(Notification notification) {
        if (Build.VERSION.SDK_INT >= 29) startForeground(NOTIFICATION, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
        else startForeground(NOTIFICATION, notification);
    }

    @Override
    public void onStateChanged(FGUpdateDownload.State state) {
        if (stopping || !FGUpdateDownload.isSame(update, state.update)) return;
        boolean active = state.phase == FGUpdateDownload.Phase.DOWNLOADING || state.phase == FGUpdateDownload.Phase.VERIFYING;
        if (state.generation != requestId && !(state.phase == FGUpdateDownload.Phase.PAUSED
                && state.generation > requestId && FGUpdateDownload.isCurrent(state.generation))) return;
        try {
            if (!dismissed) getSystemService(NotificationManager.class).notify(NOTIFICATION, notification(state, state.phase));
        } catch (RuntimeException ignored) {
        }
        if (!active) {
            stopping = true;
            detach();
            stopSelf();
        }
    }

    private void pauseAndStop() {
        FGUpdateDownload.State state = FGUpdateDownload.getState();
        FGUpdateDownload.pause();
        stopping = true;
        detach();
        if (state.update != null && !dismissed) {
            update = state.update;
            try {
                getSystemService(NotificationManager.class).notify(NOTIFICATION,
                        notification(state, FGUpdateDownload.Phase.PAUSED));
            } catch (RuntimeException ignored) {
            }
        }
        stopSelf();
    }

    @Override
    public void onTimeout(int startId, int foregroundServiceType) {
        pauseAndStop();
    }

    @Override
    public void onTimeout(int startId) {
        pauseAndStop();
    }

    @Override
    public void onDestroy() {
        FGUpdateDownload.removeListener(this);
        if (!stopping && requestId >= 0) FGUpdateDownload.serviceStopped(requestId);
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void detach() {
        if (Build.VERSION.SDK_INT >= 24) stopForeground(STOP_FOREGROUND_DETACH);
        else stopForeground(false);
    }

    private Notification notification(FGUpdateDownload.State state, FGUpdateDownload.Phase phase) {
        Notification.Builder builder = Build.VERSION.SDK_INT >= 26
                ? new Notification.Builder(this, CHANNEL) : new Notification.Builder(this);
        Intent open = new Intent(this, LaunchActivity.class).setAction("com.th3nekit.finegram.OPEN_UPDATER")
                .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent content = PendingIntent.getActivity(this, NOTIFICATION, open,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        PendingIntent dismiss = PendingIntent.getService(this, NOTIFICATION + 3,
                new Intent(this, FGUpdateService.class).setAction(ACTION_DISMISS),
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        boolean active = phase == FGUpdateDownload.Phase.DOWNLOADING || phase == FGUpdateDownload.Phase.VERIFYING;
        int text = phase == FGUpdateDownload.Phase.PAUSED ? R.string.FGUpdateNotificationPaused
                : phase == FGUpdateDownload.Phase.VERIFYING ? R.string.FGUpdateNotificationVerifying
                : phase == FGUpdateDownload.Phase.READY ? R.string.FGUpdateNotificationReady
                : phase == FGUpdateDownload.Phase.FAILED ? R.string.FGUpdateNotificationFailed
                : R.string.FGUpdateNotificationDownloading;
        boolean same = FGUpdateDownload.isSame(update, state.update);
        long done = same ? state.downloaded : 0;
        FGClientUpdates.Build build = update == null ? null : update.preferred();
        long total = same ? state.total : build == null ? 0 : build.size;
        String detail = getString(text);
        if (phase == FGUpdateDownload.Phase.DOWNLOADING || phase == FGUpdateDownload.Phase.PAUSED) {
            detail += " · " + Formatter.formatShortFileSize(this, done);
            if (total > 0) detail += " / " + Formatter.formatShortFileSize(this, total);
        }
        if (phase == FGUpdateDownload.Phase.FAILED && state.error != null) detail += ": " + state.error;
        builder.setSmallIcon(phase == FGUpdateDownload.Phase.READY ? android.R.drawable.stat_sys_download_done
                        : android.R.drawable.stat_sys_download)
                .setContentTitle(getString(R.string.FGUpdateNotificationTitle, update == null ? "" : update.version))
                .setContentText(detail).setContentIntent(content).setDeleteIntent(dismiss).setOnlyAlertOnce(true)
                .setOngoing(active).setAutoCancel(!active).setCategory(Notification.CATEGORY_PROGRESS)
                .setVisibility(Notification.VISIBILITY_PRIVATE).setShowWhen(false);
        if (active) {
            int progress = total <= 0 ? 0 : (int) Math.min(10000, done * 10000 / total);
            boolean indeterminate = phase == FGUpdateDownload.Phase.VERIFYING || total <= 0;
            builder.setProgress(10000, progress, indeterminate);
            if (Build.VERSION.SDK_INT >= 36) Api36.apply(builder, progress, indeterminate);
            PendingIntent pause = PendingIntent.getService(this, NOTIFICATION + 1,
                    new Intent(this, FGUpdateService.class).setAction(ACTION_PAUSE),
                    PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
            builder.addAction(new Notification.Action.Builder(android.R.drawable.ic_media_pause,
                    getString(R.string.FGUpdateNotificationPause), pause).build());
        } else if (phase == FGUpdateDownload.Phase.PAUSED || phase == FGUpdateDownload.Phase.FAILED) {
            Intent resume = new Intent(this, FGUpdateService.class).setAction(ACTION_RESUME)
                    .putExtra(EXTRA_UPDATE, FGClientUpdates.serialize(update));
            int pendingFlags = PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE;
            PendingIntent action = Build.VERSION.SDK_INT >= 26
                    ? PendingIntent.getForegroundService(this, NOTIFICATION + 2, resume, pendingFlags)
                    : PendingIntent.getService(this, NOTIFICATION + 2, resume, pendingFlags);
            builder.addAction(new Notification.Action.Builder(android.R.drawable.ic_media_play,
                    getString(R.string.FGUpdateNotificationResume), action).build());
        }
        return builder.build();
    }

    private static final class Api36 {
        static void apply(Notification.Builder builder, int progress, boolean indeterminate) {
            builder.getExtras().putBoolean(NotificationCompat.EXTRA_REQUEST_PROMOTED_ONGOING, true);
            builder.setShortCriticalText(indeterminate ? null : (progress / 100) + "%");
            Notification.ProgressStyle.Segment segment = new Notification.ProgressStyle.Segment(10000);
            if (Build.VERSION.SDK_INT >= 37) {
                try {
                    int information = Notification.class.getField("SEMANTIC_STYLE_INFO").getInt(null);
                    segment.getClass().getMethod("setSemanticStyle", int.class).invoke(segment, information);
                } catch (ReflectiveOperationException | LinkageError ignored) {
                }
            }
            builder.setStyle(new Notification.ProgressStyle().addProgressSegment(segment)
                    .setProgress(progress).setProgressIndeterminate(indeterminate).setStyledByProgress(true));
        }
    }
}

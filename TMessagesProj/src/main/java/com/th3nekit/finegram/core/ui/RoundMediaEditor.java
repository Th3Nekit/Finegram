package com.th3nekit.finegram.core.ui;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.media.MediaMetadataRetriever;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.FileLoader;
import org.telegram.messenger.FileLog;
import org.telegram.messenger.ImageLoader;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.MediaController;
import org.telegram.messenger.R;
import org.telegram.messenger.Utilities;
import org.telegram.messenger.VideoEditedInfo;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.LayoutHelper;

import java.io.File;
import java.io.FileOutputStream;
import java.util.concurrent.atomic.AtomicBoolean;

public final class RoundMediaEditor {
    public interface Callback {
        void onReady(VideoEditedInfo info);
        void onError();
        void onCancelled();
    }

    private final Context context;
    private final Theme.ResourcesProvider resourcesProvider;
    private final String source;
    private final boolean video;
    private final int account;
    private final Callback callback;
    private final AtomicBoolean cancelled = new AtomicBoolean();
    private AlertDialog progress;
    private AlertDialog editor;
    private Bitmap preview;
    private RoundMediaGeometry geometry;
    private int originalWidth;
    private int originalHeight;
    private int rotation;
    private long duration;
    private long start;
    private long length = 5000;
    private float offsetX;
    private float offsetY;
    private boolean encoding;
    private int previewRevision;
    private PreviewView previewView;

    public RoundMediaEditor(Context context, Theme.ResourcesProvider resourcesProvider, String source, boolean video, int account, boolean showEditor, Callback callback) {
        this.context = context;
        this.resourcesProvider = resourcesProvider;
        this.source = source;
        this.video = video;
        this.account = account;
        this.callback = callback;
        showProgress();
        Utilities.globalQueue.postRunnable(() -> {
            Bitmap bitmap = null;
            try {
                if (video) {
                    MediaMetadataRetriever retriever = new MediaMetadataRetriever();
                    try {
                        retriever.setDataSource(source);
                        originalWidth = Integer.parseInt(retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH));
                        originalHeight = Integer.parseInt(retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT));
                        String value = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_ROTATION);
                        rotation = value == null ? 0 : Integer.parseInt(value);
                        duration = Long.parseLong(retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION));
                        bitmap = frame(retriever, 0);
                    } finally {
                        retriever.release();
                    }
                } else {
                    bitmap = ImageLoader.loadBitmap(source, null, 1024, 1024, true);
                    if (bitmap != null) {
                        originalWidth = bitmap.getWidth();
                        originalHeight = bitmap.getHeight();
                    }
                    duration = RoundMediaGeometry.MAX_DURATION;
                }
                geometry = new RoundMediaGeometry(originalWidth, originalHeight, rotation);
                if (bitmap == null || duration <= 0) throw new IllegalArgumentException();
                length = video ? Math.min(duration, RoundMediaGeometry.MAX_DURATION) : 5000;
                Bitmap result = bitmap;
                AndroidUtilities.runOnUIThread(() -> {
                    if (cancelled.get()) {
                        result.recycle();
                        return;
                    }
                    closeProgress();
                    preview = result;
                    if (showEditor) openEditor(); else prepare();
                });
            } catch (Exception e) {
                FileLog.e(e);
                if (bitmap != null) bitmap.recycle();
                AndroidUtilities.runOnUIThread(this::fail);
            }
        });
    }

    private static Bitmap frame(MediaMetadataRetriever retriever, long time) {
        if (android.os.Build.VERSION.SDK_INT >= 27) {
            String w = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH);
            String h = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT);
            String r = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_ROTATION);
            RoundMediaGeometry g = new RoundMediaGeometry(Integer.parseInt(w), Integer.parseInt(h), r == null ? 0 : Integer.parseInt(r));
            float scale = Math.min(1, 1024f / Math.max(g.width, g.height));
            return retriever.getScaledFrameAtTime(time * 1000, MediaMetadataRetriever.OPTION_CLOSEST_SYNC, Math.max(1, (int) (g.width * scale)), Math.max(1, (int) (g.height * scale)));
        }
        return retriever.getFrameAtTime(time * 1000, MediaMetadataRetriever.OPTION_CLOSEST_SYNC);
    }

    private void showProgress() {
        progress = new AlertDialog(context, 3, resourcesProvider);
        progress.setOnCancelListener(dialog -> cancel());
        progress.show();
    }

    private void closeProgress() {
        if (progress != null) {
            progress.dismiss();
            progress = null;
        }
    }

    private void openEditor() {
        LinearLayout content = new LinearLayout(context);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(AndroidUtilities.dp(24), 0, AndroidUtilities.dp(24), AndroidUtilities.dp(8));
        previewView = new PreviewView(context);
        content.addView(previewView, LayoutHelper.createLinear(240, 240, Gravity.CENTER_HORIZONTAL));
        TextView hint = label(LocaleController.getString(R.string.FG_RoundCropHint));
        content.addView(hint, LayoutHelper.createLinear(-1, -2, 0, 12, 0, 8));
        TextView timing = label("");
        content.addView(timing, LayoutHelper.createLinear(-1, -2));
        SeekBar durationBar = new SeekBar(context);
        durationBar.setMax(1000);
        if (video) {
            SeekBar startBar = new SeekBar(context);
            startBar.setMax(1000);
            content.addView(startBar, LayoutHelper.createLinear(-1, 40));
            startBar.setOnSeekBarChangeListener(listener(value -> {
                start = RoundMediaGeometry.start(Math.round(value / 1000.0 * Math.max(0, duration - 1000)), duration);
                length = RoundMediaGeometry.length(length, start, duration);
                updateTiming(timing);
                updateDuration(durationBar);
            }, this::refreshPreview));
        }
        content.addView(durationBar, LayoutHelper.createLinear(-1, 40));
        updateDuration(durationBar);
        durationBar.setOnSeekBarChangeListener(listener(value -> {
            long max = Math.min(RoundMediaGeometry.MAX_DURATION, duration - start);
            length = Math.min(max, 1000 + Math.round(value / 1000.0 * Math.max(0, max - 1000)));
            updateTiming(timing);
        }));
        updateTiming(timing);
        editor = new AlertDialog.Builder(context, resourcesProvider)
                .setTitle(LocaleController.getString(R.string.FG_SendAsRound))
                .setView(content)
                .setPositiveButton(LocaleController.getString(R.string.Send), (dialog, which) -> prepare())
                .setNegativeButton(LocaleController.getString(R.string.Cancel), (dialog, which) -> cancel())
                .setOnDismissListener(dialog -> {
                    if (!encoding) cancel();
                }).create();
        editor.show();
    }

    private interface ProgressCallback { void changed(int value); }

    private SeekBar.OnSeekBarChangeListener listener(ProgressCallback callback) {
        return listener(callback, null);
    }

    private SeekBar.OnSeekBarChangeListener listener(ProgressCallback callback, Runnable stopped) {
        return new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar bar, int value, boolean user) { if (user) callback.changed(value); }
            public void onStartTrackingTouch(SeekBar bar) { }
            public void onStopTrackingTouch(SeekBar bar) { if (stopped != null) stopped.run(); }
        };
    }

    private void refreshPreview() {
        if (!video || encoding || cancelled.get()) return;
        final int revision = ++previewRevision;
        final long time = start;
        Utilities.globalQueue.postRunnable(() -> {
            Bitmap bitmap = null;
            try {
                MediaMetadataRetriever retriever = new MediaMetadataRetriever();
                try {
                    retriever.setDataSource(source);
                    bitmap = frame(retriever, time);
                } finally {
                    retriever.release();
                }
            } catch (Exception e) {
                FileLog.e(e);
            }
            final Bitmap result = bitmap;
            if (result == null) return;
            AndroidUtilities.runOnUIThread(() -> {
                if (cancelled.get() || encoding || revision != previewRevision) {
                    result.recycle();
                    return;
                }
                Bitmap old = preview;
                preview = result;
                if (old != null) old.recycle();
                if (previewView != null) previewView.invalidate();
            });
        });
    }

    private TextView label(String value) {
        TextView label = new TextView(context);
        label.setTextColor(Theme.getColor(Theme.key_dialogTextGray, resourcesProvider));
        label.setTextSize(14);
        label.setText(value);
        return label;
    }

    private void updateTiming(TextView label) {
        label.setText(video ? LocaleController.formatString("FG_RoundVideoRange", R.string.FG_RoundVideoRange, LocaleController.formatDuration((int) (start / 1000)), LocaleController.formatDuration((int) (length / 1000))) : LocaleController.formatString("FG_RoundPhotoDuration", R.string.FG_RoundPhotoDuration, LocaleController.formatDuration((int) (length / 1000))));
    }

    private void updateDuration(SeekBar bar) {
        long max = Math.min(RoundMediaGeometry.MAX_DURATION, duration - start);
        bar.setProgress(max <= 1000 ? 0 : (int) Math.round((length - 1000) * 1000.0 / (max - 1000)));
        bar.setEnabled(max > 1000);
    }

    private void prepare() {
        if (encoding || cancelled.get()) return;
        encoding = true;
        previewRevision++;
        if (editor != null) editor.dismiss();
        showProgress();
        Utilities.globalQueue.postRunnable(() -> {
            Bitmap square = null;
            File photo = null;
            Bitmap sourceFrame = null;
            Bitmap loadedPreview = preview;
            try {
                start = RoundMediaGeometry.start(start, duration);
                length = RoundMediaGeometry.length(length, start, duration);
                sourceFrame = loadedPreview;
                if (video && start > 0) {
                    MediaMetadataRetriever retriever = new MediaMetadataRetriever();
                    try {
                        retriever.setDataSource(source);
                        Bitmap frame = frame(retriever, start);
                        if (frame != null) sourceFrame = frame;
                    } finally {
                        retriever.release();
                    }
                }
                square = Bitmap.createBitmap(RoundMediaGeometry.SIDE, RoundMediaGeometry.SIDE, Bitmap.Config.ARGB_8888);
                drawCrop(new Canvas(square), sourceFrame, RoundMediaGeometry.SIDE, offsetX, offsetY);
                VideoEditedInfo info = new VideoEditedInfo();
                info.originalPath = source;
                info.originalWidth = originalWidth;
                info.originalHeight = originalHeight;
                info.rotationValue = rotation;
                info.cropState = new MediaController.CropState();
                info.cropState.cropPw = geometry.fractionX;
                info.cropState.cropPh = geometry.fractionY;
                info.cropState.cropPx = offsetX;
                info.cropState.cropPy = offsetY;
                info.cropState.transformWidth = info.cropState.transformHeight = RoundMediaGeometry.SIDE;
                if (!video) {
                    photo = File.createTempFile("round_", ".jpg", FileLoader.getDirectory(FileLoader.MEDIA_DIR_CACHE));
                    try (FileOutputStream stream = new FileOutputStream(photo)) {
                        if (!square.compress(Bitmap.CompressFormat.JPEG, 90, stream)) throw new IllegalStateException();
                    }
                    info.originalPath = photo.getAbsolutePath();
                    info.originalWidth = info.originalHeight = RoundMediaGeometry.SIDE;
                    info.cropState.cropPw = info.cropState.cropPh = 1;
                    info.cropState.cropPx = info.cropState.cropPy = 0;
                }
                info.startTime = start * 1000;
                info.endTime = (start + length) * 1000;
                info.originalDuration = (video ? duration : length) * 1000;
                info.estimatedDuration = length;
                info.estimatedSize = Math.max(1, length * 120000 / 1000);
                info.resultWidth = info.resultHeight = RoundMediaGeometry.SIDE;
                info.bitrate = 900000;
                info.framerate = 30;
                info.roundVideo = true;
                info.isPhoto = !video;
                info.notReadyYet = true;
                info.thumb = square;
                info.account = account;
                Bitmap finished = square;
                File finishedPhoto = photo;
                AndroidUtilities.runOnUIThread(() -> {
                    closeProgress();
                    releasePreview();
                    if (cancelled.get()) {
                        finished.recycle();
                        if (finishedPhoto != null) finishedPhoto.delete();
                    } else {
                        callback.onReady(info);
                    }
                });
            } catch (Exception e) {
                FileLog.e(e);
                if (square != null) square.recycle();
                if (photo != null) photo.delete();
                AndroidUtilities.runOnUIThread(this::fail);
            } finally {
                if (sourceFrame != null && sourceFrame != loadedPreview) sourceFrame.recycle();
            }
        });
    }

    private static void drawCrop(Canvas canvas, Bitmap bitmap, int size, float offsetX, float offsetY) {
        float scale = (float) size / Math.min(bitmap.getWidth(), bitmap.getHeight());
        float w = bitmap.getWidth() * scale, h = bitmap.getHeight() * scale;
        float left = (size - w) / 2 + offsetX * w;
        float top = (size - h) / 2 + offsetY * h;
        canvas.drawBitmap(bitmap, null, new RectF(left, top, left + w, top + h), new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG));
    }

    private void fail() {
        closeProgress();
        releasePreview();
        if (!cancelled.get()) callback.onError();
    }

    public void cancel() {
        if (!cancelled.compareAndSet(false, true)) return;
        previewRevision++;
        closeProgress();
        if (editor != null) editor.dismiss();
        if (!encoding) releasePreview();
        callback.onCancelled();
    }

    private void releasePreview() {
        if (previewView != null) previewView.setVisibility(View.GONE);
        if (preview != null) {
            preview.recycle();
            preview = null;
        }
    }

    private class PreviewView extends View {
        private final Path circle = new Path();
        private float lastX, lastY;

        PreviewView(Context context) { super(context); }

        @Override
        protected void onDraw(Canvas canvas) {
            if (preview == null || preview.isRecycled()) return;
            int save = canvas.save();
            circle.reset();
            circle.addCircle(getWidth() / 2f, getHeight() / 2f, Math.min(getWidth(), getHeight()) / 2f, Path.Direction.CW);
            canvas.clipPath(circle);
            drawCrop(canvas, preview, Math.min(getWidth(), getHeight()), offsetX, offsetY);
            canvas.restoreToCount(save);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                lastX = event.getX(); lastY = event.getY();
                getParent().requestDisallowInterceptTouchEvent(true);
                return true;
            }
            if (event.getActionMasked() == MotionEvent.ACTION_MOVE) {
                float side = Math.min(getWidth(), getHeight());
                if (side > 0) {
                    offsetX = geometry.clampX(offsetX + (event.getX() - lastX) * geometry.fractionX / side);
                    offsetY = geometry.clampY(offsetY + (event.getY() - lastY) * geometry.fractionY / side);
                    invalidate();
                }
                lastX = event.getX(); lastY = event.getY();
                return true;
            }
            if (event.getActionMasked() == MotionEvent.ACTION_UP || event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
                getParent().requestDisallowInterceptTouchEvent(false);
                return true;
            }
            return false;
        }
    }
}

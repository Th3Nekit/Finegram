package com.th3nekit.finegram.camera;

import android.graphics.Rect;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.MeteringRectangle;
import android.os.Build;
import android.os.Handler;
import android.util.Size;

import org.telegram.messenger.FileLog;

public final class Camera2FocusController {
    private MeteringRectangle[] regions;
    private boolean autofocus;
    private boolean meteringAf;
    private boolean meteringAe;
    private int generation;
    private Integer restoredMode;
    private Handler pendingHandler;
    private Runnable restore;

    public void apply(CaptureRequest.Builder builder) {
        if (regions == null) {
            if (restoredMode != null) builder.set(CaptureRequest.CONTROL_AF_MODE, restoredMode);
            return;
        }
        if (meteringAf) builder.set(CaptureRequest.CONTROL_AF_REGIONS, regions);
        if (meteringAe) builder.set(CaptureRequest.CONTROL_AE_REGIONS, regions);
        if (autofocus) builder.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_AUTO);
        builder.set(CaptureRequest.CONTROL_AF_TRIGGER, CaptureRequest.CONTROL_AF_TRIGGER_IDLE);
    }

    public void cancel() {
        generation++;
        if (pendingHandler != null && restore != null) pendingHandler.removeCallbacks(restore);
        pendingHandler = null;
        restore = null;
        regions = null;
        restoredMode = null;
    }

    public boolean focus(CameraCaptureSession session, CaptureRequest.Builder builder,
                         CameraCharacteristics cc, Size buffer, float x, float y,
                         Handler handler, Runnable update, java.util.function.BooleanSupplier current) {
        return focus(session, builder, cc, buffer, x, y, handler, update, current, false);
    }

    public boolean focus(CameraCaptureSession session, CaptureRequest.Builder builder,
                         CameraCharacteristics cc, Size buffer, float x, float y,
                         Handler handler, Runnable update, java.util.function.BooleanSupplier current,
                         boolean lock) {
        if (session == null || builder == null || cc == null || buffer == null || !current.getAsBoolean()) return false;
        Rect active = cc.get(CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            Integer distortion = builder.get(CaptureRequest.DISTORTION_CORRECTION_MODE);
            if (distortion != null && distortion == CaptureRequest.DISTORTION_CORRECTION_MODE_OFF) {
                Rect uncorrected = cc.get(CameraCharacteristics.SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE);
                if (uncorrected != null) active = uncorrected;
            }
        }
        if (active == null) return false;
        Rect crop = builder.get(CaptureRequest.SCALER_CROP_REGION);
        if (crop == null) crop = active;
        int[] box = Camera2MeteringGeometry.region(x, y,
                Math.max(active.left, crop.left), Math.max(active.top, crop.top),
                Math.min(active.right, crop.right), Math.min(active.bottom, crop.bottom),
                buffer.getWidth(), buffer.getHeight());
        if (box == null) return false;
        Integer af = cc.get(CameraCharacteristics.CONTROL_MAX_REGIONS_AF);
        Integer ae = cc.get(CameraCharacteristics.CONTROL_MAX_REGIONS_AE);
        int[] modes = cc.get(CameraCharacteristics.CONTROL_AF_AVAILABLE_MODES);
        boolean canAf = contains(modes, CaptureRequest.CONTROL_AF_MODE_AUTO);
        boolean canMeterAf = canAf && af != null && af > 0;
        boolean canMeterAe = ae != null && ae > 0;
        if (!canMeterAf && !canMeterAe) return false;
        cancel();
        autofocus = canMeterAf;
        meteringAf = canMeterAf;
        meteringAe = canMeterAe;
        regions = new MeteringRectangle[]{new MeteringRectangle(
                new Rect(box[0], box[1], box[2], box[3]), MeteringRectangle.METERING_WEIGHT_MAX)};
        int token = generation;
        final int continuousMode = contains(modes, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO)
                ? CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO
                : contains(modes, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE)
                ? CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE
                : canAf ? CaptureRequest.CONTROL_AF_MODE_AUTO : CaptureRequest.CONTROL_AF_MODE_OFF;
        final boolean cancelAf = autofocus;
        try {
            if (autofocus) {
                builder.set(CaptureRequest.CONTROL_AF_TRIGGER, CaptureRequest.CONTROL_AF_TRIGGER_CANCEL);
                session.capture(builder.build(), null, handler);
            }
            apply(builder);
            if (autofocus) {
                builder.set(CaptureRequest.CONTROL_AF_TRIGGER, CaptureRequest.CONTROL_AF_TRIGGER_START);
                session.capture(builder.build(), null, handler);
                builder.set(CaptureRequest.CONTROL_AF_TRIGGER, CaptureRequest.CONTROL_AF_TRIGGER_IDLE);
            }
            update.run();
        } catch (Exception error) {
            cancel();
            FileLog.e(error);
            try { update.run(); } catch (Exception ignored) {}
            return false;
        }
        pendingHandler = handler;
        restore = () -> {
            if (token != generation || !current.getAsBoolean()) return;
            cancel();
            try {
                if (cancelAf) {
                    builder.set(CaptureRequest.CONTROL_AF_TRIGGER, CaptureRequest.CONTROL_AF_TRIGGER_CANCEL);
                    session.capture(builder.build(), null, handler);
                }
                restoredMode = continuousMode;
                builder.set(CaptureRequest.CONTROL_AF_MODE, continuousMode);
                builder.set(CaptureRequest.CONTROL_AF_TRIGGER, CaptureRequest.CONTROL_AF_TRIGGER_IDLE);
                if (canMeterAf) builder.set(CaptureRequest.CONTROL_AF_REGIONS, new MeteringRectangle[0]);
                if (canMeterAe) builder.set(CaptureRequest.CONTROL_AE_REGIONS, new MeteringRectangle[0]);
                update.run();
            } catch (Exception error) {
                FileLog.e(error);
            }
        };
        if (!lock) handler.postDelayed(restore, 3000);
        return true;
    }

    private static boolean contains(int[] modes, int value) {
        if (modes != null) for (int mode : modes) if (mode == value) return true;
        return false;
    }
}

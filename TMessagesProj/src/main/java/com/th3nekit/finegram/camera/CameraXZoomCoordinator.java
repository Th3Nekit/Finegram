                                               

package com.th3nekit.finegram.camera;

import android.os.Looper;
import android.view.Choreographer;

import androidx.annotation.Nullable;
import androidx.camera.core.Camera;
import androidx.camera.core.CameraControl;
import androidx.camera.core.ZoomState;

import com.google.common.util.concurrent.ListenableFuture;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.FileLog;

import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executor;

final class CameraXZoomCoordinator {

    private final String operation;
    @Nullable private final Runnable zoomFailureListener;
    private final Executor mainExecutor = command -> AndroidUtilities.runOnUIThread(command);
    private final Choreographer.FrameCallback frameCallback = frameTimeNanos -> {
        frameCallbackPosted = false;
        dispatchLatest();
    };

    @Nullable private Camera camera;
    private int cameraGeneration;
    private long attachmentToken;
    private long requestVersion;
    private long submissionToken;
    private boolean ready;
    private boolean frameCallbackPosted;
    private float requestedRatio = Float.NaN;
    private float submittedRatio = Float.NaN;
    @Nullable private ListenableFuture<Void> inFlight;

    CameraXZoomCoordinator(String operation) {
        this(operation, null);
    }

    CameraXZoomCoordinator(String operation, @Nullable Runnable zoomFailureListener) {
        this.operation = operation;
        this.zoomFailureListener = zoomFailureListener;
    }

    void attach(@Nullable Camera camera, int generation) {
        detach();
        this.camera = camera;
        cameraGeneration = generation;
    }

    void detach() {
        ++attachmentToken;
        ++submissionToken;
        ready = false;
        camera = null;
        requestedRatio = Float.NaN;
        submittedRatio = Float.NaN;
        inFlight = null;
        if (frameCallbackPosted) {
            frameCallbackPosted = false;
            Runnable removeCallback = () -> {
                try {
                    Choreographer.getInstance().removeFrameCallback(frameCallback);
                } catch (Throwable ignored) {
                }
            };
            if (Looper.myLooper() == Looper.getMainLooper()) {
                removeCallback.run();
            } else {
                AndroidUtilities.runOnUIThread(removeCallback);
            }
        }
    }

    void setReady(Camera expectedCamera, int generation, boolean ready) {
        if (camera != expectedCamera || cameraGeneration != generation) return;
        this.ready = ready;
        if (ready && isFinite(requestedRatio)) scheduleForNextFrame();
    }

    void requestZoomRatio(float ratio) {
        requestZoomRatio(ratio, false);
    }

    void requestAnimatedZoomRatio(float ratio) {
        if (Looper.myLooper() != Looper.getMainLooper()) {
            requestZoomRatio(ratio);
            return;
        }
        requestZoomRatio(ratio, true);
    }

    private void requestZoomRatio(float ratio, boolean animationFrame) {
        Camera targetCamera = camera;
        if (targetCamera == null || !isFinite(ratio)) {
            return;
        }
        try {
            ZoomState state = targetCamera.getCameraInfo().getZoomState().getValue();
            if (state == null) {
                return;
            }
            requestedRatio = clamp(ratio,
                    state.getMinZoomRatio(), state.getMaxZoomRatio());
            ++requestVersion;
            if (ready) {
                if (animationFrame) {

                    if (frameCallbackPosted) {
                        Choreographer.getInstance().removeFrameCallback(frameCallback);
                        frameCallbackPosted = false;
                    }
                    dispatchLatest();
                } else {
                    scheduleForNextFrame();
                }
            }
        } catch (Throwable error) {
            FileLog.e(operation + " state lookup failed", error);
        }
    }

    float getRequestedOr(float fallback) {
        return isFinite(requestedRatio) ? requestedRatio : fallback;
    }

    private void scheduleForNextFrame() {
        if (!ready || camera == null || frameCallbackPosted) return;
        frameCallbackPosted = true;
        final long token = attachmentToken;
        AndroidUtilities.runOnUIThread(() -> {
            if (!frameCallbackPosted || attachmentToken != token) return;
            try {
                Choreographer.getInstance().postFrameCallback(frameCallback);
            } catch (Throwable error) {
                frameCallbackPosted = false;
                dispatchLatest();
            }
        });
    }

    private void dispatchLatest() {
        final Camera targetCamera = camera;
        if (!ready || targetCamera == null || !isFinite(requestedRatio)) {
            return;
        }
        final float targetRatio = requestedRatio;
        if (targetRatio == submittedRatio) {
            return;
        }

        final int generation = cameraGeneration;
        final long token = attachmentToken;
        final long version = requestVersion;
        final long submission = ++submissionToken;
        final ListenableFuture<Void> future;
        try {
            future = targetCamera.getCameraControl().setZoomRatio(targetRatio);
            if (token != attachmentToken || targetCamera != camera
                    || generation != cameraGeneration || submission != submissionToken) return;
            submittedRatio = targetRatio;
            inFlight = future;
        } catch (Throwable error) {
            FileLog.e(operation + " submit failed", error);
            handleZoomFailure(targetCamera, generation, token, version, submission);
            return;
        }

        future.addListener(() -> {
            if (token != attachmentToken || targetCamera != camera
                    || generation != cameraGeneration || submission != submissionToken
                    || future != inFlight) {
                return;
            }
            inFlight = null;
            try {
                future.get();
            } catch (Throwable error) {
                Throwable cause = error instanceof ExecutionException
                        && error.getCause() != null ? error.getCause() : error;
                if (!(cause instanceof CancellationException)
                        && !(cause instanceof CameraControl.OperationCanceledException)) {
                    FileLog.e(operation + " failed", cause);
                }
                handleZoomFailure(targetCamera, generation, token, version, submission);
                return;
            }
            if (requestedRatio == targetRatio) {
                requestedRatio = Float.NaN;
                submittedRatio = Float.NaN;
            } else {
                scheduleForNextFrame();
            }
        }, mainExecutor);
    }

    private void handleZoomFailure(Camera targetCamera, int generation, long token,
                                   long version, long submission) {
        if (token != attachmentToken || targetCamera != camera
                || generation != cameraGeneration || submission != submissionToken) return;
        submittedRatio = Float.NaN;
        inFlight = null;
        if (version != requestVersion) {
            scheduleForNextFrame();
            return;
        }
        requestedRatio = Float.NaN;
        if (zoomFailureListener != null) zoomFailureListener.run();
    }

    private static float clamp(float value, float min, float max) {
        return Math.max(min, Math.min(max, value));
    }

    private static boolean isFinite(float value) {
        return !Float.isNaN(value) && !Float.isInfinite(value);
    }
}

                                               

package org.telegram.messenger.camera;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraMetadata;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureFailure;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.Image;
import android.media.ImageReader;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.util.Log;
import android.util.Range;
import android.util.Size;
import android.util.SizeF;
import android.view.Surface;
import android.view.WindowManager;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.FileLog;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.UserConfig;
import org.telegram.messenger.Utilities;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class Camera2Session {

    private volatile boolean isError;                                                                                            
    private volatile boolean isSuccess;
    private volatile boolean isClosed;                                                                                           
    private volatile boolean deviceErrored;                                                                                              
    private volatile int lastErrorCode = -1;

    private org.telegram.messenger.Utilities.Callback<Integer> errorCallback;
    public void whenError(org.telegram.messenger.Utilities.Callback<Integer> cb) {
        if (destroyed || isClosed) {
            return;
        }
        this.errorCallback = cb;
        if (cb != null && isError) nmFireError(lastErrorCode);
    }
    private void nmFireError(int code) {
        if (destroyed || isClosed) {
            return;
        }
        AndroidUtilities.runOnUIThread(() -> {
            if (destroyed || isClosed) {
                return;
            }
            org.telegram.messenger.Utilities.Callback<Integer> cb = errorCallback;
            if (cb != null) { errorCallback = null; cb.run(code); }
        });
    }

    private final CameraManager cameraManager;
    private final boolean isFront;
    public volatile String cameraId;
    private volatile com.th3nekit.finegram.camera.Camera2LensCatalog lensCatalog;
    private volatile com.th3nekit.finegram.camera.Camera2LensCatalog.Route activeLens;
    private com.th3nekit.finegram.camera.Camera2LensCatalog.Route rollbackLens;
    private volatile com.th3nekit.finegram.camera.Camera2LensCatalog.Route failedLens;
    private volatile boolean lensSwitching;
    private boolean lensOpenAfterClose;
    private float workingZoom = 1f;
    private float rollbackZoom = 1f;
    private final com.th3nekit.finegram.camera.Camera2FocusController pointFocus =
            new com.th3nekit.finegram.camera.Camera2FocusController();
    private CameraCharacteristics cameraCharacteristics;

    private HandlerThread thread;
    private Handler handler;

    private volatile CameraDevice cameraDevice;
    private CameraDevice closingCameraDevice;
    private volatile boolean openPending;
    private SurfaceTexture surfaceTexture;
    private volatile CameraCaptureSession captureSession;
    private Surface surface;

    private final CameraDevice.StateCallback cameraStateCallback;
    private volatile int captureSessionGeneration;
    private final com.th3nekit.finegram.camera.Camera2LensFrameTracker frameTracker =
            new com.th3nekit.finegram.camera.Camera2LensFrameTracker();
    private CameraCaptureSession.CaptureCallback repeatingCaptureCallback;
    private long rearRouteStartedNanos = System.nanoTime();
    private CaptureRequest.Builder captureRequestBuilder;
    private Rect sensorSize;
    private float maxZoom = 1f;
    private float minZoom = 1f;
    private volatile float currentZoom = 1f;
    private float[] zoomShortcuts = new float[]{1f};

    private boolean zoomRatioSupported = false;

    private final Size previewSize;

    private ImageReader imageReader;

    private long lastTime;

    public static Camera2Session create(boolean front, int viewWidth, int viewHeight) {
        return create(front, viewWidth, viewHeight, true);
    }

    public static Camera2Session create(boolean front, int viewWidth, int viewHeight, boolean preferLogical) {

        return create(front, viewWidth, viewHeight, preferLogical, com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getCameraResolution());
    }

    public static Camera2Session create(boolean front, int viewWidth, int viewHeight, boolean preferLogical, int requestedHeight) {

        return create(front, viewWidth, viewHeight, preferLogical, requestedHeight, false);
    }

    public static Camera2Session create(boolean front, int viewWidth, int viewHeight, boolean preferLogical, int requestedHeight, boolean noStillSurface) {
        return create(front, viewWidth, viewHeight, preferLogical, requestedHeight, noStillSurface, null);
    }

    public static Camera2Session create(boolean front, int viewWidth, int viewHeight, boolean preferLogical, int requestedHeight, boolean noStillSurface, String excludedCameraId) {
        final Context context = ApplicationLoader.applicationContext;
        final CameraManager cameraManager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
        com.th3nekit.finegram.core.crash.CameraDiagnostics.record("camera2.select.attempt",
                "engine=camera2 front=" + front + " prefer_logical=" + preferLogical
                        + " excluded=" + (excludedCameraId != null));

        float bestAspectRatio = 0;
        Size bestSize = null;
        String cameraId = null;
        boolean selectedLogical = false;
        String manualId = "";
        try {
            String[] cameraIds = cameraManager.getCameraIdList();
            if (excludedCameraId != null) {
                ArrayList<String> availableIds = new ArrayList<>(Arrays.asList(cameraIds));
                availableIds.remove(excludedCameraId);
                cameraIds = availableIds.toArray(new String[0]);
            }
            final int wantFacing = front ? CameraCharacteristics.LENS_FACING_FRONT
                                         : CameraCharacteristics.LENS_FACING_BACK;
            manualId = com.th3nekit.finegram.camera.CameraIds.validated(cameraManager, front);
            if (!front && preferLogical && noStillSurface && excludedCameraId == null && !manualId.isEmpty()
                    && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getSmoothCameraModuleTransitions()) {
                String wideId = com.th3nekit.finegram.camera.CameraIds.validatedUltraWide(cameraManager);
                for (String logicalId : com.th3nekit.finegram.camera.CameraIds.compatibleLogicalRearIds(cameraManager, manualId, wideId)) {
                    try {
                        Size size = logicalRoundPreviewSize(cameraManager, manualId, wideId, logicalId,
                                viewWidth, viewHeight, requestedHeight);
                        if (size != null) {
                            return new Camera2Session(context, false, logicalId, size, true, true, true, true);
                        }
                    } catch (Exception error) {
                        FileLog.e(error);
                    }
                }
            }
            if (!Arrays.asList(cameraIds).contains(manualId)) {
                manualId = "";
            }
            if (!manualId.isEmpty()) {
                cameraIds = new String[]{manualId};
            }

            String logicalId = null;
            float logicalSpan = -1f;
            for (String id : preferLogical || !manualId.isEmpty() ? cameraIds : new String[0]) {
                CameraCharacteristics cc = cameraManager.getCameraCharacteristics(id);
                if (cc == null) continue;
                Integer facing = cc.get(CameraCharacteristics.LENS_FACING);
                if (facing == null || facing != wantFacing) continue;
                int[] caps = cc.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
                boolean logical = false;
                if (caps != null) {
                    for (int c : caps) {
                        if (c == CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA) {
                            logical = true;
                            break;
                        }
                    }
                }
                if (!logical) continue;
                float span = 1f;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    try {
                        Range<Float> zr = cc.get(CameraCharacteristics.CONTROL_ZOOM_RATIO_RANGE);
                        if (zr != null) span = zr.getUpper() - zr.getLower();
                    } catch (Throwable ignored) {}
                }
                if (span > logicalSpan) { logicalSpan = span; logicalId = id; }
            }

            String primaryPhysicalId = null;
            long primaryPhysicalArea = -1L;
            String anyPhysicalId = null;
            long anyPhysicalArea = -1L;
            if (logicalId == null) {
                for (String candidateId : cameraIds) {
                    CameraCharacteristics cc = cameraManager.getCameraCharacteristics(candidateId);
                    if (cc == null) continue;
                    if (cc.get(CameraCharacteristics.LENS_FACING) == null
                            || cc.get(CameraCharacteristics.LENS_FACING) != wantFacing) continue;
                    int[] caps = cc.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
                    boolean logical = false;
                    if (caps != null) for (int c : caps) if (c == CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA) { logical = true; break; }
                    if (logical) continue;
                    Size pixels = cc.get(CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE);
                    long area = pixels == null ? 0L : (long) pixels.getWidth() * pixels.getHeight();
                    float[] focals = cc.get(CameraCharacteristics.LENS_INFO_AVAILABLE_FOCAL_LENGTHS);
                    float lensMin = Float.MAX_VALUE;
                    if (focals != null) for (float f : focals) if (f > 0f && f < lensMin) lensMin = f;
                    if (area > anyPhysicalArea) {
                        anyPhysicalArea = area;
                        anyPhysicalId = candidateId;
                    }
                    if (lensMin >= 3f && lensMin <= 10f && area > primaryPhysicalArea) {
                        primaryPhysicalArea = area;
                        primaryPhysicalId = candidateId;
                    }
                }
                if (primaryPhysicalId == null) primaryPhysicalId = anyPhysicalId;
            }
            for (int i = 0; i < cameraIds.length; ++i) {
                final String id = cameraIds[i];

                if (logicalId != null && !logicalId.equals(id)) continue;
                CameraCharacteristics characteristics = cameraManager.getCameraCharacteristics(id);
                if (characteristics == null) continue;
                if (characteristics.get(CameraCharacteristics.LENS_FACING) == null
                        || characteristics.get(CameraCharacteristics.LENS_FACING) != wantFacing) {
                    continue;
                }

                if (!preferLogical && manualId.isEmpty()) {
                    int[] mcaps = characteristics.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
                    boolean mLogical = false;
                    if (mcaps != null) {
                        for (int c : mcaps) {
                            if (c == CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA) { mLogical = true; break; }
                        }
                    }
                    if (mLogical) continue;
                }
                if (logicalId == null && primaryPhysicalId != null && !primaryPhysicalId.equals(id)) continue;
                StreamConfigurationMap confMap = (StreamConfigurationMap) characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
                Size pixelSize = characteristics.get(CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE);
                float cameraAspectRatio = pixelSize == null ? 0 : (float) pixelSize.getWidth() / pixelSize.getHeight();
                if ((viewWidth / (float) viewHeight >= 1f) != (cameraAspectRatio >= 1f)) {
                    cameraAspectRatio = 1f / cameraAspectRatio;
                }
                if (bestAspectRatio <= 0 || Math.abs((float) viewWidth / viewHeight - bestAspectRatio) > Math.abs((float) viewWidth / viewHeight - cameraAspectRatio)) {
                    if (confMap != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                        Size[] outputs = confMap.getOutputSizes(SurfaceTexture.class);
                        Size size = chooseQualityAwareSize(outputs, viewWidth, viewHeight, requestedHeight);
                        size = chooseRoundVideoCompatibilitySize(Build.MANUFACTURER, Build.MODEL,
                                front, logicalId != null && logicalId.equals(id), noStillSurface,
                                size, outputs);
                        if (size != null) {
                            bestAspectRatio = cameraAspectRatio;
                            cameraId = id;
                            bestSize = size;
                            selectedLogical = (logicalId != null && logicalId.equals(id));
                        }
                    }
                }
            }
        } catch (Exception e) {
            com.th3nekit.finegram.core.crash.CameraDiagnostics.record("camera2.select.error",
                    "front=" + front + " error=" + e.getClass().getSimpleName());
            FileLog.e(e);
        }

        if (cameraId == null || bestSize == null) {
            if (!manualId.isEmpty()) {
                com.th3nekit.finegram.core.crash.CameraDiagnostics.record("camera2.select.fallback",
                        "front=" + front + " route=automatic reason=manual_unavailable");
                return create(front, viewWidth, viewHeight, preferLogical, requestedHeight, noStillSurface, manualId);
            }
            com.th3nekit.finegram.core.crash.CameraDiagnostics.record("camera2.select.result",
                    "front=" + front + " success=false reason=no_preview_size");
            return null;
        }
        return new Camera2Session(context, front, cameraId, bestSize, selectedLogical, noStillSurface, !manualId.isEmpty(), false);
    }

    private static Size logicalRoundPreviewSize(CameraManager manager, String mainId, String wideId,
            String logicalId, int viewWidth, int viewHeight, int requestedHeight) throws Exception {
        CameraCharacteristics main = manager.getCameraCharacteristics(mainId);
        StreamConfigurationMap map = main.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        if (map == null) return null;
        Size size = chooseQualityAwareSize(map.getOutputSizes(SurfaceTexture.class), viewWidth, viewHeight, requestedHeight);
        if (size == null) return null;
        CameraCharacteristics logical = manager.getCameraCharacteristics(logicalId);
        if (!preservesPreviewQuality(main, logical, size)
                || !preservesPreviewQuality(main, manager.getCameraCharacteristics(wideId), size)) return null;
        StreamConfigurationMap logicalMap = logical.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        Size compatibleSize = chooseRoundVideoCompatibilitySize(Build.MANUFACTURER, Build.MODEL,
                false, true, true, size, logicalMap.getOutputSizes(SurfaceTexture.class));
        return size.equals(compatibleSize) ? size : null;
    }

    private static boolean preservesPreviewQuality(CameraCharacteristics main, CameraCharacteristics candidate, Size size) {
        StreamConfigurationMap mainMap = main.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        StreamConfigurationMap map = candidate.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        if (mainMap == null || map == null) return false;
        Size[] sizes = map.getOutputSizes(SurfaceTexture.class);
        if (sizes == null || !Arrays.asList(sizes).contains(size)) return false;
        long duration = mainMap.getOutputMinFrameDuration(SurfaceTexture.class, size);
        long candidateDuration = map.getOutputMinFrameDuration(SurfaceTexture.class, size);
        if (duration <= 0 || candidateDuration <= 0 || candidateDuration > duration) return false;
        Range<Integer>[] mainFps = main.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
        Range<Integer>[] candidateFps = candidate.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
        if (mainFps == null || mainFps.length == 0 || candidateFps == null || candidateFps.length == 0) return false;
        Range<Integer> requested = com.th3nekit.finegram.camera.CameraXUtils.getTargetFpsRange();
        if (requested == null && !com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getCameraCompatibility()) {
            requested = new Range<>(30, 60);
        }
        Range<Integer> baseline = com.th3nekit.finegram.camera.CameraXUtils.chooseSupportedFpsRange(requested, Arrays.asList(mainFps));
        Range<Integer> supported = com.th3nekit.finegram.camera.CameraXUtils.chooseSupportedFpsRange(requested, Arrays.asList(candidateFps));
        if (!java.util.Objects.equals(baseline, supported)) return false;
        if (baseline != null) return true;
        return maximumFps(mainFps) > 0 && maximumFps(candidateFps) >= maximumFps(mainFps);
    }

    private static int maximumFps(Range<Integer>[] ranges) {
        int maximum = 0;
        for (Range<Integer> range : ranges) {
            if (range != null) maximum = Math.max(maximum, range.getUpper());
        }
        return maximum;
    }

    private boolean nmIsLogical;

    public boolean isLogical() { return nmIsLogical; }

    private final boolean manualCamera;
    private final boolean compatibility;

    public boolean isManualCamera() { return manualCamera; }

    private Camera2Session(Context context, boolean isFront, String cameraId, Size size, boolean logicalCamera, boolean noStillSurface, boolean manualCamera, boolean preferLogicalZoom) {
        thread = new HandlerThread("tg_camera2");
        thread.start();
        handler = new Handler(thread.getLooper());

        cameraStateCallback = new CameraDevice.StateCallback() {
            @Override
            public void onOpened(@NonNull CameraDevice camera) {
                openPending = false;
                if (destroyed) {
                    closingCameraDevice = camera;
                    try { camera.close(); } catch (Throwable ignored) {}
                    return;
                }
                Camera2Session.this.cameraDevice = camera;
                Camera2Session.this.lastTime = System.currentTimeMillis();
                recordCameraDiagnostics("camera2.open.result", "success=true");
                recordRearRouteStage("open", captureSessionGeneration);
                FileLog.d("Camera2Session camera #" + cameraId + " opened");
                checkOpen();
            }

            @Override
            public void onDisconnected(@NonNull CameraDevice camera) {
                openPending = false;
                if (closingCameraDevice == camera) return;
                if (lensSwitching && rollbackLens != null) cameraDevice = camera;
                recordCameraDiagnostics("camera2.lifecycle.error", "stage=disconnected code=-1");
                if (recoverLens("camera disconnected")) return;
                deviceErrored = true;
                lastErrorCode = -1;
                FileLog.d("Camera2Session camera #" + cameraId + " disconnected");
                closingCameraDevice = camera;
                try { camera.close(); } catch (Throwable ignored) {}
                Camera2Session.this.cameraDevice = null;
                if (!destroyed) {
                    AndroidUtilities.runOnUIThread(() -> {
                        isError = true;
                        nmFireError(lastErrorCode);
                    });
                }
            }

            @Override
            public void onError(@NonNull CameraDevice camera, int error) {
                openPending = false;
                if (closingCameraDevice == camera) return;
                if (lensSwitching && rollbackLens != null) cameraDevice = camera;
                recordCameraDiagnostics("camera2.lifecycle.error", "stage=device code=" + error);
                if (recoverLens("camera error " + error)) return;
                closingCameraDevice = camera;
                Camera2Session.this.cameraDevice = null;
                deviceErrored = true;                                                                         
                lastErrorCode = error;
                FileLog.e("Camera2Session camera #" + cameraId + " received " + error + " error");
                if (!destroyed) {
                    AndroidUtilities.runOnUIThread(() -> {
                        isError = true;
                        nmFireError(error);
                    });
                }
                try { camera.close(); } catch (Throwable ignored) {}
            }

            @Override
            public void onClosed(@NonNull CameraDevice camera) {
                openPending = false;
                if (cameraDevice == camera) cameraDevice = null;
                if (closingCameraDevice == camera) closingCameraDevice = null;
                if (destroyed) completeDestroyOnHandler();
                else if (lensOpenAfterClose && !isClosed) {
                    lensOpenAfterClose = false;
                    openCameraDevice();
                }
            }
        };

        this.isFront = isFront;
        this.manualCamera = manualCamera;
        this.compatibility = com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getCameraCompatibility();
        this.cameraId = cameraId;
        this.nmIsLogical = logicalCamera;
        this.previewSize = size;
        this.lastTime = System.currentTimeMillis();

        this.imageReader = noStillSurface ? null : ImageReader.newInstance(size.getWidth(), size.getHeight(), ImageFormat.JPEG, 1);
        cameraManager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
        recordCameraDiagnostics("camera2.select.result",
                "success=true logical=" + logicalCamera + " manual=" + manualCamera
                        + " no_still=" + noStillSurface);
        try {
            cameraCharacteristics = cameraManager.getCameraCharacteristics(cameraId);
            sensorSize = cameraCharacteristics.get(CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE);
            final Float value = cameraCharacteristics.get(CameraCharacteristics.SCALER_AVAILABLE_MAX_DIGITAL_ZOOM);
            maxZoom = (value == null || value < 1f) ? 1f : value;
            minZoom = 1f;
            zoomRatioSupported = false;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                try {
                    Range<Float> zr = cameraCharacteristics.get(
                            CameraCharacteristics.CONTROL_ZOOM_RATIO_RANGE);
                    if (zr != null && zr.getUpper() > zr.getLower()) {
                        zoomRatioSupported = true;
                        minZoom = zr.getLower();
                        maxZoom = zr.getUpper();
                    }
                } catch (Throwable ignored) {}
            }
            lensCatalog = new com.th3nekit.finegram.camera.Camera2LensCatalog(
                    cameraManager, cameraId, cameraCharacteristics, isFront, preferLogicalZoom);
            activeLens = lensCatalog.main;
            minZoom = lensCatalog.minimum;
            maxZoom = lensCatalog.maximum;
            zoomShortcuts = lensCatalog.shortcuts(maxZoom);
            handler.post(this::openCameraDevice);
        } catch (Exception e) {
            openPending = false;
            recordCameraDiagnostics("camera2.lifecycle.error",
                    "stage=initialize error=" + e.getClass().getSimpleName());
            FileLog.e(e);
            deviceErrored = true;
            lastErrorCode = -1;
            AndroidUtilities.runOnUIThread(() -> {
                isError = true;
                nmFireError(lastErrorCode);
            });
        }
    }

    private void openCameraDevice() {
        if (destroyed || isClosed) {
            return;
        }
        openPending = true;
        try {
            recordCameraDiagnostics("camera2.open.attempt", "switching=" + lensSwitching);
            cameraManager.openCamera(cameraId, cameraStateCallback, handler);
        } catch (Exception e) {
            openPending = false;
            recordCameraDiagnostics("camera2.open.result",
                    "success=false error=" + e.getClass().getSimpleName());
            FileLog.e(e);
            if (destroyed || isClosed) {
                closeResourcesForDestroy();
                return;
            }
            if (recoverLens("open rejected: " + e)) return;
            deviceErrored = true;
            lastErrorCode = -1;
            AndroidUtilities.runOnUIThread(() -> {
                if (destroyed || isClosed) {
                    return;
                }
                isError = true;
                nmFireError(lastErrorCode);
            });
        }
    }

    private Runnable doneCallback;
    public void whenDone(Runnable doneCallback) {
        if (destroyed || isClosed) {
            return;
        }
        if (isInitiated()) {
            doneCallback.run();
            this.doneCallback = null;
        } else {
            this.doneCallback = doneCallback;
        }
    }

    public void open(SurfaceTexture surfaceTexture) {
        if (destroyed || isClosed) {
            return;
        }
        handler.post(() -> {
            if (destroyed || isClosed) {
                return;
            }
            this.surfaceTexture = surfaceTexture;
            if (surfaceTexture != null) {
                surfaceTexture.setDefaultBufferSize(getPreviewWidth(), getPreviewHeight());
            }
            checkOpen();
        });
    }

    private boolean opened = false;
    private void checkOpen() {
        if (opened || destroyed || isClosed) return;
        if (surfaceTexture == null || cameraDevice == null) return;

        if (isError || deviceErrored) {
            nmFireError(lastErrorCode); return;
        }
        opened = true;

        if (surface == null) surface = new Surface(surfaceTexture);

        try {
            ArrayList<Surface> surfaces = new ArrayList<>();
            surfaces.add(surface);
            if (imageReader != null) {                                                                                          
                surfaces.add(imageReader.getSurface());
            }
            final CameraDevice expectedDevice = cameraDevice;
            final int generation = advanceCaptureSessionGeneration();
            recordCameraDiagnostics("camera2.bind.attempt", "switching=" + lensSwitching);
            com.th3nekit.finegram.camera.Camera2LensSessions.create(expectedDevice, surfaces,
                    activeLens, createCaptureStateCallback(expectedDevice, generation), handler);
        } catch (Exception e) {
            recordCameraDiagnostics("camera2.bind.result",
                    "success=false stage=create error=" + e.getClass().getSimpleName());
            FileLog.e(e);
            if (recoverLens("session creation rejected: " + e)) return;
            deviceErrored = true;
            lastErrorCode = -1;
            AndroidUtilities.runOnUIThread(() -> {
                isError = true;
                nmFireError(lastErrorCode);
            });
        }
    }

    private CameraCaptureSession.StateCallback createCaptureStateCallback(
            final CameraDevice expectedDevice, final int generation) {
        return new CameraCaptureSession.StateCallback() {
            private boolean isCurrent(@NonNull CameraCaptureSession session) {
                return !destroyed
                        && !isClosed
                        && generation == captureSessionGeneration
                        && cameraDevice == expectedDevice
                        && (captureSession == null || captureSession == session);
            }

            private void closeStale(@NonNull CameraCaptureSession session) {
                try {
                    session.close();
                } catch (Throwable ignored) {
                }
            }

            @Override
            public void onConfigured(@NonNull CameraCaptureSession session) {
                boolean current;
                synchronized (frameTracker) {
                    current = isCurrent(session);
                    if (current) {
                        captureSession = session;
                        frameTracker.beginSession(session, generation);
                        repeatingCaptureCallback = createRepeatingCaptureCallback(session, generation);
                    }
                }
                if (!current) {
                    closeStale(session);
                    return;
                }
                recordRearRouteStage("configured", generation);
                FileLog.e("Camera2Session camera #" + cameraId + " capture session configured");
                Camera2Session.this.lastTime = System.currentTimeMillis();
                try {
                    if (!updateCaptureRequest()) {
                        if (!isCurrent(session)) {
                            closeStale(session);
                            return;
                        }
                        recordCameraDiagnostics("camera2.bind.result",
                                "success=false stage=repeating_rejected");
                        if (recoverLens("repeating request rejected")) return;
                        deviceErrored = true;
                        lastErrorCode = -1;
                        AndroidUtilities.runOnUIThread(() -> {
                            if (isCurrent(session)) {
                                isError = true;
                                nmFireError(lastErrorCode);
                            }
                        });
                        return;
                    }
                    recordCameraDiagnostics("camera2.bind.result", "success=true stage=repeating");
                    workingZoom = currentZoom;
                    boolean switchedLens = lensSwitching;
                    lensSwitching = false;
                    rollbackLens = null;
                    if (switchedLens) {
                        recordCameraDiagnostics("camera2.switch.complete", "stage=repeating elapsed_ms="
                                + ((System.nanoTime() - rearRouteStartedNanos) / 1_000_000L));
                    }
                    if (switchedLens) handler.post(Camera2Session.this::applyRequestedZoom);
                    AndroidUtilities.runOnUIThread(() -> {
                        if (!isCurrent(session)) {
                            return;
                        }
                        if (!isSuccess) {
                            recordCameraDiagnostics("camera2.ready", "stage=repeating elapsed_ms="
                                    + ((System.nanoTime() - rearRouteStartedNanos) / 1_000_000L));
                        }
                        isSuccess = true;
                        if (doneCallback != null) {
                            doneCallback.run();
                            doneCallback = null;
                        }
                    });
                } catch (Exception e) {
                    recordCameraDiagnostics("camera2.bind.result",
                            "success=false stage=configure error=" + e.getClass().getSimpleName());
                    FileLog.e(e);
                }
            }

            @Override
            public void onConfigureFailed(@NonNull CameraCaptureSession session) {
                if (!isCurrent(session)) {
                    closeStale(session);
                    return;
                }
                recordCameraDiagnostics("camera2.bind.result", "success=false stage=configure_failed");
                if (recoverLens("capture session configuration failed")) {
                    closeStale(session);
                    return;
                }
                deviceErrored = true;
                lastErrorCode = -1;
                FileLog.e("Camera2Session camera #" + cameraId + " capture session failed to configure");
                closeStale(session);
                AndroidUtilities.runOnUIThread(() -> {
                    if (generation == captureSessionGeneration
                            && !destroyed && !isClosed && cameraDevice == expectedDevice) {
                        isError = true;
                        nmFireError(-1);
                    }
                });
            }
        };
    }

    public int getCaptureSessionGeneration() {
        return captureSessionGeneration;
    }

    public int getFrameGeneration(long surfaceTimestampNanos) {
        synchronized (frameTracker) {
            if (destroyed || isClosed || cameraDevice == null || deviceErrored || isError) return -1;
            return frameTracker.getFrameGeneration(captureSession, captureSessionGeneration, surfaceTimestampNanos);
        }
    }

    public boolean hasCurrentDesiredFrame(long surfaceTimestampNanos) {
        synchronized (frameTracker) {
            return getFrameGeneration(surfaceTimestampNanos) >= 0 && !willSwitchLens(currentZoom);
        }
    }

    private int advanceCaptureSessionGeneration() {
        synchronized (frameTracker) {
            int generation = ++captureSessionGeneration;
            frameTracker.invalidate(generation);
            repeatingCaptureCallback = null;
            return generation;
        }
    }

    private CameraCaptureSession.CaptureCallback createRepeatingCaptureCallback(
            final CameraCaptureSession expectedSession, final int generation) {
        return new CameraCaptureSession.CaptureCallback() {
            @Override
            public void onCaptureStarted(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request,
                                         long timestamp, long frameNumber) {
                synchronized (frameTracker) {
                    if (destroyed || isClosed || cameraDevice == null || deviceErrored || isError
                            || generation != captureSessionGeneration
                            || session != expectedSession || captureSession != expectedSession) return;
                    if (frameTracker.record(session, generation, timestamp)) {
                        recordRearRouteStage("first_frame", generation);
                    }
                }
            }
        };
    }

    private boolean submitRepeatingRequest(CaptureRequest request) throws android.hardware.camera2.CameraAccessException {
        final CameraCaptureSession session;
        final CameraCaptureSession.CaptureCallback callback;
        synchronized (frameTracker) {
            if (destroyed || isClosed || captureSession == null || repeatingCaptureCallback == null) return false;
            session = captureSession;
            callback = repeatingCaptureCallback;
        }
        session.setRepeatingRequest(request, callback, handler);
        return true;
    }

    private void recordCameraDiagnostics(String event, String state) {
        com.th3nekit.finegram.core.crash.CameraDiagnostics.record(event,
                "engine=camera2 camera=" + cameraId + " front=" + isFront
                        + " generation=" + captureSessionGeneration + " " + state);
    }

    private void recordRearRouteStage(String stage, int generation) {
        if (isFront) return;
        com.th3nekit.finegram.core.crash.CameraDiagnostics.record("camera2.rear.route." + stage,
                "generation=" + generation + " elapsed_ms=" + ((System.nanoTime() - rearRouteStartedNanos) / 1_000_000L));
    }

    public boolean isInitiated() {
        return !isError && isSuccess && !isClosed;
    }

    public int getDisplayOrientation() {
        try {
            Context context = ApplicationLoader.applicationContext;
            if (context == null) {
                return 0;
            }
            int rotation = ((WindowManager) context.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay().getRotation();
            int degrees = 0;
            switch (rotation) {
                case Surface.ROTATION_0:
                    degrees = 0;
                    break;
                case Surface.ROTATION_90:
                    degrees = 90;
                    break;
                case Surface.ROTATION_180:
                    degrees = 180;
                    break;
                case Surface.ROTATION_270:
                    degrees = 270;
                    break;
            }

            Integer sensorOrientation = cameraCharacteristics.get(CameraCharacteristics.SENSOR_ORIENTATION);
            int displayOrientation;
            if (isFront) {
                displayOrientation = (sensorOrientation + degrees) % 360;
                displayOrientation = (360 - displayOrientation) % 360;
            } else {
                displayOrientation = (sensorOrientation - degrees + 360) % 360;
            }
            return displayOrientation;
        } catch (Exception e) {
            FileLog.e(e);
        }
        return 0;
    }

    private int getJpegOrientation() {
        try {
            Context context = ApplicationLoader.applicationContext;
            if (context == null) {
                return 0;
            }
            int rotation = ((WindowManager) context.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay().getRotation();
            int degrees = 0;
            switch (rotation) {
                case Surface.ROTATION_0:
                    degrees = 0;
                    break;
                case Surface.ROTATION_90:
                    degrees = 90;
                    break;
                case Surface.ROTATION_180:
                    degrees = 180;
                    break;
                case Surface.ROTATION_270:
                    degrees = 270;
                    break;
            }

            Integer sensorOrientation = cameraCharacteristics.get(CameraCharacteristics.SENSOR_ORIENTATION);
            int jpegOrientation;
            if (isFront) {
                jpegOrientation = (sensorOrientation + degrees) % 360;
                jpegOrientation = (360 - jpegOrientation) % 360;
            } else {
                jpegOrientation = (sensorOrientation - degrees + 360) % 360;
            }
            return jpegOrientation;
        } catch (Exception e) {
            FileLog.e(e);
        }
        return 0;
    }

    public int getWorldAngle() {
        int displayOrientation = getDisplayOrientation();
        int jpegOrientation = getJpegOrientation();
        int diffOrientation = jpegOrientation - displayOrientation;
        if (diffOrientation < 0) {
            diffOrientation += 360;
        }
        return diffOrientation;
    }

    public int getCurrentOrientation() {
        return getJpegOrientation();
    }

    public boolean willSwitchLens(float targetRatio) {
        if (!Float.isFinite(targetRatio) || !isInitiated() || destroyed || deviceErrored) return false;
        if (cameraDevice == null && !lensSwitching) return false;
        com.th3nekit.finegram.camera.Camera2LensCatalog catalog = lensCatalog;
        com.th3nekit.finegram.camera.Camera2LensCatalog.Route active = activeLens;
        com.th3nekit.finegram.camera.Camera2LensCatalog.Route failed = failedLens;
        if (catalog == null || active == null) return false;
        float ratio = Utilities.clamp(targetRatio, maxZoom, minZoom);
        com.th3nekit.finegram.camera.Camera2LensCatalog.Route target = catalog.resolve(ratio);
        return !target.sameLens(active) && !target.sameLens(failed)
                && com.th3nekit.finegram.camera.Camera2LensSessions.supports(target, previewSize)
                && (imageReader == null
                        || com.th3nekit.finegram.camera.Camera2LensSessions.supportsJpeg(target, previewSize));
    }

    private final Rect cropRegion = new Rect();
    public void setZoom(float value) {
        if (!isInitiated() || !Float.isFinite(value)) return;
        currentZoom = Utilities.clamp(value, maxZoom, minZoom);
        handler.post(this::applyRequestedZoom);
    }

    private void applyRequestedZoom() {
        if (destroyed || isClosed || lensSwitching || cameraDevice == null || lensCatalog == null) return;
        com.th3nekit.finegram.camera.Camera2LensCatalog.Route target = lensCatalog.resolve(currentZoom);
        if (!target.sameLens(activeLens) && !target.sameLens(failedLens)) {
            if (!com.th3nekit.finegram.camera.Camera2LensSessions.supports(target, previewSize)
                    || imageReader != null && !com.th3nekit.finegram.camera.Camera2LensSessions.supportsJpeg(target, previewSize)) {
                recordCameraDiagnostics("camera2.switch.result",
                        "success=false reason=unsupported_streams target=" + target.cameraId);
                FileLog.d("Camera2 lens route unavailable for current streams: " + target.cameraId
                        + "/" + target.physicalId + ", size=" + previewSize);
                failedLens = target;
                currentZoom = workingZoom;
                return;
            }
            rollbackLens = activeLens;
            rollbackZoom = workingZoom;
            switchLens(target);
            return;
        }
        if (target.sameLens(failedLens) && !target.sameLens(activeLens)) {
            currentZoom = workingZoom;
            return;
        }
        if (updateCaptureRequest()) workingZoom = currentZoom;
        else { currentZoom = workingZoom; updateCaptureRequest(); }
    }

    private void switchLens(com.th3nekit.finegram.camera.Camera2LensCatalog.Route target) {
        pointFocus.cancel();
        lensSwitching = true;
        rearRouteStartedNanos = System.nanoTime();
        int generation = advanceCaptureSessionGeneration();
        recordRearRouteStage("request", generation);
        recordRearRouteStage("start", generation);
        opened = false;
        if (captureSession != null) {
            try { captureSession.close(); } catch (Exception error) { FileLog.e(error); }
            captureSession = null;
        }
        captureRequestBuilder = null;
        activeLens = target;
        cameraId = target.cameraId;
        cameraCharacteristics = target.controlCharacteristics;
        sensorSize = cameraCharacteristics.get(CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE);
        zoomRatioSupported = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                && cameraCharacteristics.get(CameraCharacteristics.CONTROL_ZOOM_RATIO_RANGE) != null;
        deviceErrored = false;
        CameraDevice previous = cameraDevice;
        cameraDevice = null;
        recordCameraDiagnostics("camera2.switch.request",
                "physical=" + target.physicalId);
        FileLog.d("Camera2 lens route: device=" + cameraId + ", physical=" + target.physicalId
                + ", ratio=" + currentZoom + ", local=" + target.localZoom(currentZoom));
        if (previous == null) openCameraDevice();
        else {
            closingCameraDevice = previous;
            lensOpenAfterClose = true;
            try { previous.close(); } catch (Exception error) {
                FileLog.e(error);
                lensOpenAfterClose = false;
                openCameraDevice();
            }
        }
    }

    private boolean recoverLens(String reason) {
        if (!lensSwitching || rollbackLens == null || destroyed || isClosed) return false;
        com.th3nekit.finegram.camera.Camera2LensCatalog.Route previous = rollbackLens;
        rollbackLens = null;
        failedLens = activeLens;
        currentZoom = rollbackZoom;
        recordCameraDiagnostics("camera2.bind.fallback",
                "route=previous_lens target=" + previous.cameraId);
        FileLog.d("Camera2 lens route failed, restoring " + previous.cameraId + ": " + reason);
        switchLens(previous);
        return true;
    }

    public boolean focusToPoint(float x, float y) {
        return focusToPoint(x, y, false);
    }

    public boolean focusToPoint(float x, float y, boolean lock) {
        if (!Float.isFinite(x) || !Float.isFinite(y) || x < 0 || x > 1 || y < 0 || y > 1
                || !isInitiated() || destroyed || lensSwitching || captureSession == null) return false;
        final int generation = captureSessionGeneration;
        return handler.post(() -> {
            CameraCaptureSession session = captureSession;
            if (generation != captureSessionGeneration || destroyed || isClosed || lensSwitching) return;
            pointFocus.focus(session, captureRequestBuilder, cameraCharacteristics, previewSize, x, y,
                    handler, this::updateCaptureRequest,
                    () -> !destroyed && !isClosed && !lensSwitching && captureSession == session
                            && generation == captureSessionGeneration, lock);
        });
    }

    private volatile boolean flashing;

    private volatile int flashIntensityPercent = 100;
    private boolean torchStrengthUnsupportedReported;
    public void setFlash(boolean flash) {
        setFlash(flash, flashIntensityPercent);
    }
    public void setFlash(boolean flash, int intensityPercent) {
        if (destroyed || isClosed) return;
        int intensity = Utilities.clamp(intensityPercent, 100, 0);
        boolean changed = (flashing != flash) || (flashIntensityPercent != intensity);
        flashing = flash;
        flashIntensityPercent = intensity;
        if (changed) scheduleFlashUpdate();
    }
    public void setFlashIntensity(int intensityPercent) {
        setFlash(flashing, intensityPercent);
    }
    public boolean getFlash() {
        return flashing;
    }

    private void scheduleFlashUpdate() {
        com.th3nekit.finegram.core.crash.CameraDiagnostics.record("camera2.torch",
                "engine=camera2 cameraId=" + cameraId + " torch=" + flashing
                        + " intensity=" + flashIntensityPercent + " stage=request result=queued");
        handler.post(() -> {
            if (destroyed || isClosed) return;
            boolean submitted = updateCaptureRequest();
            com.th3nekit.finegram.core.crash.CameraDiagnostics.record("camera2.torch",
                    "engine=camera2 cameraId=" + cameraId + " torch=" + flashing
                            + " stage=capture result=" + (submitted ? "submitted" : "deferred"));
        });
    }

    private void applyTorchStrength() {
        if (!flashing || flashIntensityPercent <= 0 || !recordingVideo || isFront) return;
        try {
            if (Build.VERSION.SDK_INT >= 35 && cameraCharacteristics != null) {
                Integer max = cameraCharacteristics.get(CameraCharacteristics.FLASH_TORCH_STRENGTH_MAX_LEVEL);
                if (max != null && max > 1
                        && cameraCharacteristics.getAvailableCaptureRequestKeys() != null
                        && cameraCharacteristics.getAvailableCaptureRequestKeys().contains(CaptureRequest.FLASH_STRENGTH_LEVEL)) {
                    int strength = Utilities.clamp(Math.round(flashIntensityPercent / 100f * max), max, 1);
                    captureRequestBuilder.set(CaptureRequest.FLASH_STRENGTH_LEVEL, strength);
                    return;
                }
            }
            if (flashIntensityPercent != 100 && !torchStrengthUnsupportedReported) {
                torchStrengthUnsupportedReported = true;
                com.th3nekit.finegram.core.crash.CameraDiagnostics.record("camera2.torch",
                        "engine=camera2 cameraId=" + cameraId + " torch=true stage=strength result=default_only");
            }
        } catch (Throwable t) {
            com.th3nekit.finegram.core.crash.CameraDiagnostics.record("camera2.torch",
                    "engine=camera2 cameraId=" + cameraId + " stage=strength result=" + t.getClass().getSimpleName());
            FileLog.e("Camera2Session capture torch strength failed", t);
        }
    }

    public float getZoom() {
        return currentZoom;
    }

    private int currentEvIndex = 0;
    public boolean isExposureCompensationSupported() {
        if (cameraCharacteristics == null) return false;
        try {
            Range<Integer> r = cameraCharacteristics.get(CameraCharacteristics.CONTROL_AE_COMPENSATION_RANGE);
            return r != null && (r.getUpper() - r.getLower()) > 0;
        } catch (Throwable t) {
            return false;
        }
    }
    public void setExposureCompensation(float value0to1) {
        if (!isInitiated()) return;
        if (captureRequestBuilder == null || cameraDevice == null || captureSession == null) return;
        try {
            Range<Integer> r = cameraCharacteristics.get(CameraCharacteristics.CONTROL_AE_COMPENSATION_RANGE);
            if (r == null) return;
            int lo = r.getLower();
            int hi = r.getUpper();
            int span = hi - lo;
            if (span <= 0) return;
            float v = value0to1;
            if (v < 0f) v = 0f; else if (v > 1f) v = 1f;
            int idx = lo + Math.round(span * v);
            currentEvIndex = idx;
            captureRequestBuilder.set(CaptureRequest.CONTROL_AE_EXPOSURE_COMPENSATION, idx);
            submitRepeatingRequest(captureRequestBuilder.build());
        } catch (Throwable t) {
            FileLog.e("Camera2Session setExposureCompensation failed", t);
        }
    }
    public int getExposureCompensationIndex() {
        return currentEvIndex;
    }

    public float getMaxZoom() {
        return maxZoom;
    }

    public float getMinZoom() {

        return minZoom;
    }

    public float[] getZoomShortcuts() {
        return zoomShortcuts.clone();
    }

    private float[] loadZoomShortcuts() {
        return com.th3nekit.finegram.camera.Camera2ZoomShortcuts.load(
                cameraManager, cameraCharacteristics, isFront, minZoom, maxZoom);
    }

    public int getPreviewWidth() {
        return previewSize.getWidth();
    }

    public int getPreviewHeight() {
        return previewSize.getHeight();
    }

    public void destroy(boolean async) {
        destroy(async, null);
    }

    private volatile boolean destroyed;
    private boolean destroyComplete;
    private boolean destroyCompletionStarted;
    private final ArrayList<Runnable> destroyCallbacks = new ArrayList<>();

    public void destroy(boolean async, Runnable afterCallback) {
        isClosed = true;
        synchronized (destroyCallbacks) {
            if (destroyComplete) {
                if (afterCallback != null) AndroidUtilities.runOnUIThread(afterCallback);
                return;
            }
            if (afterCallback != null) destroyCallbacks.add(afterCallback);
            if (destroyed) return;
            destroyed = true;
            advanceCaptureSessionGeneration();
            errorCallback = null;
            doneCallback = null;
        }

        if (Looper.myLooper() == thread.getLooper()) {
            closeResourcesForDestroy();
        } else {
            handler.post(this::closeResourcesForDestroy);
        }
    }

    private void closeResourcesForDestroy() {
        pointFocus.cancel();
        lensOpenAfterClose = false;
        rollbackLens = null;
        CameraCaptureSession session = captureSession;
        captureSession = null;
        if (session != null) {
            try {
                session.close();
            } catch (Throwable error) {
                FileLog.e(error);
            }
        }
        ImageReader reader = imageReader;
        imageReader = null;
        if (reader != null) {
            try {
                reader.close();
            } catch (Throwable error) {
                FileLog.e(error);
            }
        }
        Surface previewSurface = surface;
        surface = null;
        surfaceTexture = null;
        if (previewSurface != null) {
            try {
                previewSurface.release();
            } catch (Throwable error) {
                FileLog.e(error);
            }
        }
        CameraDevice device = cameraDevice != null ? cameraDevice : closingCameraDevice;
        cameraDevice = null;
        if (device != null) {
            closingCameraDevice = device;
            try { device.close(); } catch (Throwable ignored) {}
        } else if (openPending) {

        } else {
            completeDestroyOnHandler();
        }
    }

    private void completeDestroyOnHandler() {
        if (destroyCompletionStarted) return;
        destroyCompletionStarted = true;
        thread.quitSafely();
        joinCameraThread(this::dispatchDestroyCallbacks);
    }

    private void dispatchDestroyCallbacks() {
        ArrayList<Runnable> callbacks;
        synchronized (destroyCallbacks) {
            destroyComplete = true;
            callbacks = new ArrayList<>(destroyCallbacks);
            destroyCallbacks.clear();
        }
        for (Runnable callback : callbacks) {
            try {
                callback.run();
            } catch (Throwable e) {
                FileLog.e(e);
            }
        }
    }

    private void joinCameraThread(Runnable afterCallback) {
        Runnable join = () -> {
            try {
                if (Thread.currentThread() != thread) thread.join();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            } catch (Throwable e) {
                FileLog.e(e);
            }
            if (afterCallback != null) AndroidUtilities.runOnUIThread(afterCallback);
        };
        if (Looper.myLooper() == Looper.getMainLooper() || Thread.currentThread() == thread) {
            Utilities.globalQueue.postRunnable(join);
        } else {
            join.run();
        }
    }

    private boolean recordingVideo;
    public void setRecordingVideo(boolean recording) {
        if (recordingVideo != recording) {
            recordingVideo = recording;
            updateCaptureRequest();
        }
    }

    private boolean scanningBarcode;
    public void setScanningBarcode(boolean scanning) {
        if (scanningBarcode != scanning) {
            scanningBarcode = scanning;
            updateCaptureRequest();
        }
    }

    private boolean nightMode;
    public void setNightMode(boolean enable) {
        if (nightMode != enable) {
            nightMode = enable;
            updateCaptureRequest();
        }
    }

    private Range<Integer> nmValidateFpsRange(Range<Integer> requested) {
        if (requested == null) return null;
        try {
            CameraCharacteristics cc = cameraCharacteristics;
            if (cc == null) return null;
            Range<Integer>[] ranges = cc.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
            if (ranges == null || ranges.length == 0) return null;

            return com.th3nekit.finegram.camera.CameraXUtils.chooseSupportedFpsRange(
                    requested, java.util.Arrays.asList(ranges));
        } catch (Throwable t) {
            FileLog.e("Camera2Session nmValidateFpsRange failed", t);
            return null;
        }
    }

    private static Integer choosePreviewDistortionMode(int[] supportedModes) {
        if (supportedModes != null) {
            for (int mode : supportedModes) {
                if (mode == CaptureRequest.DISTORTION_CORRECTION_MODE_FAST) {
                    return mode;
                }
            }
        }
        return null;
    }

    private static Integer chooseStabilizationMode(int[] supportedModes, boolean enabled, int on, int off) {
        Integer fallback = null;
        if (supportedModes != null) {
            for (int mode : supportedModes) {
                if (enabled && mode == on) return on;
                if (mode == off) fallback = off;
            }
        }
        return fallback;
    }

    private static Integer[] chooseStabilizationModes(int[] opticalModes, int[] videoModes,
            Integer templateOptical, Integer templateVideo, boolean opticalEnabled, boolean videoEnabled) {
        final int opticalOn = CaptureRequest.LENS_OPTICAL_STABILIZATION_MODE_ON;
        final int opticalOff = CaptureRequest.LENS_OPTICAL_STABILIZATION_MODE_OFF;
        final int videoOn = CaptureRequest.CONTROL_VIDEO_STABILIZATION_MODE_ON;
        final int videoOff = CaptureRequest.CONTROL_VIDEO_STABILIZATION_MODE_OFF;
        Integer optical = chooseStabilizationMode(opticalModes, opticalEnabled, opticalOn, opticalOff);
        Integer disableVideo = chooseStabilizationMode(videoModes, false, videoOn, videoOff);

        if (Integer.valueOf(opticalOn).equals(optical) && disableVideo == null
                && !Integer.valueOf(videoOff).equals(templateVideo)) {
            optical = chooseStabilizationMode(opticalModes, false, opticalOn, opticalOff);
        }
        Integer effectiveOptical = optical != null ? optical : templateOptical;
        Integer video = chooseStabilizationMode(videoModes,
                videoEnabled && Integer.valueOf(opticalOff).equals(effectiveOptical), videoOn, videoOff);

        return new Integer[] { optical, video };
    }

    private void applyStabilizationModes(boolean opticalEnabled, boolean videoEnabled) {
        if (cameraCharacteristics == null) return;
        List<CaptureRequest.Key<?>> requestKeys = cameraCharacteristics.getAvailableCaptureRequestKeys();
        if (requestKeys == null) return;
        int[] opticalModes = requestKeys.contains(CaptureRequest.LENS_OPTICAL_STABILIZATION_MODE)
                ? cameraCharacteristics.get(CameraCharacteristics.LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION) : null;
        int[] videoModes = requestKeys.contains(CaptureRequest.CONTROL_VIDEO_STABILIZATION_MODE)
                ? cameraCharacteristics.get(CameraCharacteristics.CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES) : null;
        Integer[] modes = chooseStabilizationModes(opticalModes, videoModes,
                captureRequestBuilder.get(CaptureRequest.LENS_OPTICAL_STABILIZATION_MODE),
                captureRequestBuilder.get(CaptureRequest.CONTROL_VIDEO_STABILIZATION_MODE),
                opticalEnabled, videoEnabled);

        if (Integer.valueOf(CaptureRequest.LENS_OPTICAL_STABILIZATION_MODE_OFF).equals(modes[0])) {
            captureRequestBuilder.set(CaptureRequest.LENS_OPTICAL_STABILIZATION_MODE, modes[0]);
        }
        if (modes[1] != null) {
            captureRequestBuilder.set(CaptureRequest.CONTROL_VIDEO_STABILIZATION_MODE, modes[1]);
        }
        if (Integer.valueOf(CaptureRequest.LENS_OPTICAL_STABILIZATION_MODE_ON).equals(modes[0])) {
            captureRequestBuilder.set(CaptureRequest.LENS_OPTICAL_STABILIZATION_MODE, modes[0]);
        }
    }

    private static Integer chooseSupportedMode(int[] supportedModes, int... preferredModes) {
        if (supportedModes != null) {
            for (int preferred : preferredModes) {
                for (int supported : supportedModes) {
                    if (preferred == supported) return preferred;
                }
            }
        }
        return null;
    }

    private void applySupportedCaptureModes() {
        if (cameraCharacteristics == null) return;
        List<CaptureRequest.Key<?>> requestKeys = cameraCharacteristics.getAvailableCaptureRequestKeys();
        if (requestKeys == null) return;
        try {
            if (requestKeys.contains(CaptureRequest.CONTROL_AF_MODE)) {
                int[] modes = cameraCharacteristics.get(CameraCharacteristics.CONTROL_AF_AVAILABLE_MODES);
                Integer af = com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getContinuousAutofocus()
                        ? chooseSupportedMode(modes, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO,
                                CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE,
                                CaptureRequest.CONTROL_AF_MODE_AUTO, CaptureRequest.CONTROL_AF_MODE_OFF)
                        : chooseSupportedMode(modes, CaptureRequest.CONTROL_AF_MODE_AUTO, CaptureRequest.CONTROL_AF_MODE_OFF);
                if (af != null) captureRequestBuilder.set(CaptureRequest.CONTROL_AF_MODE, af);
            }
        } catch (Throwable ignored) {}
        try {
            if (requestKeys.contains(CaptureRequest.NOISE_REDUCTION_MODE)) {
                int[] modes = cameraCharacteristics.get(CameraCharacteristics.NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES);
                Integer nr = com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getNoiceReduction()
                        ? chooseSupportedMode(modes, recordingVideo
                                ? CaptureRequest.NOISE_REDUCTION_MODE_HIGH_QUALITY : CaptureRequest.NOISE_REDUCTION_MODE_FAST,
                                CaptureRequest.NOISE_REDUCTION_MODE_FAST, CaptureRequest.NOISE_REDUCTION_MODE_OFF)
                        : chooseSupportedMode(modes, CaptureRequest.NOISE_REDUCTION_MODE_OFF);
                if (nr != null) captureRequestBuilder.set(CaptureRequest.NOISE_REDUCTION_MODE, nr);
            }
        } catch (Throwable ignored) {}
        try {
            if (requestKeys.contains(CaptureRequest.STATISTICS_FACE_DETECT_MODE)) {
                int[] modes = cameraCharacteristics.get(CameraCharacteristics.STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES);
                Integer face = com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getFaceDetection()
                        ? chooseSupportedMode(modes, CaptureRequest.STATISTICS_FACE_DETECT_MODE_SIMPLE,
                                CaptureRequest.STATISTICS_FACE_DETECT_MODE_FULL, CaptureRequest.STATISTICS_FACE_DETECT_MODE_OFF)
                        : chooseSupportedMode(modes, CaptureRequest.STATISTICS_FACE_DETECT_MODE_OFF);
                if (face != null) captureRequestBuilder.set(CaptureRequest.STATISTICS_FACE_DETECT_MODE, face);
            }
        } catch (Throwable ignored) {}
    }

    private boolean updateCaptureRequest() {
        if (destroyed || isClosed || cameraDevice == null || surface == null || captureSession == null) return false;
        try {
            int template;
            if (recordingVideo) {
                template = CameraDevice.TEMPLATE_RECORD;
            } else if (scanningBarcode) {
                template = CameraDevice.TEMPLATE_STILL_CAPTURE;
            } else {
                template = CameraDevice.TEMPLATE_PREVIEW;
            }
            captureRequestBuilder = cameraDevice.createCaptureRequest(template);

            if (scanningBarcode) {
                captureRequestBuilder.set(CaptureRequest.CONTROL_SCENE_MODE, CameraMetadata.CONTROL_SCENE_MODE_BARCODE);
            } else if (nightMode) {
                captureRequestBuilder.set(CaptureRequest.CONTROL_SCENE_MODE, isFront ? CameraMetadata.CONTROL_SCENE_MODE_NIGHT_PORTRAIT : CameraMetadata.CONTROL_SCENE_MODE_NIGHT);
            }

            captureRequestBuilder.set(CaptureRequest.FLASH_MODE, flashing && flashIntensityPercent > 0
                    ? (recordingVideo ? CaptureRequest.FLASH_MODE_TORCH : CaptureRequest.FLASH_MODE_SINGLE)
                    : CaptureRequest.FLASH_MODE_OFF);
            applyTorchStrength();

            if (!scanningBarcode) {
                Range<Integer> requestedFps = com.th3nekit.finegram.camera.CameraXUtils.getTargetFpsRange();
                if (requestedFps == null && recordingVideo && !compatibility) {
                    requestedFps = new Range<>(30, 60);
                }
                Range<Integer> supportedFps = nmValidateFpsRange(requestedFps);
                if (supportedFps != null) {
                    captureRequestBuilder.set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE, supportedFps);
                }
            }
            if (recordingVideo) {
                captureRequestBuilder.set(CaptureRequest.CONTROL_CAPTURE_INTENT, CaptureRequest.CONTROL_CAPTURE_INTENT_VIDEO_RECORD);
            }

            if (!compatibility) {
                try {
                    applyStabilizationModes(com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getOpticalStabilisation(),
                            com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getVideoStabilisation());
                } catch (Throwable ignored) {}

                try {
                    applySupportedCaptureModes();
                } catch (Throwable ignored) {}

                try {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                        Integer distortionMode = choosePreviewDistortionMode(cameraCharacteristics.get(
                                CameraCharacteristics.DISTORTION_CORRECTION_AVAILABLE_MODES));
                        if (distortionMode != null) {
                            captureRequestBuilder.set(CaptureRequest.DISTORTION_CORRECTION_MODE,
                                    distortionMode);
                        }
                    }
                } catch (Throwable ignored) {}

            }

            if (currentEvIndex != 0) {
                try {
                    captureRequestBuilder.set(CaptureRequest.CONTROL_AE_EXPOSURE_COMPENSATION, currentEvIndex);
                } catch (Throwable ignored) {}
            }

            float localZoom = activeLens == null ? currentZoom : activeLens.localZoom(currentZoom);
            if (zoomRatioSupported && Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {

                try {
                    captureRequestBuilder.set(CaptureRequest.CONTROL_ZOOM_RATIO, localZoom);
                } catch (Throwable ignored) {}
            } else if (sensorSize != null && Math.abs(localZoom - 1f) >= 0.01f) {
                final int centerX = sensorSize.centerX();
                final int centerY = sensorSize.centerY();
                final int deltaX = (int) ((0.5f * sensorSize.width()) / localZoom);
                final int deltaY = (int) ((0.5f * sensorSize.height()) / localZoom);
                cropRegion.set(
                        centerX - deltaX,
                        centerY - deltaY,
                        centerX + deltaX,
                        centerY + deltaY
                );
                captureRequestBuilder.set(CaptureRequest.SCALER_CROP_REGION, cropRegion);
            }

            captureRequestBuilder.addTarget(surface);
            pointFocus.apply(captureRequestBuilder);
            return submitRepeatingRequest(captureRequestBuilder.build());
        } catch (Exception e) {
            FileLog.e("Camera2Sessions setRepeatingRequest error in updateCaptureRequest", e);
            return false;
        }
    }

    public boolean takePicture(final File file, Utilities.Callback<Integer> whenDone) {
        if (imageReader == null) return false;                                                                 
        if (cameraDevice == null || captureSession == null) return false;
        try {
            CaptureRequest.Builder captureRequestBuilder = cameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_STILL_CAPTURE);
            final int orientation = getJpegOrientation();
            captureRequestBuilder.set(CaptureRequest.JPEG_ORIENTATION, orientation);
            imageReader.setOnImageAvailableListener(new ImageReader.OnImageAvailableListener() {
                @Override
                public void onImageAvailable(ImageReader reader) {
                    Image image = reader.acquireLatestImage();
                    if (image == null) return;                                                                                
                    ByteBuffer buffer = image.getPlanes()[0].getBuffer();
                    byte[] bytes = new byte[buffer.remaining()];
                    buffer.get(bytes);

                    FileOutputStream output = null;
                    try {
                        output = new FileOutputStream(file);
                        output.write(bytes);
                    } catch (IOException e) {
                        e.printStackTrace();
                    } finally {
                        image.close();
                        if (null != output) {
                            try {
                                output.close();
                            } catch (IOException e) {
                                e.printStackTrace();
                            }
                        }
                    }

                    AndroidUtilities.runOnUIThread(() -> {
                        if (whenDone != null) {
                            whenDone.run(orientation);
                        }
                    });
                }
            }, null);
            if (scanningBarcode) {
                captureRequestBuilder.set(CaptureRequest.CONTROL_SCENE_MODE, CameraMetadata.CONTROL_SCENE_MODE_BARCODE);
            }
            captureRequestBuilder.addTarget(imageReader.getSurface());
            captureSession.capture(captureRequestBuilder.build(), new CameraCaptureSession.CaptureCallback() {}, null);
            return true;
        } catch (Exception e) {
            FileLog.e("Camera2Sessions takePicture error", e);
            return false;
        }
    }

    public static Size chooseQualityAwareSize(Size[] choices, int viewWidth, int viewHeight, int requestedHeight) {
        if (choices == null || choices.length == 0) return null;
        if (requestedHeight <= 0) {
            return chooseOptimalSize(choices, viewWidth, viewHeight, false);
        }

        final int w = Math.max(viewWidth, viewHeight);
        final int h = Math.min(viewWidth, viewHeight);
        final float targetRatio = h == 0 ? 0f : (float) w / h;

        final long heightCap = (long) requestedHeight * 2;
        Size bestAspect = null;
        long bestAspectDelta = Long.MAX_VALUE;
        Size bestAnyCapped = null;
        long bestAnyCappedDelta = Long.MAX_VALUE;
        Size bestAnyUncapped = null;
        long bestAnyUncappedDelta = Long.MAX_VALUE;

        for (Size s : choices) {
            if (s == null) continue;
            long dHeight = Math.abs((long) s.getHeight() - requestedHeight);
            if (dHeight < bestAnyUncappedDelta) {
                bestAnyUncappedDelta = dHeight;
                bestAnyUncapped = s;
            }
            if (s.getHeight() <= heightCap && dHeight < bestAnyCappedDelta) {
                bestAnyCappedDelta = dHeight;
                bestAnyCapped = s;
            }
            if (targetRatio > 0 && s.getHeight() > 0) {
                float ratio = (float) s.getWidth() / s.getHeight();
                if (Math.abs(ratio - targetRatio) < 0.05f) {

                    if (dHeight < bestAspectDelta && s.getHeight() <= heightCap) {
                        bestAspectDelta = dHeight;
                        bestAspect = s;
                    }
                }
            }
        }
        if (bestAspect != null) return bestAspect;
        if (bestAnyCapped != null) return bestAnyCapped;
        return bestAnyUncapped;
    }

    static Size chooseRoundVideoCompatibilitySize(String manufacturer, String model,
            boolean front, boolean logical, boolean noStillSurface, Size selected, Size[] outputs) {
        if (!"OPPO".equalsIgnoreCase(manufacturer) || !"CPH2791".equalsIgnoreCase(model)
                || front || !logical || !noStillSurface || selected == null || outputs == null
                || selected.getWidth() != 1088 || selected.getHeight() != 1088) {
            return selected;
        }
        for (Size size : outputs) {
            if (size != null && size.getWidth() == 1920 && size.getHeight() == 1088) {
                return size;
            }
        }

        return selected;
    }

    public static Size chooseOptimalSize(Size[] choices, int width, int height, boolean notBigger) {
        List<Size> bigEnoughWithAspectRatio = new ArrayList<>(choices.length);
        List<Size> bigEnough = new ArrayList<>(choices.length);
        int w = width;
        int h = height;
        for (int a = 0; a < choices.length; a++) {
            Size option = choices[a];
            if (notBigger && (option.getHeight() > height || option.getWidth() > width)) {
                continue;
            }
            if (option.getHeight() == option.getWidth() * h / w && option.getWidth() >= width && option.getHeight() >= height) {
                bigEnoughWithAspectRatio.add(option);
            } else if (option.getHeight() * option.getWidth() <= width * height * 4 && option.getWidth() >= width && option.getHeight() >= height) {
                bigEnough.add(option);
            }
        }
        if (bigEnoughWithAspectRatio.size() > 0) {
            return Collections.min(bigEnoughWithAspectRatio, new CompareSizesByArea());
        } else if (bigEnough.size() > 0) {
            return Collections.min(bigEnough, new CompareSizesByArea());
        } else {
            return Collections.max(Arrays.asList(choices), new CompareSizesByArea());
        }
    }
    static class CompareSizesByArea implements Comparator<Size> {
        @Override
        public int compare(Size lhs, Size rhs) {
            return Long.signum((long) lhs.getWidth() * lhs.getHeight() - (long) rhs.getWidth() * rhs.getHeight());
        }
    }

}

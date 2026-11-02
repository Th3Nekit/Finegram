package com.th3nekit.finegram.camera;

import android.annotation.SuppressLint;
import android.content.Context;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.TotalCaptureResult;
import android.os.Build;
import android.util.Range;
import android.view.WindowManager;
import android.content.pm.PackageManager;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RestrictTo;
import androidx.camera.core.AspectRatio;
import androidx.camera.core.Camera;
import androidx.camera.core.CameraControl;
import androidx.camera.core.CameraSelector;
import androidx.camera.core.CameraState;
import androidx.camera.core.ConcurrentCamera;
import androidx.camera.core.ExposureState;
import androidx.camera.core.FocusMeteringAction;
import androidx.camera.core.ImageCapture;
import androidx.camera.core.ImageCaptureException;
import androidx.camera.core.MeteringPoint;
import androidx.camera.core.MeteringPointFactory;
import androidx.camera.core.Preview;
import androidx.camera.core.SessionConfig;
import androidx.camera.core.UseCase;
import androidx.camera.core.UseCaseGroup;
import androidx.camera.core.ZoomState;
import androidx.camera.camera2.interop.Camera2CameraInfo;
import androidx.camera.camera2.interop.Camera2Interop;
import androidx.camera.lifecycle.ProcessCameraProvider;
import androidx.core.content.ContextCompat;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.LifecycleRegistry;
import androidx.lifecycle.Observer;

import com.google.common.util.concurrent.ListenableFuture;

import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.FileLog;
import org.telegram.messenger.camera.Size;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;


public class CameraXController implements CameraXProviderCoordinator.Owner, CameraZoomTarget {

    public static final class ConcurrentCameraResourceException
            extends IllegalStateException {
        ConcurrentCameraResourceException(String message, @Nullable Throwable cause) {
            super(message, cause);
        }
    }

    private boolean isFrontface;
    private final CameraLifecycle lifecycle;
    private ProcessCameraProvider provider;
    private final MeteringPointFactory meteringPointFactory;
    private final Preview.SurfaceProvider surfaceProvider;

    @Nullable private Camera boundCamera;
    @Nullable private Preview boundPreview;
    @Nullable private ImageCapture boundImageCapture;
    @Nullable private CameraSelector boundSelector;
    @Nullable private SessionConfig boundSessionConfig;
    @Nullable private Range<Integer> appliedTargetFpsRange;
    @Nullable private LiveData<CameraState> boundCameraState;
    @Nullable private Observer<CameraState> boundCameraStateObserver;
    @Nullable private Runnable concurrentCameraInUseFailureRunnable;
    private int boundCameraGeneration;
    private float[] zoomShortcutCandidates = new float[0];
    private int initialZoomPreparedGeneration = -1;
    private volatile boolean boundCameraReady;
    private long diagnosticsBindStartedNanos;
    private long diagnosticsSwitchStartedNanos;
    private int diagnosticsReadyGeneration = -1;
    private int diagnosticsErrorMask;
    private float baseZoomRatio = 1f;
    @Nullable private volatile String activePhysicalCameraId;
    @Nullable private volatile String expectedInitialPhysicalCameraId;
    private final CameraXLensFrameTracker lensFrameTracker = new CameraXLensFrameTracker();

    private long latestPhysicalFrameTimestampNanos;
    private final CameraXZoomCoordinator zoomCoordinator =
            new CameraXZoomCoordinator("CameraX surface zoom", this::onZoomFailure);
    private boolean initiated;
    private volatile boolean closed;
    private final Object initializationLock = new Object();
    private int initializationGeneration;
    private int targetRotation = android.view.Surface.ROTATION_0;
    private int imageCaptureTargetRotation = android.view.Surface.ROTATION_0;
    @Nullable private final android.util.Size targetResolution;
    private final boolean enableImageCapture;
    private final boolean deferInitialBind;
    @Nullable private CameraXController concurrentPeer;
    private volatile boolean lastConcurrentBindBusy;
    private int flashMode = ImageCapture.FLASH_MODE_AUTO;
    private boolean torchRequested;
    private Camera torchSubmittedCamera;
    private int torchSubmittedGeneration;
    private boolean torchSubmittedValue;
    private ListenableFuture<Void> torchSubmittedFuture;
    private boolean useConfiguredUltraWide = true;
    @Nullable private Runnable readyCallback;
    @Nullable private org.telegram.messenger.Utilities.Callback<Throwable> failureCallback;

    public static final int CAMERA_NONE = 0;
    public static final int CAMERA_NIGHT = 1;
    public static final int CAMERA_HDR = 2;
    public static final int CAMERA_AUTO = 3;
    public static final int CAMERA_WIDE = 4;
    private int selectedEffect = CAMERA_NONE;
    private boolean platformFallbackGraph;
    @Nullable private volatile List<CameraXUtils.LensRoute> rearLensRoutes;
    @Nullable private volatile CameraXUtils.LensRoute selectedLensRoute;
    @Nullable private org.telegram.messenger.Utilities.Callback<Runnable> lensSwitchHandler;
    private volatile boolean lensSwitchPending;
    private int lensSwitchGeneration;
    private long zoomRequestSequence;
    private volatile float pendingGlobalZoom = Float.NaN;
    private boolean explicitLensZoom;
    private boolean continuousRearDisabled;
    private volatile boolean currentGraphProducedFrames;
    private float lastGlobalZoom = Float.NaN;

    public void setLensSwitchHandler(org.telegram.messenger.Utilities.Callback<Runnable> handler) {
        lensSwitchHandler = handler;
    }

    private void resetLensSelection() {
        diagnosticsSwitchStartedNanos = 0;
        ++zoomRequestSequence;
        ++lensSwitchGeneration;
        selectedLensRoute = null;
        lensSwitchPending = false;
        pendingGlobalZoom = Float.NaN;
        explicitLensZoom = false;
        lastGlobalZoom = Float.NaN;
    }

    private void ensureRearLensRoutes() {
        if (!isFrontface && rearLensRoutes == null && provider != null) {
            rearLensRoutes = CameraXUtils.findRearLensRoutes(provider, preferContinuousRear());
        }
        if (!isFrontface && selectedLensRoute == null && rearLensRoutes != null && preferContinuousRear()) {
            for (CameraXUtils.LensRoute route : rearLensRoutes) {
                if (route.fallbackSelector != null) {
                    selectedLensRoute = route;
                    break;
                }
            }
        }
    }

    private boolean preferContinuousRear() {
        return !enableImageCapture && !continuousRearDisabled
                && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getSmoothCameraModuleTransitions();
    }

    private boolean hasOptimizedRearSelection() {
        return !isFrontface && concurrentPeer == null && !continuousRearDisabled
                && selectedLensRoute != null && selectedLensRoute.fallbackSelector != null;
    }

    private boolean preferContinuousRearZoom() {
        if (!preferContinuousRear() || rearLensRoutes == null) return false;
        for (CameraXUtils.LensRoute route : rearLensRoutes) {
            if (route.fallbackSelector != null) return true;
        }
        return false;
    }

    private boolean hasOptimizedRearGraph() {
        return hasOptimizedRearSelection()
                && selectedLensRoute.key.equals(CameraXUtils.lensRouteKey(provider, boundSelector));
    }

    private void disableContinuousRear() {
        continuousRearDisabled = true;
        rearLensRoutes = null;
        selectedLensRoute = null;
    }

    private boolean recoverContinuousRear(Camera camera, int generation) {
        if (closed || camera == null || camera != boundCamera || generation != boundCameraGeneration
                || !hasOptimizedRearGraph() || lensSwitchHandler == null) return false;
        float requested = Float.isFinite(lastGlobalZoom) ? lastGlobalZoom : getZoomRatio();
        disableContinuousRear();
        pendingGlobalZoom = requested;
        lensSwitchPending = true;
        int token = ++lensSwitchGeneration;
        diagnosticsSwitchStartedNanos = System.nanoTime();
        recordCameraDiagnostics("camerax.bind.fallback", "route=physical reason=continuous_unready");
        FileLog.d("CameraX continuous rear route rejected; restoring physical selection");
        lensSwitchHandler.run(() -> AndroidUtilities.runOnUIThread(() -> {
            if (closed || token != lensSwitchGeneration || isFrontface || concurrentPeer != null) return;
            if (bindUseCases()) notifyReady();
            else reportFailure(new IllegalStateException("CameraX physical route recovery failed"));
        }));
        return true;
    }

    private void onZoomFailure() {
        recoverContinuousRear(boundCamera, boundCameraGeneration);
    }

    private float opticalZoomScale() {
        return selectedLensRoute == null || concurrentPeer != null || platformFallbackGraph
                ? 1f : selectedLensRoute.opticalRatio;
    }

    private boolean canSwitchLenses() {
        return !isFrontface && concurrentPeer == null && lensSwitchHandler != null
                && rearLensRoutes != null && !rearLensRoutes.isEmpty();
    }

    public static class CameraLifecycle implements LifecycleOwner {

        private final LifecycleRegistry lifecycleRegistry;

        public CameraLifecycle() {
            lifecycleRegistry = new LifecycleRegistry(this);
            lifecycleRegistry.setCurrentState(Lifecycle.State.CREATED);
        }

        public void start() {
            try {
                lifecycleRegistry.setCurrentState(Lifecycle.State.RESUMED);
            } catch (IllegalStateException ignored) {
                com.th3nekit.finegram.core.crash.CameraDiagnostics.record(
                        "camerax.lifecycle.error", "stage=resume error=IllegalStateException");
            }
        }

        public void stop() {
            try {

                lifecycleRegistry.setCurrentState(Lifecycle.State.CREATED);
            } catch (IllegalStateException ignored) {
                com.th3nekit.finegram.core.crash.CameraDiagnostics.record(
                        "camerax.lifecycle.error", "stage=stop error=IllegalStateException");
            }
        }

        @NonNull
        public Lifecycle getLifecycle() {
            return lifecycleRegistry;
        }

    }

    public CameraXController(CameraLifecycle lifecycle, MeteringPointFactory factory, Preview.SurfaceProvider surfaceProvider) {
        this(lifecycle, factory, surfaceProvider, null);
    }

    public CameraXController(CameraLifecycle lifecycle, MeteringPointFactory factory,
                                    Preview.SurfaceProvider surfaceProvider,
                                    @Nullable android.util.Size targetResolution) {
        this(lifecycle, factory, surfaceProvider, targetResolution, false);
    }

    public CameraXController(CameraLifecycle lifecycle, MeteringPointFactory factory,
                                    Preview.SurfaceProvider surfaceProvider,
                                    @Nullable android.util.Size targetResolution,
                                    boolean enableImageCapture) {
        this(lifecycle, factory, surfaceProvider, targetResolution, enableImageCapture, false);
    }

    public CameraXController(CameraLifecycle lifecycle, MeteringPointFactory factory,
                                    Preview.SurfaceProvider surfaceProvider,
                                    @Nullable android.util.Size targetResolution,
                                    boolean enableImageCapture, boolean deferInitialBind) {
        this.lifecycle = lifecycle;
        this.meteringPointFactory = factory;
        this.surfaceProvider = surfaceProvider;
        this.targetResolution = targetResolution;
        this.enableImageCapture = enableImageCapture;
        this.deferInitialBind = deferInitialBind;
    }

    public boolean isInitiated() {
        return initiated && boundCamera != null
                && CameraXProviderCoordinator.isOwner(this);
    }

    public boolean isZoomReady() {
        return !closed && boundCameraReady && isInitiated();
    }

    public void setFrontFace(boolean isFrontFace) {

        if (isFrontface == isFrontFace && isInitiated()) return;
        resetLensSelection();
        isFrontface = isFrontFace;
        diagnosticsSwitchStartedNanos = System.nanoTime();
        recordCameraDiagnostics("camerax.switch.request", "kind=facing");
        if (provider != null && !bindUseCases()) {
            reportFailure(new IllegalStateException("CameraX camera switch bind failed"));
        }
    }

    public boolean isFrontface() {
        return isFrontface;
    }

    public void initCamera(Context context, boolean isInitialFrontface, Runnable onPreInit) {
        initCamera(context, isInitialFrontface, onPreInit, null);
    }

    public void initCamera(Context context, boolean isInitialFrontface, @Nullable Runnable onReady,
                           @Nullable org.telegram.messenger.Utilities.Callback<Throwable> onFailure) {
        if (context == null) return;
        final int requestGeneration;
        synchronized (initializationLock) {
            requestGeneration = ++initializationGeneration;
            closed = false;
            isFrontface = isInitialFrontface;
            readyCallback = onReady;
            failureCallback = onFailure;
        }
        try {
            ListenableFuture<ProcessCameraProvider> future = CameraXUtils.getProviderFuture(context);
            future.addListener(() -> {
                synchronized (initializationLock) {
                    if (closed || requestGeneration != initializationGeneration) {
                        return;
                    }
                    try {
                        ProcessCameraProvider resolvedProvider = future.get();
                        if (closed || requestGeneration != initializationGeneration) {
                            return;
                        }
                        provider = resolvedProvider;
                        if (deferInitialBind) {

                            lifecycle.stop();
                            if (prepareUseCases(false)) {
                                notifyReady();
                            } else {
                                reportFailure(new IllegalStateException("CameraX use-case preparation failed"));
                            }
                        } else {
                            lifecycle.start();
                            bindUseCasesAndReport();
                        }
                    } catch (Throwable t) {
                        if (closed || requestGeneration != initializationGeneration) {
                            return;
                        }
                        initiated = false;
                        recordCameraDiagnostics("camerax.lifecycle.error",
                                "stage=provider error=" + t.getClass().getSimpleName());
                        FileLog.e(t);
                        reportFailure(t);
                    }
                }
            }, androidx.core.content.ContextCompat.getMainExecutor(context));
        } catch (Throwable t) {
            synchronized (initializationLock) {
                if (closed || requestGeneration != initializationGeneration) {
                    return;
                }
                initiated = false;
                recordCameraDiagnostics("camerax.lifecycle.error",
                        "stage=provider_request error=" + t.getClass().getSimpleName());
                FileLog.e(t);
                reportFailure(t);
            }
        }
    }

    public void setCameraEffect(@EffectFacing int effect) {
        if (selectedEffect == effect) return;
        selectedEffect = effect;
        bindUseCasesAndReport();
    }

    public int getCameraEffect() {
        return selectedEffect;
    }

    public void setUseConfiguredUltraWide(boolean useConfiguredUltraWide) {
        this.useConfiguredUltraWide = useConfiguredUltraWide;
    }

    public void switchCamera() {
        resetLensSelection();
        isFrontface = !isFrontface;
        diagnosticsSwitchStartedNanos = System.nanoTime();
        recordCameraDiagnostics("camerax.switch.request", "kind=facing");
        if (provider != null && !bindUseCases()) {
            reportFailure(new IllegalStateException("CameraX camera switch bind failed"));
        }
    }

    public void closeCamera() {
        synchronized (initializationLock) {
            closed = true;
            resetLensSelection();
            retireLensCaptureGraph();
            ++initializationGeneration;
            enableTorch(false);
            detachConcurrentPeer();
            CameraXProviderCoordinator.release(
                    provider, this, () -> {
                        unbindOwnUseCases();
                        return true;
                    });
            invalidateBoundCameraControls();
            boundPreview = null;
            boundImageCapture = null;
            boundCamera = null;
            boundSessionConfig = null;
            appliedTargetFpsRange = null;
            initiated = false;
            lifecycle.stop();
        }
    }

    @SuppressLint("RestrictedApi")
    public boolean hasFrontFaceCamera() {
        if (provider == null) return false;
        try {
            return provider.hasCamera(CameraSelector.DEFAULT_FRONT_CAMERA);
        } catch (Throwable t) {
            return false;
        }
    }

    @SuppressLint("RestrictedApi")
    public static boolean hasGoodCamera(Context context) {
        return context != null && CameraXUtils.isCameraXSupported()
                && context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_CAMERA_ANY);
    }

    public int setNextFlashMode() {
        if (flashMode == ImageCapture.FLASH_MODE_AUTO) {
            setFlashMode(ImageCapture.FLASH_MODE_ON);
        } else if (flashMode == ImageCapture.FLASH_MODE_ON) {
            setFlashMode(ImageCapture.FLASH_MODE_OFF);
        } else {
            setFlashMode(ImageCapture.FLASH_MODE_AUTO);
        }
        return flashMode;
    }

    public int getCurrentFlashMode() {
        return flashMode;
    }

    public void setFlashMode(int mode) {
        if (mode != ImageCapture.FLASH_MODE_AUTO && mode != ImageCapture.FLASH_MODE_ON
                && mode != ImageCapture.FLASH_MODE_OFF) {
            mode = ImageCapture.FLASH_MODE_AUTO;
        }
        flashMode = mode;
        if (boundImageCapture != null) {
            try {
                boundImageCapture.setFlashMode(mode);
            } catch (Throwable t) {
                FileLog.e(t);
            }
        }
    }

    public boolean isFlashAvailable() {
        try {
            return boundCamera != null && boundCamera.getCameraInfo().hasFlashUnit();
        } catch (Throwable t) {
            return false;
        }
    }

    public void enableTorch(boolean enabled) {
        if (android.os.Looper.myLooper() != android.os.Looper.getMainLooper()) {
            AndroidUtilities.runOnUIThread(() -> enableTorch(enabled));
            return;
        }
        torchRequested = enabled;
        recordTorchEvent(enabled, "request", "queued");
        applyRequestedTorch();
    }

    private void applyRequestedTorch() {
        if (android.os.Looper.myLooper() != android.os.Looper.getMainLooper()) {
            AndroidUtilities.runOnUIThread(this::applyRequestedTorch);
            return;
        }
        Camera camera = boundCamera;
        final int generation = boundCameraGeneration;
        if (camera == null || !boundCameraReady || closed) {
            recordTorchEvent(torchRequested, "control", closed ? "closed" : "deferred");
            return;
        }
        if (torchSubmittedCamera == camera && torchSubmittedGeneration == generation
                && torchSubmittedValue == torchRequested) return;
        final boolean requested = torchRequested;
        try {
            if (camera.getCameraInfo().hasFlashUnit()) {
                ListenableFuture<Void> result = camera.getCameraControl().enableTorch(requested);
                torchSubmittedCamera = camera;
                torchSubmittedGeneration = generation;
                torchSubmittedValue = requested;
                torchSubmittedFuture = result;
                result.addListener(() -> {
                    boolean current = camera == boundCamera && generation == boundCameraGeneration;
                    try {
                        result.get();
                        recordTorchEvent(requested, "complete", current ? "applied" : "stale");
                    } catch (Throwable error) {
                        if (torchSubmittedFuture == result && torchSubmittedCamera == camera
                                && torchSubmittedGeneration == generation) {
                            torchSubmittedCamera = null;
                            torchSubmittedFuture = null;
                        }
                        Throwable cause = error instanceof ExecutionException && error.getCause() != null
                                ? error.getCause() : error;
                        recordTorchEvent(requested, "complete", current ? cause.getClass().getSimpleName() : "stale");
                        if (current) com.th3nekit.finegram.core.crash.CameraDiagnostics.record("camerax.torch.failure", cause);
                    }
                }, ContextCompat.getMainExecutor(ApplicationLoader.applicationContext));
                trackControlFuture(
                        result, camera, generation, "CameraX torch request", null);
            } else {
                recordTorchEvent(requested, "control", "no_flash_unit");
            }
        } catch (Throwable t) {
            recordTorchEvent(requested, "control", t.getClass().getSimpleName());
            com.th3nekit.finegram.core.crash.CameraDiagnostics.record("camerax.torch.failure", t);
            FileLog.e(t);
        }
    }

    private void recordTorchEvent(boolean requested, String stage, String result) {
        com.th3nekit.finegram.core.crash.CameraDiagnostics.record("camerax.torch",
                "engine=camerax torch=" + requested + " stage=" + stage + " result=" + result
                        + " generation=" + boundCameraGeneration + " front=" + isFrontface
                        + " ready=" + boundCameraReady);
    }

    public boolean isAvailableHdrMode() {
        return isExtensionAvailable(currentSelector(), EXTENSION_MODE_HDR);
    }

    public boolean isAvailableNightMode() {
        return isExtensionAvailable(currentSelector(), EXTENSION_MODE_NIGHT);
    }

    public boolean isAvailableWideMode() {
        if (provider == null) return false;
        try {
            return CameraXUtils.isWideAngleAvailable(provider);
        } catch (Throwable t) {
            return false;
        }
    }

    public boolean isAvailableAutoMode() {
        return isExtensionAvailable(currentSelector(), EXTENSION_MODE_AUTO);
    }

    public boolean isAvailableSlowMoMode() {
        if (boundCamera == null) return false;
        try {

            android.util.Range<Integer>[] ranges = androidx.camera.camera2.interop.Camera2CameraInfo
                    .from(boundCamera.getCameraInfo())
                    .getCameraCharacteristic(android.hardware.camera2.CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
            if (ranges == null) return false;
            for (android.util.Range<Integer> r : ranges) {
                if (r != null && r.getUpper() != null && r.getUpper() >= 120) return true;
            }
            return false;
        } catch (Throwable t) {
            return false;
        }
    }

    @SuppressLint({"RestrictedApi", "UnsafeExperimentalUsageError", "UnsafeOptInUsageError"})
    public boolean bindUseCases() {
        if (closed || provider == null || surfaceProvider == null) {
            return false;
        }
        diagnosticsBindStartedNanos = System.nanoTime();
        recordCameraDiagnostics("camerax.bind.attempt", "mode=single");
        try {
            boolean result = CameraXProviderCoordinator.withSingleOwner(
                    provider, this, this::bindUseCasesOwned);
            recordCameraDiagnostics("camerax.bind.result",
                    "mode=single success=" + result + " elapsed_ms="
                            + ((System.nanoTime() - diagnosticsBindStartedNanos) / 1_000_000L));
            return result;
        } catch (Throwable error) {
            recordCameraDiagnostics("camerax.bind.result",
                    "mode=single success=false error=" + error.getClass().getSimpleName());
            FileLog.e("CameraX ownership bind failed", error);
            clearPreparedUseCases();
            return false;
        }
    }

    private boolean bindUseCasesOwned() {
        initiated = false;
        if (closed || provider == null || surfaceProvider == null) return false;
        lifecycle.start();
        boolean requestedFpsWasApplied = false;
        try {
            detachConcurrentPeer();
            unbindOwnUseCases();
            if (prepareUseCases(true)) {
                requestedFpsWasApplied = appliedTargetFpsRange != null;
                if (bindPreparedUseCases()) return true;
            }
        } catch (Throwable t) {
            recordCameraDiagnostics("camerax.bind.error",
                    "route=optimized error=" + t.getClass().getSimpleName());
            FileLog.e("CameraX optimized bind failed", t);
        }

        if (hasOptimizedRearSelection()) {
            recordCameraDiagnostics("camerax.bind.fallback", "route=physical");
            disableContinuousRear();
            try {
                unbindOwnUseCases();
                if (prepareUseCases(true)) {
                    requestedFpsWasApplied = appliedTargetFpsRange != null;
                    if (bindPreparedUseCases()) return true;
                }
            } catch (Throwable retryError) {
                recordCameraDiagnostics("camerax.bind.error",
                        "route=physical error=" + retryError.getClass().getSimpleName());
                FileLog.e("CameraX physical route retry failed", retryError);
            }
        }

        if (requestedFpsWasApplied) {
            recordCameraDiagnostics("camerax.bind.fallback", "route=platform_fps");
            try {
                unbindOwnUseCases();
                if (prepareUseCases(true, false, false, true)
                        && bindPreparedUseCases()) {
                    return true;
                }
            } catch (Throwable retryError) {
                recordCameraDiagnostics("camerax.bind.error",
                        "route=platform_fps error=" + retryError.getClass().getSimpleName());
                FileLog.e("CameraX bind retry with platform FPS failed", retryError);
            }
        }

        recordCameraDiagnostics("camerax.bind.fallback", "route=safe");
        try {
            unbindOwnUseCases();
            if (prepareUseCases(true, false, false, false)
                    && bindPreparedUseCases()) {
                return true;
            }
        } catch (Throwable retryError) {
            recordCameraDiagnostics("camerax.bind.error",
                    "route=safe error=" + retryError.getClass().getSimpleName());
            FileLog.e("CameraX safe bind fallback failed", retryError);
        }

        if (lensSwitchPending) {
            clearPreparedUseCases();
            return false;
        }

        recordCameraDiagnostics("camerax.bind.fallback", "route=platform");
        try {
            unbindOwnUseCases();
            CameraSelector platformSelector = isFrontface
                    ? CameraSelector.DEFAULT_FRONT_CAMERA : CameraSelector.DEFAULT_BACK_CAMERA;
            if (prepareUseCases(true, false, false, false, platformSelector)
                    && bindPreparedUseCases()) {
                return true;
            }
        } catch (Throwable retryError) {
            recordCameraDiagnostics("camerax.bind.error",
                    "route=platform error=" + retryError.getClass().getSimpleName());
            FileLog.e("CameraX platform camera bind failed", retryError);
        }

        clearPreparedUseCases();
        return false;
    }

    private boolean bindPreparedUseCases() {
        if (boundSessionConfig == null) return false;
        boundCamera = provider.bindToLifecycle(
                lifecycle, boundSelector, boundSessionConfig);
        initiated = boundCamera != null;
        attachBoundCamera(boundCamera, true);
        return initiated;
    }

    private void clearPreparedUseCases() {
        retireLensCaptureGraph();
        boundCamera = null;
        boundPreview = null;
        boundImageCapture = null;
        boundSelector = null;
        boundSessionConfig = null;
        appliedTargetFpsRange = null;
        initiated = false;
    }

    private void bindUseCasesAndReport() {
        if (bindUseCases()) {
            notifyReady();
        } else {
            reportFailure(new IllegalStateException("CameraX preview bind failed"));
        }
    }

    @SuppressLint({"RestrictedApi", "UnsafeExperimentalUsageError", "UnsafeOptInUsageError"})
    private boolean prepareUseCases(boolean rebuild) {
        return prepareUseCases(rebuild, false, true, true);
    }

    private boolean prepareUseCases(boolean rebuild, boolean concurrentPreview,
                                    boolean applyRequestedFps, boolean applyEnhancements) {
        return prepareUseCases(rebuild, concurrentPreview, applyRequestedFps,
                applyEnhancements, null, null);
    }

    private boolean prepareUseCases(boolean rebuild, boolean concurrentPreview,
                                    boolean applyRequestedFps, boolean applyEnhancements,
                                    @Nullable CameraSelector selectorOverride) {
        return prepareUseCases(rebuild, concurrentPreview, applyRequestedFps,
                applyEnhancements, selectorOverride, null);
    }

    private boolean prepareUseCases(boolean rebuild, boolean concurrentPreview,
                                    boolean applyRequestedFps, boolean applyEnhancements,
                                    @Nullable CameraSelector selectorOverride,
                                    @Nullable Range<Integer> concurrentFpsRange) {
        if (closed || provider == null || surfaceProvider == null) return false;

        if (!rebuild && boundPreview != null && boundSelector != null
                && lensFrameTracker.hasActiveGraph()) return true;
        platformFallbackGraph = !concurrentPreview && selectorOverride != null
                && (selectorOverride == CameraSelector.DEFAULT_FRONT_CAMERA
                || selectorOverride == CameraSelector.DEFAULT_BACK_CAMERA);
        final Object captureGraphToken;
        synchronized (lensFrameTracker) {
            captureGraphToken = lensFrameTracker.beginGraph();
            activePhysicalCameraId = null;
            expectedInitialPhysicalCameraId = null;
            latestPhysicalFrameTimestampNanos = 0;
            currentGraphProducedFrames = false;
        }
        try {
            ensureRearLensRoutes();
            if (!isFrontface && !concurrentPreview && selectedLensRoute == null
                    && Float.isFinite(pendingGlobalZoom) && rearLensRoutes != null) {
                selectedLensRoute = CameraXOpticalZoom.choose(
                        rearLensRoutes, pendingGlobalZoom, preferContinuousRearZoom());
            }
            CameraSelector selector;
            if (selectorOverride != null) {
                selector = selectorOverride;
            } else if (!concurrentPreview && !isFrontface && selectedLensRoute != null) {
                selector = selectedLensRoute.selector;
            } else if (!isFrontface && selectedEffect == CAMERA_WIDE) {
                selector = CameraXUtils.buildUltraWideSelector(provider);
                if (selector == null) {
                    selector = CameraXUtils.buildIntendedCameraSelector(
                            provider, false, useConfiguredUltraWide);
                }
            } else {
                selector = CameraXUtils.buildIntendedCameraSelector(
                        provider, isFrontface, useConfiguredUltraWide);
            }
            if (!isFrontface && !concurrentPreview && rearLensRoutes != null && !platformFallbackGraph) {
                String key = CameraXUtils.lensRouteKey(provider, selector);
                selectedLensRoute = null;
                for (CameraXUtils.LensRoute route : rearLensRoutes) {
                    if (route.key.equals(key)) { selectedLensRoute = route; break; }
                }
            }
            boundSelector = selectorOverride != null ? selector : applyExtensionMode(selector);
            Preview.Builder previewBuilder = new Preview.Builder().setTargetRotation(targetRotation);

            if (!concurrentPreview && !platformFallbackGraph && !enableImageCapture
                    && hasOptimizedRearGraph()) {
                if (!CameraXUtils.supportsContinuousPreviewTarget(
                        provider, selectedLensRoute.fallbackSelector, targetResolution)) {
                    throw new IllegalArgumentException("CameraX fallback preview target size is unknown");
                }
                previewBuilder.setResolutionSelector(CameraXUtils.buildContinuousPreviewResolutionSelector(
                        targetResolution, CameraSettings.roundResolution(512)));
            } else if (targetResolution != null && !platformFallbackGraph) {
                boolean roundVideoPreview = !enableImageCapture;
                float targetRatio = Math.max(targetResolution.getWidth(),
                        targetResolution.getHeight()) / (float) Math.max(1,
                        Math.min(targetResolution.getWidth(), targetResolution.getHeight()));
                int aspectRatio = Math.abs(targetRatio - 4f / 3f)
                        < Math.abs(targetRatio - 16f / 9f)
                        ? AspectRatio.RATIO_4_3 : AspectRatio.RATIO_16_9;
                boolean preferCaptureRate = roundVideoPreview
                        || Math.min(
                        targetResolution.getWidth(),
                        targetResolution.getHeight()) <= 1080;
                previewBuilder.setResolutionSelector(concurrentPreview
                        ? CameraXUtils.buildConcurrentPreviewResolutionSelector(
                                targetResolution, aspectRatio)
                        : CameraXUtils.buildResolutionSelector(
                                targetResolution, aspectRatio, preferCaptureRate,
                                roundVideoPreview ? CameraSettings.roundResolution(512) : 0));
            }
            appliedTargetFpsRange = null;
            if (concurrentPreview && applyRequestedFps && concurrentFpsRange != null) {

                previewBuilder.setTargetFrameRate(concurrentFpsRange);
                appliedTargetFpsRange = concurrentFpsRange;
            }
            boolean configuredStartFromUltraWide = !explicitLensZoom && !Float.isFinite(pendingGlobalZoom)
                    && !platformFallbackGraph && !isFrontface
                    && (useConfiguredUltraWide && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getStartFromUltraWideCam()
                    || selectedEffect == CAMERA_WIDE);

            boolean startFromUltraWide = configuredStartFromUltraWide
                    && CameraXUtils.supportsSubOneZoom(provider, boundSelector)
                    && (!concurrentPreview
                    || !CameraXUtils.isOppoCph2791ConcurrentQuirk());
            boolean observeCaptureFrames = !enableImageCapture;
            boolean observeLensMetadata = false;
            if (!isFrontface && Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                try {
                    androidx.camera.core.CameraInfo selectedInfo =
                            provider.getCameraInfo(boundSelector);
                    observeLensMetadata = selectedInfo != null
                            && selectedInfo.isLogicalMultiCameraSupported();
                } catch (Throwable ignored) {

                }
            }
            Camera2Interop.Extender<Preview> previewExtender =
                    applyEnhancements || startFromUltraWide || observeLensMetadata || observeCaptureFrames
                            ? new Camera2Interop.Extender<>(previewBuilder) : null;
            if (applyEnhancements) {
                CameraXUtils.applyCamera2Controls(provider, boundSelector,
                        previewExtender, false);
            }
            if (startFromUltraWide && previewExtender != null
                    && Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                try {
                    androidx.camera.core.CameraInfo info =
                            provider.getCameraInfo(boundSelector);
                    ZoomState zoomState = info == null ? null
                            : info.getZoomState().getValue();
                    float initialRatio = CameraXUtils.getBaseZoomRatio(zoomState, true);
                    if (initialRatio < 0.999f) {

                        baseZoomRatio = initialRatio;
                    }
                } catch (Throwable error) {
                }
            }
            if (startFromUltraWide && previewExtender != null
                    && Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                expectedInitialPhysicalCameraId =
                        CameraXUtils.findBackUltraWideCameraId(provider);
            }
            if (applyEnhancements && !concurrentPreview
                    && CameraXUtils.shouldEnablePreviewStabilization(provider, boundSelector)) {
                previewBuilder.setPreviewStabilizationEnabled(true);
            }
            if (previewExtender != null && (observeLensMetadata || startFromUltraWide || observeCaptureFrames)) {
                previewExtender.setSessionCaptureCallback(createLensCaptureCallback(captureGraphToken));
            }
            boundPreview = previewBuilder.build();
            boundPreview.setSurfaceProvider(request -> {

                if (closed || !lensFrameTracker.isCurrent(captureGraphToken)) {
                    request.willNotProvideSurface();
                    return;
                }
                surfaceProvider.onSurfaceRequested(request);
            });
            if (enableImageCapture && !concurrentPreview) {
                ImageCapture.Builder captureBuilder = new ImageCapture.Builder()
                        .setCaptureMode(ImageCapture.CAPTURE_MODE_MAXIMIZE_QUALITY)
                        .setJpegQuality(100)
                        .setTargetRotation(imageCaptureTargetRotation);
                if (targetResolution != null && !platformFallbackGraph) {
                    captureBuilder.setResolutionSelector(CameraXUtils.buildResolutionSelector(
                            targetResolution, AspectRatio.RATIO_16_9, false));
                }
                if (applyEnhancements) {
                    CameraXUtils.applyCamera2Controls(provider, boundSelector,
                            new Camera2Interop.Extender<>(captureBuilder), true);
                }
                boundImageCapture = captureBuilder.build();
                boundImageCapture.setFlashMode(flashMode);
            } else {
                boundImageCapture = null;
            }
            if (concurrentPreview) {

                boundSessionConfig = null;
            } else {
                ArrayList<UseCase> useCases = new ArrayList<>(2);
                useCases.add(boundPreview);
                if (boundImageCapture != null) useCases.add(boundImageCapture);
                if (hasOptimizedRearGraph() && !CameraXUtils.preservesFrameRates(
                        provider, boundSelector, selectedLensRoute.fallbackSelector, useCases)) {
                    throw new IllegalStateException("CameraX continuous route cannot preserve frame rates");
                }
                if (applyRequestedFps) {
                    appliedTargetFpsRange =
                            CameraXUtils.getSupportedTargetFpsRange(
                                    provider, boundSelector, useCases);
                }
                SessionConfig.Builder sessionBuilder =
                        new SessionConfig.Builder(useCases);
                if (appliedTargetFpsRange != null) {
                    sessionBuilder.setFrameRateRange(appliedTargetFpsRange);
                }
                boundSessionConfig = sessionBuilder.build();
            }
            return true;
        } catch (Throwable t) {
            recordCameraDiagnostics("camerax.bind.error",
                    "stage=prepare error=" + t.getClass().getSimpleName());
            retireLensCaptureGraph();
            FileLog.e(t);
            boundPreview = null;
            boundImageCapture = null;
            boundSelector = null;
            boundSessionConfig = null;
            appliedTargetFpsRange = null;
            return false;
        }
    }

    private CameraCaptureSession.CaptureCallback createLensCaptureCallback(
            final Object captureGraphToken) {
        return new CameraCaptureSession.CaptureCallback() {
            private boolean captureBeginRecorded;

            @Override
            public void onCaptureStarted(@NonNull CameraCaptureSession session,
                                         @NonNull CaptureRequest request,
                                         long timestamp, long frameNumber) {
                synchronized (lensFrameTracker) {
                    if (closed || !lensFrameTracker.isCurrent(captureGraphToken)) return;
                    if (!enableImageCapture && !captureBeginRecorded) {
                        captureBeginRecorded = true;
                        com.th3nekit.finegram.core.crash.CameraDiagnostics.record(
                                "camerax.capture_begin", "engine=camerax");
                    }
                    lensFrameTracker.recordCaptureStarted(captureGraphToken, timestamp);
                }
            }

            @Override
            public void onCaptureProgressed(@NonNull CameraCaptureSession session,
                                            @NonNull CaptureRequest request,
                                            @NonNull CaptureResult partialResult) {
                if (closed || Build.VERSION.SDK_INT < Build.VERSION_CODES.P
                        || !lensFrameTracker.isCurrent(captureGraphToken)) return;
                Long timestamp;
                String physicalId;
                try {
                    timestamp = partialResult.get(CaptureResult.SENSOR_TIMESTAMP);
                    if (timestamp == null || timestamp <= 0) return;
                    physicalId = partialResult.get(CaptureResult.LOGICAL_MULTI_CAMERA_ACTIVE_PHYSICAL_ID);
                    if (physicalId == null || physicalId.isEmpty()) return;
                } catch (Throwable ignored) {
                    return;
                }
                float zoomRatio = Float.NaN;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    try {
                        Float ratio = partialResult.get(CaptureResult.CONTROL_ZOOM_RATIO);
                        if (ratio != null) zoomRatio = ratio;
                    } catch (Throwable ignored) {
                    }
                }
                synchronized (lensFrameTracker) {
                    if (closed || !lensFrameTracker.isCurrent(captureGraphToken)) return;
                    lensFrameTracker.recordPartial(captureGraphToken, physicalId, zoomRatio, timestamp);
                }
            }

            @Override
            public void onCaptureCompleted(@NonNull CameraCaptureSession session,
                                           @NonNull CaptureRequest request,
                                           @NonNull TotalCaptureResult result) {
                if (closed || !lensFrameTracker.isCurrent(captureGraphToken)) return;
                Long timestamp = null;
                String physicalId = null;
                float zoomRatio = Float.NaN;
                try {
                    timestamp = result.get(CaptureResult.SENSOR_TIMESTAMP);
                } catch (Throwable ignored) {
                }
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                    try {
                        physicalId = result.get(CaptureResult.LOGICAL_MULTI_CAMERA_ACTIVE_PHYSICAL_ID);
                    } catch (Throwable ignored) {
                    }
                }
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    try {
                        Float ratio = result.get(CaptureResult.CONTROL_ZOOM_RATIO);
                        if (ratio != null) zoomRatio = ratio;
                    } catch (Throwable ignored) {
                    }
                    if (!CameraXLensFrameTracker.isValidRatio(zoomRatio)) {
                        try {

                            Float ratio = request.get(CaptureRequest.CONTROL_ZOOM_RATIO);
                            if (ratio != null) zoomRatio = ratio;
                        } catch (Throwable ignored) {
                        }
                    }
                }
                final long timestampNanos = timestamp == null ? 0L : timestamp;
                synchronized (lensFrameTracker) {

                    if (closed || !lensFrameTracker.isCurrent(captureGraphToken)) return;
                    if (timestampNanos > 0) currentGraphProducedFrames = true;
                    lensFrameTracker.record(captureGraphToken, physicalId, zoomRatio, timestampNanos);
                    if (physicalId != null && !physicalId.isEmpty()
                            && (latestPhysicalFrameTimestampNanos == 0
                            || timestampNanos >= latestPhysicalFrameTimestampNanos)) {
                        latestPhysicalFrameTimestampNanos = Math.max(0L, timestampNanos);
                        if (!physicalId.equals(activePhysicalCameraId)) {
                            activePhysicalCameraId = physicalId;
                        }
                    }
                }
            }
        };
    }

    private void retireLensCaptureGraph() {
        synchronized (lensFrameTracker) {
            lensFrameTracker.retireGraph();
            activePhysicalCameraId = null;
            latestPhysicalFrameTimestampNanos = 0;
        }
    }

    private void unbindOwnUseCases() {
        if (provider == null) return;
        invalidateBoundCameraControls();
        try {
            if (boundSessionConfig != null && provider.isBound(boundSessionConfig)) {
                provider.unbind(boundSessionConfig);
                return;
            }
            if (boundPreview == null) return;
            if (boundImageCapture != null && provider.isBound(boundImageCapture)) {
                provider.unbind(boundPreview, boundImageCapture);
            } else if (provider.isBound(boundPreview)) {
                provider.unbind(boundPreview);
            }
        } catch (Throwable t) {
            FileLog.e(t);
        }
    }

    public void unbindForSurfaceRebind() {
        if (closed) return;
        boolean unboundSingle = CameraXProviderCoordinator.runIfSingleOwner(
                provider, this, () -> {
                    unbindOwnUseCases();
                    return true;
                });
        if (!unboundSingle) {
            CameraXProviderCoordinator.release(
                    provider, this, () -> true);
        }
        invalidateBoundCameraControls();
        boundCamera = null;
        initiated = false;
    }

    private void detachConcurrentPeer() {
        CameraXController peer = concurrentPeer;
        concurrentPeer = null;
        if (peer != null && peer.concurrentPeer == this) {
            peer.concurrentPeer = null;
            peer.invalidateBoundCameraControls();
            peer.initiated = false;
            peer.boundCamera = null;
        }
    }

    public boolean isPrepared() {
        return !closed && provider != null && boundPreview != null && boundSelector != null;
    }

    public boolean isConcurrentWith(@Nullable CameraXController other) {
        return other != null && concurrentPeer == other && other.concurrentPeer == this
                && CameraXProviderCoordinator.isConcurrentPair(this, other)
                && isInitiated() && other.isInitiated();
    }

    public boolean bindConcurrentWith(@Nullable CameraXController other) {
        return bindConcurrentWith(other, false);
    }

    public boolean bindConcurrentWith(@Nullable CameraXController other,
                                      boolean compatibilityProfile) {
        lastConcurrentBindBusy = false;
        if (other == null || other == this || closed || other.closed) return false;
        if (isConcurrentWith(other)) return true;
        if (provider == null || provider != other.provider) {
            recordCameraDiagnostics("camerax.dual.eligibility", "eligible=false reason=provider");
            return false;
        }
        final CameraXController backController = isFrontface ? other : this;
        final CameraXController frontController = isFrontface ? this : other;
        if (backController.isFrontface || !frontController.isFrontface) {
            recordCameraDiagnostics("camerax.dual.eligibility", "eligible=false reason=lens_facing");
            return false;
        }

        boolean preferUltraWide = backController.wantsInitialUltraWide();
        CameraSelector[] selectors = CameraXUtils.buildConcurrentCameraSelectors(
                provider, false, true, preferUltraWide);
        if (selectors == null) {
            recordCameraDiagnostics("camerax.dual.eligibility", "advertised_pair=false");
            FileLog.e("CameraX concurrent bind failed: no advertised front/back pair");
            return false;
        }

        diagnosticsBindStartedNanos = System.nanoTime();
        other.diagnosticsBindStartedNanos = diagnosticsBindStartedNanos;
        recordCameraDiagnostics("camerax.dual.eligibility",
                "advertised_pair=true compatibility=" + compatibilityProfile);
        recordCameraDiagnostics("camerax.bind.attempt", "mode=dual");
        try {
            return CameraXProviderCoordinator.withConcurrentOwners(
                    provider, backController, frontController, () -> {

            backController.lifecycle.stop();
            frontController.lifecycle.stop();
            backController.boundCamera = null;
            frontController.boundCamera = null;
            backController.initiated = false;
            frontController.initiated = false;

            Range<Integer> commonFps = compatibilityProfile ? null
                    : CameraXUtils.getCommonSupportedTargetFpsRange(
                            provider, selectors[0], selectors[1]);

            if (!backController.prepareUseCases(true, true, !compatibilityProfile, false,
                    selectors[0], commonFps)
                    || !frontController.prepareUseCases(true, true, !compatibilityProfile,
                    false, selectors[1], commonFps)) {
                throw new IllegalArgumentException(
                        "Concurrent CameraX use-case preparation failed");
            }

            UseCaseGroup firstGroup =
                    new UseCaseGroup.Builder().addUseCase(backController.boundPreview).build();
            UseCaseGroup secondGroup =
                    new UseCaseGroup.Builder().addUseCase(frontController.boundPreview).build();
            List<ConcurrentCamera.SingleCameraConfig> configs = Arrays.asList(
                    new ConcurrentCamera.SingleCameraConfig(
                            selectors[0], firstGroup, backController.lifecycle),
                    new ConcurrentCamera.SingleCameraConfig(
                            selectors[1], secondGroup, backController.lifecycle));

            ConcurrentCamera concurrentCamera = provider.bindToLifecycle(configs);
            List<Camera> cameras = concurrentCamera.getCameras();
            if (cameras == null || cameras.size() != 2) {
                throw new IllegalStateException(
                        "CameraX returned an incomplete concurrent camera pair");
            }
            backController.boundCamera = cameras.get(0);
            frontController.boundCamera = cameras.get(1);
            backController.initiated = true;
            frontController.initiated = true;
            concurrentPeer = other;
            other.concurrentPeer = this;
            backController.attachBoundCamera(backController.boundCamera, true);
            frontController.attachBoundCamera(frontController.boundCamera, true);

            backController.lifecycle.start();
            frontController.lifecycle.start();
            backController.recordCameraDiagnostics("camerax.bind.result", "mode=dual success=true");
            frontController.recordCameraDiagnostics("camerax.bind.result", "mode=dual success=true");

            return true;
                    });
        } catch (Throwable error) {
            lastConcurrentBindBusy = isCameraAlreadyRunningFailure(error);
            other.lastConcurrentBindBusy = lastConcurrentBindBusy;
            recordCameraDiagnostics("camerax.bind.result",
                    "mode=dual success=false busy=" + lastConcurrentBindBusy
                            + " error=" + error.getClass().getSimpleName());
            FileLog.e("CameraX concurrent bind failed", error);
        }
        invalidateBoundCameraControls();
        other.invalidateBoundCameraControls();
        boundCamera = null;
        other.boundCamera = null;
        boundSessionConfig = null;
        other.boundSessionConfig = null;
        appliedTargetFpsRange = null;
        other.appliedTargetFpsRange = null;
        initiated = false;
        other.initiated = false;
        concurrentPeer = null;
        other.concurrentPeer = null;
        return false;
    }

    public boolean wasLastConcurrentBindBusy() {
        return lastConcurrentBindBusy;
    }

    private static boolean isCameraAlreadyRunningFailure(@Nullable Throwable error) {
        Throwable current = error;
        while (current != null) {
            if (current instanceof UnsupportedOperationException) {
                String message = current.getMessage();
                if (message != null && message.toLowerCase(java.util.Locale.US)
                        .contains("already running")) {
                    return true;
                }
            }
            current = current.getCause();
        }
        return false;
    }

    @Override
    public void onCameraXOwnershipInvalidated(long generation) {
        resetLensSelection();
        concurrentPeer = null;
        invalidateBoundCameraControls();
        clearPreparedUseCases();
        lifecycle.stop();
    }

    public boolean rebindSingle() {
        if (closed) return false;
        boolean result = bindUseCases();
        if (result) notifyReady();
        return result;
    }

    private void notifyReady() {
        if (readyCallback == null) return;
        try {
            readyCallback.run();
        } catch (Throwable callbackError) {
            recordCameraDiagnostics("camerax.lifecycle.error",
                    "stage=ready_callback error=" + callbackError.getClass().getSimpleName());
            FileLog.e("CameraX ready callback failed", callbackError);
        }
    }

    private void reportFailure(Throwable throwable) {
        if (failureCallback == null) return;
        try {
            failureCallback.run(throwable);
        } catch (Throwable callbackError) {
            recordCameraDiagnostics("camerax.lifecycle.error",
                    "stage=failure_callback error=" + callbackError.getClass().getSimpleName());
            FileLog.e("CameraX failure callback failed", callbackError);
        }
    }

    public void setZoom(float value) {
        if (!Float.isFinite(value)) return;
        float minimum = getMinZoomRatio();
        setZoomRatio(minimum + Math.max(0f, Math.min(1f, value)) * (getMaxZoomRatio() - minimum));
    }

    public float normalizedForRatio(float ratio) {
        float minimum = getMinZoomRatio();
        float range = getMaxZoomRatio() - minimum;
        return range <= 0f || !Float.isFinite(ratio) ? 0f
                : Math.max(0f, Math.min(1f, (ratio - minimum) / range));
    }

    public void setZoomRatio(float ratio) {
        requestGlobalZoom(ratio, false);
    }

    public void setAnimatedZoomRatio(float ratio) {
        requestGlobalZoom(ratio, true);
    }

    private void requestGlobalZoom(float ratio, boolean animated) {
        if (closed || !Float.isFinite(ratio) || ratio <= 0f) return;
        ++zoomRequestSequence;
        ratio = Math.max(getMinZoomRatio(), Math.min(getMaxZoomRatio(), ratio));
        lastGlobalZoom = ratio;
        explicitLensZoom = true;
        if (lensSwitchPending) {
            pendingGlobalZoom = ratio;
            return;
        }
        if (boundCamera == null || !boundCameraReady) return;
        if (canSwitchLenses()) {
            CameraXUtils.LensRoute target = CameraXOpticalZoom.choose(rearLensRoutes, ratio, preferContinuousRearZoom());
            if (target != null && (selectedLensRoute == null
                    || !target.key.equals(selectedLensRoute.key))) {
                final CameraXUtils.LensRoute previous = selectedLensRoute;
                final ArrayList<CameraXUtils.LensRoute> previousRoutes = new ArrayList<>(rearLensRoutes);
                final float previousZoom = getZoomRatio();
                final int token = ++lensSwitchGeneration;
                pendingGlobalZoom = ratio;
                lensSwitchPending = true;
                selectedLensRoute = target;
                diagnosticsSwitchStartedNanos = System.nanoTime();
                recordCameraDiagnostics("camerax.switch.request", "kind=lens");
                lensSwitchHandler.run(() -> AndroidUtilities.runOnUIThread(() -> {
                    if (closed || token != lensSwitchGeneration || isFrontface || concurrentPeer != null) return;
                    if (!bindUseCases()) {
                        recordCameraDiagnostics("camerax.bind.fallback", "route=previous_lens");
                        if (continuousRearDisabled) {
                            rearLensRoutes = null;
                            selectedLensRoute = null;
                        } else {
                            rearLensRoutes = previousRoutes;
                            rearLensRoutes.remove(target);
                            selectedLensRoute = previous;
                        }
                        pendingGlobalZoom = previousZoom;
                        lensSwitchPending = false;
                        if (!bindUseCases()) {
                            reportFailure(new IllegalStateException("CameraX lens recovery failed"));
                            return;
                        }
                    }
                    notifyReady();
                }));
                return;
            }
        }
        float nativeRatio = ratio / opticalZoomScale();
        if (animated) zoomCoordinator.requestAnimatedZoomRatio(nativeRatio);
        else zoomCoordinator.requestZoomRatio(nativeRatio);
    }

    public float getZoomRatio() {
        if (Float.isFinite(pendingGlobalZoom)) return pendingGlobalZoom;
        float scale = opticalZoomScale();
        if (boundCamera == null) return baseZoomRatio * scale;
        try {
            ZoomState state = boundCamera.getCameraInfo().getZoomState().getValue();
            float observed = state != null ? state.getZoomRatio() : baseZoomRatio;
            return zoomCoordinator.getRequestedOr(observed) * scale;
        } catch (Throwable t) {
            return baseZoomRatio * scale;
        }
    }

    public float getObservedZoomRatio() {
        Camera camera = boundCamera;
        if (camera == null) return opticalZoomScale();
        try {
            ZoomState state = camera.getCameraInfo().getZoomState().getValue();
            return (state == null ? 1f : state.getZoomRatio()) * opticalZoomScale();
        } catch (Throwable error) {
            return opticalZoomScale();
        }
    }

    public boolean hasCurrentCaptureFrame(long surfaceTimestampNanos) {
        return !closed && lensFrameTracker.hasCurrentCaptureFrame(surfaceTimestampNanos);
    }

    @Nullable
    public CameraXLensFrame getLensFrame(long surfaceTimestampNanos) {
        synchronized (lensFrameTracker) {
            CameraXLensFrame frame = closed ? null : lensFrameTracker.getLensFrame(surfaceTimestampNanos);
            if (frame == null || opticalZoomScale() == 1f) return frame;
            return new CameraXLensFrame(frame.physicalId, frame.zoomRatio * opticalZoomScale(), frame.timestampNanos);
        }
    }

    public boolean isInitialLensReady(long surfaceTimestampNanos) {
        if (isFrontface || !wantsInitialUltraWide()) return true;
        if (concurrentPeer != null && CameraXUtils.isOppoCph2791ConcurrentQuirk()) return true;
        Camera camera = boundCamera;
        if (camera == null || !boundCameraReady) return false;
        try {
            CameraXLensFrame frame = getLensFrame(surfaceTimestampNanos);
            String expectedPhysical = expectedInitialPhysicalCameraId;
            if (expectedPhysical != null) {
                return frame != null && expectedPhysical.equals(frame.physicalId);
            }
            ZoomState state = camera.getCameraInfo().getZoomState().getValue();
            if (state == null || state.getMinZoomRatio() >= 0.999f) return true;
            if (frame == null || !CameraXLensFrameTracker.isValidRatio(frame.zoomRatio)) {
                return false;
            }
            float target = state.getMinZoomRatio();
            float tolerance = Math.max(0.025f, target * 0.06f);
            return Math.abs(frame.zoomRatio - target) <= tolerance;
        } catch (Throwable error) {

            return false;
        }
    }

    public boolean isInitialLensReady() {
        if (isFrontface) return true;
        boolean wantsUltraWide = wantsInitialUltraWide();
        if (!wantsUltraWide) return true;

        if (concurrentPeer != null
                && CameraXUtils.isOppoCph2791ConcurrentQuirk()) return true;
        Camera camera = boundCamera;
        if (camera == null || !boundCameraReady) return false;
        try {
            String expectedPhysical = expectedInitialPhysicalCameraId;
            if (expectedPhysical != null) {
                String activePhysical = activePhysicalCameraId;
                if (activePhysical != null) {
                    return expectedPhysical.equals(activePhysical);
                }

                return false;
            }
            ZoomState state = camera.getCameraInfo().getZoomState().getValue();
            if (state == null || state.getMinZoomRatio() >= 0.999f) return true;
            float target = state.getMinZoomRatio();
            float tolerance = Math.max(0.025f, target * 0.06f);
            return Math.abs(state.getZoomRatio() - target) <= tolerance;
        } catch (Throwable error) {

            return false;
        }
    }

    @Nullable
    public String getActivePhysicalCameraId() {
        return activePhysicalCameraId;
    }

    @Nullable
    public String getExpectedInitialPhysicalCameraId() {
        return expectedInitialPhysicalCameraId;
    }

    public float getMinZoomRatio() {
        if (canSwitchLenses()) return CameraXOpticalZoom.minimum(rearLensRoutes);
        if (boundCamera == null) return Math.min(1f, baseZoomRatio);
        try {
            ZoomState state = boundCamera.getCameraInfo().getZoomState().getValue();
            return (state != null ? state.getMinZoomRatio() : Math.min(1f, baseZoomRatio)) * opticalZoomScale();
        } catch (Throwable t) {
            return Math.min(1f, baseZoomRatio);
        }
    }

    public float getMaxZoomRatio() {
        if (canSwitchLenses()) return CameraXOpticalZoom.maximum(rearLensRoutes);
        if (boundCamera == null) return Math.max(1f, baseZoomRatio);
        try {
            ZoomState state = boundCamera.getCameraInfo().getZoomState().getValue();
            return (state != null ? state.getMaxZoomRatio() : Math.max(1f, baseZoomRatio)) * opticalZoomScale();
        } catch (Throwable t) {
            return Math.max(1f, baseZoomRatio);
        }
    }

    public float[] getZoomShortcuts() {
        Camera camera = boundCamera;
        if (camera == null || !isZoomShortcutsReady()) return new float[0];
        try {
            ZoomState state = camera.getCameraInfo().getZoomState().getValue();
            if (state == null) return new float[0];
            if (canSwitchLenses()) {
                float[] candidates = new float[rearLensRoutes.size() + zoomShortcutCandidates.length];
                for (int i = 0; i < rearLensRoutes.size(); i++) candidates[i] = rearLensRoutes.get(i).opticalRatio;
                System.arraycopy(zoomShortcutCandidates, 0, candidates, rearLensRoutes.size(), zoomShortcutCandidates.length);
                return CameraXZoomShortcuts.select(getMinZoomRatio(), getMaxZoomRatio(), candidates);
            }
            return CameraXZoomShortcuts.select(state.getMinZoomRatio(), state.getMaxZoomRatio(), zoomShortcutCandidates);
        } catch (Throwable ignored) {
            return new float[0];
        }
    }

    public boolean isZoomShortcutsReady() {
        Camera camera = boundCamera;
        if (closed || camera == null) return false;
        try {
            ZoomState state = camera.getCameraInfo().getZoomState().getValue();
            if (state == null) return false;
            float min = state.getMinZoomRatio();
            float max = state.getMaxZoomRatio();
            return Float.isFinite(min) && Float.isFinite(max) && min > 0f && max >= min;
        } catch (Throwable ignored) {
            return false;
        }
    }

    public float resetZoom() {
        Camera camera = boundCamera;
        if (camera == null || !boundCameraReady) return 0f;
        setZoomRatio(getMinZoomRatio());
        return 0f;
    }

    @SuppressLint("UnsafeExperimentalUsageError")
    public boolean isExposureCompensationSupported() {
        if (boundCamera == null) return false;
        try {
            ExposureState es = boundCamera.getCameraInfo().getExposureState();
            return es != null && es.isExposureCompensationSupported();
        } catch (Throwable t) {
            return false;
        }
    }

    private volatile Float exposureOverride;

    @SuppressLint("UnsafeExperimentalUsageError")
    public void setExposureCompensation(float value0to1) {
        exposureOverride = Math.abs(value0to1 - 0.5f) < 0.001f ? null : value0to1;
        Camera camera = boundCamera;
        final int generation = boundCameraGeneration;
        if (camera == null || !boundCameraReady) return;
        try {
            ExposureState es = camera.getCameraInfo().getExposureState();
            if (es == null || !es.isExposureCompensationSupported()) return;
            int lo = es.getExposureCompensationRange().getLower();
            int hi = es.getExposureCompensationRange().getUpper();
            int idx = CameraXUtils.normalizedExposureToIndex(value0to1, lo, hi);
            ListenableFuture<Integer> result =
                    camera.getCameraControl().setExposureCompensationIndex(idx);
            trackControlFuture(result, camera, generation,
                    "CameraX exposure compensation", null);
        } catch (Throwable t) {
            FileLog.e(t);
        }
    }

    public int setExposureCompensationIndex(int index) {
        Camera camera = boundCamera;
        final int generation = boundCameraGeneration;
        if (camera == null || !boundCameraReady) return Integer.MIN_VALUE;
        try {
            ExposureState es = camera.getCameraInfo().getExposureState();
            if (es == null || !es.isExposureCompensationSupported()) return Integer.MIN_VALUE;
            int lo = es.getExposureCompensationRange().getLower();
            int hi = es.getExposureCompensationRange().getUpper();
            int clamped = Math.max(lo, Math.min(hi, index));
            ListenableFuture<Integer> result =
                    camera.getCameraControl().setExposureCompensationIndex(clamped);
            int configured =
                    CameraXUtils.exposureIndexToConfigured(clamped, lo, hi);
            trackControlFuture(result, camera, generation,
                    "CameraX exposure compensation",
                    () -> com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.setCameraExposureIndex(configured));
            return clamped;
        } catch (Throwable t) {
            FileLog.e(t);
            return Integer.MIN_VALUE;
        }
    }

    public int getExposureCompensationIndex() {
        if (boundCamera == null) return 0;
        try {
            ExposureState es = boundCamera.getCameraInfo().getExposureState();
            if (es == null) return 0;
            return es.getExposureCompensationIndex();
        } catch (Throwable t) {
            return 0;
        }
    }

    @SuppressLint({"UnsafeExperimentalUsageError", "RestrictedApi"})
    public void setTargetOrientation(int rotation) {
        targetRotation = CameraXUtils.toSurfaceRotation(rotation);
        imageCaptureTargetRotation = targetRotation;
        if (boundPreview != null) {
            try { boundPreview.setTargetRotation(targetRotation); } catch (Throwable t) { FileLog.e(t); }
        }
        if (boundImageCapture != null) {
            try { boundImageCapture.setTargetRotation(targetRotation); } catch (Throwable t) { FileLog.e(t); }
        }
    }

    @SuppressLint({"UnsafeExperimentalUsageError", "RestrictedApi"})
    public void setImageCaptureTargetOrientation(int rotation) {
        imageCaptureTargetRotation = CameraXUtils.toSurfaceRotation(rotation);
        if (boundImageCapture != null) {
            try {
                boundImageCapture.setTargetRotation(imageCaptureTargetRotation);
            } catch (Throwable t) {
                FileLog.e(t);
            }
        }
    }

    @SuppressLint({"UnsafeExperimentalUsageError", "RestrictedApi"})
    public void setWorldCaptureOrientation(int rotation) {
        setImageCaptureTargetOrientation(rotation);
    }

    @SuppressLint({"UnsafeExperimentalUsageError", "RestrictedApi"})
    public boolean focusToPoint(float x, float y) {
        return focusToPoint(x, y, false);
    }

    @SuppressLint({"UnsafeExperimentalUsageError", "RestrictedApi"})
    public boolean focusToPoint(float x, float y, boolean lock) {
        Camera camera = boundCamera;
        if (!isZoomReady() || camera == null || boundPreview == null
                || !Float.isFinite(x) || !Float.isFinite(y) || x < 0f || x > 1f || y < 0f || y > 1f) return false;
        try {
            MeteringPoint point = new androidx.camera.core.SurfaceOrientedMeteringPointFactory(1f, 1f, boundPreview)
                    .createPoint(x, y);
            FocusMeteringAction.Builder builder = new FocusMeteringAction.Builder(point,
                    FocusMeteringAction.FLAG_AF | FocusMeteringAction.FLAG_AE);
            if (lock) builder.disableAutoCancel();
            else builder.setAutoCancelDuration(3, TimeUnit.SECONDS);
            FocusMeteringAction action = builder.build();
            if (!camera.getCameraInfo().isFocusMeteringSupported(action)) return false;
            trackControlFuture(camera.getCameraControl().startFocusAndMetering(action), camera,
                    boundCameraGeneration, "CameraX tap focus", null);
            return true;
        } catch (Throwable error) {
            FileLog.e("CameraX tap focus failed", error);
            return false;
        }
    }

    public void focusToPoint(int x, int y) {
        focusAndLock(x, y, false, false);
    }

    @SuppressLint({"UnsafeExperimentalUsageError", "RestrictedApi"})
    public boolean focusAndLock(int x, int y, boolean lockAE, boolean lockAF) {
        if (boundCamera == null || meteringPointFactory == null) return false;
        try {
            MeteringPoint point = meteringPointFactory.createPoint(x, y);
            return focusAndLock(point, lockAE, lockAF);
        } catch (Throwable t) {
            FileLog.e(t);
            return false;
        }
    }

    @SuppressLint({"UnsafeExperimentalUsageError", "RestrictedApi"})
    public boolean focusAndLock(@NonNull MeteringPoint point, boolean lockAE, boolean lockAF) {
        if (boundCamera == null) return false;
        try {
            int flags = 0;
            if (lockAF) flags |= FocusMeteringAction.FLAG_AF;
            if (lockAE) flags |= FocusMeteringAction.FLAG_AE;
            if (flags == 0) flags = FocusMeteringAction.FLAG_AF | FocusMeteringAction.FLAG_AE;
            FocusMeteringAction.Builder b = new FocusMeteringAction.Builder(point, flags);
            if (lockAE || lockAF) {
                b.disableAutoCancel();
            } else {
                b.setAutoCancelDuration(3, TimeUnit.SECONDS);
            }
            boundCamera.getCameraControl().startFocusAndMetering(b.build());
            return true;
        } catch (Throwable t) {
            FileLog.e(t);
            return false;
        }
    }

    public void cancelFocusAndLock() {
        if (boundCamera == null) return;
        try {
            boundCamera.getCameraControl().cancelFocusAndMetering();
        } catch (Throwable ignored) {
        }
    }

    @SuppressLint({"RestrictedApi", "MissingPermission"})
    public void recordVideo(final File path, boolean mirror, BaseCameraView.VideoSavedCallback onStop) {
        if (onStop != null) onStop.onFinishVideoRecording(null, 0L);
    }

    @SuppressLint("RestrictedApi")
    public void stopVideoRecording(final boolean abandon) {

    }

    public void takePicture(final File file, Runnable onTake) {
        takePicture(file, onTake, null);
    }

    public boolean takePicture(final File file, @Nullable Runnable onTake,
                               @Nullable org.telegram.messenger.Utilities.Callback<Throwable> onError) {
        ImageCapture capture = boundImageCapture;
        if (capture == null || file == null) {
            return false;
        }
        try {
            ImageCapture.Metadata metadata = new ImageCapture.Metadata();

            metadata.setReversedHorizontal(isFrontface);
            ImageCapture.OutputFileOptions options =
                    new ImageCapture.OutputFileOptions.Builder(file)
                            .setMetadata(metadata)
                            .build();
            capture.takePicture(options,
                    androidx.core.content.ContextCompat.getMainExecutor(ApplicationLoader.applicationContext),
                    new ImageCapture.OnImageSavedCallback() {
                        @Override
                        public void onImageSaved(@NonNull ImageCapture.OutputFileResults outputFileResults) {
                            if (onTake != null) onTake.run();
                        }

                        @Override
                        public void onError(@NonNull ImageCaptureException exception) {
                            FileLog.e(exception);
                            if (onError != null) onError.run(exception);
                        }
                    });
            return true;
        } catch (Throwable t) {
            FileLog.e(t);
            if (onError != null) onError.run(t);
            return false;
        }
    }

    public boolean canTakePicture() {
        return !closed && boundImageCapture != null;
    }

    public int getImageCaptureRotationDegrees() {
        ImageCapture capture = boundImageCapture;
        if (capture == null) return 0;
        try {
            androidx.camera.core.ResolutionInfo info = capture.getResolutionInfo();
            return info == null ? 0 : ((info.getRotationDegrees() % 360) + 360) % 360;
        } catch (Throwable t) {
            return 0;
        }
    }

    @SuppressLint("RestrictedApi")
    public Size getPreviewSize() {
        return targetResolution == null ? new Size(0, 0)
                : new Size(targetResolution.getWidth(), targetResolution.getHeight());
    }

    public int getDisplayOrientation() {
        WindowManager mgr = (WindowManager) ApplicationLoader.applicationContext.getSystemService(Context.WINDOW_SERVICE);
        return mgr.getDefaultDisplay().getRotation();
    }

    @SuppressLint("UnsafeOptInUsageError")
    public int getCameraId() {
        if (boundCamera == null) return 0;
        try {
            return Camera2CameraInfo.from(boundCamera.getCameraInfo()).getCameraId().hashCode();
        } catch (Throwable ignored) {
            return 0;
        }
    }

    public void attachBoundCamera(@Nullable Camera camera) {
        attachBoundCamera(camera, false);
    }

    private void attachBoundCamera(@Nullable Camera camera, boolean preparedGraph) {
        if (preparedGraph && camera != null) {

            resetBoundCameraControls();
        } else {
            invalidateBoundCameraControls();
        }
        this.boundCamera = camera;
        baseZoomRatio = 1f;
        if (camera == null) return;

        final int generation = boundCameraGeneration;
        diagnosticsErrorMask = 0;
        if (hasOptimizedRearGraph()) {
            AndroidUtilities.runOnUIThread(() -> {
                if (!currentGraphProducedFrames) recoverContinuousRear(camera, generation);
            }, 2500L);
        }
        zoomCoordinator.attach(camera, generation);
        final CameraSelector shortcutSelector = boundSelector;
        float[] cachedShortcuts = CameraXUtils.getCachedZoomShortcuts(camera.getCameraInfo(), shortcutSelector);
        if (cachedShortcuts != null) {
            zoomShortcutCandidates = cachedShortcuts;
        }
        if (cachedShortcuts == null) org.telegram.messenger.Utilities.globalQueue.postRunnable(() -> {
            float[] discovered;
            try {
                discovered = CameraXUtils.findZoomShortcutCandidates(
                        camera.getCameraInfo(), shortcutSelector);
            } catch (Throwable ignored) {
                discovered = new float[0];
            }
            final float[] candidates = discovered;
            AndroidUtilities.runOnUIThread(() -> {
                if (!closed && camera == boundCamera && generation == boundCameraGeneration) {
                    zoomShortcutCandidates = candidates;
                }
            });
        });
        try {
            boundCameraState = camera.getCameraInfo().getCameraState();
            boundCameraStateObserver = state -> {
                if (state == null || camera != boundCamera
                        || generation != boundCameraGeneration) {
                    return;
                }
                CameraState.StateError stateError = state.getError();
                if (stateError != null) {
                    boundCameraReady = false;
                    torchSubmittedCamera = null;
                    torchSubmittedFuture = null;
                    zoomCoordinator.setReady(camera, generation, false);
                    Throwable cause = stateError.getCause();
                    int diagnosticsErrorBit = 1 << (stateError.getCode() & 31);
                    if ((diagnosticsErrorMask & diagnosticsErrorBit) == 0) {
                        diagnosticsErrorMask |= diagnosticsErrorBit;
                        recordCameraDiagnostics("camerax.lifecycle.error",
                                "stage=state code=" + stateError.getCode() + " error="
                                        + (cause == null ? "none" : cause.getClass().getSimpleName()));
                    }
                    FileLog.e("CameraX camera state error " + stateError.getCode(),
                            cause != null ? cause : new IllegalStateException(
                                    "CameraX state error " + stateError.getCode()));
                    if (hasOptimizedRearGraph()
                            && (stateError.getCode() == CameraState.ERROR_CAMERA_IN_USE
                            || stateError.getCode() == CameraState.ERROR_MAX_CAMERAS_IN_USE
                            || stateError.getCode() == CameraState.ERROR_STREAM_CONFIG
                            || stateError.getCode() == CameraState.ERROR_CAMERA_FATAL_ERROR)) {
                        AndroidUtilities.runOnUIThread(() -> recoverContinuousRear(camera, generation));
                    } else if ((stateError.getCode() == CameraState.ERROR_CAMERA_IN_USE
                            || stateError.getCode() == CameraState.ERROR_MAX_CAMERAS_IN_USE)
                            && concurrentPeer != null) {
                        scheduleConcurrentCameraInUseFailure(
                                camera, generation, stateError.getCode(), cause);
                    } else if (stateError.getCode() == CameraState.ERROR_STREAM_CONFIG
                            || stateError.getCode() == CameraState.ERROR_CAMERA_FATAL_ERROR) {
                        AndroidUtilities.runOnUIThread(() -> {
                            if (camera == boundCamera
                                    && generation == boundCameraGeneration) {
                                reportFailure(cause != null ? cause
                                        : new IllegalStateException(
                                                "CameraX state error "
                                                        + stateError.getCode()));
                            }
                        });
                    }
                    return;
                }
                if (state.getType() == CameraState.Type.OPEN) {
                    cancelConcurrentCameraInUseFailure();

                    boundCameraReady = true;
                    if (diagnosticsReadyGeneration != generation) {
                        diagnosticsReadyGeneration = generation;
                        recordCameraDiagnostics("camerax.ready", "stage=open elapsed_ms="
                                + (diagnosticsBindStartedNanos == 0 ? -1
                                        : (System.nanoTime() - diagnosticsBindStartedNanos) / 1_000_000L));
                        if (diagnosticsSwitchStartedNanos != 0) {
                            recordCameraDiagnostics("camerax.switch.complete", "stage=open elapsed_ms="
                                    + ((System.nanoTime() - diagnosticsSwitchStartedNanos) / 1_000_000L));
                            diagnosticsSwitchStartedNanos = 0;
                        }
                    }
                    final boolean initialZoomPrepared =
                            prepareInitialZoomIfGraphReady(camera, generation);
                    zoomCoordinator.setReady(camera, generation, true);
                    prepareConcurrentPeerInitialZoomIfReady();

                    AndroidUtilities.runOnUIThread(() -> {
                        if (camera != boundCamera
                                || generation != boundCameraGeneration
                                || closed) {
                            return;
                        }
                        if (!initialZoomPrepared) {

                            prepareInitialZoomIfGraphReady(camera, generation);
                            prepareConcurrentPeerInitialZoomIfReady();
                        }
                        applyDeferredInitialCameraControls(camera, generation);
                    }, 120L);
                } else if (state.getType() == CameraState.Type.CLOSING
                        || state.getType() == CameraState.Type.CLOSED) {
                    boundCameraReady = false;
                    torchSubmittedCamera = null;
                    torchSubmittedFuture = null;
                    zoomCoordinator.setReady(camera, generation, false);
                }
            };
            boundCameraState.observe(lifecycle, boundCameraStateObserver);
        } catch (Throwable t) {
            recordCameraDiagnostics("camerax.lifecycle.error",
                    "stage=observe error=" + t.getClass().getSimpleName());
            FileLog.e(t);
        }
    }

    private void recordCameraDiagnostics(String event, String state) {
        com.th3nekit.finegram.core.crash.CameraDiagnostics.record(event,
                "engine=camerax generation=" + boundCameraGeneration + " camera=" + cameraId(boundCamera)
                        + " front=" + isFrontface + " dual=" + (concurrentPeer != null) + " " + state);
    }

    private static String cameraId(@Nullable Camera camera) {
        if (camera == null) return "null";
        try {
            return Camera2CameraInfo.from(camera.getCameraInfo()).getCameraId();
        } catch (Throwable error) {
            return "unknown:" + error.getClass().getSimpleName();
        }
    }

    private synchronized boolean prepareInitialZoom(Camera camera, int generation) {
        if (camera != boundCamera || generation != boundCameraGeneration
                || !boundCameraReady) {
            return false;
        }
        if (initialZoomPreparedGeneration == generation) {
            return true;
        }
        try {
            ZoomState state = camera.getCameraInfo().getZoomState().getValue();
            if (state == null) {
                return false;
            }
            boolean startFromUltraWide = wantsInitialUltraWide()
                    && CameraXUtils.supportsSubOneZoom(provider, boundSelector)
                    && (concurrentPeer == null
                    || !CameraXUtils.isOppoCph2791ConcurrentQuirk());
            baseZoomRatio = CameraXUtils.getBaseZoomRatio(
                    state, startFromUltraWide);
            float requested = pendingGlobalZoom;
            if (Float.isFinite(requested)) {
                baseZoomRatio = Math.max(state.getMinZoomRatio(), Math.min(state.getMaxZoomRatio(),
                        requested / opticalZoomScale()));
            }
            zoomCoordinator.requestZoomRatio(baseZoomRatio);
            lastGlobalZoom = baseZoomRatio * opticalZoomScale();
            initialZoomPreparedGeneration = generation;
            pendingGlobalZoom = Float.NaN;
            lensSwitchPending = false;
            if (Float.isFinite(requested)) {
                long sequence = zoomRequestSequence;
                AndroidUtilities.runOnUIThread(() -> {
                    if (!closed && camera == boundCamera && generation == boundCameraGeneration
                            && sequence == zoomRequestSequence) {
                        requestGlobalZoom(requested, false);
                    }
                });
            }
            return true;
        } catch (Throwable t) {
            FileLog.e(t);
            return false;
        }
    }

    private boolean prepareInitialZoomIfGraphReady(Camera camera, int generation) {

        return prepareInitialZoom(camera, generation);
    }

    private void prepareConcurrentPeerInitialZoomIfReady() {
        CameraXController peer = concurrentPeer;
        if (peer == null || !boundCameraReady || !peer.boundCameraReady) return;
        Camera peerCamera = peer.boundCamera;
        if (peerCamera != null) {
            peer.prepareInitialZoom(peerCamera, peer.boundCameraGeneration);
        }
    }

    private boolean wantsInitialUltraWide() {
        return !platformFallbackGraph && !isFrontface
                && !Float.isFinite(pendingGlobalZoom) && !lensSwitchPending
                && (!explicitLensZoom || selectedLensRoute == null || selectedLensRoute.opticalRatio < .95f)
                && ((useConfiguredUltraWide && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getStartFromUltraWideCam())
                || selectedEffect == CAMERA_WIDE);
    }

    private void applyDeferredInitialCameraControls(Camera camera, int generation) {
        if (camera != boundCamera || generation != boundCameraGeneration
                || !boundCameraReady) {
            return;
        }
        try {
            ExposureState exposure = camera.getCameraInfo().getExposureState();
            if (exposure != null && exposure.isExposureCompensationSupported()) {
                int lower = exposure.getExposureCompensationRange().getLower();
                int upper = exposure.getExposureCompensationRange().getUpper();
                Float override = exposureOverride;
                int index = override != null
                        ? CameraXUtils.normalizedExposureToIndex(override, lower, upper)
                        : CameraXUtils.configuredExposureToIndex(
                        com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getCameraExposureIndex(), lower, upper);
                ListenableFuture<Integer> exposureResult =
                        camera.getCameraControl().setExposureCompensationIndex(index);
                trackControlFuture(exposureResult, camera, generation,
                        "CameraX initial exposure", null);
            }
        } catch (Throwable t) {
            FileLog.e(t);
        }
        applyRequestedTorch();
    }

    private void invalidateBoundCameraControls() {
        retireLensCaptureGraph();
        resetBoundCameraControls();
    }

    private void resetBoundCameraControls() {
        zoomShortcutCandidates = new float[0];
        torchSubmittedCamera = null;
        torchSubmittedFuture = null;
        cancelConcurrentCameraInUseFailure();
        boundCameraReady = false;
        zoomCoordinator.detach();
        ++boundCameraGeneration;
        try {
            if (boundCameraState != null && boundCameraStateObserver != null) {
                boundCameraState.removeObserver(boundCameraStateObserver);
            }
        } catch (Throwable error) {
            FileLog.e(error);
        }
        boundCameraState = null;
        boundCameraStateObserver = null;
    }

    private void scheduleConcurrentCameraInUseFailure(
            Camera camera, int generation, int errorCode, @Nullable Throwable cause) {
        if (concurrentCameraInUseFailureRunnable != null) return;
        Runnable task = new Runnable() {
            @Override
            public void run() {
                if (concurrentCameraInUseFailureRunnable != this) return;
                concurrentCameraInUseFailureRunnable = null;
                if (camera != boundCamera || generation != boundCameraGeneration
                        || concurrentPeer == null || boundCameraReady || closed) {
                    return;
                }
                ConcurrentCameraResourceException persistentError =
                        new ConcurrentCameraResourceException(
                        "Concurrent CameraX camera remained unavailable, error=" + errorCode,
                        cause);
                reportFailure(persistentError);
            }
        };
        concurrentCameraInUseFailureRunnable = task;

        AndroidUtilities.runOnUIThread(task, 1400L);
    }

    private void cancelConcurrentCameraInUseFailure() {
        Runnable task = concurrentCameraInUseFailureRunnable;
        concurrentCameraInUseFailureRunnable = null;
        if (task != null) {
            AndroidUtilities.cancelRunOnUIThread(task);
        }
    }

    private void trackControlFuture(ListenableFuture<?> future, Camera camera,
                                    int generation, String operation,
                                    @Nullable Runnable onSuccess) {
        if (future == null) return;
        future.addListener(() -> {
            try {
                future.get();
                if (camera == boundCamera && generation == boundCameraGeneration
                        && onSuccess != null) {
                    onSuccess.run();
                }
            } catch (Throwable error) {
                Throwable cause = error instanceof ExecutionException
                        && error.getCause() != null ? error.getCause() : error;
                boolean stale = camera != boundCamera
                        || generation != boundCameraGeneration;
                if (!stale && !(cause instanceof CancellationException)
                        && !(cause instanceof CameraControl.OperationCanceledException)) {
                    FileLog.e(operation + " failed", cause);
                }
            }
        }, ContextCompat.getMainExecutor(ApplicationLoader.applicationContext));
    }

    public void attachProvider(@Nullable ProcessCameraProvider provider) {
        this.provider = provider;
    }

    @Nullable
    public Camera getBoundCamera() {
        return boundCamera;
    }

    @NonNull
    public CameraSelector applyExtensionMode(@NonNull CameraSelector base) {
        int mode = EXTENSION_MODE_NONE;
        if (selectedEffect == CAMERA_HDR) {
            mode = EXTENSION_MODE_HDR;
        } else if (selectedEffect == CAMERA_NIGHT) {
            mode = EXTENSION_MODE_NIGHT;
        } else if (selectedEffect == CAMERA_AUTO) {
            mode = EXTENSION_MODE_AUTO;
        }
        if (mode == EXTENSION_MODE_NONE) return base;
        CameraSelector wrapped = wrapWithExtensionMode(base, mode);
        return wrapped != null ? wrapped : base;
    }

    private static final int EXTENSION_MODE_NONE = 0;
    private static final int EXTENSION_MODE_BOKEH = 1;
    private static final int EXTENSION_MODE_HDR = 2;
    private static final int EXTENSION_MODE_NIGHT = 3;
    private static final int EXTENSION_MODE_FACE_RETOUCH = 4;
    private static final int EXTENSION_MODE_AUTO = 5;

    @Nullable private static volatile Boolean sExtensionsClassAvailable;

    private static boolean extensionsClassAvailable() {
        Boolean cached = sExtensionsClassAvailable;
        if (cached != null) return cached;
        try {
            Class.forName("androidx.camera.extensions.ExtensionsManager");
            sExtensionsClassAvailable = Boolean.TRUE;
            return true;
        } catch (Throwable t) {
            sExtensionsClassAvailable = Boolean.FALSE;
            return false;
        }
    }

    @NonNull
    private CameraSelector currentSelector() {
        return provider != null ? CameraXUtils.buildIntendedCameraSelector(
                provider, isFrontface, useConfiguredUltraWide)
                : (isFrontface ? CameraSelector.DEFAULT_FRONT_CAMERA : CameraSelector.DEFAULT_BACK_CAMERA);
    }

    private boolean isExtensionAvailable(@NonNull CameraSelector selector, int mode) {
        if (provider == null) return false;
        if (!extensionsClassAvailable()) return false;
        try {
            Object manager = getExtensionsManagerBlocking();
            if (manager == null) return false;
            Method isAvail = manager.getClass().getMethod("isExtensionAvailable", CameraSelector.class, int.class);
            Object result = isAvail.invoke(manager, selector, mode);
            return Boolean.TRUE.equals(result);
        } catch (Throwable t) {
            return false;
        }
    }

    @Nullable
    private CameraSelector wrapWithExtensionMode(@NonNull CameraSelector base, int mode) {
        if (provider == null) return null;
        if (!extensionsClassAvailable()) return null;
        try {
            Object manager = getExtensionsManagerBlocking();
            if (manager == null) return null;
            Method isAvail = manager.getClass().getMethod("isExtensionAvailable", CameraSelector.class, int.class);
            if (!Boolean.TRUE.equals(isAvail.invoke(manager, base, mode))) return null;
            Method wrap = manager.getClass().getMethod("getExtensionEnabledCameraSelector", CameraSelector.class, int.class);
            Object wrapped = wrap.invoke(manager, base, mode);
            return wrapped instanceof CameraSelector ? (CameraSelector) wrapped : null;
        } catch (Throwable t) {
            return null;
        }
    }

    @Nullable
    private Object getExtensionsManagerBlocking() {
        try {
            Class<?> mgrClass = Class.forName("androidx.camera.extensions.ExtensionsManager");
            Method async = mgrClass.getMethod("getInstanceAsync", Context.class, ProcessCameraProvider.class);
            Object future = async.invoke(null, ApplicationLoader.applicationContext, provider);
            if (future instanceof ListenableFuture) {
                return ((ListenableFuture<?>) future).get(2, TimeUnit.SECONDS);
            }
            return null;
        } catch (Throwable t) {
            return null;
        }
    }

    @IntDef({CAMERA_NONE, CAMERA_AUTO, CAMERA_HDR, CAMERA_NIGHT})
    @Retention(RetentionPolicy.SOURCE)
    @RestrictTo(RestrictTo.Scope.LIBRARY_GROUP)
    public @interface EffectFacing {
    }

}

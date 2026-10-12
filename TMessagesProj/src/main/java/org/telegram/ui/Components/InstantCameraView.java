/*
 * This is the source code of Telegram for Android v. 5.x.x.
 * It is licensed under GNU GPL v. 2 or later.
 * You should have received a copy of the license in this archive (see LICENSE).
 *
 * Copyright Nikolai Kudashov, 2013-2018.
 */

package org.telegram.ui.Components;

import com.th3nekit.finegram.camera.RoundZoomControl;
import com.th3nekit.finegram.camera.RoundZoomScale;
import com.th3nekit.finegram.camera.CameraXFrameCrossfade;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.Manifest;
import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ImageFormat;
import android.graphics.Outline;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTimestamp;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.media.MediaRecorder;
import android.net.Uri;
import android.opengl.EGL14;
import android.opengl.EGLExt;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.opengl.GLUtils;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.SystemClock;
import android.view.Gravity;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewOutlineProvider;
import android.view.WindowManager;
import android.view.animation.DecelerateInterpolator;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.core.graphics.ColorUtils;

import androidx.media3.exoplayer.ExoPlayer;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.AutoDeleteMediaTask;
import org.telegram.messenger.BuildVars;
import org.telegram.messenger.DispatchQueue;
import org.telegram.messenger.FileLoader;
import org.telegram.messenger.FileLog;
import org.telegram.messenger.ImageLoader;
import org.telegram.messenger.ImageReceiver;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.MediaController;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.R;
import org.telegram.messenger.SharedConfig;
import org.telegram.messenger.UserConfig;
import org.telegram.messenger.Utilities;
import org.telegram.messenger.VideoEditedInfo;
import org.telegram.messenger.camera.Camera2Session;
import org.telegram.messenger.camera.CameraController;
import org.telegram.messenger.camera.CameraInfo;
import org.telegram.messenger.camera.CameraSession;
import org.telegram.messenger.camera.Size;
import org.telegram.messenger.video.MP4Builder;
import org.telegram.messenger.video.Mp4Movie;
import org.telegram.tgnet.ConnectionsManager;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.blur3.BlurredBackgroundDrawableViewFactory;
import org.telegram.ui.Components.blur3.drawable.BlurredBackgroundDrawable;
import org.telegram.ui.Components.blur3.drawable.color.BlurredBackgroundColorProvider;
import org.telegram.ui.Components.voip.CellFlickerDrawable;
import org.telegram.ui.Stories.recorder.DualCameraView;
import org.telegram.ui.Stories.recorder.FlashViews;
import org.telegram.ui.Stories.recorder.StoryEntry;

import com.th3nekit.finegram.camera.SlideControlView;
import com.th3nekit.finegram.camera.CameraXRoundLensTransition;
import com.th3nekit.finegram.camera.CameraXLensFrame;
import com.th3nekit.finegram.camera.CameraXSurfaceSession;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Timer;
import java.util.TimerTask;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.CountDownLatch;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;

@SuppressLint("ViewConstructor")
public class InstantCameraView extends InstantCameraViewBase implements NotificationCenter.NotificationCenterDelegate {

    public boolean WRITE_TO_FILE_IN_BACKGROUND;

    private int currentAccount = UserConfig.selectedAccount;
    private InstantViewCameraContainer cameraContainer;
    private Delegate delegate;
    private Paint paint;
    private RectF rect;
    private final FlashViews.ImageViewInvertable switchCameraButton;
    private final FlashViews.ImageViewInvertable flashButton;
    private final FlashViews flashViews;
    private RLottieDrawable flashOnDrawable, flashOffDrawable;
    private RLottieDrawable switchCameraDrawable;
    private ImageView muteImageView;
    private float progress;
    private CameraInfo selectedCamera;
    private boolean isFrontface = true;
    private volatile boolean cameraReady;
    private AnimatorSet muteAnimation;
    private TLRPC.InputFile file;
    private TLRPC.InputEncryptedFile encryptedFile;
    private byte[] key;
    private byte[] iv;
    private long size;
    private boolean isSecretChat;
    @Nullable
    private VideoEditedInfo videoEditedInfo;

    private int recordedVideoBitrate = 1000000;
    private VideoPlayer videoPlayer;
    private Bitmap lastBitmap;

    private int cameraCoverGeneration;
    private int recordingGuid;

    private volatile boolean cameraTextureAvailable;
    private final int[] position = new int[2];
    private volatile int[] cameraTexture = new int[] { Integer.MIN_VALUE, Integer.MIN_VALUE };
    private final int[] oldCameraTexture = new int[1];
    private float cameraTextureAlpha = 1.0f;
    private float cameraTextureAlphaProgress = 1.0f;

    private AnimatorSet animatorSet;

    private boolean deviceHasGoodCamera;
    private boolean requestingPermissions;
    private File cameraFile;

    private Object pausePreviewToken;
    private long recordStartTime;

    private VideoRecorder heavyOperationsOwner;
    private long recordPlusTime;
    private boolean recording;
    private long recordedTime;
    private boolean cancelled;

    private volatile CameraGLThread cameraThread;
    private volatile int cameraThreadGeneration;
    private Size[] previewSize = new Size[2];
    private Size pictureSize;
    private Size aspectRatio = SharedConfig.roundCamera16to9 ? new Size(16, 9) : new Size(4, 3);
    private TextureView textureView;
    private BackupImageView textureOverlayView;
    private final boolean useCameraX =
            com.th3nekit.finegram.camera.CameraXUtils.isCurrentCameraCameraX();
    private final boolean useCamera2 =
            com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getCameraType() == com.th3nekit.finegram.camera.CameraXUtils.CAMERA_2;
    private final com.th3nekit.finegram.camera.VideoMessagesHelper videoMessagesHelper =
            new com.th3nekit.finegram.camera.VideoMessagesHelper();
    private CameraSession cameraSession;
    private boolean bothCameras;
    private Camera2Session[] camera2Sessions = new Camera2Session[2];
    private Camera2Session camera2SessionCurrent;

    private int nmCamera2SwitchGeneration;
    private volatile boolean nmCamera2SwitchPending;
    private boolean needDrawFlickerStub;

    private boolean isCameraSessionInitiated() {
        if (useCameraX) {
            return videoMessagesHelper.isInitiated();
        } else if (useCamera2) {
            return camera2SessionCurrent != null && camera2SessionCurrent.isInitiated();
        } else {
            return cameraSession != null && cameraSession.isInitied();
        }
    }

    public boolean isCameraXFrontFacing() {
        return isFrontface;
    }

    public void onCameraXSessionReady(
            com.th3nekit.finegram.camera.CameraXSurfaceSession session,
            int width, int height) {

        if (!useCameraX || session == null || !session.hasTransformationInfo()) return;
        int index = videoMessagesHelper.getSessionIndex(session);
        if (index < 0 || index >= previewSize.length) return;

        previewSize[index] = new Size(Math.max(1, width), Math.max(1, height));
        CameraGLThread thread = cameraThread;
        if (thread != null) {
            boolean currentSession = session == videoMessagesHelper.getCurrentSession();
            if (currentSession && cameraXSingleSwitchAwaitingBind
                    && session.isFrontFacing() == isFrontface) {

                pendingCameraXSingleSessionGeneration = session.getSurfaceRequestGeneration();
                pendingCameraXSingleSession = session;
                updateFlash();
                return;
            }
            thread.setCurrentSession(session);
            if (currentSession) {

                updateFlash();
            }

            thread.refreshPreviewGeometry();
        }
    }

    public void onCameraXAttemptStarting(boolean dual) {
        cancelCameraXRearLensTransition();
        nmCancelCameraXDualFrameWatchdog();
        nmCancelCameraXInitialWideWait();
        nmCameraXActiveFrameObserved = false;
        nmCameraXFrameMask = 0;
        cameraReady = false;

        cameraXInitialWideWaitActive = (!isFrontface || dual)
                && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getStartFromUltraWideCam();
        cameraXInitialWideWaitStartedMs = cameraXInitialWideWaitActive
                ? SystemClock.elapsedRealtime() : 0L;
        cameraXInitialWideWaitTimeoutMs = dual
                ? NM_CAMERAX_DUAL_INITIAL_WIDE_TIMEOUT_MS
                : NM_CAMERAX_INITIAL_WIDE_TIMEOUT_MS;
        cameraXInitialWideTimeoutRenderPending = false;
        if (cameraXInitialWideWaitActive) {
            nmArmCameraXInitialWideTimeout();
        }
        CameraGLThread thread = cameraThread;
        if (thread != null) {
            thread.resetCameraXFrameState();
        }
        if (!useCameraX || !dual || cancelled) {
            return;
        }
        final int watchdogGeneration = ++nmCameraXDualWatchdogGeneration;
        nmCameraXDualFrameWatchdog = () -> {
            if (watchdogGeneration != nmCameraXDualWatchdogGeneration) {
                return;
            }
            nmCameraXDualFrameWatchdog = null;
            if (cancelled || !bothCameras || nmCameraXActiveFrameObserved) {
                return;
            }

            if (!videoMessagesHelper.retryCameraXDual(this)) {
                videoMessagesHelper.fallbackCameraXDualToSingle(this);
            }
        };
        AndroidUtilities.runOnUIThread(
                nmCameraXDualFrameWatchdog, NM_CAMERAX_DUAL_FRAME_TIMEOUT_MS);
    }

    public void onCameraXUnavailable() {
        if (!useCameraX || cancelled) return;
        cameraReady = false;
        if (textureView != null) {
            cancel(false);
        } else {
            videoMessagesHelper.destroyCameraX(this);
        }
    }

    public void onCameraXDualUnavailable() {
        cancelCameraXRearLensTransition();
        nmCancelCameraXDualFrameWatchdog();
        if (dualCameraSwitchAnimator != null) {
            dualCameraSwitchAnimator.cancel();
        }
        bothCameras = false;
        cameraReady = false;
        nmCameraXActiveFrameObserved = false;
        nmCameraXFrameMask = 0;

        pendingCameraXSwitchAfterDualCollapse |= pendingCameraXSwitchAfterInitialWide;
        cameraXSingleSwitchAwaitingBind = false;
        pendingCameraXSingleSession = null;
        nmCancelCameraXInitialWideWait();
        dualVideoSwitching = false;
        previewSize[1] = null;
        CameraGLThread thread = cameraThread;
        if (thread != null) {
            thread.setSurfaceIndex(0);
        }
    }

    public void onCameraXTransitionStarting() {
        if (!useCameraX) return;
        cancelCameraXRearLensTransition();
        nmCancelCameraXDualFrameWatchdog();
        nmCancelCameraXInitialWideWait();
    }

    public boolean shouldEnableCameraXRearTorch() {
        return flashing && recording && !isFrontface;
    }

    private float panTranslationY;
    private float animationTranslationY;
    private float cameraLayoutTranslationY;

    private final float[] mMVPMatrix = new float[16];
    private final float[] mSTMatrix = new float[16];
    private final float[] moldSTMatrix = new float[16];
    private final float[] nmCameraXTransformScratch = new float[16];
    private final float[] nmCameraXMirrorMatrix = new float[] {
            -1, 0, 0, 0,
             0, 1, 0, 0,
             0, 0, 1, 0,
             0, 0, 0, 1
    };
    private final float[][] dualVideoSTMatrix = new float[2][16];
    private final float[][] dualVideoMVPMatrix = new float[2][16];
    private volatile boolean dualVideoSwitching;
    private volatile int dualVideoSwitchFrom = -1;
    private volatile int dualVideoSwitchTo = -1;
    private volatile float dualVideoSwitchProgress;
    private static final String VERTEX_SHADER =
            "uniform mat4 uMVPMatrix;\n" +
                    "uniform mat4 uSTMatrix;\n" +
                    "attribute vec4 aPosition;\n" +
                    "attribute vec4 aTextureCoord;\n" +
                    "varying vec2 vTextureCoord;\n" +
                    "void main() {\n" +
                    "   gl_Position = uMVPMatrix * aPosition;\n" +
                    "   vTextureCoord = (uSTMatrix * aTextureCoord).xy;\n" +
                    "}\n";

    private static final String FRAGMENT_SCREEN_SHADER =
            "#extension GL_OES_EGL_image_external : require\n" +
                    "precision lowp float;\n" +
                    "varying vec2 vTextureCoord;\n" +
                    "uniform samplerExternalOES sTexture;\n" +
                    "void main() {\n" +
                    "   gl_FragColor = texture2D(sTexture, vTextureCoord);\n" +
                    "}\n";

    private static final String FRAGMENT_CAMERAX_SCREEN_SHADER =
            "#extension GL_OES_EGL_image_external : require\n" +
                    "precision highp float;\n" +
                    "varying vec2 vTextureCoord;\n" +
                    "uniform samplerExternalOES sTexture;\n" +
                    "uniform float alpha;\n" +
                    "uniform vec2 texelSize;\n" +
                    "uniform float switchBlur;\n" +
                    "void main() {\n" +
                    "   float transition = smoothstep(0.0, 1.0, switchBlur);\n" +
                    "   vec2 uv = vTextureCoord;\n" +
                    "   vec4 color = texture2D(sTexture, uv);\n" +
                    "   if (switchBlur > 0.001) {\n" +
                    "       vec2 d = texelSize * (2.0 * transition);\n" +
                    "       vec2 radial = (uv - vec2(0.5)) * (0.012 * transition);\n" +
                    "       color = color * 0.52\n" +
                    "           + texture2D(sTexture, uv + vec2(d.x, 0.0)) * 0.09\n" +
                    "           + texture2D(sTexture, uv - vec2(d.x, 0.0)) * 0.09\n" +
                    "           + texture2D(sTexture, uv + vec2(0.0, d.y)) * 0.09\n" +
                    "           + texture2D(sTexture, uv - vec2(0.0, d.y)) * 0.09\n" +
                    "           + texture2D(sTexture, uv + radial) * 0.06\n" +
                    "           + texture2D(sTexture, uv - radial) * 0.06;\n" +
                    "   }\n" +
                    "   gl_FragColor = vec4(color.rgb, color.a * alpha);\n" +
                    "}\n";

    private static final String FRAGMENT_SNAPSHOT_SHADER =
            "precision highp float;\n" +
                    "varying vec2 vTextureCoord;\n" +
                    "uniform sampler2D sTexture;\n" +
                    "uniform float alpha;\n" +
                    "uniform vec2 texelSize;\n" +
                    "uniform float switchBlur;\n" +
                    "void main() {\n" +
                    "   float transition = smoothstep(0.0, 1.0, switchBlur);\n" +
                    "   vec2 uv = vTextureCoord;\n" +
                    "   vec4 color = texture2D(sTexture, uv);\n" +
                    "   if (switchBlur > 0.001) {\n" +
                    "       vec2 d = texelSize * (2.0 * transition);\n" +
                    "       vec2 radial = (uv - vec2(0.5)) * (0.012 * transition);\n" +
                    "       color = color * 0.52\n" +
                    "           + texture2D(sTexture, uv + vec2(d.x, 0.0)) * 0.09\n" +
                    "           + texture2D(sTexture, uv - vec2(d.x, 0.0)) * 0.09\n" +
                    "           + texture2D(sTexture, uv + vec2(0.0, d.y)) * 0.09\n" +
                    "           + texture2D(sTexture, uv - vec2(0.0, d.y)) * 0.09\n" +
                    "           + texture2D(sTexture, uv + radial) * 0.06\n" +
                    "           + texture2D(sTexture, uv - radial) * 0.06;\n" +
                    "   }\n" +
                    "   gl_FragColor = vec4(color.rgb, color.a * alpha);\n" +
                    "}\n";

    private FloatBuffer vertexBuffer;
    private FloatBuffer textureBuffer;
    private final FloatBuffer[] cameraTextureBuffers = new FloatBuffer[2];
    private FloatBuffer oldTextureTextureBuffer;
    private float scaleX;
    private float scaleY;

    private Size oldTexturePreviewSize;

    private boolean flipAnimationInProgress;
    private volatile boolean cameraXSingleSwitchAwaitingBind;
    private volatile com.th3nekit.finegram.camera.CameraXSurfaceSession
            pendingCameraXSingleSession;
    private volatile int pendingCameraXSingleSessionGeneration;

    private boolean pendingCameraXSwitchAfterDualCollapse;
    private ValueAnimator dualCameraSwitchAnimator;
    private ValueAnimator cameraXVideoBlurAnimator;
    private Runnable cameraXVideoBlurTimeout;
    private volatile boolean cameraXVideoTransitionActive;

    private volatile float cameraXSingleSwitchBlur;
    private volatile float cameraXSingleSwitchProgress;
    private volatile boolean cameraXSingleSwitchNewFrame;
    private boolean cameraXSingleSwitchFinishing;
    private long cameraXSingleSwitchWaitStartedMs;
    private volatile CameraXRoundLensTransition cameraXRearLensTransition;
    private volatile CameraXSurfaceSession cameraXRearLensSession;

    private final Object cameraXInitialWideWaitLock = new Object();
    private volatile boolean cameraXInitialWideWaitActive;
    private volatile boolean pendingCameraXSwitchAfterInitialWide;
    private volatile long cameraXInitialWideWaitStartedMs;
    private volatile long cameraXInitialWideWaitTimeoutMs;
    private volatile boolean cameraXInitialWideTimeoutRenderPending;
    private Runnable cameraXInitialWideTimeoutRunnable;
    private int cameraXInitialWideWaitGeneration;

    private volatile int cameraXSingleSwitchSnapshot;
    private volatile int cameraXSingleSwitchSnapshotWidth;
    private volatile int cameraXSingleSwitchSnapshotHeight;

    private volatile long cameraXSingleSwitchSnapshotTimestamp;
    private volatile CameraGLThread.CameraSnapshot cameraXSingleSwitchSnapshotHandle;
    private final float[] oldScreenSTMatrix = new float[16];
    private final float[] oldScreenMVPMatrix = new float[16];
    private FloatBuffer cameraXSnapshotTextureBuffer;
    private float[] cameraXSnapshotIdentityMatrix;

    private View parentView;
    public boolean opened;

    float pinchStartDistance;

    float pinchScale;
    private float cameraXPinchStartRatio = 1f;

    boolean isInPinchToZoomTouchMode;
    boolean maybePinchToZoomTouchMode;

    private boolean singleZoomMaybe;
    private boolean singleZoomActive;
    private float singleZoomStartY;
    private float singleZoomStartRatio;
    private float legacyZoom;

    private int pointerId1, pointerId2;
    private int textureViewSize;
    private boolean isMessageTransition;
    private final Theme.ResourcesProvider resourcesProvider;
    private RoundZoomControl roundZoomControl;
    private boolean roundZoomControlSpace;
    private boolean roundZoomAwaitingInitialValue;
    private boolean roundControlColorsSet;
    private int roundControlIconColor;
    private final Runnable refreshRoundZoomControl = new Runnable() {
        @Override public void run() {
            if (!isAttachedToWindow() || getVisibility() != VISIBLE || roundZoomControl == null) return;
            if ((useCameraX || useCamera2) && !isRoundCameraZoomReady() && roundZoomSession != null) {
                stopRoundZoomSpring();
            }
            boolean visible = (useCameraX || useCamera2) && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getRoundZoomScale()
                    && roundZoomControlSpace && opened && !cancelled && videoPlayer == null
                    && !isMessageTransition;
            float min = visible ? roundZoomMin() : 1f;
            float max = visible ? roundZoomMax() : 1f;
            boolean bindingReady = visible && (useCameraX ? !cameraXSingleSwitchAwaitingBind
                    && videoMessagesHelper.isInitiated()
                    && videoMessagesHelper.getCurrentSession() != null
                    && videoMessagesHelper.getCurrentSession().isFrontFacing() == isFrontface
                    : camera2SessionCurrent != null && camera2SessionCurrent.isInitiated());
            boolean metadataReady = bindingReady && (!useCameraX || videoMessagesHelper.isZoomShortcutsReady());
            boolean zoomReady = metadataReady && (!useCameraX || videoMessagesHelper.isZoomReady());
            if (zoomReady && cameraReady && (!useCameraX || !cameraXInitialWideWaitActive)) {
                roundZoomAwaitingInitialValue = false;
            }
            boolean startingWide = roundZoomAwaitingInitialValue
                    && !isFrontface && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getStartFromUltraWideCam();
            float[] shortcuts = metadataReady ? (useCameraX ? videoMessagesHelper.getZoomShortcuts()
                    : camera2SessionCurrent.getZoomShortcuts()) : null;
            boolean rangeReady = metadataReady && RoundZoomScale.valid(min, max)
                    && shortcuts != null && shortcuts.length > 0;
            if (useCameraX && visible && !rangeReady && !roundZoomControl.hasKnownRange()
                    && !cameraXSingleSwitchAwaitingBind && !flipAnimationInProgress) {
                prepareRoundZoomPreview();
            }
            boolean fixedRange = zoomReady && recording && cameraReady && !startingWide
                    && Float.isFinite(min) && Float.isFinite(max)
                    && min > 0f && max >= min && max <= min + .001f;
            if (rangeReady) {
                Object identity = roundZoomSessionIdentity();
                roundZoomMotion.setBounds(min, max);
                roundZoomSpring.setBounds(min, max);
                roundZoomControl.setRange(identity, min, max, shortcuts);
                boolean ownsZoom = roundZoomSession != null && roundZoomSession == identity;
                float value = ownsZoom ? roundZoomDisplayedValue()
                        : startingWide ? min : roundZoomObserved();
                roundZoomControl.setValue(value,
                        ownsZoom ? roundZoomTarget() : startingWide ? value : currentRoundZoomRatio());
            }
            visible = visible && !fixedRange && roundZoomControl.hasKnownRange();
            roundZoomControl.setInteractionReady(rangeReady && isRoundZoomControlReady());
            roundZoomControl.setPresented(visible);
            if ((useCameraX || useCamera2) && opened && !cancelled && videoPlayer == null
                    && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getRoundZoomScale()) postDelayed(this, 120);
        }
    };

    private final static int audioSampleRate = 48000;

    private static final int[] ALLOW_BIG_CAMERA_WHITELIST = {
            285904780,
            -1394191079
    };
    private boolean allowSendingWhileRecording;

    private final LinearLayout buttonsLayout;
    private final int buttonsSizePx;

    private SlideControlView evControlView;
    private Runnable evControlHideRunnable;

    @SuppressLint("ClickableViewAccessibility")
    public InstantCameraView(Context context, Delegate delegate, Theme.ResourcesProvider resourcesProvider, boolean isNewDesign) {
        super(context);
        buttonsSizePx = dp(isNewDesign ? 24 : 28);

        WRITE_TO_FILE_IN_BACKGROUND = false;
        this.resourcesProvider = resourcesProvider;
        parentView = delegate.getFragmentView();
        setWillNotDraw(false);

        this.delegate = delegate;
        recordingGuid = delegate.getClassGuid();
        isSecretChat = delegate.isSecretChat();
        paint = new Paint(Paint.ANTI_ALIAS_FLAG) {
            @Override
            public void setAlpha(int a) {
                super.setAlpha(a);
                invalidate();
            }
        };
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeCap(Paint.Cap.ROUND);
        paint.setStrokeWidth(dp(3));
        paint.setColor(0xffffffff);

        rect = new RectF();

        flashViews = new FlashViews(getContext(), null, this, null);
        flashViews.setWarmth(0.5f);
        flashViews.setIntensity(1.0f);
        addView(flashViews.backgroundView, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.MATCH_PARENT, Gravity.FILL));

        cameraContainer = new InstantViewCameraContainer(context) {
            @Override
            public void setRotationY(float rotationY) {
                super.setRotationY(rotationY);
                InstantCameraView.this.invalidate();
            }

            @Override
            public void setAlpha(float alpha) {
                super.setAlpha(alpha);
                InstantCameraView.this.invalidate();
            }
        };
        cameraContainer.setOutlineProvider(new ViewOutlineProvider() {
            @Override
            public void getOutline(View view, Outline outline) {
                outline.setOval(0, 0, textureViewSize, textureViewSize);
            }
        });
        cameraContainer.setClipToOutline(true);
        cameraContainer.setWillNotDraw(false);

        addView(cameraContainer, new LayoutParams(AndroidUtilities.roundPlayingMessageSize, AndroidUtilities.roundPlayingMessageSize, Gravity.CENTER));
        addView(flashViews.foregroundView, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.MATCH_PARENT, Gravity.FILL));

        evControlView = new SlideControlView(context, SlideControlView.SLIDER_MODE_EV);
        evControlView.setSliderValue(0.5f, false);
        evControlView.setRotation(270f);
        addView(evControlView, LayoutHelper.createFrame(
                30, 100, Gravity.RIGHT | Gravity.CENTER_VERTICAL, 0, 0, -25, 0));
        evControlView.setDelegate(value -> {
            if (useCameraX && videoMessagesHelper.isExposureCompensationSupported()) {
                videoMessagesHelper.setExposureCompensation(value);
            } else if (useCamera2 && camera2SessionCurrent != null
                    && camera2SessionCurrent.isExposureCompensationSupported()) {
                camera2SessionCurrent.setExposureCompensation(value);
            }
            nmShowEvControl();
        });
        nmShowEvControl();

        buttonsLayout = new LinearLayout(context) {
            @Override
            protected void dispatchDraw(Canvas canvas) {
                if (roundZoomControl != null && (useCameraX || useCamera2)
                        && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getRoundZoomScale()
                        && getBackground() == null) {
                    roundZoomControl.drawControlsBackgroundFallback(canvas, getWidth(), getHeight());
                }
                super.dispatchDraw(canvas);
            }
        };
        buttonsLayout.setPadding(dp(6), dp(6), dp(6), dp(6));

        roundZoomControl = new RoundZoomControl(context, resourcesProvider, new RoundZoomControl.Listener() {
            @Override
            public void onZoom(float ratio, boolean preset) {
                if (!isRoundZoomControlReady()) {
                    roundZoomControl.setInteractionReady(false);
                    return;
                }
                requestRoundZoomControl(ratio, preset);
            }

            @Override
            public void onGestureStart() {
                if (!isRoundZoomControlReady()) return;
                sampleRoundZoomAtInput();
                float ratio = roundZoomSession == roundZoomSessionIdentity()
                        ? roundZoomDisplayedValue() : roundZoomObserved();
                requestRoundZoomControl(ratio, false);
            }
        });
        roundZoomControl.setVisibility(INVISIBLE);
        addView(roundZoomControl, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL, 28, 0, 28, 64));

        buttonsLayout.setOrientation(LinearLayout.HORIZONTAL);
        if (com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getCenterCameraControlButtons()) {
            buttonsLayout.setGravity(Gravity.CENTER);
            addView(buttonsLayout, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT, 56, Gravity.CENTER_HORIZONTAL | Gravity.BOTTOM, 0, 0, 0, 0));
        } else {
            addView(buttonsLayout, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT, 56, Gravity.LEFT | Gravity.BOTTOM, 1, 0, 0, 0));
        }

        switchCameraButton = new FlashViews.ImageViewInvertable(context);
        switchCameraButton.setScaleType(ImageView.ScaleType.CENTER);
        switchCameraButton.setContentDescription(LocaleController.getString(R.string.AccDescrSwitchCamera));
        buttonsLayout.addView(switchCameraButton, LayoutHelper.createLinear(44, 44));
        switchCameraButton.setOnClickListener(v -> {
            final boolean concurrentReady = useCameraX && bothCameras
                    && videoMessagesHelper.isConcurrentDualReady();
            if (!cameraReady || !isCameraSessionInitiated() || cameraThread == null
                    || flipAnimationInProgress || nmCamera2SwitchPending) {
                return;
            }
            if (roundZoomControl != null) {
                roundZoomControl.setInteractionReady(false);
            }
            if (useCameraX) {
                stopRoundZoomSpring();
                cancelCameraXRearLensTransition();
                roundZoomBindingSession = null;
                roundZoomBindingToken = null;
            }
            if (bothCameras && useCameraX
                    && (!concurrentReady
                    || (nmCameraXFrameMask & (1 << (1 - surfaceIndex))) == 0)) {

                pendingCameraXSwitchAfterDualCollapse =
                        !pendingCameraXSwitchAfterDualCollapse;
                videoMessagesHelper.fallbackCameraXDualToSingle(this);
                return;
            }
            if (bothCameras && useCameraX && isFrontface
                    && cameraXInitialWideWaitActive) {

                pendingCameraXSwitchAfterInitialWide =
                        !pendingCameraXSwitchAfterInitialWide;
                return;
            }

            pendingCameraXSwitchAfterInitialWide = false;
            if (switchCameraDrawable != null) {
                switchCameraDrawable.setCurrentFrame(0);
                switchCameraDrawable.start();
            }
            if (bothCameras && useCameraX) {

                flipAnimationInProgress = true;
                switchCamera();
                return;
            }
            if (useCameraX && !bothCameras) {

                flipAnimationInProgress = true;
                cameraXSingleSwitchAwaitingBind = true;
                cameraXSingleSwitchWaitStartedMs = SystemClock.elapsedRealtime();
                pendingCameraXSingleSession = null;
                CameraGLThread activeThread = cameraThread;
                activeThread.captureCameraXSingleSwitchSnapshot(() -> {
                    if (cameraThread != activeThread || !cameraXSingleSwitchAwaitingBind) {
                        flipAnimationInProgress = false;
                        return;
                    }
                    startCameraXVideoTransition();
                    switchCamera();
                });
                return;
            }
            if (!bothCameras) {
                switchCamera();
            }
            flipAnimationInProgress = true;
            ValueAnimator valueAnimator = ValueAnimator.ofFloat(0, 1f);
            valueAnimator.setDuration(580);
            valueAnimator.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
            final boolean[] didSwap = new boolean[1];
            Runnable doSwap = () -> {
                if (bothCameras) {
                    switchCamera();
                }
            };
            cameraContainer.setCameraDistance(cameraContainer.getMeasuredHeight() * 8f);
            textureOverlayView.setCameraDistance(textureOverlayView.getMeasuredHeight() * 8f);
            valueAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
                @Override
                public void onAnimationUpdate(ValueAnimator valueAnimator) {
                    float p = (float) valueAnimator.getAnimatedValue();
                    if (p > 0.5f && !didSwap[0]) {
                        didSwap[0] = true;
                        doSwap.run();
                    }
                    float rotation = p < 0.5f ? p : p - 1f;
                    rotation *= 180;
                    cameraContainer.setRotationY(rotation);
                    textureOverlayView.setRotationY(rotation);
                }
            });
            valueAnimator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    super.onAnimationEnd(animation);
                    if (!didSwap[0]) {
                        didSwap[0] = true;
                        doSwap.run();
                    }
                    cameraContainer.setRotationY(0f);
                    textureOverlayView.setRotationY(0f);
                    flipAnimationInProgress = false;
                    invalidate();
                }
            });
            valueAnimator.start();
        });

        flashButton = new FlashViews.ImageViewInvertable(context);
        flashButton.setScaleType(ImageView.ScaleType.CENTER);
        buttonsLayout.addView(flashButton, LayoutHelper.createLinear(44, 44));
        flashButton.setOnClickListener(v -> {
            flashing = !flashing;
            updateFlash();

            com.th3nekit.finegram.camera.CameraSettings.advanceFlashHint();
        });
        updateFlash();

        if (!isNewDesign) {
            flashViews.add(switchCameraButton);
            flashViews.add(flashButton);
        } else if (!resourcesProvider.isDark()) {
            switchCameraButton.setInvert(0.6f);
            flashButton.setInvert(0.6f);
        }

        muteImageView = new ImageView(context);
        muteImageView.setScaleType(ImageView.ScaleType.CENTER);
        muteImageView.setImageResource(R.drawable.video_mute);
        muteImageView.setAlpha(0.0f);
        addView(muteImageView, LayoutHelper.createFrame(48, 48, Gravity.CENTER));

        Paint blackoutPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        blackoutPaint.setColor(ColorUtils.setAlphaComponent(Color.BLACK, 40));
        textureOverlayView = new BackupImageView(getContext()) {

            CellFlickerDrawable flickerDrawable = new CellFlickerDrawable();

            @Override
            protected void onDraw(Canvas canvas) {
                super.onDraw(canvas);
                if (needDrawFlickerStub) {
                    flickerDrawable.setParentWidth(textureViewSize);
                    AndroidUtilities.rectTmp.set(0, 0, textureViewSize, textureViewSize);
                    float rad = AndroidUtilities.rectTmp.width() / 2f;
                    canvas.drawRoundRect(AndroidUtilities.rectTmp, rad, rad, blackoutPaint);
                    AndroidUtilities.rectTmp.inset(dp(1), dp(1));
                    flickerDrawable.draw(canvas, AndroidUtilities.rectTmp, rad, null);
                    invalidate();
                }
            }
        };
        addView(textureOverlayView, new LayoutParams(AndroidUtilities.roundPlayingMessageSize, AndroidUtilities.roundPlayingMessageSize, Gravity.CENTER));

        setVisibilityFromPause = false;
        setVisibility(INVISIBLE);
    }

    public void setButtonsBackground(BlurredBackgroundDrawableViewFactory factory, BlurredBackgroundColorProvider colorProvider) {
        if (useCameraX && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getRoundZoomScale()) {
            colorProvider = RoundZoomControl.createControlColorProvider(resourcesProvider, colorProvider);
        }
        BlurredBackgroundDrawable drawable = factory.create(buttonsLayout, colorProvider);
        drawable.setPadding(dp(6));
        drawable.setRadius(dp(21));
        if (useCameraX && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getRoundZoomScale()) {
            RoundZoomControl.configureGlassMaterial(drawable);
            drawable.setRadius(dp(22));
        }
        buttonsLayout.setBackground(drawable);
        if (roundZoomControl != null && useCameraX && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getRoundZoomScale()) {
            roundZoomControl.setBlurBackground(factory, colorProvider);
        }
    }

    private Boolean wasFlashing;
    private boolean flashing;
    private boolean frontFlashing;
    private void updateFlash() {
        final boolean shouldFrontFlash = flashing && recording && isFrontface;
        if (frontFlashing != shouldFrontFlash) {
            if (frontFlashing = shouldFrontFlash) {
                flashViews.setWarmth(0.5f);
                flashViews.setIntensity(1.0f);
                flashViews.flashIn(null);
            } else {
                flashViews.flashOut();
            }
        }

        final boolean rearTorchOn = flashing && !isFrontface && recording;
        final int rearTorchIntensity = 100;
        if (useCameraX) {
            videoMessagesHelper.updateCameraXFlash(this);
        } else if (useCamera2) {
            if (camera2Sessions[1] != null) {
                camera2Sessions[1].setFlash(rearTorchOn, rearTorchIntensity);
            }
        } else {
            if (cameraSession != null) {

                cameraSession.setTorchEnabled(rearTorchOn, rearTorchIntensity);
            }
        }

        if (flashButton != null && (wasFlashing == null || wasFlashing != flashing)) {
            flashButton.setContentDescription(LocaleController.getString(flashing ? R.string.AccDescrCameraFlashOff : R.string.AccDescrCameraFlashOn));
            if (!flashing) {
                if (flashOnDrawable == null) {
                    flashOnDrawable = new RLottieDrawable(R.raw.roundcamera_flash_on, buttonsSizePx, buttonsSizePx);
                    flashOnDrawable.setCallback(flashButton);
                }
                flashButton.setImageDrawable(flashOnDrawable);
                if (wasFlashing == null) {
                    flashOnDrawable.setCurrentFrame(flashOnDrawable.getFramesCount() - 1);
                } else {
                    flashOnDrawable.setCurrentFrame(0);
                    flashOnDrawable.start();
                }
            } else {
                if (flashOffDrawable == null) {
                    flashOffDrawable = new RLottieDrawable(R.raw.roundcamera_flash_off, buttonsSizePx, buttonsSizePx);
                    flashOffDrawable.setCallback(flashButton);
                }
                flashButton.setImageDrawable(flashOffDrawable);
                if (wasFlashing == null) {
                    flashOffDrawable.setCurrentFrame(flashOffDrawable.getFramesCount() - 1);
                } else {
                    flashOffDrawable.setCurrentFrame(0);
                    flashOffDrawable.start();
                }
            }
            wasFlashing = flashing;
        }
    }

    private int internalPaddingBottom;

    public void setInternalPadding(int padding) {

        internalPaddingBottom = padding;
        setPadding(0, 0, 0, padding);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        boolean hadRoundZoomControlSpace = roundZoomControlSpace;

        if (!isMessageTransition) {
            final int width = MeasureSpec.getSize(widthMeasureSpec);
            final int height = MeasureSpec.getSize(heightMeasureSpec);
            final int availableWidth = Math.max(0, width - getPaddingLeft() - getPaddingRight());
            final int availableHeight = Math.max(0, height - getPaddingTop() - getPaddingBottom());
            boolean centered = com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getCenterCameraControlButtons();
            int zoomMargin = dp(centered ? 28 : 16);
            final int zoomWidth = Math.max(0, availableWidth - zoomMargin * 2);
            final int progressInset = dp(8) + (int) Math.ceil(paint.getStrokeWidth() / 2f);
            int reservedControlsHeight = centered
                    ? dp(64) + roundZoomControl.getReservedHeight()
                    : dp(16 + 56) + roundZoomControl.getReservedHeight();
            roundZoomControlSpace = (useCameraX || useCamera2) && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getRoundZoomScale()
                    && availableHeight >= dp(240) && zoomWidth >= dp(192)
                    && availableHeight - reservedControlsHeight - dp(12) - progressInset * 2 >= dp(120);
            updateRoundControlsLayout();

            final int preferredSize = availableHeight > width * 1.3f
                    ? AndroidUtilities.roundPlayingMessageSize : AndroidUtilities.roundMessageSize;

            int newSize = Math.max(1, Math.min(preferredSize,
                    Math.min(availableWidth, availableHeight) - 2 * progressInset));
            float layoutTranslationY = 0f;
            if (roundZoomControlSpace && roundZoomControl != null && videoPlayer == null) {
                LayoutParams zoomParams = (LayoutParams) roundZoomControl.getLayoutParams();
                LayoutParams buttonsParams = (LayoutParams) buttonsLayout.getLayoutParams();
                int controlsTopInset = Math.max(zoomParams.bottomMargin + roundZoomControl.getReservedHeight(),
                        buttonsParams.bottomMargin + buttonsParams.height);
                float circleBottomLimit = availableHeight - controlsTopInset - dp(12) - progressInset;
                int maximumSize = (int) Math.floor(circleBottomLimit - progressInset);
                if (maximumSize < dp(120)) {
                    roundZoomControlSpace = false;
                } else {
                    newSize = Math.min(newSize, maximumSize);
                    layoutTranslationY = Math.min(0f,
                            circleBottomLimit - (availableHeight + newSize) / 2f);
                }
            }
            if (cameraLayoutTranslationY != layoutTranslationY) {
                cameraLayoutTranslationY = layoutTranslationY;
                updateTranslationY();
                muteImageView.setTranslationY(layoutTranslationY);
                invalidate();
            }
            if (newSize != textureViewSize) {
                textureViewSize = newSize;
                textureOverlayView.getLayoutParams().width = textureOverlayView.getLayoutParams().height = textureViewSize;
                cameraContainer.getLayoutParams().width = cameraContainer.getLayoutParams().height = textureViewSize;

                cameraContainer.setPivotX(textureViewSize / 2f);
                cameraContainer.setPivotY(textureViewSize / 2f);
                textureOverlayView.setPivotX(textureViewSize / 2f);
                textureOverlayView.setPivotY(textureViewSize / 2f);
                ((LayoutParams) muteImageView.getLayoutParams()).topMargin = textureViewSize / 2 - dp(24);
                textureOverlayView.setRoundRadius(textureViewSize / 2);
                cameraContainer.invalidateOutline();
                invalidate();
            }
        }

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        if (hadRoundZoomControlSpace != roundZoomControlSpace) {
            removeCallbacks(refreshRoundZoomControl);
            post(refreshRoundZoomControl);
        }

        final int flashWidthSpec = MeasureSpec.makeMeasureSpec(getMeasuredWidth(), MeasureSpec.EXACTLY);
        final int flashHeightSpec = MeasureSpec.makeMeasureSpec(getMeasuredHeight(), MeasureSpec.EXACTLY);
        flashViews.backgroundView.measure(flashWidthSpec, flashHeightSpec);
        flashViews.foregroundView.measure(flashWidthSpec, flashHeightSpec);
    }

    private void updateRoundControlsLayout() {
        if (buttonsLayout == null || roundZoomControl == null) return;
        boolean centered = com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getCenterCameraControlButtons();
        boolean right = !centered && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getCameraControlButtonsRight();
        LayoutParams buttons = (LayoutParams) buttonsLayout.getLayoutParams();
        if (buttons == null) return;
        int gravity = Gravity.BOTTOM | (centered ? Gravity.CENTER_HORIZONTAL : right ? Gravity.RIGHT : Gravity.LEFT);
        int buttonMargin = centered ? 0 : dp(16);
        int buttonsBottom = !centered && roundZoomControlSpace
                ? roundZoomControl.getReservedHeight() + dp(16) : 0;
        if (buttons.gravity != gravity || buttons.leftMargin != (right ? 0 : buttonMargin)
                || buttons.rightMargin != (right ? buttonMargin : 0) || buttons.bottomMargin != buttonsBottom) {
            buttons.gravity = gravity;
            buttons.leftMargin = right ? 0 : buttonMargin;
            buttons.rightMargin = right ? buttonMargin : 0;
            buttons.bottomMargin = buttonsBottom;
            buttonsLayout.setLayoutParams(buttons);
        }
        LayoutParams zoom = (LayoutParams) roundZoomControl.getLayoutParams();
        if (zoom == null) return;
        int margin = dp(centered ? 28 : 16), bottom = dp(centered ? 64 : 8);
        int zoomGravity = Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL;
        if (zoom.leftMargin != margin || zoom.rightMargin != margin || zoom.bottomMargin != bottom
                || zoom.gravity != zoomGravity) {
            zoom.leftMargin = margin;
            zoom.rightMargin = margin;
            zoom.bottomMargin = bottom;
            zoom.gravity = zoomGravity;
            roundZoomControl.setLayoutParams(zoom);
        }
    }

    private boolean checkPointerIds(MotionEvent ev) {
        if (ev.getPointerCount() < 2) {
            return false;
        }
        if (pointerId1 == ev.getPointerId(0) && pointerId2 == ev.getPointerId(1)) {
            return true;
        }
        if (pointerId1 == ev.getPointerId(1) && pointerId2 == ev.getPointerId(0)) {
            return true;
        }
        return false;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        getParent().requestDisallowInterceptTouchEvent(true);
        return super.onInterceptTouchEvent(ev);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        if (getVisibility() != VISIBLE) {
            animationTranslationY = getMeasuredHeight() / 2f;
            updateTranslationY();
        }
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        removeCallbacks(refreshRoundZoomControl);
        post(refreshRoundZoomControl);
        NotificationCenter.getInstance(currentAccount).addObserver(this, NotificationCenter.fileUploaded);
    }

    @Override
    protected void onDetachedFromWindow() {
        removeCallbacks(refreshRoundZoomControl);
        if (roundZoomControl != null) roundZoomControl.cancelGesture();
        stopRoundZoomSpring();
        ++cameraCoverGeneration;
        super.onDetachedFromWindow();
        NotificationCenter.getInstance(currentAccount).removeObserver(this, NotificationCenter.fileUploaded);
        if (flashViews != null) {
            flashViews.flashOut();
        }
    }

    @Override
    public void didReceivedNotification(int id, int account, Object... args) {
        if (id == NotificationCenter.fileUploaded) {
            final String location = (String) args[0];
            if (cameraFile != null && cameraFile.getAbsolutePath().equals(location)) {
                file = (TLRPC.InputFile) args[1];
                encryptedFile = (TLRPC.InputEncryptedFile) args[2];
                size = (Long) args[5];
                if (encryptedFile != null) {
                    key = (byte[]) args[3];
                    iv = (byte[]) args[4];
                }
            }
        }
    }

    public void destroy(boolean async) {
        stopRoundZoomSpring();
        ++cameraCoverGeneration;
        pausePreviewToken = null;
        cancelCameraXVideoTransitions();
        nmCancelRoundFrameWatchdog();
        nmCancelCameraXDualFrameWatchdog();
        nmCancelCamera2SwitchFrameWatchdog();
        ++nmCamera2SwitchGeneration;
        ++nmPhysicalReopenGeneration;
        nmCamera2SwitchPending = false;
        if (dualCameraSwitchAnimator != null) {
            dualCameraSwitchAnimator.removeAllListeners();
            dualCameraSwitchAnimator.cancel();
            dualCameraSwitchAnimator = null;
        }
        dualVideoSwitching = false;
        pendingCameraXSwitchAfterDualCollapse = false;
        cameraXSingleSwitchAwaitingBind = false;
        pendingCameraXSingleSession = null;
        nmCancelCameraXInitialWideWait();
        flipAnimationInProgress = false;
        clearCameraXVideoTransition();
        if (useCameraX) {
            videoMessagesHelper.destroyCameraX(this);
        } else if (useCamera2) {
            for (int a = 0; a < camera2Sessions.length; ++a) {
                if (camera2Sessions[a] != null) {
                    camera2Sessions[a].destroy(async);
                    camera2Sessions[a] = null;
                }
            }
            camera2SessionCurrent = null;
        } else {
            if (cameraSession != null) {
                cameraSession.destroy();
                CameraController.getInstance().close(cameraSession, !async ? new CountDownLatch(1) : null, null);
            }
        }

        if (evControlHideRunnable != null) {
            AndroidUtilities.cancelRunOnUIThread(evControlHideRunnable);
            evControlHideRunnable = null;
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        float x = cameraContainer.getX();
        float y = cameraContainer.getY();
        rect.set(x - dp(8), y - dp(8), x + cameraContainer.getMeasuredWidth() + dp(8), y + cameraContainer.getMeasuredHeight() + dp(8));
        if (recording) {
            recordedTime = SystemClock.elapsedRealtime() - recordStartTime + recordPlusTime;
            progress = Math.min(1f, recordedTime / 60000.0f);
            invalidate();
        }

        if (progress != 0) {
            canvas.save();
            if (!flipAnimationInProgress) {
                canvas.scale(cameraContainer.getScaleX(), cameraContainer.getScaleY(), rect.centerX(), rect.centerY());
            }
            canvas.drawArc(rect, -90, 360 * progress, false, paint);
            canvas.restore();
        }
    }

    private boolean setVisibilityFromPause;
    @Override
    protected void dispatchDraw(Canvas canvas) {
        if (useCameraX && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getRoundZoomScale()) {
            int color = RoundZoomControl.getControlForegroundColor(resourcesProvider);
            if (!roundControlColorsSet || roundControlIconColor != color) {
                roundControlColorsSet = true;
                roundControlIconColor = color;
                switchCameraButton.setColorFilter(new PorterDuffColorFilter(color, PorterDuff.Mode.SRC_IN));
                flashButton.setColorFilter(new PorterDuffColorFilter(color, PorterDuff.Mode.SRC_IN));
            }
        }
        if (roundZoomControl != null) {
            roundZoomControl.setHostAlpha(buttonsLayout.getAlpha());
        }
        super.dispatchDraw(canvas);
    }

    private void prepareRoundZoomPreview() {
        prepareRoundZoomPreview(false);
    }

    private void prepareRoundZoomPreview(boolean switching) {
        com.th3nekit.finegram.camera.CameraXUtils.ZoomPreview preview = useCameraX
                && !(switching ? bothCameras : com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getUseDualCamera())
                ? com.th3nekit.finegram.camera.CameraXUtils.getZoomPreview(isFrontface,
                com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getStartFromUltraWideCam()) : null;
        if (preview == null) return;
        float value = !isFrontface && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getStartFromUltraWideCam()
                ? preview.minimum : 1f;
        if (switching) {
            roundZoomControl.prepareForCameraSwitch(preview.minimum, preview.maximum, preview.getShortcuts(), value);
        } else {
            roundZoomControl.prepareForCamera(preview.minimum, preview.maximum, preview.getShortcuts(), value);
        }
    }

    @Override
    public void setVisibility(int visibility) {
        super.setVisibility(visibility);
        if (visibility == VISIBLE) updateRoundControlsLayout();
        removeCallbacks(refreshRoundZoomControl);
        if (roundZoomControl != null) {
            roundZoomControl.resetPresentation();
            if (visibility == VISIBLE) {
                if (!setVisibilityFromPause) {
                    roundZoomControl.prepareForCamera();
                    prepareRoundZoomPreview();
                }
                post(refreshRoundZoomControl);
            }
        }

        buttonsLayout.setAlpha(0.0f);
        cameraContainer.setAlpha(0.0f);
        textureOverlayView.setAlpha(0.0f);
        muteImageView.setAlpha(0.0f);
        muteImageView.setScaleX(1.0f);
        muteImageView.setScaleY(1.0f);
        cameraContainer.setScaleX(setVisibilityFromPause ? 1f : 0.1f);
        cameraContainer.setScaleY(setVisibilityFromPause ? 1f : 0.1f);
        textureOverlayView.setScaleX(setVisibilityFromPause ? 1f : 0.1f);
        textureOverlayView.setScaleY(setVisibilityFromPause ? 1f : 0.1f);
        if (cameraContainer.getMeasuredWidth() != 0) {
            cameraContainer.setPivotX(cameraContainer.getMeasuredWidth() / 2);
            cameraContainer.setPivotY(cameraContainer.getMeasuredHeight() / 2);
            textureOverlayView.setPivotX(textureOverlayView.getMeasuredWidth() / 2);
            textureOverlayView.setPivotY(textureOverlayView.getMeasuredHeight() / 2);
        }
        try {
            if (visibility == VISIBLE) {
                ((Activity) getContext()).getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            } else {
                ((Activity) getContext()).getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        } catch (Exception e) {
            FileLog.e(e);
        }
    }

    private void nmShowEvControl() {
        if (evControlView == null) {
            return;
        }
        if (evControlHideRunnable != null) {
            AndroidUtilities.cancelRunOnUIThread(evControlHideRunnable);
            evControlHideRunnable = null;
        }
        evControlView.animate().cancel();
        evControlView.setVisibility(View.VISIBLE);
        evControlView.setAlpha(1f);
        AndroidUtilities.runOnUIThread(evControlHideRunnable = () -> {
            if (evControlView != null) {
                evControlView.animate().alpha(0f).setDuration(180).withEndAction(() -> {
                    if (evControlView != null) {
                        evControlView.setVisibility(View.GONE);
                    }
                }).start();
            }
            evControlHideRunnable = null;
        }, 5000);
    }

    public void togglePause() {
        ++cameraCoverGeneration;
        pausePreviewToken = null;
        if (recording) {
            cancelCameraXVideoTransitions();
            cancelled = recordedTime < 800;
            recording = false;
            updateFlash();
            if (cameraThread != null) {
                ++nmPhysicalReopenGeneration;
                ++nmCamera2SwitchGeneration;
                nmCamera2SwitchPending = false;
                nmCancelRoundFrameWatchdog();
                nmCancelCameraXDualFrameWatchdog();
                nmCancelCamera2SwitchFrameWatchdog();
                NotificationCenter.getInstance(currentAccount).postNotificationName(NotificationCenter.recordStopped, recordingGuid, cancelled ? 4 : 2);
                saveLastCameraBitmap();
                cameraThread.shutdown(cancelled ? 0 : 2, true, 0, 0, cancelled ? 0 : -2, 0);
                cameraThread = null;
            }
            if (cancelled) {
                NotificationCenter.getInstance(currentAccount).postNotificationName(NotificationCenter.audioRecordTooShort, recordingGuid, true, (int) recordedTime);
                startAnimation(false, false);
                MediaController.getInstance().requestRecordAudioFocus(false);
            } else {
                videoEncoder.pause();
            }
        } else if (videoEncoder != null) {
            videoEncoder.resume();
            hideCamera(false);
            if (videoPlayer != null) {
                videoPlayer.releasePlayer(true);
                videoPlayer = null;
            }
            showCamera(true);
            try {
                performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP, HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING);
            } catch (Exception ignore) {}
            AndroidUtilities.lockOrientation(delegate.getParentActivity());
            invalidate();
            NotificationCenter.getInstance(currentAccount).postNotificationName(NotificationCenter.recordResumed);
        }
    }

    public boolean isPaused() {
        return !recording;
    }

    public void showCamera(boolean fromPaused) {
        if (textureView != null) {
            return;
        }
        roundZoomAwaitingInitialValue = !fromPaused;
        final int coverGeneration = ++cameraCoverGeneration;
        pausePreviewToken = null;
        ++nmPhysicalReopenGeneration;
        if (!fromPaused) {
            pendingCameraXSwitchAfterDualCollapse = false;
        }

        if (switchCameraDrawable == null) {
            switchCameraDrawable = new RLottieDrawable(R.raw.roundcamera_flip, buttonsSizePx, buttonsSizePx);
            switchCameraDrawable.setCurrentFrame(0);
            switchCameraDrawable.setCallback(switchCameraButton);
        }
        switchCameraButton.setImageDrawable(switchCameraDrawable);

        nmShowEvControl();

        textureOverlayView.animate().cancel();
        textureOverlayView.setAlpha(1.0f);
        textureOverlayView.setScaleX(1f);
        textureOverlayView.setScaleY(1f);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            textureOverlayView.setRenderEffect(null);
        }
        textureOverlayView.invalidate();
        if (lastBitmap == null) {
            textureOverlayView.setImageResource(R.drawable.icplaceholder);
            loadLastCameraBitmapAsync(coverGeneration);
        } else {
            textureOverlayView.setImageBitmap(lastBitmap);
        }
        cameraReady = false;
        cameraTextureAvailable = false;
        nmCamera2SwitchPending = false;
        selectedCamera = null;
        if (!fromPaused) {
            if (!useCameraX && !useCamera2) {
                isFrontface = true;
            }

            isFrontface = com.th3nekit.finegram.camera.CameraSettings.takeFrontFacing();
            com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.setRoundLastRear(!isFrontface);
            updateFlash();
            recordedTime = 0;
            progress = 0;
        }
        cancelled = false;
        file = null;
        encryptedFile = null;
        key = null;
        iv = null;
        needDrawFlickerStub = true;

        if (!initCamera()) {
            return;
        }
        if (MediaController.getInstance().getPlayingMessageObject() != null) {
            if (MediaController.getInstance().getPlayingMessageObject().isVideo() || MediaController.getInstance().getPlayingMessageObject().isRoundVideo()) {
                MediaController.getInstance().cleanupPlayer(true, true);
            } else if (SharedConfig.pauseMusicOnRecord) {
                MediaController.getInstance().pauseByRewind();
            }
        }

        if (!fromPaused) {
            cameraFile = new File(FileLoader.getDirectory(FileLoader.MEDIA_DIR_DOCUMENT), System.currentTimeMillis() + "_" + SharedConfig.getLastLocalId() + ".mp4") {
                @Override
                public boolean delete() {
                    if (BuildVars.LOGS_ENABLED) {
                        FileLog.e("delete camera file");
                    }
                    return super.delete();
                }
            };
        }

        SharedConfig.saveConfig();
        AutoDeleteMediaTask.lockFile(cameraFile);

        if (BuildVars.LOGS_ENABLED) {
            FileLog.d("InstantCamera show round camera " + cameraFile.getAbsolutePath());
        }

        if (useCameraX) {

            surfaceIndex = 0;
            nmCameraXActiveFrameObserved = false;
            nmCameraXFrameMask = 0;
            bothCameras = DualCameraView.roundDualAvailableStatic(getContext())
                    && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getUseDualCamera();
            int cameraSize = com.th3nekit.finegram.camera.CameraSettings.roundResolution(512);
            int captureSize = Math.min(1200, Math.max(cameraSize, cameraSize * 2));
            previewSize[0] = new Size(captureSize, captureSize);
            previewSize[1] = bothCameras ? new Size(captureSize, captureSize) : null;
        } else if (useCamera2) {
            bothCameras = DualCameraView.roundDualAvailableStatic(getContext()) && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getUseDualCamera();
            if (bothCameras) {
                for (int a = 0; a < 2; ++a) {
                    if (camera2Sessions[a] == null) {
                        camera2Sessions[a] = nmCreateRoundSession(a == 0);
                        if (camera2Sessions[a] != null) {
                            final Camera2Session session = camera2Sessions[a];
                            session.setRecordingVideo(true);
                            final int idx = a;
                            session.whenError(err -> nmHandleRoundCamError(
                                    idx, session, err == null ? -1 : err));
                            previewSize[a] = new Size(session.getPreviewWidth(), session.getPreviewHeight());
                        }
                    }
                }
                updateFlash();
                camera2SessionCurrent = camera2Sessions[isFrontface ? 0 : 1];

                surfaceIndex = isFrontface ? 0 : 1;
                if (camera2SessionCurrent != null && camera2Sessions[isFrontface ? 1 : 0] == null) {
                    bothCameras = false;
                }
                if (camera2SessionCurrent == null) {

                    for (int a = 0; a < camera2Sessions.length; ++a) {
                        if (camera2Sessions[a] != null) {
                            try { camera2Sessions[a].destroy(true); } catch (Throwable ignore) {}
                            camera2Sessions[a] = null;
                        }
                        previewSize[a] = null;
                    }
                    bothCameras = false;
                    return;
                }
            } else {
                surfaceIndex = 0;
                camera2SessionCurrent = camera2Sessions[isFrontface ? 0 : 1] = nmCreateRoundSession(isFrontface);
                if (camera2SessionCurrent == null) return;
                final Camera2Session session = camera2SessionCurrent;
                session.setRecordingVideo(true);
                final int nmSlot = isFrontface ? 0 : 1;
                session.whenError(err -> nmHandleRoundCamError(
                        nmSlot, session, err == null ? -1 : err));
                previewSize[0] = new Size(session.getPreviewWidth(), session.getPreviewHeight());

                nmArmRoundFrameWatchdog();
            }
        }
        final TextureView callbackTextureView = textureView = new TextureView(getContext());
        callbackTextureView.setSurfaceTextureListener(new TextureView.SurfaceTextureListener() {
            @Override
            public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
                if (textureView != callbackTextureView) {
                    return;
                }
                if (BuildVars.LOGS_ENABLED) {
                    FileLog.d("InstantCamera camera surface available");
                }
                if (cameraThread == null && surface != null) {
                    if (cancelled) {
                        return;
                    }
                    if (BuildVars.LOGS_ENABLED) {
                        FileLog.d("InstantCamera start create thread");
                    }
                    cameraThread = new CameraGLThread(
                            surface, width, height, ++cameraThreadGeneration);
                }
            }

            @Override
            public void onSurfaceTextureSizeChanged(SurfaceTexture surface, final int width, final int height) {
                if (textureView != callbackTextureView) {
                    return;
                }
                if (cameraThread != null) {
                    cameraThread.surfaceWidth = width;
                    cameraThread.surfaceHeight = height;
                    cameraThread.refreshPreviewGeometry();
                }
            }

            @Override
            public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
                if (textureView != null && textureView != callbackTextureView) {
                    return true;
                }
                ++cameraCoverGeneration;
                cancelCameraXVideoTransitions();
                ++cameraThreadGeneration;
                ++nmCamera2SwitchGeneration;
                ++nmPhysicalReopenGeneration;
                nmCamera2SwitchPending = false;
                nmCancelCamera2SwitchFrameWatchdog();
                final boolean cameraXCloseOwnedByThread = useCameraX && cameraThread != null;
                if (cameraThread != null) {
                    cameraThread.shutdown(
                            0, true, 0, 0, 0, 0,
                            cameraXCloseOwnedByThread ? surface : null);
                    cameraThread = null;
                }
                if (useCameraX) {
                    if (!cameraXCloseOwnedByThread) {
                        videoMessagesHelper.destroyCameraX(InstantCameraView.this);
                    }
                } else if (useCamera2) {
                    for (int a = 0; a < camera2Sessions.length; ++a) {
                        if (camera2Sessions[a] != null) {
                            camera2Sessions[a].destroy(false);
                            camera2Sessions[a] = null;
                        }
                    }
                    camera2SessionCurrent = null;
                } else {
                    if (cameraSession != null) {
                        CameraController.getInstance().close(cameraSession, null, null);
                    }
                }

                return !cameraXCloseOwnedByThread;
            }

            @Override
            public void onSurfaceTextureUpdated(SurfaceTexture surface) {
                if (textureView != callbackTextureView) {
                    return;
                }
            }
        });
        cameraContainer.addView(callbackTextureView, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.MATCH_PARENT));

        setVisibilityFromPause = fromPaused;
        setVisibility(VISIBLE);
        requestLayout();

        startAnimation(true, fromPaused);
        MediaController.getInstance().requestRecordAudioFocus(true);
    }

    private void loadLastCameraBitmapAsync(int generation) {
        Utilities.globalQueue.postRunnable(() -> {
            Bitmap decoded = null;
            try {
                File file = new File(ApplicationLoader.getFilesDirFixed(), "icthumb.jpg");
                decoded = BitmapFactory.decodeFile(file.getAbsolutePath());
            } catch (Throwable error) {
                FileLog.e(error);
            }
            final Bitmap result = decoded;
            AndroidUtilities.runOnUIThread(() -> onLastCameraBitmapLoaded(generation, result));
        });
    }

    private void onLastCameraBitmapLoaded(int generation, Bitmap decoded) {
        if (decoded == null) return;
        if (generation != cameraCoverGeneration || textureView == null || !opened
                || cancelled || cameraReady || lastBitmap != null) {
            decoded.recycle();
            return;
        }
        lastBitmap = decoded;
        textureOverlayView.setImageBitmap(decoded);
    }

    public InstantViewCameraContainer getCameraContainer() {
        return cameraContainer;
    }

    public void startAnimation(boolean open, boolean fromPaused) {
        dispatchAnimationState(open, fromPaused);
        if (animatorSet != null) {
            animatorSet.removeAllListeners();
            animatorSet.cancel();
        }
        PipRoundVideoView pipRoundVideoView = PipRoundVideoView.getInstance();
        if (pipRoundVideoView != null) {
            pipRoundVideoView.showTemporary(!open);
        }
        if (open && !opened) {
            cameraContainer.setTranslationX(0);
            textureOverlayView.setTranslationX(0);

            animationTranslationY = fromPaused ? 0 : getMeasuredHeight() / 2f;
            updateTranslationY();
        }
        opened = open;
        removeCallbacks(refreshRoundZoomControl);
        refreshRoundZoomControl.run();
        if (parentView != null) {
            parentView.invalidate();
        }
        animatorSet = new AnimatorSet();
        float toX = 0;
        if (!open) {
            toX = recordedTime > 300 ? dp(24) - getMeasuredWidth() / 2f : 0;
        }
        ValueAnimator translationYAnimator = ValueAnimator.ofFloat(open ? 1f : 0f, open ? 0 : 1f);
        translationYAnimator.addUpdateListener(animation -> {
            animationTranslationY = fromPaused ? 0 : (getMeasuredHeight() / 2f) * (float) animation.getAnimatedValue();
            updateTranslationY();
        });
        animatorSet.playTogether(
                ObjectAnimator.ofFloat(buttonsLayout, View.ALPHA, open ? 1.0f : 0.0f),
                ObjectAnimator.ofFloat(muteImageView, View.ALPHA, 0.0f),
                ObjectAnimator.ofInt(paint, AnimationProperties.PAINT_ALPHA, open ? 255 : 0),
                ObjectAnimator.ofFloat(cameraContainer, View.ALPHA, open ? 1.0f : 0.0f),
                ObjectAnimator.ofFloat(cameraContainer, View.SCALE_X, open ? 1.0f : 0.1f),
                ObjectAnimator.ofFloat(cameraContainer, View.SCALE_Y, open ? 1.0f : 0.1f),
                ObjectAnimator.ofFloat(cameraContainer, View.TRANSLATION_X, toX),
                ObjectAnimator.ofFloat(textureOverlayView, View.ALPHA, open ? 1.0f : 0.0f),
                ObjectAnimator.ofFloat(textureOverlayView, View.SCALE_X, open ? 1.0f : 0.1f),
                ObjectAnimator.ofFloat(textureOverlayView, View.SCALE_Y, open ? 1.0f : 0.1f),
                ObjectAnimator.ofFloat(textureOverlayView, View.TRANSLATION_X, toX),
                translationYAnimator
        );
        if (!open) {
            animatorSet.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    if (animation.equals(animatorSet)) {
                        hideCamera(true);
                        setVisibilityFromPause = false;
                        setVisibility(INVISIBLE);
                    }
                }
            });
        } else {
            setTranslationX(0);
            final AnimatorSet openingAnimator = animatorSet;
            openingAnimator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    if (animation == openingAnimator && animatorSet == openingAnimator
                            && cameraReady) {
                        fadeCameraPreviewCover();
                    }
                }
            });
        }
        animatorSet.setDuration(180);
        animatorSet.setInterpolator(new DecelerateInterpolator());
        animatorSet.start();
    }

    private void updateTranslationY() {
        textureOverlayView.setTranslationY(animationTranslationY + panTranslationY + cameraLayoutTranslationY);
        cameraContainer.setTranslationY(animationTranslationY + panTranslationY + cameraLayoutTranslationY);
    }

    public RectF getCameraRect() {
        cameraContainer.getLocationOnScreen(position);
        return new RectF(
                position[0],
                position[1],
                position[0] + cameraContainer.getWidth(),
                position[1] + cameraContainer.getHeight()
        );
    }

    public void changeVideoPreviewState(int state, float progress) {
        if (videoPlayer == null) {
            return;
        }
        if (state == 0) {
            startProgressTimer();
            videoPlayer.play();
        } else if (state == 1) {
            stopProgressTimer();
            videoPlayer.pause();
        } else if (state == 2) {
            videoPlayer.seekTo((long) (progress * videoPlayer.getDuration()));
        }
    }

    public void send(int state, boolean notify, int scheduleDate, int scheduleRepeatPeriod, int ttl, long effectId, long stars) {
        ++cameraCoverGeneration;
        pausePreviewToken = null;
        cancelCameraXVideoTransitions();
        if (textureView == null) {
            return;
        }
        ++nmPhysicalReopenGeneration;
        ++nmCamera2SwitchGeneration;
        nmCamera2SwitchPending = false;
        nmCancelRoundFrameWatchdog();
        nmCancelCameraXDualFrameWatchdog();
        nmCancelCamera2SwitchFrameWatchdog();
        stopProgressTimer();
        if (videoPlayer != null) {
            videoPlayer.releasePlayer(true);
            videoPlayer = null;
        }
        if (state == 4) {
            if (videoEncoder != null && recordedTime > 800) {
                videoEncoder.stopRecording(VideoRecorder.ENCODER_SEND_SEND, new SendOptions(notify, scheduleDate, scheduleRepeatPeriod, ttl, effectId, stars));
                return;
            }
            if (BuildVars.DEBUG_VERSION && !cameraFile.exists()) {
                FileLog.e(new RuntimeException("file not found :( round video"));
            }
            if (videoEditedInfo == null) {
                videoEditedInfo = new VideoEditedInfo();
                videoEditedInfo.startTime = -1;
                videoEditedInfo.endTime = -1;
            }

            videoEditedInfo.roundVideo = true;
            if (videoEditedInfo.needConvert()) {
                file = null;
                encryptedFile = null;
                key = null;
                iv = null;
                double totalDuration = videoEditedInfo.estimatedDuration;
                long startTime = videoEditedInfo.startTime >= 0 ? videoEditedInfo.startTime : 0;
                long endTime = videoEditedInfo.endTime >= 0 ? videoEditedInfo.endTime : videoEditedInfo.estimatedDuration;
                videoEditedInfo.estimatedDuration = endTime - startTime;
                videoEditedInfo.estimatedSize = Math.max(1, (long) (size * (videoEditedInfo.estimatedDuration / totalDuration)));
                videoEditedInfo.bitrate = recordedVideoBitrate;
                if (videoEditedInfo.startTime > 0) {
                    videoEditedInfo.startTime *= 1000;
                }
                if (videoEditedInfo.endTime > 0) {
                    videoEditedInfo.endTime *= 1000;
                }
                FileLoader.getInstance(currentAccount).cancelFileUpload(cameraFile.getAbsolutePath(), false);
            } else {
                videoEditedInfo.estimatedSize = Math.max(1, size);
            }
            videoEditedInfo.file = file;
            videoEditedInfo.encryptedFile = encryptedFile;
            videoEditedInfo.key = key;
            videoEditedInfo.iv = iv;
            MediaController.PhotoEntry entry = new MediaController.PhotoEntry(0, 0, 0, cameraFile.getAbsolutePath(), 0, true, 0, 0, 0);
            entry.ttl = ttl;
            entry.effectId = effectId;
            delegate.sendMedia(entry, videoEditedInfo, notify, scheduleDate, scheduleRepeatPeriod, false, stars);
            if (scheduleDate != 0) {
                startAnimation(false, false);
            }
            MediaController.getInstance().requestRecordAudioFocus(false);
        } else {
            cancelled = recordedTime < 800;
            recording = false;
            flashing = false;
            updateFlash();
            int reason;
            if (cancelled) {
                reason = 4;
            } else {
                reason = state == 3 ? 2 : 5;
            }
            if (cameraThread != null) {
                NotificationCenter.getInstance(currentAccount).postNotificationName(NotificationCenter.recordStopped, recordingGuid, reason);
                int send;
                if (cancelled) {
                    send = 0;
                } else if (state == 3) {
                    send = 2;
                } else {
                    send = 1;
                }
                saveLastCameraBitmap();
                cameraThread.shutdown(send, notify, scheduleDate, scheduleRepeatPeriod, ttl, effectId);
                cameraThread = null;
            }
            if (cancelled) {
                NotificationCenter.getInstance(currentAccount).postNotificationName(NotificationCenter.audioRecordTooShort, recordingGuid, true, (int) recordedTime);
                startAnimation(false, false);
                MediaController.getInstance().requestRecordAudioFocus(false);
            }
        }
    }

    private void saveLastCameraBitmap() {
        if (textureView == null || !textureView.isAvailable()) {
            return;
        }
        Bitmap bitmap = textureView.getBitmap(50, 50);
        if (bitmap != null && bitmap.getPixel(0, 0) != 0) {
            lastBitmap = bitmap;
            Utilities.blurBitmap(lastBitmap, 7);
            try {
                File file = new File(ApplicationLoader.getFilesDirFixed(), "icthumb.jpg");
                FileOutputStream stream = new FileOutputStream(file);
                lastBitmap.compress(Bitmap.CompressFormat.JPEG, 87, stream);
                stream.close();
            } catch (Throwable ignore) {
            }
        } else if (bitmap != null) {
            bitmap.recycle();
        }
    }

    public void cancel(boolean byGesture) {
        ++cameraCoverGeneration;
        pausePreviewToken = null;
        cancelCameraXVideoTransitions();
        stopProgressTimer();
        if (videoPlayer != null) {
            videoPlayer.releasePlayer(true);
            videoPlayer = null;
        }
        if (textureView == null) {
            return;
        }
        cancelled = true;
        ++nmPhysicalReopenGeneration;
        ++nmCamera2SwitchGeneration;
        nmCamera2SwitchPending = false;
        nmCancelRoundFrameWatchdog();
        nmCancelCameraXDualFrameWatchdog();
        nmCancelCamera2SwitchFrameWatchdog();
        recording = false;
        flashing = false;
        updateFlash();
        NotificationCenter.getInstance(currentAccount).postNotificationName(NotificationCenter.recordStopped, recordingGuid, byGesture ? 0 : 6);
        if (cameraThread != null) {
            saveLastCameraBitmap();
            cameraThread.shutdown(0, true, 0, 0, 0, 0);
            cameraThread = null;
        } else if (videoEncoder != null) {
            videoEncoder.stopRecording(VideoRecorder.ENCODER_SEND_CANCEL, new SendOptions(true, 0, 0, 0, 0, 0));
        }
        if (cameraFile != null) {
            if (BuildVars.LOGS_ENABLED) {
                FileLog.e("delete camera file by cancel");
            }
            cameraFile.delete();
            AutoDeleteMediaTask.unlockFile(cameraFile);
            cameraFile = null;
        }
        MediaController.getInstance().requestRecordAudioFocus(false);
        startAnimation(false, false);
        invalidate();
    }

    public View getButtonsLayout() {
        return buttonsLayout;
    }

    public View getMuteImageView() {
        return muteImageView;
    }

    public Paint getPaint() {
        return paint;
    }

    public void hideCamera(boolean async) {
        destroy(async);
        cameraContainer.setTranslationX(0);
        textureOverlayView.setTranslationX(0);
        animationTranslationY = 0;
        updateTranslationY();
        MediaController.getInstance().resumeByRewind();

        if (textureView != null) {
            ViewGroup parent = (ViewGroup) textureView.getParent();
            if (parent != null) {
                parent.removeView(textureView);
            }
        }
        textureView = null;
        cameraContainer.setImageReceiver(null);
    }

    private static int nmRoundCaptureHeight(int roundVideoSize) {

        return Math.min(1200, Math.max(roundVideoSize, 2 * roundVideoSize));
    }

    private Camera2Session nmCreateRoundSession(boolean front) {

        final MessagesController mc = MessagesController.getInstance(UserConfig.selectedAccount);
        mc.roundVideoSize = com.th3nekit.finegram.camera.CameraSettings.roundResolution(512);
        final int size = mc.roundVideoSize;
        final int capH = nmRoundCaptureHeight(size);
        final boolean preferLogical = !com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getRoundCamLogicalDisabled() && !nmRoundLogicalSlowThisSession;
        Camera2Session s = Camera2Session.create(front, size, size, preferLogical, capH, true);
        if (s == null && preferLogical) {

            com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.setRoundCamLogicalDisabled(true);
            s = Camera2Session.create(front, size, size, false, capH, true);
        }
        return s;
    }

    private void nmHandleRoundCamError(final int slot, final Camera2Session expectedSession,
                                       final int errorCode) {
        if (expectedSession == null || cancelled) {
            return;
        }
        if (slot < 0 || slot >= camera2Sessions.length
                || camera2Sessions[slot] != expectedSession) {
            return;
        }
        Camera2Session s = expectedSession;
        final boolean wasLogical = s != null && s.isLogical();
        if (wasLogical && !com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getRoundCamLogicalDisabled()) {

            final boolean structural =
                    errorCode == android.hardware.camera2.CameraDevice.StateCallback.ERROR_MAX_CAMERAS_IN_USE
                    || errorCode == -1;
            if (structural) {
                com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.setRoundCamLogicalDisabled(true);
            }
            if (!bothCameras) {
                nmReopenSinglePhysical();
                return;
            }
        }
        fallbackToSingleCamera(slot, errorCode);
    }

    private static volatile boolean nmRoundLogicalSlowThisSession;
    private Runnable nmRoundFrameWatchdog;
    private Runnable nmCamera2SwitchFrameWatchdog;
    private Runnable nmCameraXDualFrameWatchdog;
    private int nmPhysicalReopenGeneration;
    private int nmCameraXDualWatchdogGeneration;
    private volatile boolean nmCameraXActiveFrameObserved;
    private volatile int nmCameraXFrameMask;

    private static final long NM_ROUND_FRAME_TIMEOUT_MS = 2500;
    private static final long NM_CAMERAX_DUAL_FRAME_TIMEOUT_MS = 3500;
    private static final long NM_CAMERAX_INITIAL_WIDE_TIMEOUT_MS = 1450;

    private static final long NM_CAMERAX_DUAL_INITIAL_WIDE_TIMEOUT_MS = 2200;

    private static final long NM_CAMERAX_SINGLE_BLUR_IN_MS = 100;
    private static final long NM_CAMERAX_SINGLE_REVEAL_MS = 180;

    private void nmArmRoundFrameWatchdog() {
        nmCancelRoundFrameWatchdog();
        final Camera2Session watched = camera2SessionCurrent;
        if (watched == null || !watched.isLogical()) {
            return;
        }
        nmRoundFrameWatchdog = () -> {
            nmRoundFrameWatchdog = null;

            if (cancelled || cameraReady || bothCameras
                    || camera2SessionCurrent != watched
                    || camera2SessionCurrent == null || !camera2SessionCurrent.isLogical()
                    || com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getRoundCamLogicalDisabled() || nmRoundLogicalSlowThisSession) {
                return;
            }

            nmRoundLogicalSlowThisSession = true;
            nmReopenSinglePhysical();
        };
        AndroidUtilities.runOnUIThread(nmRoundFrameWatchdog, NM_ROUND_FRAME_TIMEOUT_MS);
    }

    private void nmCancelRoundFrameWatchdog() {
        if (nmRoundFrameWatchdog != null) {
            AndroidUtilities.cancelRunOnUIThread(nmRoundFrameWatchdog);
            nmRoundFrameWatchdog = null;
        }
    }

    private void nmCancelCameraXDualFrameWatchdog() {
        ++nmCameraXDualWatchdogGeneration;
        if (nmCameraXDualFrameWatchdog != null) {
            AndroidUtilities.cancelRunOnUIThread(nmCameraXDualFrameWatchdog);
            nmCameraXDualFrameWatchdog = null;
        }
    }

    private void nmCancelCameraXInitialWideWait() {
        final Runnable timeout;
        synchronized (cameraXInitialWideWaitLock) {
            ++cameraXInitialWideWaitGeneration;
            timeout = cameraXInitialWideTimeoutRunnable;
            cameraXInitialWideTimeoutRunnable = null;
            cameraXInitialWideWaitActive = false;
            cameraXInitialWideTimeoutRenderPending = false;
            pendingCameraXSwitchAfterInitialWide = false;
        }
        if (timeout != null) {
            AndroidUtilities.cancelRunOnUIThread(timeout);
        }
    }

    private void nmArmCameraXInitialWideTimeout() {
        final int generation;
        final long timeoutMs = cameraXInitialWideWaitTimeoutMs;
        final Runnable timeout;
        synchronized (cameraXInitialWideWaitLock) {
            generation = ++cameraXInitialWideWaitGeneration;
            timeout = new Runnable() {
                @Override
                public void run() {
                    final boolean shouldRender;
                    synchronized (cameraXInitialWideWaitLock) {
                        if (generation != cameraXInitialWideWaitGeneration
                                || cameraXInitialWideTimeoutRunnable != this
                                || !cameraXInitialWideWaitActive || cancelled) {
                            return;
                        }
                        cameraXInitialWideTimeoutRunnable = null;
                        cameraXInitialWideWaitActive = false;

                        cameraXInitialWideTimeoutRenderPending = !isFrontface;
                        shouldRender = cameraXInitialWideTimeoutRenderPending;
                    }
                    CameraGLThread thread = cameraThread;
                    if (shouldRender && thread != null) {

                        thread.requestRender(false, false);
                    }
                    nmMaybeReplayCameraXSwitchAfterInitialWide();
                }
            };
            cameraXInitialWideTimeoutRunnable = timeout;
        }
        AndroidUtilities.runOnUIThread(timeout, timeoutMs);
    }

    private void nmReleaseCameraXInitialWideWait() {
        final Runnable timeout;
        synchronized (cameraXInitialWideWaitLock) {
            ++cameraXInitialWideWaitGeneration;
            timeout = cameraXInitialWideTimeoutRunnable;
            cameraXInitialWideTimeoutRunnable = null;
            cameraXInitialWideWaitActive = false;
            cameraXInitialWideTimeoutRenderPending = false;
        }
        if (timeout != null) {
            AndroidUtilities.cancelRunOnUIThread(timeout);
        }
        nmMaybeReplayCameraXSwitchAfterInitialWide();
    }

    private void nmMaybeReplayCameraXSwitchAfterInitialWide() {
        if (!pendingCameraXSwitchAfterInitialWide || cameraXInitialWideWaitActive) return;
        AndroidUtilities.runOnUIThread(() -> {
            if (!pendingCameraXSwitchAfterInitialWide || cameraXInitialWideWaitActive
                    || cancelled || !bothCameras || !isFrontface || cameraThread == null
                    || !cameraReady || !isCameraSessionInitiated()) return;
            pendingCameraXSwitchAfterInitialWide = false;
            switchCameraButton.performClick();
        });
    }

    private boolean nmConsumeCameraXInitialWideTimeoutRender() {
        if (!cameraXInitialWideTimeoutRenderPending) return false;
        synchronized (cameraXInitialWideWaitLock) {
            if (!cameraXInitialWideTimeoutRenderPending) return false;
            cameraXInitialWideTimeoutRenderPending = false;
            return true;
        }
    }

    private void nmOnCameraXFrameAvailable(int index) {
        if (!useCameraX || index < 0 || index > 1) return;
        nmCameraXFrameMask |= 1 << index;

        if (bothCameras) {
            if ((nmCameraXFrameMask & 0b11) != 0b11) return;
        } else if (index != surfaceIndex) {
            return;
        }
        if (nmCameraXActiveFrameObserved) return;
        nmCameraXActiveFrameObserved = true;
        AndroidUtilities.runOnUIThread(this::nmCancelCameraXDualFrameWatchdog);
    }

    private boolean nmShouldHoldCameraXInitialWideFrame(long frameTimestamp) {
        if (!cameraXInitialWideWaitActive) return false;
        com.th3nekit.finegram.camera.CameraXSurfaceSession session =
                videoMessagesHelper.getRearSession();
        boolean lensReady = session != null && session.isInitialLensReady(frameTimestamp);
        long waitMs = SystemClock.elapsedRealtime()
                - cameraXInitialWideWaitStartedMs;
        if (lensReady || waitMs >= cameraXInitialWideWaitTimeoutMs) {
            nmReleaseCameraXInitialWideWait();
            return false;
        }
        return true;
    }

    private void nmArmCamera2SwitchFrameWatchdog(
            final int generation,
            final Camera2Session watched,
            final CameraGLThread expectedThread) {
        nmCancelCamera2SwitchFrameWatchdog();
        if (watched == null || expectedThread == null) {
            return;
        }
        nmCamera2SwitchFrameWatchdog = () -> {
            nmCamera2SwitchFrameWatchdog = null;
            if (generation != nmCamera2SwitchGeneration
                    || cancelled
                    || !nmCamera2SwitchPending
                    || bothCameras
                    || cameraThread != expectedThread
                    || camera2SessionCurrent != watched) {
                return;
            }

            nmReopenSinglePhysical();
        };
        AndroidUtilities.runOnUIThread(nmCamera2SwitchFrameWatchdog, 4000L);
    }

    private void nmCancelCamera2SwitchFrameWatchdog() {
        if (nmCamera2SwitchFrameWatchdog != null) {
            AndroidUtilities.cancelRunOnUIThread(nmCamera2SwitchFrameWatchdog);
            nmCamera2SwitchFrameWatchdog = null;
        }
    }

    private void nmReopenSinglePhysical() {
        try {
            if (!useCamera2) return;
            nmCancelRoundFrameWatchdog();
            nmCancelCamera2SwitchFrameWatchdog();
            bothCameras = false;
            final int generation = ++nmPhysicalReopenGeneration;
            final boolean targetFront = isFrontface;
            final int slot = targetFront ? 0 : 1;
            final Camera2Session previous = camera2SessionCurrent;
            camera2SessionCurrent = null;
            if (camera2Sessions[slot] == previous) camera2Sessions[slot] = null;
            Runnable reopen = () -> {
                if (generation != nmPhysicalReopenGeneration || !useCamera2 || cancelled
                        || isFrontface != targetFront) {
                    return;
                }
                try {
                    final int reopenSize = MessagesController.getInstance(UserConfig.selectedAccount).roundVideoSize;
                    final Camera2Session replacement = Camera2Session.create(targetFront,
                            reopenSize, reopenSize, false, nmRoundCaptureHeight(reopenSize), true);
                    if (replacement == null) {
                        return;
                    }
                    if (generation != nmPhysicalReopenGeneration || cancelled
                            || isFrontface != targetFront) {
                        replacement.destroy(true);
                        return;
                    }
                    camera2SessionCurrent = camera2Sessions[slot] = replacement;
                    replacement.setRecordingVideo(true);
                    replacement.whenError(err -> {
                        if (generation != nmPhysicalReopenGeneration
                                || cancelled
                                || camera2SessionCurrent != replacement
                                || camera2Sessions[slot] != replacement) {
                            return;
                        }
                        nmHandleRoundCamError(
                                slot, replacement, err == null ? -1 : err);
                    });
                    previewSize[0] = new Size(replacement.getPreviewWidth(), replacement.getPreviewHeight());
                    if (cameraThread != null) {
                        cameraThread.setCurrentSession(replacement);
                        cameraReady = false;
                        cameraThread.reinitForNewCamera();
                    }
                } catch (Throwable t) {
                    FileLog.e("Finegram: round-cam physical reopen failed", t);
                }
            };
            if (previous != null) {
                previous.destroy(true, reopen);
            } else {
                reopen.run();
            }
        } catch (Throwable t) {
            FileLog.e("Finegram: round-cam physical reopen failed", t);
        }
    }

    private void fallbackToSingleCamera(final int failedIndex, final int errorCode) {
        if (!useCamera2 || !bothCameras) {
            return;
        }
        bothCameras = false;
        final int keep = 1 - failedIndex;
        if (failedIndex >= 0 && failedIndex < camera2Sessions.length && camera2Sessions[failedIndex] != null) {
            try { camera2Sessions[failedIndex].destroy(true); } catch (Throwable ignore) {}
            camera2Sessions[failedIndex] = null;
        }
        camera2SessionCurrent = (keep >= 0 && keep < camera2Sessions.length) ? camera2Sessions[keep] : null;
        isFrontface = (keep == 0);
        if (cameraThread != null) {

            cameraThread.setSurfaceIndex(keep);
            if (camera2SessionCurrent != null) {
                cameraThread.setCurrentSession(camera2SessionCurrent);
                nmArmRoundFrameWatchdog();
            }
        }
        updateFlash();

        if (errorCode == android.hardware.camera2.CameraDevice.StateCallback.ERROR_MAX_CAMERAS_IN_USE) {
            DualCameraView.disableRoundDual();
        }
        try {
            org.telegram.ui.Components.BulletinFactory.global()
                    .createSimpleBulletin(R.raw.info, LocaleController.getString(R.string.FG_CameraDualUnavailable))
                    .show();
        } catch (Throwable ignore) {}
    }

    private void switchCamera() {
        stopRoundZoomSpring();
        if (useCameraX && bothCameras
                && (!videoMessagesHelper.isConcurrentDualReady()
                || (nmCameraXFrameMask & (1 << (1 - surfaceIndex))) == 0)) {

            flipAnimationInProgress = false;
            return;
        }
        cancelRoundZoomGesture();
        if (roundZoomControl != null) roundZoomControl.prepareForCameraSwitch();
        roundZoomAwaitingInitialValue = useCameraX && !bothCameras;
        isFrontface = !isFrontface;
        com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.setRoundLastRear(!isFrontface);
        if (useCameraX) {
            if (roundZoomControl != null && !bothCameras) prepareRoundZoomPreview(true);

            videoMessagesHelper.switchCameraX(this);
            com.th3nekit.finegram.camera.CameraXSurfaceSession current =
                    videoMessagesHelper.getCurrentSession();
            if (current != null && cameraThread != null) {
                if (bothCameras) {
                    int targetSurface = videoMessagesHelper.getSessionIndex(current);
                    startDualCameraCrossfade(current, targetSurface);
                    removeCallbacks(refreshRoundZoomControl);
                    refreshRoundZoomControl.run();
                }
            }
            updateFlash();

            if (!bothCameras) {
                cameraReady = false;
            }
            return;
        } else if (useCamera2) {
            updateFlash();
            if (bothCameras) {
                camera2SessionCurrent = camera2Sessions[isFrontface ? 0 : 1];
                cameraThread.setCurrentSession(camera2SessionCurrent);
                cameraThread.flipSurfaces();
                return;
            } else {
                nmSwitchSingleCamera2(isFrontface);
                return;
            }
        } else {
            updateFlash();
            if (cameraSession != null) {
                cameraSession.destroy();
                CameraController.getInstance().close(cameraSession, null, null);
                cameraSession = null;
            }
        }
        initCamera();
        cameraReady = false;
        cameraThread.reinitForNewCamera();
    }

    private void nmSwitchSingleCamera2(final boolean targetFront) {
        ++nmPhysicalReopenGeneration;
        final int generation = ++nmCamera2SwitchGeneration;
        final CameraGLThread expectedThread = cameraThread;
        final Camera2Session previous = camera2SessionCurrent;

        nmCancelRoundFrameWatchdog();
        nmCancelCamera2SwitchFrameWatchdog();
        nmCamera2SwitchPending = true;
        cameraReady = false;
        camera2SessionCurrent = null;
        for (int i = 0; i < camera2Sessions.length; i++) {
            if (camera2Sessions[i] == previous) {
                camera2Sessions[i] = null;
            }
        }

        final Runnable openReplacement = () -> {
            if (generation != nmCamera2SwitchGeneration
                    || cancelled
                    || expectedThread == null
                    || cameraThread != expectedThread
                    || bothCameras
                    || isFrontface != targetFront) {
                return;
            }

            final Camera2Session replacement = nmCreateRoundSession(targetFront);
            if (replacement == null) {
                nmCamera2SwitchPending = false;
                flipAnimationInProgress = false;
                return;
            }

            final int slot = targetFront ? 0 : 1;
            camera2Sessions[slot] = replacement;
            camera2SessionCurrent = replacement;
            replacement.setRecordingVideo(true);
            replacement.whenError(error -> {
                if (generation != nmCamera2SwitchGeneration
                        || cancelled
                        || camera2SessionCurrent != replacement) {
                    return;
                }
                nmHandleRoundCamError(
                        slot, replacement, error == null ? -1 : error);
            });
            previewSize[0] = new Size(
                    replacement.getPreviewWidth(), replacement.getPreviewHeight());
            updateFlash();

            expectedThread.reinitForNewCamera();
            nmArmRoundFrameWatchdog();
            nmArmCamera2SwitchFrameWatchdog(generation, replacement, expectedThread);
        };

        if (previous != null) {
            previous.destroy(true, openReplacement);
        } else {
            openReplacement.run();
        }
    }

    private void nmOnCamera2SwitchFirstFrame() {
        if (!useCamera2 || !nmCamera2SwitchPending) {
            return;
        }
        final int generation = nmCamera2SwitchGeneration;
        AndroidUtilities.runOnUIThread(() -> {
            if (generation != nmCamera2SwitchGeneration || !nmCamera2SwitchPending) {
                return;
            }
            nmCamera2SwitchPending = false;
            nmCancelCamera2SwitchFrameWatchdog();
        });
    }

    private void startDualCameraCrossfade(Object targetSession, int targetSurface) {
        CameraGLThread thread = cameraThread;
        if (thread == null || targetSession == null || targetSurface < 0 || targetSurface > 1
                || targetSurface == surfaceIndex) {
            flipAnimationInProgress = false;
            return;
        }
        if (dualCameraSwitchAnimator != null) {
            dualCameraSwitchAnimator.cancel();
        }
        thread.beginDualSurfaceSwitch(targetSession, targetSurface);
        dualCameraSwitchAnimator = ValueAnimator.ofFloat(0f, 1f);
        dualCameraSwitchAnimator.setDuration(300L);
        dualCameraSwitchAnimator.setInterpolator(CubicBezierInterpolator.EASE_BOTH);
        dualCameraSwitchAnimator.addUpdateListener(animation -> {
            CameraGLThread activeThread = cameraThread;
            if (activeThread != null) {
                activeThread.setDualSurfaceSwitchProgress((float) animation.getAnimatedValue());
            }
        });
        dualCameraSwitchAnimator.addListener(new AnimatorListenerAdapter() {
            private boolean cancelled;

            @Override
            public void onAnimationCancel(Animator animation) {
                cancelled = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                CameraGLThread activeThread = cameraThread;
                if (activeThread != null) {
                    if (cancelled) {
                        activeThread.cancelDualSurfaceSwitch();
                    } else {
                        activeThread.finishDualSurfaceSwitch();
                    }
                } else {
                    onDualCameraSwitchFinished();
                }
            }
        });
        dualCameraSwitchAnimator.start();
    }

    private void onDualCameraSwitchFinished() {
        dualCameraSwitchAnimator = null;
        flipAnimationInProgress = false;
        if (cameraThread == null) {
            dualVideoSwitching = false;
        }
        removeCallbacks(refreshRoundZoomControl);
        refreshRoundZoomControl.run();
    }

    private void startCameraXVideoTransition() {
        clearCameraXVideoTransition();
        cameraXVideoTransitionActive = true;
        cameraXSingleSwitchProgress = 0f;
        cameraXSingleSwitchNewFrame = false;

        cameraXVideoBlurAnimator = ValueAnimator.ofFloat(0f, 0.82f);
        cameraXVideoBlurAnimator.setDuration(NM_CAMERAX_SINGLE_BLUR_IN_MS);
        cameraXVideoBlurAnimator.setInterpolator(CubicBezierInterpolator.EASE_OUT);
        cameraXVideoBlurAnimator.addUpdateListener(animation -> {
            cameraXSingleSwitchBlur = (float) animation.getAnimatedValue();
            CameraGLThread thread = cameraThread;
            if (thread != null) thread.requestRender(false, false);
        });
        cameraXVideoBlurAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                if (cameraXVideoBlurAnimator == animation) {
                    cameraXVideoBlurAnimator = null;
                    cameraXSingleSwitchBlur = 0.82f;
                }
            }
        });
        cameraXVideoBlurAnimator.start();
        cameraXVideoBlurTimeout = () -> {
            cameraXVideoBlurTimeout = null;

            clearCameraXVideoTransition();
            flipAnimationInProgress = false;
            CameraGLThread thread = cameraThread;
            if (thread != null) thread.requestRender(false, false);
        };
        AndroidUtilities.runOnUIThread(cameraXVideoBlurTimeout, 1800L);
    }

    private void finishCameraXVideoTransition() {
        if (cameraXSingleSwitchFinishing) {
            return;
        }
        if (!cameraXVideoTransitionActive || !cameraXSingleSwitchNewFrame) {
            if (useCameraX && !bothCameras) flipAnimationInProgress = false;
            return;
        }
        cameraXSingleSwitchFinishing = true;
        removeCallbacks(refreshRoundZoomControl);
        refreshRoundZoomControl.run();
        if (cameraXVideoBlurTimeout != null) {
            AndroidUtilities.cancelRunOnUIThread(cameraXVideoBlurTimeout);
            cameraXVideoBlurTimeout = null;
        }
        if (cameraXVideoBlurAnimator != null) {
            cameraXVideoBlurAnimator.removeAllListeners();
            cameraXVideoBlurAnimator.cancel();
            cameraXVideoBlurAnimator = null;
        }
        final float blurFrom = cameraXSingleSwitchBlur;
        final float progressFrom = cameraXSingleSwitchProgress;
        cameraXVideoBlurAnimator = ValueAnimator.ofFloat(0f, 1f);
        cameraXVideoBlurAnimator.setDuration(NM_CAMERAX_SINGLE_REVEAL_MS);
        cameraXVideoBlurAnimator.setInterpolator(CubicBezierInterpolator.EASE_BOTH);
        cameraXVideoBlurAnimator.addUpdateListener(animation -> {
            float p = (float) animation.getAnimatedValue();
            cameraXSingleSwitchProgress = progressFrom + (1f - progressFrom) * p;
            cameraXSingleSwitchBlur = blurFrom * (1f - p);
            CameraGLThread thread = cameraThread;
            if (thread != null) thread.requestRender(false, false);
        });
        cameraXVideoBlurAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                if (cameraXVideoBlurAnimator != animation) return;
                cameraXVideoBlurAnimator = null;
                cameraXSingleSwitchProgress = 1f;
                cameraXVideoTransitionActive = false;
                cameraXSingleSwitchBlur = 0f;
                cameraXSingleSwitchNewFrame = false;
                cameraXSingleSwitchFinishing = false;
                flipAnimationInProgress = false;
                CameraGLThread thread = cameraThread;
                if (thread != null) thread.requestRender(false, false);
            }
        });
        cameraXVideoBlurAnimator.start();
    }

    private void clearCameraXVideoTransition() {
        if (cameraXVideoBlurTimeout != null) {
            AndroidUtilities.cancelRunOnUIThread(cameraXVideoBlurTimeout);
            cameraXVideoBlurTimeout = null;
        }
        if (cameraXVideoBlurAnimator != null) {
            cameraXVideoBlurAnimator.removeAllListeners();
            cameraXVideoBlurAnimator.cancel();
            cameraXVideoBlurAnimator = null;
        }
        cameraXVideoTransitionActive = false;
        cameraXSingleSwitchBlur = 0f;
        cameraXSingleSwitchProgress = 0f;
        cameraXSingleSwitchNewFrame = false;
        cameraXSingleSwitchFinishing = false;
    }

    private void requestRoundCameraXZoom(float ratio) {
        if (Float.isNaN(ratio) || Float.isInfinite(ratio) || cancelled) return;
        if (flipAnimationInProgress || cameraXSingleSwitchAwaitingBind) return;
        final CameraXSurfaceSession session = videoMessagesHelper.getCurrentSession();
        final CameraGLThread thread = cameraThread;
        if (session == null) return;
        if (!com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getSmoothCameraModuleTransitions() || session.isFrontFacing()
                || !cameraReady || !recording || thread == null) {
            cancelCameraXRearLensTransition();
            session.setZoomRatio(ratio);
            return;
        }
        if (cameraXRearLensSession != session) cancelCameraXRearLensTransition();
        if (CameraXRoundLensTransition.crossesWideBoundary(
                session.getZoomRatio(), ratio, session.getMinZoomRatio())) {
            CameraXRoundLensTransition previous = cameraXRearLensTransition;
            long timestamp = thread.rearLensFrameTimestamp;
            CameraXLensFrame frame = session.getLensFrame(timestamp);
            CameraXRoundLensTransition transition = new CameraXRoundLensTransition(
                    timestamp, ratio, frame == null ? Float.NaN : frame.zoomRatio,
                    frame == null ? session.getActivePhysicalCameraId() : frame.physicalId,
                    SystemClock.elapsedRealtime(), previous == null ? 0f : previous.getStrength());
            cameraXRearLensSession = session;
            cameraXRearLensTransition = transition;
        }
        session.setZoomRatio(ratio);
    }

    private float onCameraXRearLensFrame(long timestamp) {
        CameraXRoundLensTransition transition = cameraXRearLensTransition;
        final CameraXSurfaceSession session = videoMessagesHelper.getCurrentSession();
        final CameraGLThread thread = cameraThread;
        if (session == null || thread == null || session.isFrontFacing()
                || !com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getSmoothCameraModuleTransitions()) return 0f;
        int generation = session.getSurfaceRequestGeneration();
        if (thread.rearLensMetadataOwner != session || thread.rearLensMetadataGeneration != generation) {
            thread.rearLensMetadata = null;
            thread.rearLensMetadataOwner = session;
            thread.rearLensMetadataGeneration = generation;
            cancelCameraXRearLensTransition();
            transition = null;
        }
        CameraXLensFrame frame = session.getLensFrame(timestamp);
        CameraXLensFrame previous = thread.rearLensMetadata;
        if (frame != null && (previous == null || frame.timestampNanos > previous.timestampNanos)) {
            if (previous != null && previous.physicalId != null && frame.physicalId != null
                    && !previous.physicalId.equals(frame.physicalId)) {
                transition = new CameraXRoundLensTransition(previous.timestampNanos, frame.zoomRatio,
                        previous.zoomRatio, previous.physicalId, SystemClock.elapsedRealtime(),
                        transition == null ? 0f : transition.getStrength());
                transition.setRevealStart(frame.timestampNanos);
                cameraXRearLensSession = session;
                cameraXRearLensTransition = transition;
            }
            thread.rearLensMetadata = frame;
        }
        float strength = transition == null || session != cameraXRearLensSession ? 0f
                : transition.onFrame(timestamp, frame == null ? 0 : frame.timestampNanos,
                frame == null ? Float.NaN : frame.zoomRatio,
                frame == null ? null : frame.physicalId, SystemClock.elapsedRealtime());
        return strength;
    }

    private void cancelCameraXRearLensTransition() {
        cameraXRearLensTransition = null;
        cameraXRearLensSession = null;
    }

    private void cancelCameraXVideoTransitions() {
        if (!useCameraX) return;
        cancelCameraXRearLensTransition();
        if (dualCameraSwitchAnimator != null) {
            dualCameraSwitchAnimator.removeAllListeners();
            dualCameraSwitchAnimator.cancel();
            dualCameraSwitchAnimator = null;
        }
        dualVideoSwitching = false;
        pendingCameraXSwitchAfterDualCollapse = false;
        nmCancelCameraXInitialWideWait();
        cameraXSingleSwitchAwaitingBind = false;
        pendingCameraXSingleSession = null;
        clearCameraXVideoTransition();
        flipAnimationInProgress = false;
    }

    private void onCameraPreviewReady() {
        removeCallbacks(refreshRoundZoomControl);
        refreshRoundZoomControl.run();

        ++cameraCoverGeneration;
        if (animatorSet == null || !animatorSet.isRunning() || !opened) {
            fadeCameraPreviewCover();
        }
        if (cameraXSingleSwitchNewFrame) {
            finishCameraXVideoTransition();
        }
        nmMaybeReplayCameraXSwitchAfterInitialWide();
        if (useCameraX && !bothCameras
                && pendingCameraXSwitchAfterDualCollapse) {
            pendingCameraXSwitchAfterDualCollapse = false;

            AndroidUtilities.runOnUIThread(
                    () -> switchCameraButton.performClick());
        }
    }

    private void fadeCameraPreviewCover() {
        if (textureOverlayView == null || textureView == null || !opened
                || !textureView.isAvailable() || cancelled || !cameraReady) return;
        textureOverlayView.animate().cancel();
        textureOverlayView.animate()
                .alpha(0f)
                .setDuration(120L)
                .setInterpolator(new DecelerateInterpolator())
                .start();
    }

    @Override
    public View getZoomControlView() { return roundZoomControl; }

    @Override
    public View getEvControlView() { return evControlView; }

    @Deprecated
    private boolean initCamera() {
        if (useCameraX || useCamera2) {
            return true;
        }
        ArrayList<CameraInfo> cameraInfos = CameraController.getInstance().getCameras();
        if (cameraInfos == null) {
            return false;
        }
        CameraInfo notFrontface = null;
        selectedCamera = null;
        String manualId = com.th3nekit.finegram.core.configs.FinegramCameraConfig.manualIdFor(isFrontface).trim();
        for (CameraInfo info : cameraInfos) {
            if (info.isFrontface() == isFrontface && String.valueOf(info.getCameraId()).equals(manualId)) {
                selectedCamera = info;
                break;
            }
        }
        for (int a = 0; a < cameraInfos.size(); a++) {
            CameraInfo cameraInfo = cameraInfos.get(a);
            if (!cameraInfo.isFrontface()) {
                notFrontface = cameraInfo;
            }
            if (selectedCamera == null && cameraInfo.isFrontface() == isFrontface) {
                selectedCamera = cameraInfo;
                break;
            } else {
                notFrontface = cameraInfo;
            }
        }
        if (selectedCamera == null) {
            selectedCamera = notFrontface;
        }
        if (selectedCamera == null) {
            return false;
        }

        ArrayList<Size> previewSizes = selectedCamera.getPreviewSizes();
        ArrayList<Size> pictureSizes = selectedCamera.getPictureSizes();

        previewSize[0] = chooseOptimalSize(previewSizes);
        pictureSize = chooseOptimalSize(pictureSizes);
        if (previewSize[0].mWidth != pictureSize.mWidth) {
            boolean found = false;
            for (int a = previewSizes.size() - 1; a >= 0; a--) {
                Size preview = previewSizes.get(a);
                for (int b = pictureSizes.size() - 1; b >= 0; b--) {
                    Size picture = pictureSizes.get(b);
                    if (preview.mWidth >= pictureSize.mWidth && preview.mHeight >= pictureSize.mHeight && preview.mWidth == picture.mWidth && preview.mHeight == picture.mHeight) {
                        previewSize[0] = preview;
                        pictureSize = picture;
                        found = true;
                        break;
                    }
                }
                if (found) {
                    break;
                }
            }

            if (!found) {
                for (int a = previewSizes.size() - 1; a >= 0; a--) {
                    Size preview = previewSizes.get(a);
                    for (int b = pictureSizes.size() - 1; b >= 0; b--) {
                        Size picture = pictureSizes.get(b);
                        if (preview.mWidth >= 360 && preview.mHeight >= 360 && preview.mWidth == picture.mWidth && preview.mHeight == picture.mHeight) {
                            previewSize[0] = preview;
                            pictureSize = picture;
                            found = true;
                            break;
                        }
                    }
                    if (found) {
                        break;
                    }
                }
            }
        }
        if (BuildVars.LOGS_ENABLED) {
            FileLog.d("InstantCamera preview w = " + previewSize[0].mWidth + " h = " + previewSize[0].mHeight);
        }
        return true;
    }

    @Deprecated
    private Size chooseOptimalSize(ArrayList<Size> previewSizes) {
        ArrayList<Size> sortedSizes = new ArrayList<>();
        boolean allowBigSizeCamera = allowBigSizeCamera();
        int maxVideoSize = allowBigSizeCamera ? 1440 : 1200;
        if (Build.MANUFACTURER.equalsIgnoreCase("Samsung")) {

            maxVideoSize = 1200;
        }
        for (int i = 0; i < previewSizes.size(); i++) {
            if (Math.max(previewSizes.get(i).mHeight, previewSizes.get(i).mWidth) <= maxVideoSize && Math.min(previewSizes.get(i).mHeight, previewSizes.get(i).mWidth) >= 320) {
                sortedSizes.add(previewSizes.get(i));
            }
        }
        if (sortedSizes.isEmpty() || !allowBigSizeCamera()) {
            ArrayList<Size> sizes = sortedSizes;
            if (!sortedSizes.isEmpty()) {
                sizes = sortedSizes;
            } else {
                sizes = previewSizes;
            }
            if (Build.MANUFACTURER.equalsIgnoreCase("Xiaomi")) {
                return CameraController.chooseOptimalSize(sizes, 640, 480, aspectRatio, false);
            } else {
                return CameraController.chooseOptimalSize(sizes, 480, 270, aspectRatio, false);
            }
        }
        Collections.sort(sortedSizes, (o1, o2) -> {
            float a1 = Math.abs(1f - Math.min(o1.mHeight, o1.mWidth) / (float) Math.max(o1.mHeight, o1.mWidth));
            float a2 = Math.abs(1f - Math.min(o2.mHeight, o2.mWidth) / (float) Math.max(o2.mHeight, o2.mWidth));

            if (a1 < a2) {
                return -1;
            } else if (a1 > a2) {
                return 1;
            }
            return 0;
        });
        return sortedSizes.get(0);
    }

    @Deprecated
    private boolean allowBigSizeCamera() {
        if (SharedConfig.bigCameraForRound) {
            return true;
        }
        if (SharedConfig.deviceIsAboveAverage()) {
            return true;
        }
        int devicePerformanceClass = Math.max(SharedConfig.getDevicePerformanceClass(), SharedConfig.getLegacyDevicePerformanceClass());
        if (devicePerformanceClass == SharedConfig.PERFORMANCE_CLASS_HIGH) {
            return true;
        }
        int hash = (Build.MANUFACTURER + " " + Build.DEVICE).toUpperCase().hashCode();
        for (int i = 0; i < ALLOW_BIG_CAMERA_WHITELIST.length; ++i) {
            if (ALLOW_BIG_CAMERA_WHITELIST[i] == hash) {
                return true;
            }
        }
        return false;
    }

    @Deprecated
    public static boolean allowBigSizeCameraDebug() {
        int devicePerformanceClass = Math.max(SharedConfig.getDevicePerformanceClass(), SharedConfig.getLegacyDevicePerformanceClass());
        if (devicePerformanceClass == SharedConfig.PERFORMANCE_CLASS_HIGH) {
            return true;
        }
        int hash = (Build.MANUFACTURER + " " + Build.DEVICE).toUpperCase().hashCode();
        for (int i = 0; i < ALLOW_BIG_CAMERA_WHITELIST.length; ++i) {
            if (ALLOW_BIG_CAMERA_WHITELIST[i] == hash) {
                return true;
            }
        }
        return false;
    }

    private void createCamera(final int index, final SurfaceTexture surfaceTexture,
                              final CameraGLThread expectedThread,
                              final int expectedGeneration) {
        AndroidUtilities.runOnUIThread(() -> {
            if (expectedThread.shutdownRequested
                    || cameraThread != expectedThread
                    || cameraThreadGeneration != expectedGeneration) {
                return;
            }
            if (BuildVars.LOGS_ENABLED) {
                FileLog.d("InstantCamera create camera session " + index);
            }

            if (useCameraX) {
                synchronized (expectedThread) {
                    if (expectedThread.shutdownRequested
                            || cameraThread != expectedThread
                            || cameraThreadGeneration != expectedGeneration) {
                        return;
                    }
                    if (!bothCameras && index == 0) {

                        videoMessagesHelper.createCameraX(this, surfaceTexture);
                    } else if (bothCameras && index == 1) {

                        videoMessagesHelper.createCameraX(this,
                                expectedThread.cameraSurface[0],
                                expectedThread.cameraSurface[1]);
                    }
                }
            } else if (useCamera2) {
                if (bothCameras) {
                    if (camera2Sessions[index] != null) {
                        camera2Sessions[index].open(surfaceTexture);
                    }
                } else {
                    if (index == 1) return;

                    final Camera2Session session = camera2SessionCurrent;
                    if (session == null) {
                        return;
                    }
                    expectedThread.setCurrentSession(session);
                    session.open(surfaceTexture);
                }
            } else {
                if (index == 1) return;
                surfaceTexture.setDefaultBufferSize(previewSize[0].getWidth(), previewSize[0].getHeight());
                cameraSession = new CameraSession(selectedCamera, previewSize[0], pictureSize, ImageFormat.JPEG, true);
                updateFlash();
                expectedThread.setCurrentSession(cameraSession);
                CameraController.getInstance().openRound(cameraSession, surfaceTexture, () -> {
                    if (cameraSession != null
                            && !expectedThread.shutdownRequested
                            && cameraThread == expectedThread
                            && cameraThreadGeneration == expectedGeneration) {
                        updateFlash();

                        boolean updateScale = false;
                        try {
                            Camera.Size size = cameraSession.getCurrentPreviewSize();
                            if (size.width != previewSize[0].getWidth() || size.height != previewSize[0].getHeight()) {
                                previewSize[0] = new Size(size.width, size.height);
                                FileLog.d("InstantCamera change preview size to w = " + previewSize[0].getWidth() + " h = " + previewSize[0].getHeight());
                            }
                        } catch (Exception e) {
                            FileLog.e(e);
                        }

                        try {
                            Camera.Size size = cameraSession.getCurrentPictureSize();
                            if (size.width != pictureSize.getWidth() || size.height != pictureSize.getHeight()) {
                                pictureSize = new Size(size.width, size.height);
                                FileLog.d("InstantCamera change picture size to w = " + pictureSize.getWidth() + " h = " + pictureSize.getHeight());
                                updateScale = true;
                            }
                        } catch (Exception e) {
                            FileLog.e(e);
                        }
                        if (BuildVars.LOGS_ENABLED) {
                            FileLog.d("InstantCamera camera initied");
                        }
                        cameraSession.setInitied();
                        if (updateScale) {
                            expectedThread.reinitForNewCamera();
                        }
                    }
                }, () -> {
                    if (!expectedThread.shutdownRequested
                            && cameraThread == expectedThread
                            && cameraThreadGeneration == expectedGeneration) {
                        expectedThread.setCurrentSession(cameraSession);
                    }
                });
            }
        });
    }

    private int loadShader(int type, String shaderCode) {
        int shader = GLES20.glCreateShader(type);
        GLES20.glShaderSource(shader, shaderCode);
        GLES20.glCompileShader(shader);
        int[] compileStatus = new int[1];
        GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, compileStatus, 0);
        if (compileStatus[0] == 0) {
            if (BuildVars.LOGS_ENABLED) {
                FileLog.e(GLES20.glGetShaderInfoLog(shader));
            }
            GLES20.glDeleteShader(shader);
            shader = 0;
        }
        return shader;
    }

    private Timer progressTimer;

    private void startProgressTimer() {
        if (progressTimer != null) {
            try {
                progressTimer.cancel();
                progressTimer = null;
            } catch (Exception e) {
                FileLog.e(e);
            }
        }
        progressTimer = new Timer();
        progressTimer.schedule(new TimerTask() {
            @Override
            public void run() {
                AndroidUtilities.runOnUIThread(() -> {
                    try {
                        if (videoPlayer != null && videoEditedInfo != null && videoEditedInfo.endTime > 0 && videoPlayer.getCurrentPosition() >= videoEditedInfo.endTime) {
                            videoPlayer.seekTo(videoEditedInfo.startTime > 0 ? videoEditedInfo.startTime : 0);
                        }
                    } catch (Exception e) {
                        FileLog.e(e);
                    }
                });
            }
        }, 0, 17);
    }

    private void stopProgressTimer() {
        if (progressTimer != null) {
            try {
                progressTimer.cancel();
                progressTimer = null;
            } catch (Exception e) {
                FileLog.e(e);
            }
        }
    }

    public void onPanTranslationUpdate(float y) {
        panTranslationY = y / 2f;
        updateTranslationY();
    }

    public TextureView getTextureView() {
        return textureView;
    }

    public void setIsMessageTransition(boolean isMessageTransition) {
        if (this.isMessageTransition == isMessageTransition) {
            return;
        }
        this.isMessageTransition = isMessageTransition;
        if (!isMessageTransition) {
            requestLayout();
        }
    }

    public void resetCameraFile() {
        cameraFile = null;
    }

    private VideoRecorder videoEncoder;

    private Bitmap firstFrameThumb;
    private volatile int surfaceIndex;

    public class CameraGLThread extends DispatchQueue {
        private volatile long rearLensFrameTimestamp;
        private CameraXLensFrame rearLensMetadata;
        private Object rearLensMetadataOwner;
        private int rearLensMetadataGeneration = -1;
        private float rearLensFrameMix;
        private boolean rearFrameCrossfade;
        private final CameraXFrameCrossfade rearCrossfade = new CameraXFrameCrossfade();

        private final static int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
        private final static int EGL_OPENGL_ES2_BIT = 4;
        private final SurfaceTexture surfaceTexture;
        private final int generation;
        private volatile boolean shutdownRequested;
        private VideoRecorder shutdownEncoder;
        private volatile SurfaceTexture outputSurfaceToReleaseOnShutdown;
        private EGL10 egl10;
        private EGLDisplay eglDisplay;
        private EGLContext eglContext;
        private EGLSurface eglSurface;
        private boolean initied;

        private Object currentSession;
        private final Object[] surfaceSessions = new Object[2];
        private final int[] surfaceSessionGenerations = new int[2];

        private final SurfaceTexture[] cameraSurface = new SurfaceTexture[2];

        private final int[] cameraTexture = new int[] { Integer.MIN_VALUE, Integer.MIN_VALUE };

        private final int DO_RENDER_MESSAGE = 0;
        private final int DO_SHUTDOWN_MESSAGE = 1;
        private final int DO_REINIT_MESSAGE = 2;
        private final int DO_SETSESSION_MESSAGE = 3;
        private final int DO_FLIP = 4;
        private final int DO_SET_SURFACE_INDEX = 5;
        private final int DO_DUAL_SWITCH_BEGIN = 6;
        private final int DO_DUAL_SWITCH_PROGRESS = 7;
        private final int DO_DUAL_SWITCH_FINISH = 8;
        private final int DO_DUAL_SWITCH_CANCEL = 9;
        private final int DO_REFRESH_PREVIEW_GEOMETRY = 10;
        private final int DO_CAMERA_X_SINGLE_SNAPSHOT = 11;
        private final int DO_RESET_CAMERAX_FRAME_STATE = 12;

        private int drawProgram;
        private int vertexMatrixHandle;
        private int textureMatrixHandle;
        private int positionHandle;
        private int textureHandle;
        private int alphaHandle;
        private int texelSizeHandle;
        private int switchBlurHandle;
        private int snapshotProgram;
        private int snapshotVertexMatrixHandle;
        private int snapshotTextureMatrixHandle;
        private int snapshotPositionHandle;
        private int snapshotTextureHandle;
        private int snapshotAlphaHandle;
        private int snapshotTexelSizeHandle;
        private int snapshotSwitchBlurHandle;
        private volatile boolean snapshotContextClosed;

        private final class CameraSnapshot {
            final int texture;
            final int width;
            final int height;
            final long timestamp;
            final FloatBuffer textureBuffer;
            final float[] identityMatrix;
            private int references = 1;
            private boolean presented;

            CameraSnapshot(int texture, int width, int height, long timestamp) {
                this.texture = texture;
                this.width = width;
                this.height = height;
                this.timestamp = timestamp;
                textureBuffer = cameraXSnapshotTextureBuffer.duplicate();
                identityMatrix = cameraXSnapshotIdentityMatrix.clone();
            }

            boolean belongsTo(CameraGLThread thread) {
                return CameraGLThread.this == thread;
            }

            boolean hasLiveSource() {
                return !snapshotContextClosed && isCurrentGeneration();
            }

            synchronized boolean retain() {
                if (references == 0) return false;
                references++;
                return true;
            }

            void release() {
                synchronized (this) {
                    if (references == 0 || --references != 0) return;
                }

                Handler ownerHandler = getHandler();
                if (ownerHandler != null && !snapshotContextClosed) {
                    ownerHandler.post(() -> {
                        if (snapshotContextClosed || eglContext == null || eglSurface == null) return;
                        if (!egl10.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) return;
                        GLES20.glFinish();
                        GLES20.glDeleteTextures(1, new int[] { texture }, 0);
                    });
                }

            }
        }

        private void clearCameraXSnapshot() {
            CameraSnapshot snapshot = cameraXSingleSwitchSnapshotHandle;
            if (snapshot != null && !snapshot.belongsTo(this)) return;
            cameraXSingleSwitchSnapshotHandle = null;
            cameraXSingleSwitchSnapshot = 0;
            cameraXSingleSwitchSnapshotWidth = 0;
            cameraXSingleSwitchSnapshotHeight = 0;
            cameraXSingleSwitchSnapshotTimestamp = 0;
            if (snapshot != null) snapshot.release();
        }

        private boolean recording;

        private Integer cameraId = 0;
        private final boolean[] cameraFrameAvailable = new boolean[2];
        private final boolean[] cameraFrameLatched = new boolean[2];
        private final long[] cameraFrameTimestamp = new long[2];
        private final float[][] screenSTMatrix = new float[2][16];
        private final float[][] screenMVPMatrix = new float[2][16];
        private boolean dualSurfaceSwitching;
        private int dualSwitchFrom = -1;
        private int dualSwitchTo = -1;
        private float dualSwitchProgress;
        private Object dualSwitchTargetSession;

        private int surfaceWidth;
        private int surfaceHeight;

        public CameraGLThread(SurfaceTexture surface, int surfaceWidth,
                              int surfaceHeight, int generation) {
            super("CameraGLThread");
            surfaceTexture = surface;
            this.generation = generation;

            this.surfaceWidth = surfaceWidth;
            this.surfaceHeight = surfaceHeight;
        }

        private void postToUiIfCurrent(Runnable action) {
            if (action == null) return;
            final CameraGLThread expectedThread = this;
            final int expectedGeneration = generation;
            final SurfaceTexture expectedSurface = surfaceTexture;
            AndroidUtilities.runOnUIThread(() -> {
                TextureView expectedTextureView = textureView;
                if (expectedThread.shutdownRequested || cameraThread != expectedThread
                        || cameraThreadGeneration != expectedGeneration
                        || expectedTextureView == null
                        || expectedTextureView.getSurfaceTexture() != expectedSurface) {
                    return;
                }
                action.run();
            });
        }

        private boolean isCurrentGeneration() {
            return !shutdownRequested
                    && cameraThread == this
                    && cameraThreadGeneration == generation;
        }

        private void updateScale(int index) {
            int width, height;
            if (index >= 0 && index < previewSize.length && previewSize[index] != null) {
                width = Math.max(1, previewSize[index].getWidth());
                height = Math.max(1, previewSize[index].getHeight());
            } else {
                scaleX = 1f;
                scaleY = 1f;
                return;
            }

            if (surfaceSessions[index] instanceof CameraXSurfaceSession) {
                float[] matrix = screenSTMatrix[index];
                double horizontal = Math.hypot((double) matrix[0] * width,
                        (double) matrix[1] * height);
                double vertical = Math.hypot((double) matrix[4] * width,
                        (double) matrix[5] * height);
                double shortest = Math.min(horizontal, vertical);
                if (shortest > 0.0001 && !Double.isNaN(horizontal)
                        && !Double.isNaN(vertical) && !Double.isInfinite(horizontal)
                        && !Double.isInfinite(vertical)) {
                    scaleX = (float) (horizontal / shortest);
                    scaleY = (float) (vertical / shortest);
                    return;
                }
            }

            float scale = surfaceWidth / (float) Math.min(width, height);
            float scaledWidth = width * scale;
            float scaledHeight = height * scale;

            if (Math.abs(scaledWidth - scaledHeight) < 0.001f) {
                scaleX = 1f;
                scaleY = 1f;
            } else if (scaledWidth > scaledHeight) {
                scaleX = 1.0f;
                scaleY = scaledWidth / Math.max(1f, surfaceHeight);
            } else {
                scaleX = scaledHeight / Math.max(1f, surfaceWidth);
                scaleY = 1.0f;
            }
            FileLog.d("InstantCamera camera[" + index + "] scaleX = "
                    + scaleX + " scaleY = " + scaleY);
        }

        private void rebuildTextureBuffer() {
            rebuildTextureBuffer(surfaceIndex);
        }

        private void rebuildTextureBuffer(int index) {
            if (index < 0 || index >= cameraTextureBuffers.length) {
                return;
            }
            updateScale(index);
            float tX = 1.0f / scaleX / 2.0f;
            float tY = 1.0f / scaleY / 2.0f;
            FloatBuffer previous = cameraTextureBuffers[index];
            if (previous != null && previous.limit() == 8
                    && previous.get(0) == 0.5f - tX && previous.get(1) == 0.5f - tY
                    && previous.get(2) == 0.5f + tX && previous.get(3) == 0.5f - tY
                    && previous.get(4) == 0.5f - tX && previous.get(5) == 0.5f + tY
                    && previous.get(6) == 0.5f + tX && previous.get(7) == 0.5f + tY) {
                if (index == surfaceIndex) {
                    textureBuffer = previous;
                }
                return;
            }
            float[] texData = {
                    0.5f - tX, 0.5f - tY,
                    0.5f + tX, 0.5f - tY,
                    0.5f - tX, 0.5f + tY,
                    0.5f + tX, 0.5f + tY
            };
            FloatBuffer buffer = ByteBuffer.allocateDirect(texData.length * 4)
                    .order(ByteOrder.nativeOrder()).asFloatBuffer();
            buffer.put(texData).position(0);
            cameraTextureBuffers[index] = buffer;
            if (index == surfaceIndex) {
                textureBuffer = buffer;
            }
        }

        private void rebuildAllTextureBuffers() {
            for (int i = 0; i < cameraTextureBuffers.length; i++) {
                if (previewSize[i] != null || i == surfaceIndex) {
                    rebuildTextureBuffer(i);
                }
            }
            FloatBuffer activeBuffer = getTextureBuffer(surfaceIndex);
            if (activeBuffer != null) {
                textureBuffer = activeBuffer;
            }
        }

        private FloatBuffer getTextureBuffer(int index) {
            if (index >= 0 && index < cameraTextureBuffers.length
                    && cameraTextureBuffers[index] != null) {
                return cameraTextureBuffers[index];
            }
            return textureBuffer;
        }

        private boolean initGL() {
            if (BuildVars.LOGS_ENABLED) {
                FileLog.d("InstantCamera start init gl");
            }
            egl10 = (EGL10) EGLContext.getEGL();

            eglDisplay = egl10.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
            if (eglDisplay == EGL10.EGL_NO_DISPLAY) {
                if (BuildVars.LOGS_ENABLED) {
                    FileLog.e("InstantCamera eglGetDisplay failed " + GLUtils.getEGLErrorString(egl10.eglGetError()));
                }
                finish();
                return false;
            }

            int[] version = new int[2];
            if (!egl10.eglInitialize(eglDisplay, version)) {
                if (BuildVars.LOGS_ENABLED) {
                    FileLog.e("InstantCamera eglInitialize failed " + GLUtils.getEGLErrorString(egl10.eglGetError()));
                }
                finish();
                return false;
            }

            int[] configsCount = new int[1];
            EGLConfig[] configs = new EGLConfig[1];
            int[] configSpec = new int[]{
                    EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                    EGL10.EGL_RED_SIZE, 8,
                    EGL10.EGL_GREEN_SIZE, 8,
                    EGL10.EGL_BLUE_SIZE, 8,
                    EGL10.EGL_ALPHA_SIZE, 0,
                    EGL10.EGL_DEPTH_SIZE, 0,
                    EGL10.EGL_STENCIL_SIZE, 0,
                    EGL10.EGL_NONE
            };
            EGLConfig eglConfig;
            if (!egl10.eglChooseConfig(eglDisplay, configSpec, configs, 1, configsCount)) {
                if (BuildVars.LOGS_ENABLED) {
                    FileLog.e("InstantCamera eglChooseConfig failed " + GLUtils.getEGLErrorString(egl10.eglGetError()));
                }
                finish();
                return false;
            } else if (configsCount[0] > 0) {
                eglConfig = configs[0];
            } else {
                if (BuildVars.LOGS_ENABLED) {
                    FileLog.e("InstantCamera eglConfig not initialized");
                }
                finish();
                return false;
            }

            int[] attrib_list = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL10.EGL_NONE};
            eglContext = egl10.eglCreateContext(eglDisplay, eglConfig, EGL10.EGL_NO_CONTEXT, attrib_list);
            if (eglContext == null) {
                if (BuildVars.LOGS_ENABLED) {
                    FileLog.e("InstantCamera eglCreateContext failed " + GLUtils.getEGLErrorString(egl10.eglGetError()));
                }
                finish();
                return false;
            }

            if (surfaceTexture instanceof SurfaceTexture) {
                eglSurface = egl10.eglCreateWindowSurface(eglDisplay, eglConfig, surfaceTexture, null);
            } else {
                finish();
                return false;
            }

            if (eglSurface == null || eglSurface == EGL10.EGL_NO_SURFACE) {
                if (BuildVars.LOGS_ENABLED) {
                    FileLog.e("InstantCamera createWindowSurface failed " + GLUtils.getEGLErrorString(egl10.eglGetError()));
                }
                finish();
                return false;
            }
            if (!egl10.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
                if (BuildVars.LOGS_ENABLED) {
                    FileLog.e("InstantCamera eglMakeCurrent failed " + GLUtils.getEGLErrorString(egl10.eglGetError()));
                }
                finish();
                return false;
            }

            float[] verticesData = {
                    -1.0f, -1.0f, 0,
                    1.0f, -1.0f, 0,
                    -1.0f, 1.0f, 0,
                    1.0f, 1.0f, 0
            };

            if (videoEncoder == null) {
                videoEncoder = new VideoRecorder();
            }

            vertexBuffer = ByteBuffer.allocateDirect(verticesData.length * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
            vertexBuffer.put(verticesData).position(0);

            rebuildAllTextureBuffers();

            android.opengl.Matrix.setIdentityM(mSTMatrix, 0);
            if (useCameraX) {
                cameraXSnapshotIdentityMatrix = new float[16];
                float[] fullTextureData = {
                        0.0f, 0.0f,
                        1.0f, 0.0f,
                        0.0f, 1.0f,
                        1.0f, 1.0f
                };
                cameraXSnapshotTextureBuffer = ByteBuffer.allocateDirect(fullTextureData.length * 4)
                        .order(ByteOrder.nativeOrder()).asFloatBuffer();
                cameraXSnapshotTextureBuffer.put(fullTextureData).position(0);
                android.opengl.Matrix.setIdentityM(cameraXSnapshotIdentityMatrix, 0);
            }

            int vertexShader = loadShader(GLES20.GL_VERTEX_SHADER, VERTEX_SHADER);
            int fragmentShader = loadShader(GLES20.GL_FRAGMENT_SHADER,
                    useCameraX ? FRAGMENT_CAMERAX_SCREEN_SHADER : FRAGMENT_SCREEN_SHADER);
            if (vertexShader != 0 && fragmentShader != 0) {
                drawProgram = GLES20.glCreateProgram();
                GLES20.glAttachShader(drawProgram, vertexShader);
                GLES20.glAttachShader(drawProgram, fragmentShader);
                GLES20.glLinkProgram(drawProgram);
                int[] linkStatus = new int[1];
                GLES20.glGetProgramiv(drawProgram, GLES20.GL_LINK_STATUS, linkStatus, 0);
                if (linkStatus[0] == 0) {
                    if (BuildVars.LOGS_ENABLED) {
                        FileLog.e("InstantCamera failed link shader");
                    }
                    GLES20.glDeleteProgram(drawProgram);
                    drawProgram = 0;
                } else {
                    positionHandle = GLES20.glGetAttribLocation(drawProgram, "aPosition");
                    textureHandle = GLES20.glGetAttribLocation(drawProgram, "aTextureCoord");
                    vertexMatrixHandle = GLES20.glGetUniformLocation(drawProgram, "uMVPMatrix");
                    textureMatrixHandle = GLES20.glGetUniformLocation(drawProgram, "uSTMatrix");
                    alphaHandle = GLES20.glGetUniformLocation(drawProgram, "alpha");
                    texelSizeHandle = GLES20.glGetUniformLocation(drawProgram, "texelSize");
                    switchBlurHandle = GLES20.glGetUniformLocation(drawProgram, "switchBlur");
                }
            } else {
                if (BuildVars.LOGS_ENABLED) {
                    FileLog.e("InstantCamera failed creating shader");
                }
                finish();
                return false;
            }

            if (useCameraX) {
                int snapshotVertexShader = loadShader(GLES20.GL_VERTEX_SHADER, VERTEX_SHADER);
                int snapshotFragmentShader = loadShader(GLES20.GL_FRAGMENT_SHADER, FRAGMENT_SNAPSHOT_SHADER);
                if (snapshotVertexShader != 0 && snapshotFragmentShader != 0) {
                    snapshotProgram = GLES20.glCreateProgram();
                    GLES20.glAttachShader(snapshotProgram, snapshotVertexShader);
                    GLES20.glAttachShader(snapshotProgram, snapshotFragmentShader);
                    GLES20.glLinkProgram(snapshotProgram);
                    int[] linkStatus = new int[1];
                    GLES20.glGetProgramiv(snapshotProgram, GLES20.GL_LINK_STATUS, linkStatus, 0);
                    if (linkStatus[0] == 0) {
                        GLES20.glDeleteProgram(snapshotProgram);
                        snapshotProgram = 0;
                    } else {
                        snapshotPositionHandle = GLES20.glGetAttribLocation(snapshotProgram, "aPosition");
                        snapshotTextureHandle = GLES20.glGetAttribLocation(snapshotProgram, "aTextureCoord");
                        snapshotVertexMatrixHandle = GLES20.glGetUniformLocation(snapshotProgram, "uMVPMatrix");
                        snapshotTextureMatrixHandle = GLES20.glGetUniformLocation(snapshotProgram, "uSTMatrix");
                        snapshotAlphaHandle = GLES20.glGetUniformLocation(snapshotProgram, "alpha");
                        snapshotTexelSizeHandle = GLES20.glGetUniformLocation(snapshotProgram, "texelSize");
                        snapshotSwitchBlurHandle = GLES20.glGetUniformLocation(snapshotProgram, "switchBlur");
                    }
                }
                if (snapshotProgram == 0) {
                    if (BuildVars.LOGS_ENABLED) {
                        FileLog.e("InstantCamera failed creating CameraX snapshot shader");
                    }
                    finish();
                    return false;
                }
            }

            android.opengl.Matrix.setIdentityM(mMVPMatrix, 0);
            for (int i = 0; i < 2; i++) {
                android.opengl.Matrix.setIdentityM(screenSTMatrix[i], 0);
                android.opengl.Matrix.setIdentityM(screenMVPMatrix[i], 0);
            }

            GLES20.glEnable(GLES20.GL_BLEND);
            GLES20.glBlendFunc(GLES20.GL_SRC_ALPHA, GLES20.GL_ONE_MINUS_SRC_ALPHA);

            synchronized (this) {
                if (shutdownRequested) {
                    finish();
                    return false;
                }
                GLES20.glGenTextures(2, cameraTexture, 0);
                synchronized (InstantCameraView.this) {
                    if (shutdownRequested || cameraThreadGeneration != generation) {
                        finish();
                        return false;
                    }
                    InstantCameraView.this.cameraTexture = cameraTexture;
                }
                for (int a = 0; a < 2; ++a) {
                    GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, cameraTexture[a]);
                    GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
                    GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
                    GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
                    GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);

                    cameraSurface[a] = new SurfaceTexture(cameraTexture[a]);
                    final int i = a;
                    cameraSurface[a].setOnFrameAvailableListener(surfaceTexture -> {
                        if (!isCurrentGeneration()) {
                            return;
                        }

                        requestRender(i == 0, i == 1);
                    });
                    createCamera(a, cameraSurface[a], this, generation);
                }
            }

            if (BuildVars.LOGS_ENABLED) {
                FileLog.e("InstantCamera gl initied");
            }

            return true;
        }

        public void reinitForNewCamera() {
            Handler handler = getHandler();
            if (handler != null) {
                sendMessage(handler.obtainMessage(DO_REINIT_MESSAGE), 0);
            }
        }

        public void captureCameraXSingleSwitchSnapshot(Runnable afterCapture) {
            Handler handler = getHandler();
            if (handler != null && !shutdownRequested
                    && handler.sendMessage(handler.obtainMessage(DO_CAMERA_X_SINGLE_SNAPSHOT, afterCapture))) return;
            clearCameraXSnapshot();
            if (afterCapture != null) postToUiIfCurrent(afterCapture);
        }

        public void finish() {

            snapshotContextClosed = true;
            clearCameraXSnapshot();
            if (cameraSurface != null) {
                for (int a = 0; a < 2; ++a) {
                    if (cameraSurface[a] != null) {
                        cameraSurface[a].release();
                        cameraSurface[a] = null;
                    }
                }
            }
            synchronized (InstantCameraView.this) {
                if (InstantCameraView.this.cameraTexture == cameraTexture) {
                    cameraTextureAvailable = false;
                }
            }
            cameraFrameAvailable[0] = false;
            cameraFrameAvailable[1] = false;
            cameraFrameLatched[0] = false;
            cameraFrameLatched[1] = false;
            if (eglSurface != null && eglContext != null) {
                if (!eglContext.equals(egl10.eglGetCurrentContext()) || !eglSurface.equals(egl10.eglGetCurrentSurface(EGL10.EGL_DRAW))) {
                    egl10.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
                }
                if (cameraTexture != null && cameraTexture[0] != Integer.MIN_VALUE) {
                    GLES20.glDeleteTextures(1, cameraTexture, 0);
                    cameraTexture[0] = Integer.MIN_VALUE;
                }
                if (cameraTexture != null && cameraTexture[1] != Integer.MIN_VALUE) {
                    GLES20.glDeleteTextures(1, cameraTexture, 1);
                    cameraTexture[1] = Integer.MIN_VALUE;
                }

                rearCrossfade.release();
                if (snapshotProgram != 0) {
                    GLES20.glDeleteProgram(snapshotProgram);
                    snapshotProgram = 0;
                }
            }
            if (eglSurface != null) {
                egl10.eglMakeCurrent(eglDisplay, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_CONTEXT);
                egl10.eglDestroySurface(eglDisplay, eglSurface);
                eglSurface = null;
            }
            if (eglContext != null) {
                egl10.eglDestroyContext(eglDisplay, eglContext);
                eglContext = null;
            }
            if (eglDisplay != null) {
                egl10.eglTerminate(eglDisplay);
                eglDisplay = null;
            }
        }

        public void setCurrentSession(CameraSession session) {
            Handler handler = getHandler();
            if (handler != null) {
                sendMessage(handler.obtainMessage(DO_SETSESSION_MESSAGE, session), 0);
            }
        }

        public void setCurrentSession(Camera2Session session) {
            Handler handler = getHandler();
            if (handler != null) {
                sendMessage(handler.obtainMessage(DO_SETSESSION_MESSAGE, session), 0);
            }
        }

        public void setCurrentSession(
                com.th3nekit.finegram.camera.CameraXSurfaceSession session) {
            Handler handler = getHandler();
            if (handler != null) {
                sendMessage(handler.obtainMessage(DO_SETSESSION_MESSAGE,
                        session.getSurfaceRequestGeneration(), 0, session), 0);
            }
        }

        public void flipSurfaces() {
            Handler handler = getHandler();
            if (handler != null) {
                sendMessage(handler.obtainMessage(DO_FLIP), 0);
                requestRender(true, true);
            }
        }

        public void beginDualSurfaceSwitch(Object targetSession, int targetSurface) {
            Handler handler = getHandler();
            if (handler != null) {
                sendMessage(handler.obtainMessage(
                        DO_DUAL_SWITCH_BEGIN, targetSurface, 0, targetSession), 0);
            }
        }

        public void setDualSurfaceSwitchProgress(float progress) {
            Handler handler = getHandler();
            if (handler != null) {
                handler.removeMessages(DO_DUAL_SWITCH_PROGRESS);
                sendMessage(handler.obtainMessage(DO_DUAL_SWITCH_PROGRESS,
                        Float.valueOf(Utilities.clamp(progress, 1f, 0f))), 0);
            }
        }

        public void finishDualSurfaceSwitch() {
            Handler handler = getHandler();
            if (handler != null) {
                handler.removeMessages(DO_DUAL_SWITCH_PROGRESS);
                sendMessage(handler.obtainMessage(DO_DUAL_SWITCH_FINISH), 0);
            }
        }

        public void cancelDualSurfaceSwitch() {
            Handler handler = getHandler();
            if (handler != null) {
                handler.removeMessages(DO_DUAL_SWITCH_PROGRESS);
                sendMessage(handler.obtainMessage(DO_DUAL_SWITCH_CANCEL), 0);
            }
        }

        public void refreshPreviewGeometry() {
            Handler handler = getHandler();
            if (handler != null) {
                handler.removeMessages(DO_REFRESH_PREVIEW_GEOMETRY);
                sendMessage(handler.obtainMessage(DO_REFRESH_PREVIEW_GEOMETRY), 0);
            }
        }

        public void resetCameraXFrameState() {
            Handler handler = getHandler();
            if (handler != null) {
                handler.removeMessages(DO_RESET_CAMERAX_FRAME_STATE);
                sendMessage(handler.obtainMessage(
                        DO_RESET_CAMERAX_FRAME_STATE), 0);
            }
        }

        public void setSurfaceIndex(int index) {
            Handler handler = getHandler();
            if (handler != null) {
                sendMessage(handler.obtainMessage(DO_SET_SURFACE_INDEX, index, 0), 0);
                requestRender(true, true);
            }
        }

        private int getSessionSurfaceIndex(Object session) {
            if (session instanceof com.th3nekit.finegram.camera.CameraXSurfaceSession) {
                int index = videoMessagesHelper.getSessionIndex(
                        (com.th3nekit.finegram.camera.CameraXSurfaceSession) session);
                if (index >= 0) return index;
            } else if (session instanceof Camera2Session) {

                if (!bothCameras) {
                    return surfaceIndex;
                }
                for (int i = 0; i < camera2Sessions.length; i++) {
                    if (camera2Sessions[i] == session) return i;
                }
            }
            return surfaceIndex;
        }

        private void updateSessionMatrix(Object session, int index) {
            if (index < 0 || index >= screenMVPMatrix.length) return;
            surfaceSessions[index] = session;
            float[] matrix = screenMVPMatrix[index];
            int rotationAngle;
            if (session instanceof CameraSession) {
                rotationAngle = ((CameraSession) session).getWorldAngle();
            } else if (session instanceof Camera2Session) {
                rotationAngle = ((Camera2Session) session).getWorldAngle();
            } else if (session instanceof com.th3nekit.finegram.camera.CameraXSurfaceSession) {
                CameraXSurfaceSession cameraXSession = (CameraXSurfaceSession) session;

                surfaceSessionGenerations[index] = cameraXSession.getSurfaceRequestGeneration();
                rotationAngle = cameraXSession.getWorldAngle();
            } else {
                rotationAngle = 0;
            }
            android.opengl.Matrix.setIdentityM(matrix, 0);
            if (rotationAngle != 0) {
                android.opengl.Matrix.rotateM(matrix, 0, rotationAngle, 0, 0, 1);
            }
            if (session instanceof com.th3nekit.finegram.camera.CameraXSurfaceSession) {
                com.th3nekit.finegram.camera.CameraXSurfaceSession cameraXSession =
                        (com.th3nekit.finegram.camera.CameraXSurfaceSession) session;

                if (cameraXSession.isMirrored() && !cameraXSession.hasCameraTransform()) {
                    android.opengl.Matrix.multiplyMM(nmCameraXTransformScratch, 0,
                            nmCameraXMirrorMatrix, 0, matrix, 0);
                    System.arraycopy(nmCameraXTransformScratch, 0, matrix, 0, 16);
                }
            }
        }

        private void copyActiveMatrices(int index) {
            if (index < 0 || index >= screenSTMatrix.length) return;
            System.arraycopy(screenSTMatrix[index], 0, mSTMatrix, 0, 16);
            System.arraycopy(screenMVPMatrix[index], 0, mMVPMatrix, 0, 16);
        }

        private void publishDualVideoSwitch() {
            if (!dualSurfaceSwitching || dualSwitchFrom < 0 || dualSwitchTo < 0) return;
            System.arraycopy(screenSTMatrix[dualSwitchFrom], 0,
                    dualVideoSTMatrix[dualSwitchFrom], 0, 16);
            System.arraycopy(screenSTMatrix[dualSwitchTo], 0,
                    dualVideoSTMatrix[dualSwitchTo], 0, 16);
            System.arraycopy(screenMVPMatrix[dualSwitchFrom], 0,
                    dualVideoMVPMatrix[dualSwitchFrom], 0, 16);
            System.arraycopy(screenMVPMatrix[dualSwitchTo], 0,
                    dualVideoMVPMatrix[dualSwitchTo], 0, 16);
            dualVideoSwitchFrom = dualSwitchFrom;
            dualVideoSwitchTo = dualSwitchTo;

            dualVideoSwitchProgress = dualSwitchProgress;
            dualVideoSwitching = true;
        }

        private void drawScreenCamera(int index, float alpha) {
            if (index < 0 || index >= cameraTexture.length
                    || cameraTexture[index] == Integer.MIN_VALUE
                    || cameraTexture[index] == 0 || !cameraFrameAvailable[index]) {
                return;
            }
            FloatBuffer indexTextureBuffer = getTextureBuffer(index);
            if (indexTextureBuffer == null) {
                return;
            }
            GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT,
                    false, 8, indexTextureBuffer);
            GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, cameraTexture[index]);
            GLES20.glUniformMatrix4fv(textureMatrixHandle, 1, false, screenSTMatrix[index], 0);
            GLES20.glUniformMatrix4fv(vertexMatrixHandle, 1, false, screenMVPMatrix[index], 0);
            GLES20.glUniform1f(alphaHandle, Utilities.clamp(alpha, 1f, 0f));
            Size size = previewSize[index];
            if (size != null) {
                GLES20.glUniform2f(texelSizeHandle,
                        1f / Math.max(1, size.getWidth()),
                        1f / Math.max(1, size.getHeight()));
            }
            GLES20.glUniform1f(switchBlurHandle,
                    useCameraX && !bothCameras ? cameraXSingleSwitchBlur : 0f);
            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
        }

        private boolean hasCurrentCameraXFrame(int index) {
            if (!cameraFrameLatched[index]
                    || !(surfaceSessions[index] instanceof CameraXSurfaceSession)) {
                return false;
            }
            CameraXSurfaceSession session =
                    (CameraXSurfaceSession) surfaceSessions[index];
            return videoMessagesHelper.getSessionIndex(session) == index
                    && session.isSurfaceRequestCurrent(surfaceSessionGenerations[index]);
        }

        private boolean captureCameraXSingleSwitchSnapshot() {

            clearCameraXSnapshot();
            if (!useCameraX || bothCameras
                    || snapshotProgram == 0
                    || surfaceIndex < 0 || surfaceIndex >= cameraTexture.length
                    || !cameraFrameLatched[surfaceIndex]
                    || cameraTexture[surfaceIndex] == Integer.MIN_VALUE
                    || cameraTexture[surfaceIndex] == 0
                    || vertexBuffer == null || textureBuffer == null) {
                return false;
            }
            if (!eglContext.equals(egl10.eglGetCurrentContext())
                    || !eglSurface.equals(egl10.eglGetCurrentSurface(EGL10.EGL_DRAW))) {
                if (!egl10.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
                    return false;
                }
            }

            final int width = Math.max(1, surfaceWidth);
            final int height = Math.max(1, surfaceHeight);
            int[] texture = new int[1];
            int[] framebuffer = new int[1];
            final long capturedTimestamp = cameraFrameTimestamp[surfaceIndex];
            final FloatBuffer capturedTextureBuffer = getTextureBuffer(surfaceIndex);
            boolean success = false;
            try {
                GLES20.glGenTextures(1, texture, 0);
                GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, texture[0]);
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
                GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA,
                        width, height, 0, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, null);

                GLES20.glGenFramebuffers(1, framebuffer, 0);
                GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, framebuffer[0]);
                GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER,
                        GLES20.GL_COLOR_ATTACHMENT0, GLES20.GL_TEXTURE_2D, texture[0], 0);
                if (GLES20.glCheckFramebufferStatus(GLES20.GL_FRAMEBUFFER)
                        != GLES20.GL_FRAMEBUFFER_COMPLETE) {
                    throw new IllegalStateException("CameraX snapshot framebuffer is incomplete");
                }

                GLES20.glViewport(0, 0, width, height);
                GLES20.glClearColor(0f, 0f, 0f, 1f);
                GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
                GLES20.glUseProgram(drawProgram);
                GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
                GLES20.glVertexAttribPointer(positionHandle, 3, GLES20.GL_FLOAT,
                        false, 12, vertexBuffer);
                GLES20.glEnableVertexAttribArray(positionHandle);
                GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT,
                        false, 8, capturedTextureBuffer);
                GLES20.glEnableVertexAttribArray(textureHandle);
                GLES20.glUniformMatrix4fv(textureMatrixHandle, 1, false,
                        screenSTMatrix[surfaceIndex], 0);
                GLES20.glUniformMatrix4fv(vertexMatrixHandle, 1, false,
                        screenMVPMatrix[surfaceIndex], 0);
                GLES20.glUniform1f(alphaHandle, 1f);
                Size size = previewSize[surfaceIndex];
                GLES20.glUniform2f(texelSizeHandle,
                        1f / Math.max(1, size == null ? width : size.getWidth()),
                        1f / Math.max(1, size == null ? height : size.getHeight()));
                GLES20.glUniform1f(switchBlurHandle, 0f);
                GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
                        cameraTexture[surfaceIndex]);
                GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
                GLES20.glDisableVertexAttribArray(positionHandle);
                GLES20.glDisableVertexAttribArray(textureHandle);
                GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, 0);
                GLES20.glUseProgram(0);

                GLES20.glFinish();
                CameraSnapshot snapshot = new CameraSnapshot(
                        texture[0], width, height, capturedTimestamp);
                cameraXSingleSwitchSnapshotWidth = width;
                cameraXSingleSwitchSnapshotHeight = height;
                cameraXSingleSwitchSnapshot = texture[0];
                cameraXSingleSwitchSnapshotTimestamp = capturedTimestamp;
                cameraXSingleSwitchSnapshotHandle = snapshot;
                success = true;
            } catch (Throwable t) {
                FileLog.e(t);
            } finally {
                GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
                GLES20.glViewport(0, 0, Math.max(1, surfaceWidth), Math.max(1, surfaceHeight));
                GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
                if (framebuffer[0] != 0) {
                    GLES20.glDeleteFramebuffers(1, framebuffer, 0);
                }
                if (!success && texture[0] != 0) {
                    GLES20.glDeleteTextures(1, texture, 0);
                }
                if (!success) {
                    clearCameraXSnapshot();
                }
            }
            return success;
        }

        private void drawCameraXSnapshot(float alpha, float blur) {
            CameraSnapshot snapshot = cameraXSingleSwitchSnapshotHandle;
            if (snapshotProgram == 0 || snapshot == null || !snapshot.belongsTo(this)) {
                return;
            }
            snapshot.presented = true;
            GLES20.glUseProgram(snapshotProgram);
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            GLES20.glVertexAttribPointer(snapshotPositionHandle, 3, GLES20.GL_FLOAT,
                    false, 12, vertexBuffer);
            GLES20.glEnableVertexAttribArray(snapshotPositionHandle);
            GLES20.glVertexAttribPointer(snapshotTextureHandle, 2, GLES20.GL_FLOAT,
                    false, 8, snapshot.textureBuffer);
            GLES20.glEnableVertexAttribArray(snapshotTextureHandle);
            GLES20.glUniformMatrix4fv(snapshotVertexMatrixHandle, 1, false,
                    snapshot.identityMatrix, 0);
            GLES20.glUniformMatrix4fv(snapshotTextureMatrixHandle, 1, false,
                    snapshot.identityMatrix, 0);
            GLES20.glUniform1f(snapshotAlphaHandle, Utilities.clamp(alpha, 1f, 0f));
            GLES20.glUniform2f(snapshotTexelSizeHandle,
                    1f / Math.max(1, snapshot.width),
                    1f / Math.max(1, snapshot.height));
            GLES20.glUniform1f(snapshotSwitchBlurHandle, blur);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, snapshot.texture);
            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
            GLES20.glDisableVertexAttribArray(snapshotPositionHandle);
            GLES20.glDisableVertexAttribArray(snapshotTextureHandle);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
            GLES20.glUseProgram(0);
        }

        private void onDraw(Integer cameraId, boolean updateTexImage1, boolean updateTexImage2) {
            if (!initied || cameraThread != this
                    || cameraThreadGeneration != generation) {
                return;
            }

            if (!eglContext.equals(egl10.eglGetCurrentContext()) || !eglSurface.equals(egl10.eglGetCurrentSurface(EGL10.EGL_DRAW))) {
                if (!egl10.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
                    if (BuildVars.LOGS_ENABLED) {
                        FileLog.e("eglMakeCurrent failed " + GLUtils.getEGLErrorString(egl10.eglGetError()));
                    }
                    return;
                }
            }
            boolean updatedTexImage1 = false;
            boolean updatedTexImage2 = false;
            if (updateTexImage1) {
                cameraFrameLatched[0] = false;
                try {
                    long previousTimestamp = cameraFrameTimestamp[0];
                    cameraSurface[0].updateTexImage();
                    cameraSurface[0].getTransformMatrix(screenSTMatrix[0]);
                    if (surfaceSessions[0] instanceof CameraXSurfaceSession) {
                        rebuildTextureBuffer(0);
                    }
                    cameraFrameTimestamp[0] = cameraSurface[0].getTimestamp();
                    cameraFrameLatched[0] = true;

                    updatedTexImage1 = cameraFrameTimestamp[0] <= 0
                            || cameraFrameTimestamp[0] != previousTimestamp;
                } catch (Throwable t) {
                    FileLog.e(t);
                }
            }
            if (updateTexImage2) {
                cameraFrameLatched[1] = false;
                try {
                    long previousTimestamp = cameraFrameTimestamp[1];
                    cameraSurface[1].updateTexImage();
                    cameraSurface[1].getTransformMatrix(screenSTMatrix[1]);
                    if (surfaceSessions[1] instanceof CameraXSurfaceSession) {
                        rebuildTextureBuffer(1);
                    }
                    cameraFrameTimestamp[1] = cameraSurface[1].getTimestamp();
                    cameraFrameLatched[1] = true;
                    updatedTexImage2 = cameraFrameTimestamp[1] <= 0
                            || cameraFrameTimestamp[1] != previousTimestamp;
                } catch (Throwable t) {
                    FileLog.e(t);
                }
            }
            final boolean activeCameraFrame = surfaceIndex == 0 && updatedTexImage1
                    || surfaceIndex == 1 && updatedTexImage2;
            if (useCameraX && activeCameraFrame) {
                rearLensFrameTimestamp = cameraFrameTimestamp[surfaceIndex];
                rearLensFrameMix = hasCurrentCameraXFrame(surfaceIndex)
                        ? onCameraXRearLensFrame(rearLensFrameTimestamp) : 0f;
            }
            if (!com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getSmoothCameraModuleTransitions()) {
                rearLensFrameMix = 0f;
            }
            if (useCameraX && !bothCameras && updatedTexImage1
                    && cameraXSingleSwitchAwaitingBind) {
                com.th3nekit.finegram.camera.CameraXSurfaceSession pending =
                        pendingCameraXSingleSession;
                if (pending != null && pending.isFrontFacing() == isFrontface
                        && pending.isSurfaceRequestCurrent(pendingCameraXSingleSessionGeneration)) {
                    long lensWaitMs = SystemClock.elapsedRealtime()
                            - cameraXSingleSwitchWaitStartedMs;
                    long frameTimestamp = cameraFrameTimestamp[0];
                    boolean lensReady = pending.isInitialLensReady(frameTimestamp);
                    boolean newerThanSnapshot = cameraXSingleSwitchSnapshotTimestamp <= 0
                            || frameTimestamp > cameraXSingleSwitchSnapshotTimestamp;

                    if (lensReady && newerThanSnapshot || lensWaitMs >= 1450L) {
                        pendingCameraXSingleSession = null;
                        updateSessionMatrix(pending, 0);
                        currentSession = pending;

                        this.cameraId++;

                        rebuildTextureBuffer(0);
                        copyActiveMatrices(0);
                        cameraXSingleSwitchAwaitingBind = false;
                        cameraXSingleSwitchNewFrame = true;
                        postToUiIfCurrent(
                                InstantCameraView.this::finishCameraXVideoTransition);
                    }
                }
            }

            if (!useCameraX) {
                if (updatedTexImage1) cameraFrameAvailable[0] = true;
                if (updatedTexImage2) cameraFrameAvailable[1] = true;
                if (surfaceIndex >= 0 && surfaceIndex < cameraFrameLatched.length
                        && cameraFrameLatched[surfaceIndex]) {
                    cameraTextureAvailable = true;
                }
            }
            if (useCameraX) {
                if (updatedTexImage1 && hasCurrentCameraXFrame(0)) {
                    cameraFrameAvailable[0] = true;
                    cameraTextureAvailable = true;
                    nmOnCameraXFrameAvailable(0);
                }
                if (updatedTexImage2 && hasCurrentCameraXFrame(1)) {
                    cameraFrameAvailable[1] = true;
                    cameraTextureAvailable = true;
                    nmOnCameraXFrameAvailable(1);
                }
                if (bothCameras && isFrontface && cameraXInitialWideWaitActive) {
                    CameraXSurfaceSession rear = videoMessagesHelper.getRearSession();
                    int rearIndex = videoMessagesHelper.getSessionIndex(rear);
                    if (rearIndex >= 0 && rearIndex < cameraFrameTimestamp.length
                            && (rearIndex == 0 ? updatedTexImage1 : updatedTexImage2)
                            && hasCurrentCameraXFrame(rearIndex)) {

                        nmShouldHoldCameraXInitialWideFrame(cameraFrameTimestamp[rearIndex]);
                    }
                }
            }
            if (dualSurfaceSwitching) {
                publishDualVideoSwitch();
            }

            CameraSnapshot snapshot = cameraXSingleSwitchSnapshotHandle;
            if (!cameraXVideoTransitionActive && snapshot != null
                    && snapshot.belongsTo(this)
                    && (snapshot.presented || !cameraXSingleSwitchAwaitingBind)) {
                clearCameraXSnapshot();
                snapshot = null;
            }
            final boolean snapshotTransition = useCameraX
                    && !bothCameras
                    && cameraXVideoTransitionActive && snapshot != null
                    && snapshot.belongsTo(this);

            if (surfaceIndex < 0 || surfaceIndex >= cameraFrameAvailable.length
                    || (!cameraFrameLatched[surfaceIndex] && !snapshotTransition)) {
                return;
            }
            if (useCameraX && !cameraFrameAvailable[surfaceIndex]
                    && !snapshotTransition) {
                return;
            }

            final CameraXSurfaceSession liveCameraXSession =
                    surfaceSessions[surfaceIndex] instanceof CameraXSurfaceSession
                            ? (CameraXSurfaceSession) surfaceSessions[surfaceIndex] : null;
            final int liveSurfaceGeneration = surfaceSessionGenerations[surfaceIndex];
            if (useCameraX && !snapshotTransition && (liveCameraXSession == null
                    || videoMessagesHelper.getSessionIndex(liveCameraXSession) != surfaceIndex
                    || !liveCameraXSession.isSurfaceRequestCurrent(liveSurfaceGeneration))) {

                return;
            }

            boolean initialWideTimeoutRender =
                    nmConsumeCameraXInitialWideTimeoutRender();
            if (bothCameras && !activeCameraFrame && !dualSurfaceSwitching
                    && !initialWideTimeoutRender
                    && (updateTexImage1 || updateTexImage2 || !snapshotTransition)) {

                return;
            }

            if (!initialWideTimeoutRender && useCameraX && !isFrontface
                    && activeCameraFrame
                    && nmShouldHoldCameraXInitialWideFrame(
                            cameraFrameTimestamp[surfaceIndex])) {
                return;
            }

            boolean captureFirstFrameThumb = false;
            if (!recording) {
                if (videoEncoder == null) {
                    videoEncoder = new VideoRecorder();
                }
                if (videoEncoder.started) {
                    if (!useCameraX && !cameraReady && !cameraXSingleSwitchAwaitingBind) {
                        postToUiIfCurrent(() -> {
                            if (!cameraReady && !cameraXSingleSwitchAwaitingBind) {
                                cameraReady = true;
                                onCameraPreviewReady();
                            }
                        });
                    }
                } else {
                    captureFirstFrameThumb = true;
                }
                videoEncoder.startRecording(cameraFile, EGL14.eglGetCurrentContext());
                recording = true;
                legacyZoom = 0f;
                updateFlash();
            }

            rearFrameCrossfade = useCameraX && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getSmoothCameraModuleTransitions()
                    && !snapshotTransition && !dualSurfaceSwitching && !dualVideoSwitching
                    && cameraTextureAlpha >= 1f
                    && surfaceSessions[surfaceIndex] instanceof CameraXSurfaceSession
                    && !((CameraXSurfaceSession) surfaceSessions[surfaceIndex]).isFrontFacing()
                    && hasCurrentCameraXFrame(surfaceIndex);
            if (!rearFrameCrossfade) {
                rearCrossfade.resetHistory();
            }

            if (videoEncoder != null
                    && (activeCameraFrame
                    || initialWideTimeoutRender && cameraFrameLatched[surfaceIndex])) {
                copyActiveMatrices(surfaceIndex);
                videoEncoder.frameAvailable(cameraSurface[surfaceIndex], bothCameras ? surfaceIndex : this.cameraId, System.nanoTime(), this);
            } else if (videoEncoder != null && recording && useCameraX && !bothCameras
                    && cameraXVideoTransitionActive) {

                videoEncoder.transitionFrameAvailable(this.cameraId, this);
            }

            GLES20.glViewport(0, 0, Math.max(1, surfaceWidth), Math.max(1, surfaceHeight));
            boolean compositeRearFrame = rearFrameCrossfade && rearCrossfade.begin(
                    Math.max(1, surfaceWidth), Math.max(1, surfaceHeight), rearLensFrameTimestamp,
                    surfaceSessions[surfaceIndex], surfaceSessionGenerations[surfaceIndex], rearLensFrameMix);
            GLES20.glUseProgram(drawProgram);
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);

            GLES20.glVertexAttribPointer(positionHandle, 3, GLES20.GL_FLOAT, false, 12, vertexBuffer);
            GLES20.glEnableVertexAttribArray(positionHandle);

            GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT, false, 8, textureBuffer);
            GLES20.glEnableVertexAttribArray(textureHandle);

            GLES20.glClearColor(0f, 0f, 0f, 1f);
            GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
            if (snapshotTransition) {

                drawCameraXSnapshot(1f, cameraXSingleSwitchBlur);
                if (cameraXSingleSwitchNewFrame && liveCameraXSession != null
                        && liveCameraXSession.isSurfaceRequestCurrent(liveSurfaceGeneration)) {

                    GLES20.glUseProgram(drawProgram);
                    GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
                    GLES20.glVertexAttribPointer(positionHandle, 3, GLES20.GL_FLOAT,
                            false, 12, vertexBuffer);
                    GLES20.glEnableVertexAttribArray(positionHandle);
                    GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT,
                            false, 8, textureBuffer);
                    GLES20.glEnableVertexAttribArray(textureHandle);
                    drawScreenCamera(surfaceIndex, cameraXSingleSwitchProgress);
                }
            } else if (dualSurfaceSwitching && dualSwitchFrom >= 0 && dualSwitchTo >= 0
                    && cameraFrameAvailable[dualSwitchTo]) {

                drawScreenCamera(dualSwitchFrom, 1f);
                drawScreenCamera(dualSwitchTo, dualSwitchProgress);
            } else if ((useCameraX || useCamera2) && !bothCameras && oldCameraTexture[0] != 0
                    && oldTextureTextureBuffer != null && cameraTextureAlpha < 1f) {

                GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT,
                        false, 8, oldTextureTextureBuffer);
                GLES20.glUniformMatrix4fv(textureMatrixHandle, 1, false,
                        oldScreenSTMatrix, 0);
                GLES20.glUniformMatrix4fv(vertexMatrixHandle, 1, false,
                        oldScreenMVPMatrix, 0);
                GLES20.glUniform1f(alphaHandle, 1f);
                if (oldTexturePreviewSize != null) {
                    GLES20.glUniform2f(texelSizeHandle,
                            1f / Math.max(1, oldTexturePreviewSize.getWidth()),
                            1f / Math.max(1, oldTexturePreviewSize.getHeight()));
                }
                GLES20.glUniform1f(switchBlurHandle,
                        useCameraX ? cameraXSingleSwitchBlur : 0f);
                GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, oldCameraTexture[0]);
                GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

                GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT,
                        false, 8, textureBuffer);
                drawScreenCamera(surfaceIndex, cameraTextureAlpha);
            } else {
                drawScreenCamera(surfaceIndex, 1f);
            }

            GLES20.glDisableVertexAttribArray(positionHandle);
            GLES20.glDisableVertexAttribArray(textureHandle);
            GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, 0);
            GLES20.glUseProgram(0);

            if (compositeRearFrame) rearCrossfade.end(rearLensFrameMix);
            if (!egl10.eglSwapBuffers(eglDisplay, eglSurface)) {
                return;
            }

            if (useCameraX && !cameraReady && !cameraXSingleSwitchAwaitingBind) {
                postToUiIfCurrent(() -> {
                    if (!cameraReady && !cameraXSingleSwitchAwaitingBind
                            && liveCameraXSession != null
                            && liveCameraXSession.isSurfaceRequestCurrent(liveSurfaceGeneration)) {
                        cameraReady = true;
                        onCameraPreviewReady();
                    }
                });
            }

            if (captureFirstFrameThumb) {
                postToUiIfCurrent(() -> {
                    if (firstFrameThumb != null) {
                        firstFrameThumb.recycle();
                        firstFrameThumb = null;
                    }
                    firstFrameThumb = textureView.getBitmap(360, 360);
                });
            }
        }

        @Override
        public void run() {
            initied = initGL();
            super.run();
        }

        @Override
        public void handleMessage(Message inputMessage) {
            int what = inputMessage.what;
            if (shutdownRequested && what != DO_SHUTDOWN_MESSAGE) {
                return;
            }

            switch (what) {
                case DO_RENDER_MESSAGE:
                    onDraw(inputMessage.arg1,
                            inputMessage.obj == updateTexBoth
                                    || inputMessage.obj == updateTex1,
                            inputMessage.obj == updateTexBoth
                                    || inputMessage.obj == updateTex2);
                    break;
                case DO_SHUTDOWN_MESSAGE: {
                    dualSurfaceSwitching = false;
                    synchronized (InstantCameraView.this) {
                        if (InstantCameraView.this.cameraTexture == cameraTexture) {
                            dualVideoSwitching = false;
                        }
                    }
                    finish();
                    SurfaceTexture outputSurface = outputSurfaceToReleaseOnShutdown;
                    outputSurfaceToReleaseOnShutdown = null;
                    if (outputSurface != null) {
                        try {
                            outputSurface.release();
                        } catch (Throwable error) {
                            FileLog.e(error);
                        }
                    }
                    if (recording && (!(inputMessage.obj instanceof SendOptions) || ((SendOptions) inputMessage.obj).ttl != -2) && shutdownEncoder != null) {
                        shutdownEncoder.stopRecording(inputMessage.arg1, inputMessage.obj instanceof SendOptions ? (SendOptions) inputMessage.obj : null);
                    }
                    Looper looper = Looper.myLooper();
                    if (looper != null) {
                        looper.quit();
                    }
                    break;
                }
                case DO_REINIT_MESSAGE: {

                    surfaceIndex = 0;
                    dualSurfaceSwitching = false;
                    dualVideoSwitching = false;
                    if (!egl10.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
                        if (BuildVars.LOGS_ENABLED) {
                            FileLog.d("InstantCamera eglMakeCurrent failed " + GLUtils.getEGLErrorString(egl10.eglGetError()));
                        }
                        return;
                    }

                    if (cameraSurface[0] != null) {

                        System.arraycopy(mSTMatrix, 0, moldSTMatrix, 0, 16);
                        System.arraycopy(mSTMatrix, 0, oldScreenSTMatrix, 0, 16);
                        System.arraycopy(mMVPMatrix, 0, oldScreenMVPMatrix, 0, 16);
                        cameraSurface[0].setOnFrameAvailableListener(null);
                        cameraSurface[0].release();
                        oldCameraTexture[0] = cameraTexture[0];
                        cameraTextureAlpha = 0.0f;
                        cameraTextureAlphaProgress = 0.0f;
                        cameraTexture[0] = 0;
                        oldTextureTextureBuffer = textureBuffer.duplicate();
                        oldTexturePreviewSize = previewSize[0];
                    }
                    cameraId++;
                    cameraReady = false;
                    cameraFrameAvailable[0] = false;
                    cameraFrameLatched[0] = false;
                    cameraFrameTimestamp[0] = 0;

                    GLES20.glGenTextures(1, cameraTexture, 0);
                    GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, cameraTexture[0]);
                    GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
                    GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
                    GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
                    GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);

                    cameraSurface[0] = new SurfaceTexture(cameraTexture[0]);
                    cameraSurface[0].setOnFrameAvailableListener(surfaceTexture -> {
                        if (!isCurrentGeneration()) {
                            return;
                        }
                        cameraTextureAvailable = true;
                        cameraFrameAvailable[0] = true;
                        nmOnCamera2SwitchFirstFrame();
                        requestRender(true, false);
                    });
                    createCamera(0, cameraSurface[0], this, generation);

                    rebuildTextureBuffer();
                    break;
                }
                case DO_SETSESSION_MESSAGE: {
                    if (BuildVars.LOGS_ENABLED) {
                        FileLog.d("InstantCamera set gl renderer session");
                    }
                    Object newSession = inputMessage.obj;
                    if (newSession instanceof CameraXSurfaceSession) {
                        CameraXSurfaceSession session = (CameraXSurfaceSession) newSession;
                        if (videoMessagesHelper.getSessionIndex(session) < 0
                                || !session.isSurfaceRequestCurrent(inputMessage.arg1)) break;
                    }
                    int sessionSurface = getSessionSurfaceIndex(newSession);
                    boolean firstCameraXSessionWithLatchedFrame =
                            newSession instanceof CameraXSurfaceSession
                            && surfaceSessions[sessionSurface] == null
                            && cameraFrameLatched[sessionSurface];
                    if (newSession instanceof CameraXSurfaceSession
                            && surfaceSessions[sessionSurface] != null
                            && (surfaceSessions[sessionSurface] != newSession
                            || surfaceSessionGenerations[sessionSurface] != inputMessage.arg1)) {

                        cameraFrameAvailable[sessionSurface] = false;
                        cameraFrameLatched[sessionSurface] = false;
                        cameraFrameTimestamp[sessionSurface] = 0;
                        if (sessionSurface == surfaceIndex) cameraTextureAvailable = false;
                    }
                    if (newSession instanceof CameraXSurfaceSession && !bothCameras
                            && currentSession != null && sessionSurface == surfaceIndex
                            && (surfaceSessions[sessionSurface] != newSession
                            || surfaceSessionGenerations[sessionSurface] != inputMessage.arg1)) {

                        cameraId++;
                    }
                    updateSessionMatrix(newSession, sessionSurface);
                    if (firstCameraXSessionWithLatchedFrame
                            && hasCurrentCameraXFrame(sessionSurface)) {

                        cameraFrameAvailable[sessionSurface] = true;
                        if (sessionSurface == surfaceIndex) cameraTextureAvailable = true;
                        nmOnCameraXFrameAvailable(sessionSurface);
                    }
                    if (!dualSurfaceSwitching && sessionSurface == surfaceIndex) {
                        currentSession = newSession;
                        copyActiveMatrices(surfaceIndex);
                    }
                    break;
                }
                case DO_FLIP: {
                    surfaceIndex = 1 - surfaceIndex;
                    rebuildTextureBuffer();
                    break;
                }
                case DO_SET_SURFACE_INDEX: {

                    surfaceIndex = inputMessage.arg1;
                    dualSurfaceSwitching = false;
                    dualVideoSwitching = false;
                    rebuildTextureBuffer();
                    break;
                }
                case DO_DUAL_SWITCH_BEGIN: {
                    int target = inputMessage.arg1;
                    if (!bothCameras || target < 0 || target > 1 || target == surfaceIndex) {
                        postToUiIfCurrent(InstantCameraView.this::onDualCameraSwitchFinished);
                        break;
                    }
                    dualSwitchFrom = surfaceIndex;
                    dualSwitchTo = target;
                    dualSwitchProgress = 0f;
                    dualSwitchTargetSession = inputMessage.obj;
                    updateSessionMatrix(dualSwitchTargetSession, dualSwitchTo);

                    surfaceIndex = dualSwitchTo;
                    currentSession = dualSwitchTargetSession;
                    copyActiveMatrices(surfaceIndex);
                    rebuildAllTextureBuffers();
                    dualSurfaceSwitching = true;
                    publishDualVideoSwitch();
                    onDraw(cameraId, false, false);
                    break;
                }
                case DO_DUAL_SWITCH_PROGRESS: {
                    if (dualSurfaceSwitching && inputMessage.obj instanceof Float) {
                        dualSwitchProgress = Utilities.clamp((Float) inputMessage.obj, 1f, 0f);
                        publishDualVideoSwitch();
                        onDraw(cameraId, false, false);
                    }
                    break;
                }
                case DO_DUAL_SWITCH_FINISH: {
                    if (dualSurfaceSwitching) {
                        dualSwitchProgress = 1f;
                        publishDualVideoSwitch();
                        onDraw(cameraId, false, false);
                        dualSurfaceSwitching = false;
                        dualVideoSwitching = false;
                        dualSwitchFrom = dualSwitchTo = -1;
                        dualSwitchTargetSession = null;
                        onDraw(cameraId, false, false);
                    }
                    postToUiIfCurrent(InstantCameraView.this::onDualCameraSwitchFinished);
                    break;
                }
                case DO_DUAL_SWITCH_CANCEL: {
                    dualSurfaceSwitching = false;
                    dualSwitchProgress = 0f;
                    dualSwitchFrom = dualSwitchTo = -1;
                    dualSwitchTargetSession = null;
                    dualVideoSwitching = false;
                    onDraw(cameraId, false, false);
                    postToUiIfCurrent(InstantCameraView.this::onDualCameraSwitchFinished);
                    break;
                }
                case DO_REFRESH_PREVIEW_GEOMETRY: {
                    rebuildAllTextureBuffers();
                    onDraw(cameraId, false, false);
                    break;
                }
                case DO_CAMERA_X_SINGLE_SNAPSHOT: {
                    boolean captured = captureCameraXSingleSwitchSnapshot();
                    if (inputMessage.obj instanceof Runnable) {
                        postToUiIfCurrent((Runnable) inputMessage.obj);
                    }
                    break;
                }
                case DO_RESET_CAMERAX_FRAME_STATE: {
                    cameraFrameAvailable[0] = false;
                    cameraFrameAvailable[1] = false;
                    cameraFrameLatched[0] = false;
                    cameraFrameLatched[1] = false;
                    cameraFrameTimestamp[0] = 0;
                    cameraFrameTimestamp[1] = 0;
                    cameraTextureAvailable = false;
                    break;
                }
            }
        }

        public void shutdown(int send, boolean notify, int scheduleDate, int scheduleRepeatPeriod, int ttl, long effectId) {
            shutdown(send, notify, scheduleDate, scheduleRepeatPeriod, ttl, effectId, null);
        }

        public void shutdown(int send, boolean notify, int scheduleDate,
                             int scheduleRepeatPeriod, int ttl, long effectId,
                             SurfaceTexture outputSurfaceToRelease) {
            synchronized (this) {
                if (outputSurfaceToRelease != null
                        && outputSurfaceToReleaseOnShutdown == null) {
                    outputSurfaceToReleaseOnShutdown = outputSurfaceToRelease;
                }
                if (shutdownRequested) {
                    return;
                }
                shutdownRequested = true;
                shutdownEncoder = videoEncoder;
            }
            nmCancelCameraXDualFrameWatchdog();
            final SendOptions options =
                    new SendOptions(notify, scheduleDate, scheduleRepeatPeriod, ttl, effectId, 0);
            Runnable enqueueShutdown = () -> {
                Handler handler = getHandler();
                if (handler != null) {
                    sendMessage(handler.obtainMessage(
                            DO_SHUTDOWN_MESSAGE, send, 0, options), 0);
                }
            };
            Runnable afterCameraClosed = () -> {
                VideoRecorder encoder = shutdownEncoder;
                if (encoder != null) {
                    encoder.afterVideoFrames(enqueueShutdown);
                } else {
                    enqueueShutdown.run();
                }
            };
            if (useCameraX) {

                videoMessagesHelper.destroyCameraX(
                        InstantCameraView.this, afterCameraClosed);
            } else {
                afterCameraClosed.run();
            }
        }

        private final Object renderRequestLock = new Object();
        private final Object updateTexNone = new Object();
        private final Object updateTex1 = new Object();
        private final Object updateTex2 = new Object();
        private final Object updateTexBoth = new Object();

        public void requestRender(boolean updateTexImage1, boolean updateTexImage2) {
            Handler handler = getHandler();
            if (handler == null || shutdownRequested) {
                return;
            }
            synchronized (renderRequestLock) {
                if (!updateTexImage1 && !updateTexImage2
                        && (handler.hasMessages(DO_RENDER_MESSAGE, updateTexNone)
                        || handler.hasMessages(DO_RENDER_MESSAGE, updateTex1)
                        || handler.hasMessages(DO_RENDER_MESSAGE, updateTex2)
                        || handler.hasMessages(DO_RENDER_MESSAGE, updateTexBoth))) {
                    return;
                }
                if ((updateTexImage1 || updateTexImage2)
                        && handler.hasMessages(DO_RENDER_MESSAGE, updateTexBoth)) {
                    return;
                }
                if (updateTexImage1 && !updateTexImage2
                        && handler.hasMessages(DO_RENDER_MESSAGE, updateTex1)
                        || updateTexImage2 && !updateTexImage1
                        && handler.hasMessages(DO_RENDER_MESSAGE, updateTex2)) {
                    return;
                }
                if (!updateTexImage1
                        && handler.hasMessages(DO_RENDER_MESSAGE, updateTex1)) {
                    updateTexImage1 = true;
                }
                if (!updateTexImage2
                        && handler.hasMessages(DO_RENDER_MESSAGE, updateTex2)) {
                    updateTexImage2 = true;
                }
                handler.removeMessages(DO_RENDER_MESSAGE);
                Object token = updateTexImage1 && updateTexImage2
                        ? updateTexBoth : updateTexImage1
                        ? updateTex1 : updateTexImage2 ? updateTex2 : updateTexNone;
                sendMessage(handler.obtainMessage(
                        DO_RENDER_MESSAGE, cameraId, 0, token), 0);
            }
        }
    }

    private static final int MSG_START_RECORDING = 0;
    private static final int MSG_STOP_RECORDING = 1;
    private static final int MSG_VIDEOFRAME_AVAILABLE = 2;
    private static final int MSG_AUDIOFRAME_AVAILABLE = 3;
    private static final int MSG_PAUSE_RECORDING = 4;
    private static final int MSG_RESUME_RECORDING = 5;

    private static final class CameraVideoFrameState {
        final Integer cameraId;
        final CameraGLThread liveSource;
        final CameraXSurfaceSession liveCameraXSession;
        final int liveSurfaceGeneration;
        final boolean singleCameraXTransition;
        final boolean replacementFrameReady;
        final int snapshotTexture;
        final int snapshotWidth;
        final int snapshotHeight;
        final float progress;
        final float blur;
        final boolean rearCrossfade;
        final float rearFrameMix;
        final long rearFrameTimestamp;
        final CameraGLThread.CameraSnapshot snapshot;
        final FloatBuffer snapshotTextureBuffer;
        final int activeSurfaceIndex;
        final int liveTexture;
        final FloatBuffer liveTextureBuffer;
        final Size livePreviewSize;
        final float[] liveSTMatrix;
        final float[] liveMVPMatrix;

        CameraVideoFrameState(Integer cameraId, CameraGLThread liveSource, boolean singleCameraXTransition,
                boolean replacementFrameReady, CameraGLThread.CameraSnapshot snapshot,
                float progress, float blur, int activeSurfaceIndex, int liveTexture,
                FloatBuffer liveTextureBuffer, Size livePreviewSize,
                float[] liveSTMatrix, float[] liveMVPMatrix) {
            this.cameraId = cameraId;
            this.liveSource = liveSource;
            this.liveCameraXSession = liveSource.surfaceSessions[activeSurfaceIndex] instanceof CameraXSurfaceSession
                    ? (CameraXSurfaceSession) liveSource.surfaceSessions[activeSurfaceIndex] : null;
            this.liveSurfaceGeneration = liveSource.surfaceSessionGenerations[activeSurfaceIndex];
            this.singleCameraXTransition = singleCameraXTransition;
            this.replacementFrameReady = replacementFrameReady;
            this.snapshot = snapshot;
            this.snapshotTextureBuffer = snapshot == null ? null : snapshot.textureBuffer.duplicate();
            this.snapshotTexture = snapshot == null ? 0 : snapshot.texture;
            this.snapshotWidth = snapshot == null ? 0 : snapshot.width;
            this.snapshotHeight = snapshot == null ? 0 : snapshot.height;
            this.progress = progress;
            this.blur = blur;
            this.rearCrossfade = liveSource.rearFrameCrossfade;
            this.rearFrameMix = liveSource.rearLensFrameMix;
            this.rearFrameTimestamp = liveSource.rearLensFrameTimestamp;
            this.activeSurfaceIndex = activeSurfaceIndex;
            this.liveTexture = liveTexture;
            this.liveTextureBuffer = liveTextureBuffer == null ? null : liveTextureBuffer.duplicate();
            this.livePreviewSize = livePreviewSize;
            this.liveSTMatrix = liveSTMatrix.clone();
            this.liveMVPMatrix = liveMVPMatrix.clone();
        }

        boolean hasLiveSource() {
            return liveSource.isCurrentGeneration() && (liveCameraXSession == null
                    || liveCameraXSession.isSurfaceRequestCurrent(liveSurfaceGeneration));
        }
    }

    private static class EncoderHandler extends Handler {
        private WeakReference<VideoRecorder> mWeakEncoder;

        public EncoderHandler(VideoRecorder encoder) {
            mWeakEncoder = new WeakReference<>(encoder);
        }

        @Override
        public void handleMessage(Message inputMessage) {
            int what = inputMessage.what;
            Object obj = inputMessage.obj;

            VideoRecorder encoder = mWeakEncoder.get();
            if (encoder == null) {
                return;
            }

            switch (what) {
                case MSG_START_RECORDING: {
                    try {
                        if (BuildVars.LOGS_ENABLED) {
                            FileLog.e("InstantCamera start encoder");
                        }
                        encoder.prepareEncoder(inputMessage.arg1 == 1);
                    } catch (Exception e) {
                        FileLog.e(e);
                        encoder.handleStartRecordingError();
                    }
                    break;
                }
                case MSG_STOP_RECORDING: {
                    if (BuildVars.LOGS_ENABLED) {
                        FileLog.e("InstantCamera stop encoder");
                    }
                    encoder.handleStopRecording(inputMessage.arg1, (SendOptions) inputMessage.obj);
                    break;
                }
                case MSG_PAUSE_RECORDING: {
                    if (BuildVars.LOGS_ENABLED) {
                        FileLog.e("InstantCamera pause encoder");
                    }
                    encoder.handlePauseRecording(obj);
                    break;
                }
                case MSG_RESUME_RECORDING: {
                    if (BuildVars.LOGS_ENABLED) {
                        FileLog.e("InstantCamera resume encoder");
                    }
                    encoder.handleResumeRecording();
                    break;
                }
                case MSG_VIDEOFRAME_AVAILABLE: {
                    long timestamp = (((long) inputMessage.arg1) << 32) | (((long) inputMessage.arg2) & 0xffffffffL);
                    CameraVideoFrameState frameState = (CameraVideoFrameState) inputMessage.obj;
                    encoder.handleVideoFrameAvailable(timestamp, frameState);
                    break;
                }
                case MSG_AUDIOFRAME_AVAILABLE: {
                    encoder.handleAudioFrameAvailable((AudioBufferInfo) inputMessage.obj);
                    break;
                }
            }
        }

        public void exit() {
            Looper.myLooper().quit();
        }
    }

    public static class SendOptions {
        boolean notify;
        int scheduleDate;
        int scheduleRepeatPeriod;
        int ttl;
        long effectId;
        long stars;

        public SendOptions(boolean notify, int scheduleDate, int scheduleRepeatPeriod, int ttl, long effectId, long stars) {
            this.notify = notify;
            this.scheduleDate = scheduleDate;
            this.scheduleRepeatPeriod = scheduleRepeatPeriod;
            this.ttl = ttl;
            this.effectId = effectId;
            this.stars = stars;
        }
    }

    public static class AudioBufferInfo {
        public final static int MAX_SAMPLES = 10;
        public ByteBuffer[] buffer = new ByteBuffer[MAX_SAMPLES];
        public long[] offset = new long[MAX_SAMPLES];
        public int[] read = new int[MAX_SAMPLES];
        public int results;
        public int lastWroteBuffer;
        public boolean last;

        public AudioBufferInfo() {
            for (int i = 0; i < MAX_SAMPLES; i++) {
                buffer[i] = ByteBuffer.allocateDirect(2048);
                buffer[i].order(ByteOrder.nativeOrder());
            }
        }
    }

    private class VideoRecorder implements Runnable {
        private final CameraXFrameCrossfade rearCrossfade = new CameraXFrameCrossfade();

        private static final String VIDEO_MIME_TYPE = "video/avc";
        private static final String AUDIO_MIME_TYPE = "audio/mp4a-latm";
        private static final int DEFAULT_FRAME_RATE = 30;
        private static final int HIGH_FRAME_RATE = 60;
        private static final int IFRAME_INTERVAL = 1;

        private File videoFile;

        private volatile File previewFile;
        private File fileToWrite;
        private boolean writingToDifferentFile;
        private int videoWidth;
        private int videoHeight;
        private int videoBitrate;
        private int frameRate = DEFAULT_FRAME_RATE;
        private boolean videoConvertFirstWrite = true;
        private boolean blendEnabled;

        private Surface surface;
        private android.opengl.EGLDisplay eglDisplay = EGL14.EGL_NO_DISPLAY;
        private android.opengl.EGLContext eglContext = EGL14.EGL_NO_CONTEXT;
        private android.opengl.EGLContext sharedEglContext;
        private android.opengl.EGLConfig eglConfig;
        private android.opengl.EGLSurface eglSurface = EGL14.EGL_NO_SURFACE;

        private MediaCodec videoEncoder;
        private MediaCodec audioEncoder;

        private int prependHeaderSize;
        private boolean firstEncode;
        private volatile boolean hasWrittenVideoSample;
        private volatile boolean movieFinalized;

        private MediaCodec.BufferInfo videoBufferInfo;
        private MediaCodec.BufferInfo audioBufferInfo;
        private MP4Builder mediaMuxer;
        private ArrayList<AudioBufferInfo> buffersToWrite = new ArrayList<>();
        private int videoTrackIndex = -5;
        private int audioTrackIndex = -5;

        private long lastCommittedFrameRealtimeNanos;
        private long audioStartTime = -1;
        private boolean firstVideoFrameSincePause;

        private long currentTimestamp = 0;
        private long lastTimestamp = -1;

        private volatile EncoderHandler handler;

        private final Object sync = new Object();
        public volatile boolean ready;
        private volatile boolean encoderPrepared;
        private volatile boolean running;
        private volatile int sendWhenDone;
        private volatile SendOptions sendWhenDoneOptions;
        private long skippedTime;
        private boolean skippedFirst;

        private long desyncTime;
        private long videoFirst = -1;
        private long videoLast;
        private long videoLastDt;
        private long videoDiff;
        private long prevVideoLast = -1;
        private long audioFirst = -1;
        private long audioLast = -1;
        private long audioLastDt = 0;
        private long prevAudioLast = -1;
        private long audioDiff;
        private boolean audioStopedByTime;

        private int drawProgram;
        private int vertexMatrixHandle;
        private int textureMatrixHandle;
        private int positionHandle;
        private int textureHandle;
        private int resolutionHandle;
        private int previewSizeHandle;
        private int texelSizeHandle;
        private int alphaHandle;
        private int switchBlurHandle;
        private int snapshotProgram;
        private int snapshotVertexMatrixHandle;
        private int snapshotTextureMatrixHandle;
        private int snapshotPositionHandle;
        private int snapshotTextureHandle;
        private int snapshotAlphaHandle;
        private int snapshotTexelSizeHandle;
        private int snapshotSwitchBlurHandle;
        private int zeroTimeStamps;
        private Integer lastCameraId = 0;
        private InstantCameraVideoEncoderOverlayHelper overlayHelper;

        private volatile AudioRecord audioRecorder;
        private int audioInputChannels = 1;
        private final int audioOutputChannels = 1;
        private boolean wideMicrophoneCapture;
        private boolean audioChannelsLocked;

        private ArrayBlockingQueue<AudioBufferInfo> buffers = new ArrayBlockingQueue<>(10);
        private ArrayList<Bitmap> keyframeThumbs = new ArrayList<>();
        private DispatchQueue generateKeyframeThumbsQueue;
        private int frameCount;

        DispatchQueue fileWriteQueue;

        private volatile boolean pauseRecorder;
        private Runnable recorderRunnable = new Runnable() {

            @RequiresApi(api = Build.VERSION_CODES.N)
            @Override
            public void run() {
                final AudioRecord recorder = audioRecorder;
                boolean readFailed = false;
                long audioPresentationTimeUs = -1;
                long audioFramesRead = 0;
                int readResult;
                boolean done = false;
                AudioTimestamp audioTimestamp = new AudioTimestamp();
                boolean shouldUseTimestamp = Build.VERSION.SDK_INT >= Build.VERSION_CODES.N;

                try {
                    while (!done) {
                        if ((!running || pauseRecorder) && recorder.getRecordingState() != AudioRecord.RECORDSTATE_STOPPED) {
                            try {
                                recorder.stop();
                            } catch (Exception e) {
                                done = true;
                            }
                            if (sendWhenDone == 0) {
                                break;
                            }
                        }
                        AudioBufferInfo buffer;
                        if (buffers.isEmpty()) {
                            try {
                                buffer = new AudioBufferInfo();
                            } catch (OutOfMemoryError error) {
                                System.gc();
                                buffer = new AudioBufferInfo();
                            }
                        } else {
                            buffer = buffers.poll();
                        }
                        buffer.lastWroteBuffer = 0;
                        buffer.results = AudioBufferInfo.MAX_SAMPLES;
                        for (int a = 0; a < AudioBufferInfo.MAX_SAMPLES; a++) {
                            if (audioPresentationTimeUs == -1 && !shouldUseTimestamp) {
                                audioPresentationTimeUs = System.nanoTime() / 1000;
                            }

                            ByteBuffer byteBuffer = buffer.buffer[a];
                            byteBuffer.clear();
                            readResult = com.th3nekit.finegram.camera.AudioCaptureLifecycle.read(recorder, byteBuffer, 2048);
                            while (readResult == 0 && running && !pauseRecorder) {
                                android.os.SystemClock.sleep(8);
                                readResult = com.th3nekit.finegram.camera.AudioCaptureLifecycle.read(recorder, byteBuffer, 2048);
                            }
                            if (readResult < 0 && running && !pauseRecorder) {
                                throw new IllegalStateException("AudioRecord read failed: " + readResult);
                            }
                            int capturedBytes = readResult;
                            if (readResult > 0) {
                                byteBuffer.limit(readResult);
                                if (audioInputChannels == 2 && audioOutputChannels == 1) {
                                    readResult = com.th3nekit.finegram.camera.CameraXAudioCapture.downmixStereo16(byteBuffer, readResult);
                                }
                            }
                            if (readResult > 0 && a % 2 == 0) {
                                byteBuffer.limit(readResult);
                                double s = 0;
                                for (int i = 0; i < readResult / 2; i++) {
                                    short p = byteBuffer.getShort();
                                    s += p * p;
                                }
                                double amplitude = Math.sqrt(s / readResult / 2);
                                AndroidUtilities.runOnUIThread(() -> NotificationCenter.getInstance(currentAccount).postNotificationName(NotificationCenter.recordProgressChanged, recordingGuid, amplitude));
                                byteBuffer.position(0);
                            }
                            if (readResult <= 0) {
                                buffer.results = a;
                                if (!running) {
                                    buffer.last = true;
                                }
                                break;
                            }
                            long timestamp;
                            if (shouldUseTimestamp) {
                                try {

                                    if (recorder.getTimestamp(audioTimestamp, AudioTimestamp.TIMEBASE_MONOTONIC)
                                            != AudioRecord.SUCCESS) {
                                        throw new IllegalStateException("AudioRecord timestamp unavailable");
                                    }
                                    timestamp = audioInputChannels == 2
                                            ? com.th3nekit.finegram.camera.CameraXAudioCapture.bufferTimeUs(
                                                    audioTimestamp.nanoTime, audioTimestamp.framePosition, audioFramesRead, audioSampleRate)
                                            : audioTimestamp.nanoTime / 1000;
                                } catch (Exception e) {
                                    FileLog.e(e);
                                    shouldUseTimestamp = false;
                                    timestamp = audioPresentationTimeUs = audioInputChannels == 2 && audioPresentationTimeUs >= 0
                                            ? audioPresentationTimeUs : System.nanoTime() / 1000;
                                }
                            } else {
                                timestamp = audioPresentationTimeUs;
                            }
                            buffer.offset[a] = timestamp;

                            buffer.read[a] = readResult;
                            long bufferDurationUs = com.th3nekit.finegram.camera.CameraXAudioCapture.durationUs(
                                    capturedBytes, audioSampleRate, audioInputChannels);
                            audioFramesRead += capturedBytes / (audioInputChannels * 2);
                            if (!shouldUseTimestamp || audioInputChannels == 2) {
                                audioPresentationTimeUs = timestamp + bufferDurationUs;
                            }
                        }
                        if (buffer.results >= 0 || buffer.last) {
                            if ((!running || pauseRecorder) && buffer.results < AudioBufferInfo.MAX_SAMPLES) {
                                done = true;
                            }
                            handler.sendMessage(handler.obtainMessage(MSG_AUDIOFRAME_AVAILABLE, buffer));
                        } else {
                            if (!running) {
                                done = true;
                            } else {
                                try {
                                    buffers.put(buffer);
                                } catch (Exception ignore) {

                                }
                            }
                        }
                    }
                } catch (RuntimeException e) {
                    FileLog.e(e);
                    readFailed = running && !pauseRecorder;
                } finally {
                    com.th3nekit.finegram.camera.AudioCaptureLifecycle.release(recorder);
                    if (audioRecorder == recorder) {
                        audioRecorder = null;
                    }
                }
                if (readFailed) {
                    handler.post(() -> handleStartRecordingError());
                } else if (!pauseRecorder) {
                    handler.sendMessage(handler.obtainMessage(MSG_STOP_RECORDING, sendWhenDone, 0, sendWhenDoneOptions));
                }
            }
        };

        private boolean started;

        public void startRecording(File outputFile, android.opengl.EGLContext sharedContext) {
            encoderPrepared = false;
            if (started && (handler != null && handler.getLooper() != null && handler.getLooper().getThread() != null && handler.getLooper().getThread().isAlive())) {
                sharedEglContext = sharedContext;
                handler.sendMessage(handler.obtainMessage(MSG_START_RECORDING, 1, 0));
            }

            started = true;

            MessagesController mc = MessagesController.getInstance(currentAccount);
            mc.roundVideoSize = com.th3nekit.finegram.camera.CameraSettings.roundResolution(512);
            mc.roundVideoBitrate = com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getVideoMessagesBitrateKbps();
            mc.roundAudioBitrate = com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getVideoMessagesAudioBitrateKbps();
            int resolution = mc.roundVideoSize;
            int bitrate = mc.roundVideoBitrate * 1024;
            recordedVideoBitrate = bitrate;
            setHeavyOperationsStopped(true);

            videoFile = outputFile;
            videoWidth = resolution;
            videoHeight = resolution;
            videoBitrate = bitrate;

            frameRate = useCameraX
                    && com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getCameraXFpsRange()
                    == com.th3nekit.finegram.core.configs.FinegramCameraConfig.CameraXFpsRange30to60
                    ? HIGH_FRAME_RATE : DEFAULT_FRAME_RATE;
            sharedEglContext = sharedContext;

            synchronized (sync) {
                if (running) {
                    return;
                }
                running = true;
                Thread thread = new Thread(this, "TextureMovieEncoder");
                thread.setPriority(Thread.MAX_PRIORITY);
                thread.start();
                while (!ready) {
                    try {
                        sync.wait();
                    } catch (InterruptedException ie) {

                    }
                }
            }

            if (WRITE_TO_FILE_IN_BACKGROUND) {
                fileWriteQueue = new DispatchQueue("IVR_FileWriteQueue");
                fileWriteQueue.setPriority(Thread.MAX_PRIORITY);
            }

            keyframeThumbs.clear();
            frameCount = 0;
            if (generateKeyframeThumbsQueue != null) {
                generateKeyframeThumbsQueue.cleanupQueue();
                generateKeyframeThumbsQueue.recycle();
            }
            generateKeyframeThumbsQueue = new DispatchQueue("keyframes_thumb_queue");
            handler.sendMessage(handler.obtainMessage(MSG_START_RECORDING));
        }

        public void stopRecording(int send, SendOptions options) {
            handler.sendMessage(handler.obtainMessage(MSG_STOP_RECORDING, send, 0, options));
            setHeavyOperationsStopped(false);
        }

        private void setHeavyOperationsStopped(boolean stopped) {
            AndroidUtilities.runOnUIThread(() -> {
                if (stopped) {
                    boolean alreadyStopped = heavyOperationsOwner != null;
                    heavyOperationsOwner = this;
                    if (!alreadyStopped) {
                        NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.stopAllHeavyOperations, 512);
                    }
                } else if (heavyOperationsOwner == this) {

                    heavyOperationsOwner = null;
                    NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.startAllHeavyOperations, 512);
                }
            });
        }

        public void pause() {
            final Object token = new Object();
            pausePreviewToken = token;
            handler.sendMessage(handler.obtainMessage(MSG_PAUSE_RECORDING, token));
        }

        public void resume() {
            handler.sendMessage(handler.obtainMessage(MSG_RESUME_RECORDING));
        }

        long prevTimestamp;
        private Integer lastFrameSubmissionCameraId;
        private volatile long lastFrameSubmissionRealtimeNanos;

        private final ArrayList<CameraVideoFrameState> pendingVideoFrames = new ArrayList<>();
        private final ArrayList<Runnable> pendingVideoCompletions = new ArrayList<>();

        private void finishVideoCommands() {
            if (eglContext != null && eglContext != EGL14.EGL_NO_CONTEXT
                    && eglContext.equals(EGL14.eglGetCurrentContext())) {
                GLES20.glFinish();
            }
        }

        private void afterVideoFrames(Runnable completion) {
            synchronized (sync) {
                if (ready && handler != null) {
                    pendingVideoCompletions.add(completion);
                    handler.post(() -> {
                        try {
                            finishVideoCommands();
                        } finally {
                            boolean completed;
                            synchronized (sync) {
                                completed = pendingVideoCompletions.remove(completion);
                            }
                            if (completed) completion.run();
                        }
                    });
                    return;
                }
            }
            completion.run();
        }

        private CameraVideoFrameState captureFrameState(Integer cameraId, CameraGLThread source) {
            CameraGLThread.CameraSnapshot snapshot = cameraXSingleSwitchSnapshotHandle;
            boolean transition = useCameraX && !bothCameras
                    && cameraXVideoTransitionActive
                    && snapshot != null && snapshot.belongsTo(source) && snapshot.retain();
            int index = surfaceIndex >= 0 && surfaceIndex < source.cameraTexture.length ? surfaceIndex : 0;
            return new CameraVideoFrameState(cameraId, source, transition,
                    transition && cameraXSingleSwitchNewFrame,
                    transition ? snapshot : null,
                    transition ? cameraXSingleSwitchProgress : 1f,
                    transition ? cameraXSingleSwitchBlur : 0f,
                    index, source.cameraTexture[index], cameraTextureBuffers[index] != null
                            ? cameraTextureBuffers[index] : InstantCameraView.this.textureBuffer, previewSize[index],
                    source.screenSTMatrix[index], source.screenMVPMatrix[index]);
        }

        private void submitVideoFrame(long timestamp, Integer cameraId, CameraGLThread source) {
            synchronized (sync) {
                if (!ready || !encoderPrepared || handler == null || !source.isCurrentGeneration()) return;
                CameraVideoFrameState frameState = captureFrameState(cameraId, source);
                pendingVideoFrames.add(frameState);
                if (!handler.sendMessage(handler.obtainMessage(MSG_VIDEOFRAME_AVAILABLE,
                        (int) (timestamp >> 32), (int) timestamp, frameState))) {
                    pendingVideoFrames.remove(frameState);
                    if (frameState.snapshot != null) frameState.snapshot.release();
                }
            }
        }

        public void frameAvailable(SurfaceTexture st, Integer cameraId, long timestampInternal, CameraGLThread source) {
            synchronized (sync) {
                if (!ready || !encoderPrepared || !source.isCurrentGeneration()) {
                    return;
                }
            }

            long timestamp = st.getTimestamp();
            if (timestamp == 0) {
                zeroTimeStamps++;
                if (zeroTimeStamps > 1) {
                    if (BuildVars.LOGS_ENABLED) {
                        FileLog.d("InstantCamera fix timestamp enabled");
                    }
                    timestamp = timestampInternal;
                } else {
                    return;
                }
            } else {
                zeroTimeStamps = 0;
            }

            if (cameraId.equals(lastFrameSubmissionCameraId) && timestamp <= prevTimestamp) return;
            long now = System.nanoTime();

            lastFrameSubmissionRealtimeNanos = now;
            prevTimestamp = timestamp;
            lastFrameSubmissionCameraId = cameraId;
            submitVideoFrame(timestamp, cameraId, source);
        }

        public void transitionFrameAvailable(Integer cameraId, CameraGLThread source) {

            if (bothCameras) return;
            synchronized (sync) {
                if (!ready || !encoderPrepared || handler == null || !source.isCurrentGeneration()) return;
            }

            long now = System.nanoTime();
            if (now - lastFrameSubmissionRealtimeNanos < 28_000_000L) return;
            if (prevTimestamp <= 0 || !cameraId.equals(lastFrameSubmissionCameraId)) return;

            long timestampNanos = prevTimestamp + (now - lastFrameSubmissionRealtimeNanos);
            lastFrameSubmissionRealtimeNanos = now;
            prevTimestamp = timestampNanos;
            submitVideoFrame(timestampNanos, cameraId, source);
        }

        @Override
        public void run() {
            Looper.prepare();
            synchronized (sync) {
                handler = new EncoderHandler(this);
                ready = true;
                sync.notify();
            }
            try {
                Looper.loop();
            } finally {
                try {
                    finishVideoCommands();
                } finally {
                    ArrayList<Runnable> completions;
                    synchronized (sync) {
                        ready = false;
                        encoderPrepared = false;
                        for (CameraVideoFrameState frameState : pendingVideoFrames) {
                            if (frameState.snapshot != null) frameState.snapshot.release();
                        }
                        pendingVideoFrames.clear();
                        completions = new ArrayList<>(pendingVideoCompletions);
                        pendingVideoCompletions.clear();
                    }
                    for (Runnable completion : completions) completion.run();
                }
            }
        }

        private void handleAudioFrameAvailable(AudioBufferInfo input) {
            if (pauseRecorder) {
                return;
            }
            if (audioStopedByTime) {
                return;
            }
            buffersToWrite.add(input);
            if (audioFirst == -1) {
                if (videoFirst == -1) {
                    if (BuildVars.LOGS_ENABLED) {
                        FileLog.d("InstantCamera video record not yet started");
                    }
                    return;
                }
                while (true) {
                    boolean ok = false;
                    for (int a = 0; a < input.results; a++) {
                        if (a == 0 && Math.abs(videoFirst - input.offset[a]) > 10_000_000L) {
                            desyncTime = videoFirst - input.offset[a];
                            audioFirst = input.offset[a];
                            ok = true;
                            if (BuildVars.LOGS_ENABLED) {
                                FileLog.d("InstantCamera detected desync between audio and video " + desyncTime);
                            }
                            break;
                        }
                        if (input.offset[a] >= videoFirst) {
                            input.lastWroteBuffer = a;
                            audioFirst = input.offset[a];
                            ok = true;
                            if (BuildVars.LOGS_ENABLED) {
                                FileLog.d("InstantCamera found first audio frame at " + a + " timestamp = " + input.offset[a]);
                            }
                            break;
                        } else {
                            if (BuildVars.LOGS_ENABLED) {
                                FileLog.d("InstantCamera ignore first audio frame at " + a + " timestamp = " + input.offset[a]);
                            }
                        }
                    }
                    if (!ok) {
                        if (BuildVars.LOGS_ENABLED) {
                            FileLog.d("InstantCamera first audio frame not found, removing buffers " + input.results);
                        }
                        buffersToWrite.remove(input);
                    } else {
                        break;
                    }
                    if (!buffersToWrite.isEmpty()) {
                        input = buffersToWrite.get(0);
                    } else {
                        return;
                    }
                }
            }

            if (audioStartTime == -1) {
                audioStartTime = input.offset[input.lastWroteBuffer];
            }
            if (buffersToWrite.size() > 1) {
                input = buffersToWrite.get(0);
            }
            try {
                drainEncoder(false);
            } catch (Exception e) {
                FileLog.e(e);
            }
            try {
                boolean isLast = false;
                while (input != null) {
                    int inputBufferIndex = audioEncoder.dequeueInputBuffer(0);
                    if (inputBufferIndex >= 0) {
                        ByteBuffer inputBuffer;
                        inputBuffer = audioEncoder.getInputBuffer(inputBufferIndex);
                        long startWriteTime = input.offset[input.lastWroteBuffer];
                        for (int a = input.lastWroteBuffer; a <= input.results; a++) {
                            if (a < input.results) {
                                long totalTime = input.offset[a] - audioStartTime;
                                if (!running && (input.offset[a] >= videoLast - desyncTime || totalTime >= 60_000000)) {
                                    if (BuildVars.LOGS_ENABLED) {
                                        if (totalTime >= 60_000000) {
                                            FileLog.d("InstantCamera stop audio encoding because recorded time more than 60s");
                                        } else {
                                            FileLog.d("InstantCamera stop audio encoding because of stoped video recording at " + input.offset[a] + " last video " + videoLast);
                                        }

                                    }
                                    audioStopedByTime = true;
                                    isLast = true;
                                    input = null;
                                    buffersToWrite.clear();
                                    break;
                                }
                                if (inputBuffer.remaining() < input.read[a]) {
                                    input.lastWroteBuffer = a;
                                    input = null;
                                    break;
                                }
                                inputBuffer.put(input.buffer[a]);
                            }
                            if (a >= input.results - 1) {
                                buffersToWrite.remove(input);
                                if (running) {
                                    buffers.put(input);
                                }
                                if (!buffersToWrite.isEmpty()) {
                                    input = buffersToWrite.get(0);
                                } else {
                                    isLast = input.last;
                                    input = null;
                                    break;
                                }
                            }
                        }
                        long time = startWriteTime == 0 ? 0 : startWriteTime - audioStartTime;
                        long realtime = time;
                        if (prevAudioLast >= 0) {
                            time += prevAudioLast;
                        }
                        audioLastDt = time - audioLast;
                        audioLast = time;
                        audioEncoder.queueInputBuffer(inputBufferIndex, 0, inputBuffer.position(), time, isLast ? MediaCodec.BUFFER_FLAG_END_OF_STREAM : 0);
                    }
                }
            } catch (Throwable e) {
                FileLog.e(e);
            }
        }

        private void drawCameraXSnapshot(CameraVideoFrameState frameState,
                FloatBuffer vertexBuffer) {
            if (!frameState.singleCameraXTransition || frameState.snapshotTexture == 0
                    || snapshotProgram == 0 || frameState.snapshot == null) {
                return;
            }
            GLES20.glUseProgram(snapshotProgram);
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            GLES20.glVertexAttribPointer(snapshotPositionHandle, 3, GLES20.GL_FLOAT,
                    false, 12, vertexBuffer);
            GLES20.glEnableVertexAttribArray(snapshotPositionHandle);
            GLES20.glVertexAttribPointer(snapshotTextureHandle, 2, GLES20.GL_FLOAT,
                    false, 8, frameState.snapshotTextureBuffer);
            GLES20.glEnableVertexAttribArray(snapshotTextureHandle);
            GLES20.glUniformMatrix4fv(snapshotVertexMatrixHandle, 1, false,
                    frameState.snapshot.identityMatrix, 0);
            GLES20.glUniformMatrix4fv(snapshotTextureMatrixHandle, 1, false,
                    frameState.snapshot.identityMatrix, 0);
            GLES20.glUniform1f(snapshotAlphaHandle, 1f);
            GLES20.glUniform2f(snapshotTexelSizeHandle,
                    1f / Math.max(1, frameState.snapshotWidth),
                    1f / Math.max(1, frameState.snapshotHeight));
            GLES20.glUniform1f(snapshotSwitchBlurHandle, frameState.blur);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, frameState.snapshotTexture);
            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
            GLES20.glDisableVertexAttribArray(snapshotPositionHandle);
            GLES20.glDisableVertexAttribArray(snapshotTextureHandle);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
            GLES20.glUseProgram(0);
        }

        private void handleVideoFrameAvailable(long timestampNanos,
                CameraVideoFrameState frameState) {
            try {
                renderVideoFrame(timestampNanos, frameState);
            } finally {
                try {

                    if (frameState.snapshot != null && eglContext != EGL14.EGL_NO_CONTEXT
                            && eglContext.equals(EGL14.eglGetCurrentContext())) {
                        GLES20.glFinish();
                    }
                } finally {
                    synchronized (sync) {
                        pendingVideoFrames.remove(frameState);
                    }
                    if (frameState.snapshot != null) frameState.snapshot.release();
                }
            }
        }

        private void renderVideoFrame(long timestampNanos, CameraVideoFrameState frameState) {
            if (pauseRecorder || ((!cameraTextureAvailable || !frameState.hasLiveSource())
                    && !frameState.singleCameraXTransition)) {
                return;
            }
            Integer cameraId = frameState.cameraId;
            try {
                drainEncoder(false);
            } catch (Exception e) {
                FileLog.e(e);
            }
            long dt, alphaDt;
            boolean cameraChanged = false;
            if (!lastCameraId.equals(cameraId)) {
                cameraChanged = true;
                lastCameraId = cameraId;
            }
            if (prevVideoLast >= 0) {
                if (videoDiff == -1) {
                    videoDiff = timestampNanos - prevVideoLast;
                }
                timestampNanos -= videoDiff;
            }
            if (cameraChanged || lastTimestamp == -1) {
                if (currentTimestamp != 0 && !firstVideoFrameSincePause) {

                    long dtTimestamps = (timestampNanos - lastTimestamp);
                    long dtReal = System.nanoTime() - lastCommittedFrameRealtimeNanos;
                    if (dtTimestamps < 0 || Math.abs(dtReal - dtTimestamps) > 100_000_000) {
                        dt = dtReal;
                    } else {
                        dt = dtTimestamps;
                    }
                    if (dt < 0) {
                        dt = 0;
                    }
                    alphaDt = 0;
                } else {
                    alphaDt = dt = 0;
                }
                lastTimestamp = timestampNanos;
            } else {
                alphaDt = dt = (timestampNanos - lastTimestamp);
                lastTimestamp = timestampNanos;
            }
            firstVideoFrameSincePause = false;
            lastCommittedFrameRealtimeNanos = System.nanoTime();
            if (!skippedFirst) {
                skippedTime += dt;
                if (skippedTime < 200000000) {
                    return;
                }
                skippedFirst = true;
            }
            currentTimestamp += dt;
            if (videoFirst == -1) {
                videoFirst = timestampNanos / 1000;
                if (BuildVars.LOGS_ENABLED) {
                    FileLog.d("InstantCamera first video frame was at " + videoFirst);
                }
            }
            videoLastDt = timestampNanos - videoLast;
            videoLast = timestampNanos;

            FloatBuffer textureBuffer = frameState.liveTextureBuffer;
            FloatBuffer vertexBuffer = InstantCameraView.this.vertexBuffer;
            FloatBuffer oldTextureBuffer = oldTextureTextureBuffer;
            if (textureBuffer == null || vertexBuffer == null) {
                FileLog.d("InstantCamera handleVideoFrameAvailable skip frame " + textureBuffer + " " + vertexBuffer);
                return;
            }

            if (overlayHelper != null) {
                overlayHelper.bind();
            }

            boolean compositeRearFrame = frameState.rearCrossfade && !frameState.singleCameraXTransition
                    && rearCrossfade.begin(videoWidth, videoHeight, frameState.rearFrameTimestamp,
                            frameState.liveCameraXSession, frameState.liveSurfaceGeneration, frameState.rearFrameMix);
            if (!compositeRearFrame) rearCrossfade.resetHistory();

            if (useCameraX) {

                GLES20.glBlendFunc(GLES20.GL_ONE, GLES20.GL_ONE_MINUS_SRC_ALPHA);
            }

            if (frameState.singleCameraXTransition) {
                if (!blendEnabled) {
                    GLES20.glEnable(GLES20.GL_BLEND);
                    blendEnabled = true;
                }
                drawCameraXSnapshot(frameState, vertexBuffer);
            }

            GLES20.glUseProgram(drawProgram);
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            GLES20.glVertexAttribPointer(positionHandle, 3, GLES20.GL_FLOAT, false, 12, vertexBuffer);
            GLES20.glEnableVertexAttribArray(positionHandle);
            GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT, false, 8, textureBuffer);
            GLES20.glEnableVertexAttribArray(textureHandle);
            GLES20.glUniformMatrix4fv(vertexMatrixHandle, 1, false, frameState.liveMVPMatrix, 0);

            GLES20.glUniform2f(resolutionHandle, videoWidth, videoHeight);

            final float videoSwitchProgress = dualVideoSwitchProgress;
            final int videoSwitchFrom = dualVideoSwitchFrom;
            final int videoSwitchTo = dualVideoSwitchTo;
            final boolean renderDualVideoSwitch = !frameState.singleCameraXTransition
                    && bothCameras && dualVideoSwitching
                    && videoSwitchFrom >= 0 && videoSwitchFrom < 2
                    && videoSwitchTo >= 0 && videoSwitchTo < 2
                    && cameraTexture[videoSwitchFrom] != Integer.MIN_VALUE
                    && cameraTexture[videoSwitchTo] != Integer.MIN_VALUE;
            final float singleSwitchBlur = frameState.blur;

            if (!frameState.singleCameraXTransition
                    && oldCameraTexture[0] != 0 && oldTextureBuffer != null && !bothCameras) {
                if (!blendEnabled) {
                    GLES20.glEnable(GLES20.GL_BLEND);
                    blendEnabled = true;
                }
                if (oldTexturePreviewSize != null) {
                    GLES20.glUniform2f(previewSizeHandle, oldTexturePreviewSize.getWidth(), oldTexturePreviewSize.getHeight());
                    GLES20.glUniform2f(texelSizeHandle,
                            .5f / Math.max(1, oldTexturePreviewSize.getWidth()),
                            .5f / Math.max(1, oldTexturePreviewSize.getHeight()));
                }
                GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT, false, 8, oldTextureBuffer);

                GLES20.glUniformMatrix4fv(vertexMatrixHandle, 1, false, oldScreenMVPMatrix, 0);
                GLES20.glUniformMatrix4fv(textureMatrixHandle, 1, false, moldSTMatrix, 0);
                GLES20.glUniform1f(alphaHandle, 1.0f);
                GLES20.glUniform1f(switchBlurHandle, singleSwitchBlur);
                GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, oldCameraTexture[0]);
                GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
            }

            if (renderDualVideoSwitch) {
                if (!blendEnabled) {
                    GLES20.glEnable(GLES20.GL_BLEND);
                    blendEnabled = true;
                }
                FloatBuffer fromTextureBuffer = cameraTextureBuffers[videoSwitchFrom];
                if (fromTextureBuffer != null) {
                    GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT,
                            false, 8, fromTextureBuffer);
                }
                Size fromSize = previewSize[videoSwitchFrom];
                if (fromSize != null) {
                    GLES20.glUniform2f(previewSizeHandle, fromSize.getWidth(), fromSize.getHeight());
                    GLES20.glUniform2f(texelSizeHandle,
                            .5f / fromSize.getWidth(), .5f / fromSize.getHeight());
                }
                GLES20.glUniformMatrix4fv(vertexMatrixHandle, 1, false,
                        dualVideoMVPMatrix[videoSwitchFrom], 0);
                GLES20.glUniformMatrix4fv(textureMatrixHandle, 1, false,
                        dualVideoSTMatrix[videoSwitchFrom], 0);
                GLES20.glUniform1f(alphaHandle, 1f);
                GLES20.glUniform1f(switchBlurHandle, 0f);
                GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
                        cameraTexture[videoSwitchFrom]);
                GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

                FloatBuffer toTextureBuffer = cameraTextureBuffers[videoSwitchTo];
                if (toTextureBuffer != null) {
                    GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT,
                            false, 8, toTextureBuffer);
                }
                Size toSize = previewSize[videoSwitchTo];
                if (toSize != null) {
                    GLES20.glUniform2f(previewSizeHandle, toSize.getWidth(), toSize.getHeight());
                    GLES20.glUniform2f(texelSizeHandle,
                            .5f / toSize.getWidth(), .5f / toSize.getHeight());
                }
                GLES20.glUniformMatrix4fv(vertexMatrixHandle, 1, false,
                        dualVideoMVPMatrix[videoSwitchTo], 0);
                GLES20.glUniformMatrix4fv(textureMatrixHandle, 1, false,
                        dualVideoSTMatrix[videoSwitchTo], 0);
                GLES20.glUniform1f(alphaHandle, Utilities.clamp(videoSwitchProgress, 1f, 0f));
                GLES20.glUniform1f(switchBlurHandle, 0f);
                GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
                        cameraTexture[videoSwitchTo]);
                GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
            } else if (!frameState.singleCameraXTransition
                    || frameState.replacementFrameReady && cameraTextureAvailable && frameState.hasLiveSource()
                    && frameState.snapshot.hasLiveSource()) {
                GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT,
                        false, 8, textureBuffer);
                if (frameState.livePreviewSize != null) {
                    GLES20.glUniform2f(previewSizeHandle,
                            frameState.livePreviewSize.getWidth(),
                            frameState.livePreviewSize.getHeight());
                    GLES20.glUniform2f(texelSizeHandle,
                            .5f / frameState.livePreviewSize.getWidth(),
                            .5f / frameState.livePreviewSize.getHeight());
                }

                final int tex = frameState.liveTexture;
                if (tex != Integer.MIN_VALUE && tex != 0) {
                    GLES20.glUniformMatrix4fv(vertexMatrixHandle, 1, false, frameState.liveMVPMatrix, 0);
                    GLES20.glUniformMatrix4fv(textureMatrixHandle, 1, false, frameState.liveSTMatrix, 0);
                    GLES20.glUniform1f(alphaHandle, frameState.singleCameraXTransition
                            ? Utilities.clamp(frameState.progress, 1f, 0f)
                            : cameraTextureAlpha);
                    GLES20.glUniform1f(switchBlurHandle, singleSwitchBlur);
                    GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, tex);
                    GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
                }
            }

            GLES20.glDisableVertexAttribArray(positionHandle);
            GLES20.glDisableVertexAttribArray(textureHandle);
            GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, 0);
            GLES20.glUseProgram(0);

            if (compositeRearFrame) rearCrossfade.end(frameState.rearFrameMix);
            if (useCameraX) {

                GLES20.glBlendFunc(GLES20.GL_SRC_ALPHA, GLES20.GL_ONE_MINUS_SRC_ALPHA);
            }
            if (overlayHelper != null) {
                overlayHelper.render();
                if (blendEnabled) {
                    GLES20.glEnable(GLES20.GL_BLEND);
                }
            }

            EGLExt.eglPresentationTimeANDROID(eglDisplay, eglSurface, currentTimestamp);
            EGL14.eglSwapBuffers(eglDisplay, eglSurface);

            if (renderDualVideoSwitch && videoSwitchProgress >= 1f) {

                GLES20.glDisable(GLES20.GL_BLEND);
                blendEnabled = false;
            }

            createKeyframeThumb();
            frameCount++;

            if (oldCameraTexture[0] != 0 && cameraTextureAlpha < 1.0f && !bothCameras
                    && !cameraXSingleSwitchAwaitingBind) {
                cameraTextureAlphaProgress += alphaDt / 260000000.0f;
                float p = Utilities.clamp(cameraTextureAlphaProgress, 1f, 0f);
                cameraTextureAlpha = p * p * (3f - 2f * p);
                if (cameraTextureAlphaProgress >= 1f) {
                    GLES20.glDisable(GLES20.GL_BLEND);
                    blendEnabled = false;
                    cameraTextureAlpha = 1;
                    cameraTextureAlphaProgress = 1f;
                    GLES20.glDeleteTextures(1, oldCameraTexture, 0);
                    oldCameraTexture[0] = 0;
                    if (!cameraReady && !cameraXSingleSwitchAwaitingBind) {
                        publishPreviewReady(frameState);
                    }
                }
            } else if (!cameraReady && !cameraXSingleSwitchAwaitingBind) {
                publishPreviewReady(frameState);
            }
        }

        private void publishPreviewReady(CameraVideoFrameState frameState) {

            if (useCameraX) return;
            frameState.liveSource.postToUiIfCurrent(() -> {
                if (!cameraReady && !cameraXSingleSwitchAwaitingBind && frameState.hasLiveSource()) {
                    cameraReady = true;
                    onCameraPreviewReady();
                }
            });
        }

        private void createKeyframeThumb() {
            if (generateKeyframeThumbsQueue != null && SharedConfig.getDevicePerformanceClass() == SharedConfig.PERFORMANCE_CLASS_HIGH && frameCount % 33 == 0) {
                GenerateKeyframeThumbTask task = new GenerateKeyframeThumbTask();
                generateKeyframeThumbsQueue.postRunnable(task);
            }
        }

        private class GenerateKeyframeThumbTask implements Runnable {
            @Override
            public void run() {
                final TextureView textureView = InstantCameraView.this.textureView;
                if (textureView != null) {
                    try {
                        final Bitmap bitmap = textureView.getBitmap(dp(56), dp(56));
                        AndroidUtilities.runOnUIThread(() -> {
                            if ((bitmap == null || bitmap.getPixel(0, 0) == 0) && keyframeThumbs.size() > 1) {
                                keyframeThumbs.add(keyframeThumbs.get(keyframeThumbs.size() - 1));
                            } else {
                                keyframeThumbs.add(bitmap);
                            }
                        });
                    } catch (Exception e) {
                        FileLog.e(e);
                    }

                }
            }
        }

        private void handlePauseRecording(final Object token) {
            pauseRecorder = true;
            if (previewFile != null) {
                previewFile.delete();
                previewFile = null;
            }
            final File pausedVideoFile = videoFile;
            final File pausedPreviewFile = StoryEntry.makeCacheFile(currentAccount, true);
            previewFile = pausedPreviewFile;
            try {
                FileLog.d("InstantCamera handlePauseRecording drain encoders");
                drainEncoder(false);
            } catch (Exception e) {
                FileLog.e(e);
            }

            if (mediaMuxer != null) {
                if (WRITE_TO_FILE_IN_BACKGROUND) {
                    CountDownLatch countDownLatch = new CountDownLatch(1);
                    fileWriteQueue.postRunnable(() -> {
                        try {
                            mediaMuxer.finishMovie(pausedPreviewFile);
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                        countDownLatch.countDown();
                    });
                    try {
                        countDownLatch.await();
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                } else {
                    try {
                        mediaMuxer.finishMovie(pausedPreviewFile);
                    } catch (Exception e) {
                        FileLog.e(e);
                    }
                }
            }

            AndroidUtilities.runOnUIThread(() -> {
                if (pausePreviewToken != token || cancelled || textureView == null
                        || InstantCameraView.this.videoEncoder != VideoRecorder.this
                        || pausedVideoFile == null || cameraFile != pausedVideoFile
                        || previewFile != pausedPreviewFile || !pausedPreviewFile.exists()) {
                    return;
                }

                pausePreviewToken = null;
                videoEditedInfo = new VideoEditedInfo();
                videoEditedInfo.roundVideo = true;
                videoEditedInfo.startTime = -1;
                videoEditedInfo.endTime = -1;
                videoEditedInfo.file = file;
                videoEditedInfo.encryptedFile = encryptedFile;
                videoEditedInfo.key = key;
                videoEditedInfo.iv = iv;
                videoEditedInfo.estimatedSize = Math.max(1, size);
                videoEditedInfo.framerate = frameRate;
                videoEditedInfo.resultWidth = videoEditedInfo.originalWidth = 360;
                videoEditedInfo.resultHeight = videoEditedInfo.originalHeight = 360;
                videoEditedInfo.originalPath = pausedPreviewFile.getAbsolutePath();
                setupVideoPlayer(pausedPreviewFile);
                videoEditedInfo.estimatedDuration = recordedTime;
                NotificationCenter.getInstance(currentAccount).postNotificationName(NotificationCenter.audioDidSent, recordingGuid, videoEditedInfo, pausedPreviewFile.getAbsolutePath(), keyframeThumbs);
            });
        }

        private void handleResumeRecording() {
            pauseRecorder = false;
        }

        private void setupVideoPlayer(File file) {
            videoPlayer = new VideoPlayer();
            videoPlayer.setDelegate(new VideoPlayer.VideoPlayerDelegate() {
                @Override
                public void onStateChanged(boolean playWhenReady, int playbackState) {
                    if (videoPlayer == null) {
                        return;
                    }
                    if (videoPlayer.isPlaying() && playbackState == ExoPlayer.STATE_ENDED && videoEditedInfo != null) {
                        videoPlayer.seekTo(videoEditedInfo.startTime > 0 ? videoEditedInfo.startTime : 0);
                    }
                }

                @Override
                public void onError(VideoPlayer player, Exception e) {
                    FileLog.e(e);
                }

                @Override
                public void onVideoSizeChanged(int width, int height, int unappliedRotationDegrees, float pixelWidthHeightRatio) {

                }

                @Override
                public void onRenderedFirstFrame() {

                }
            });
            videoPlayer.setTextureView(textureView);
            videoPlayer.preparePlayer(Uri.fromFile(file), "other");
            videoPlayer.play();
            videoPlayer.setMute(true);
            startProgressTimer();

            AnimatorSet animatorSet = new AnimatorSet();
            animatorSet.playTogether(
                    ObjectAnimator.ofFloat(buttonsLayout, View.ALPHA, 0.0f),
                    ObjectAnimator.ofInt(paint, AnimationProperties.PAINT_ALPHA, 0),
                    ObjectAnimator.ofFloat(muteImageView, View.ALPHA, 1.0f));
            animatorSet.setDuration(180);
            animatorSet.setInterpolator(new DecelerateInterpolator());
            animatorSet.start();

            if (eglContext != null && eglContext.equals(EGL14.eglGetCurrentContext())) {
                rearCrossfade.release();
            }
            EGL14.eglDestroySurface(eglDisplay, eglSurface);
            eglSurface = EGL14.EGL_NO_SURFACE;
            if (surface != null) {
                surface.release();
                surface = null;
            }
            if (eglDisplay != EGL14.EGL_NO_DISPLAY) {
                EGL14.eglMakeCurrent(eglDisplay, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT);
                EGL14.eglDestroyContext(eglDisplay, eglContext);
                EGL14.eglReleaseThread();
                EGL14.eglTerminate(eglDisplay);
            }
            eglDisplay = EGL14.EGL_NO_DISPLAY;
            eglContext = EGL14.EGL_NO_CONTEXT;
            eglConfig = null;
        }

        public static final int ENCODER_SEND_CANCEL = 0;
        public static final int ENCODER_SEND_SEND = 1;
        public static final int ENCODER_SEND_PLAYER = 2;

        private boolean sentMedia;
        private volatile boolean startupFailed;

        private void handleStartRecordingError() {
            encoderPrepared = false;

            startupFailed = true;
            running = false;
            if (audioRecorder != null) {
                try {
                    audioRecorder.stop();
                } catch (Exception e) {
                    FileLog.e(e);
                }
                try {
                    audioRecorder.release();
                } catch (Exception e) {
                    FileLog.e(e);
                }
                audioRecorder = null;
            }
            AndroidUtilities.runOnUIThread(() -> {
                if (InstantCameraView.this.videoEncoder != this) return;
                recording = false;
                MediaController.getInstance().requestRecordAudioFocus(false);
                NotificationCenter.getInstance(currentAccount).postNotificationName(NotificationCenter.recordStartError, recordingGuid);
                startAnimation(false, false);
            });
            handleStopRecording(ENCODER_SEND_CANCEL, null);
        }

        private void releaseEncoder(MediaCodec codec) {
            if (codec == null) return;
            try {
                codec.stop();
            } catch (Exception e) {
                FileLog.e(e);
            }
            try {
                codec.release();
            } catch (Exception e) {
                FileLog.e(e);
            }
        }

        private void handleStopRecording(final int send, final SendOptions sendOptions) {
            encoderPrepared = false;

            setHeavyOperationsStopped(false);
            final boolean runDone;
            if (send == ENCODER_SEND_SEND && hasWrittenVideoSample
                    && (videoEditedInfo == null || !videoEditedInfo.needConvert())
                    && !delegate.isInScheduleMode()) {
                runDone = false;
                if (!sentMedia) {
                    sentMedia = true;
                    AndroidUtilities.runOnUIThread(() -> {
                        videoEditedInfo = new VideoEditedInfo();
                        videoEditedInfo.startTime = -1;
                        videoEditedInfo.endTime = -1;
                        videoEditedInfo.estimatedSize = Math.max(1, size);
                        videoEditedInfo.roundVideo = true;
                        videoEditedInfo.file = file;
                        videoEditedInfo.encryptedFile = encryptedFile;
                        videoEditedInfo.key = key;
                        videoEditedInfo.iv = iv;
                        videoEditedInfo.framerate = frameRate;
                        videoEditedInfo.resultWidth = videoEditedInfo.originalWidth = videoWidth;
                        videoEditedInfo.resultHeight = videoEditedInfo.originalHeight = videoHeight;
                        videoEditedInfo.originalPath = videoFile.getAbsolutePath();
                        videoEditedInfo.notReadyYet = true;
                        videoEditedInfo.thumb = firstFrameThumb;
                        videoEditedInfo.estimatedDuration = recordedTime;
                        firstFrameThumb = null;
                        MediaController.PhotoEntry entry = new MediaController.PhotoEntry(0, 0, 0, videoFile.getAbsolutePath(), 0, true, 0, 0, 0);
                        if (sendOptions != null) {
                            entry.ttl = sendOptions.ttl;
                            entry.effectId = sendOptions.effectId;
                        }
                        delegate.sendMedia(entry, videoEditedInfo, sendOptions == null || sendOptions.notify, sendOptions != null ? sendOptions.scheduleDate : 0, sendOptions != null ? sendOptions.scheduleRepeatPeriod : 0, false, sendOptions != null ? sendOptions.stars : 0);
                    });
                }
            } else {
                runDone = true;
            }
            if (running && !pauseRecorder) {
                FileLog.d("InstantCamera handleStopRecording running=false");
                sendWhenDone = send;
                sendWhenDoneOptions = sendOptions;
                running = false;
                return;
            }
            try {
                FileLog.d("InstantCamera handleStopRecording drain encoders");
                if (!startupFailed) drainEncoder(true);
            } catch (Exception e) {
                FileLog.e(e);
            }
            releaseEncoder(videoEncoder);
            videoEncoder = null;
            releaseEncoder(audioEncoder);
            audioEncoder = null;
            try {
                setBluetoothScoOn(false);
            } catch (Exception e) {
                FileLog.e(e);
            }
            if (previewFile != null) {
                previewFile.delete();
                previewFile = null;
            }
            if (mediaMuxer != null) {
                if (WRITE_TO_FILE_IN_BACKGROUND) {
                    CountDownLatch countDownLatch = new CountDownLatch(1);
                    fileWriteQueue.postRunnable(() -> {
                        try {
                            mediaMuxer.finishMovie();
                            movieFinalized = true;
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                        countDownLatch.countDown();
                    });
                    try {
                        countDownLatch.await();
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                } else {
                    try {
                        mediaMuxer.finishMovie();
                        movieFinalized = true;
                    } catch (Exception e) {
                        FileLog.e(e);
                    }
                }
                FileLog.d("InstantCamera handleStopRecording finish muxer");
                if (writingToDifferentFile) {
                    if (videoFile.exists()) {
                        try {
                            videoFile.delete();
                        } catch (Exception e) {
                            FileLog.e("InstantCamera copying fileToWrite to videoFile, deleting videoFile error " + videoFile);
                            FileLog.e(e);
                        }
                    }
                    if (!fileToWrite.renameTo(videoFile)) {
                        FileLog.e("InstantCamera unable to rename file, try move file");
                        try {
                            AndroidUtilities.copyFile(fileToWrite, videoFile);
                            fileToWrite.delete();
                        } catch (IOException e) {
                            FileLog.e(e);
                            FileLog.e("InstantCamera unable to move file");
                        }
                    }
                }
            }
            if (send != 2) {
                if (generateKeyframeThumbsQueue != null) {
                    generateKeyframeThumbsQueue.cleanupQueue();
                    generateKeyframeThumbsQueue.recycle();
                    generateKeyframeThumbsQueue = null;
                }
            }
            FileLog.d("InstantCamera handleStopRecording send " + send);
            final boolean validVideoOutput = videoTrackIndex >= 0 && hasWrittenVideoSample
                    && movieFinalized && videoFile != null && videoFile.exists() && videoFile.length() > 0;
            if (send == ENCODER_SEND_CANCEL || !validVideoOutput) {
                if (videoFile != null) {
                    FileLoader.getInstance(currentAccount).cancelFileUpload(videoFile.getAbsolutePath(), false);
                }
                try {
                    if (fileToWrite != null) {
                        fileToWrite.delete();
                    }
                } catch (Throwable ignore) {}
                try {
                    if (videoFile != null) {
                        videoFile.delete();
                    }
                } catch (Throwable ignore) {}
                if (send != ENCODER_SEND_CANCEL) {
                    FileLog.e("InstantCamera: refusing to send a round video without a valid video track");
                    AndroidUtilities.runOnUIThread(() -> {
                        NotificationCenter.getInstance(currentAccount).postNotificationName(
                                NotificationCenter.recordStartError, recordingGuid);
                        startAnimation(false, false);
                    });
                }
            } else {
                if (runDone && (send != ENCODER_SEND_SEND || !sentMedia)) {
                    sentMedia = true;
                    AndroidUtilities.runOnUIThread(() -> {
                        if (videoEditedInfo == null) {
                            videoEditedInfo = new VideoEditedInfo();
                            videoEditedInfo.startTime = -1;
                            videoEditedInfo.endTime = -1;
                        }
                        if (videoEditedInfo.needConvert()) {
                            file = null;
                            encryptedFile = null;
                            key = null;
                            iv = null;
                            double totalDuration = videoEditedInfo.estimatedDuration;
                            long startTime = videoEditedInfo.startTime >= 0 ? videoEditedInfo.startTime : 0;
                            long endTime = videoEditedInfo.endTime >= 0 ? videoEditedInfo.endTime : videoEditedInfo.estimatedDuration;
                            videoEditedInfo.estimatedDuration = endTime - startTime;
                            videoEditedInfo.estimatedSize = Math.max(1, (long) (size * (videoEditedInfo.estimatedDuration / totalDuration)));
                            videoEditedInfo.bitrate = recordedVideoBitrate;
                            if (videoEditedInfo.startTime > 0) {
                                videoEditedInfo.startTime *= 1000;
                            }
                            if (videoEditedInfo.endTime > 0) {
                                videoEditedInfo.endTime *= 1000;
                            }
                            FileLoader.getInstance(currentAccount).cancelFileUpload(cameraFile.getAbsolutePath(), false);
                        } else {
                            videoEditedInfo.estimatedSize = Math.max(1, size);
                        }
                        videoEditedInfo.roundVideo = true;
                        videoEditedInfo.file = file;
                        videoEditedInfo.encryptedFile = encryptedFile;
                        videoEditedInfo.key = key;
                        videoEditedInfo.iv = iv;
                        videoEditedInfo.framerate = frameRate;
                        videoEditedInfo.resultWidth = videoEditedInfo.originalWidth = videoWidth;
                        videoEditedInfo.resultHeight = videoEditedInfo.originalHeight = videoHeight;
                        videoEditedInfo.originalPath = videoFile.getAbsolutePath();
                        final VideoEditedInfo info = videoEditedInfo;
                        if (send == ENCODER_SEND_SEND) {
                            if (delegate.isInScheduleMode()) {
                                AlertsCreator.createScheduleDatePickerDialog(delegate.getParentActivity(), delegate.getDialogId(), (notify, scheduleDate, scheduleRepeatPeriod) -> {
                                    MediaController.PhotoEntry entry = new MediaController.PhotoEntry(0, 0, 0, videoFile.getAbsolutePath(), 0, true, 0, 0, 0);
                                    if (sendOptions != null) {
                                        entry.ttl = sendOptions.ttl;
                                        entry.effectId = sendOptions.effectId;
                                    }
                                    delegate.sendMedia(entry, info, notify || sendOptions == null || sendOptions.notify, scheduleDate != 0 ? scheduleDate : sendOptions != null ? sendOptions.scheduleDate : 0, scheduleRepeatPeriod != 0 ? scheduleRepeatPeriod : sendOptions != null ? sendOptions.scheduleRepeatPeriod : 0, false, sendOptions != null ? sendOptions.stars : 0);
                                    startAnimation(false, false);
                                }, () -> {
                                    startAnimation(false, false);
                                }, resourcesProvider);
                            } else {
                                MediaController.PhotoEntry entry = new MediaController.PhotoEntry(0, 0, 0, videoFile.getAbsolutePath(), 0, true, 0, 0, 0);
                                if (sendOptions != null) {
                                    entry.ttl = sendOptions.ttl;
                                    entry.effectId = sendOptions.effectId;
                                }
                                delegate.sendMedia(entry, info, sendOptions == null || sendOptions.notify, sendOptions != null ? sendOptions.scheduleDate : 0, sendOptions != null ? sendOptions.scheduleRepeatPeriod : 0, false, sendOptions != null ? sendOptions.stars : 0);
                            }
                            videoEditedInfo = null;
                        } else {
                            setupVideoPlayer(videoFile);
                            info.estimatedDuration = recordedTime;
                            NotificationCenter.getInstance(currentAccount).postNotificationName(NotificationCenter.audioDidSent, recordingGuid, info, videoFile.getAbsolutePath(), keyframeThumbs);
                        }
                    });
                }
                AndroidUtilities.runOnUIThread(() -> {
                    if (sentMedia && videoEditedInfo != null) {
                        videoEditedInfo.notReadyYet = false;
                    }
                    if (send != ENCODER_SEND_CANCEL && validVideoOutput) {
                        didWriteData(videoFile, 0, true);
                    }
                    MediaController.getInstance().requestRecordAudioFocus(false);
                });
            }
            if (overlayHelper != null) {
                try {
                    overlayHelper.destroy();
                } catch (Exception e) {
                    FileLog.e(e);
                }
                overlayHelper = null;
            }
            if (eglDisplay != null && eglDisplay != EGL14.EGL_NO_DISPLAY
                    && eglSurface != null && eglSurface != EGL14.EGL_NO_SURFACE) {
                if (eglContext != null && eglContext.equals(EGL14.eglGetCurrentContext())) {
                    rearCrossfade.release();
                }
                EGL14.eglDestroySurface(eglDisplay, eglSurface);
            }
            eglSurface = EGL14.EGL_NO_SURFACE;
            if (surface != null) {
                surface.release();
                surface = null;
            }
            if (eglDisplay != null && eglDisplay != EGL14.EGL_NO_DISPLAY) {
                EGL14.eglMakeCurrent(eglDisplay, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT);
                EGL14.eglDestroyContext(eglDisplay, eglContext);
                EGL14.eglReleaseThread();
                EGL14.eglTerminate(eglDisplay);
            }
            eglDisplay = EGL14.EGL_NO_DISPLAY;
            eglContext = EGL14.EGL_NO_CONTEXT;
            eglConfig = null;
            handler.exit();
            AndroidUtilities.runOnUIThread(() -> {
                if (InstantCameraView.this.videoEncoder == this) {
                    InstantCameraView.this.videoEncoder = null;
                }
            });
        }

        private void setBluetoothScoOn(boolean scoOn) {
            AudioManager am = (AudioManager) ApplicationLoader.applicationContext.getSystemService(Context.AUDIO_SERVICE);
            if (SharedConfig.recordViaSco && !PermissionRequest.hasPermission(Manifest.permission.BLUETOOTH_CONNECT)) {
                SharedConfig.recordViaSco = false;
                SharedConfig.saveConfig();
            }
            if (am.isBluetoothScoAvailableOffCall() && SharedConfig.recordViaSco || !scoOn) {
                BluetoothAdapter btAdapter = BluetoothAdapter.getDefaultAdapter();
                try {
                    if (btAdapter != null && btAdapter.getProfileConnectionState(BluetoothProfile.HEADSET) == BluetoothProfile.STATE_CONNECTED || !scoOn) {
                        if (scoOn && !am.isBluetoothScoOn()) {
                            am.startBluetoothSco();
                        } else if (!scoOn && am.isBluetoothScoOn()) {
                            am.stopBluetoothSco();
                        }
                    }
                } catch (SecurityException ignored) {
                } catch (Throwable e) {
                    FileLog.e(e);
                    try {
                        if (!scoOn && am.isBluetoothScoOn()) {
                            am.stopBluetoothSco();
                        }
                    } catch (Exception e2) {
                        FileLog.e(e2);
                    }
                }
            }
        }

        private AudioRecord createStartedAudioRecorder(int configuredSource, int bufferSize) {
            if (!com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getNewCameraAudio()) {
                AudioRecord recorder = com.th3nekit.finegram.camera.VoiceAudioCapture.createCameraStarted(
                        ApplicationLoader.applicationContext, audioSampleRate, bufferSize, false);
                audioInputChannels = 1;
                return recorder;
            }
            int requestedChannels = audioInputChannels;
            for (int channels = requestedChannels; channels >= 1; channels--) {
                if (audioChannelsLocked && channels != requestedChannels) break;
                AudioRecord recorder = createStartedAudioRecorder(configuredSource, bufferSize, channels);
                if (recorder != null) {
                    audioInputChannels = channels;
                    return recorder;
                }
            }
            return null;
        }

        private AudioRecord createStartedAudioRecorder(int configuredSource, int bufferSize, int channels) {
            int[] sources = {configuredSource, MediaRecorder.AudioSource.CAMCORDER,
                    MediaRecorder.AudioSource.MIC, MediaRecorder.AudioSource.DEFAULT};
            for (int i = 0; i < sources.length; i++) {
                int source = sources[i];
                boolean duplicate = false;
                for (int j = 0; j < i; j++) if (sources[j] == source) { duplicate = true; break; }
                if (duplicate) continue;
                AudioRecord candidate = null;
                try {
                    int channelMask = channels == 2 ? AudioFormat.CHANNEL_IN_STEREO : AudioFormat.CHANNEL_IN_MONO;
                    int minimum = AudioRecord.getMinBufferSize(audioSampleRate, channelMask, AudioFormat.ENCODING_PCM_16BIT);
                    if (minimum <= 0) continue;
                    candidate = new AudioRecord(source, audioSampleRate,
                            channelMask, AudioFormat.ENCODING_PCM_16BIT, Math.max(bufferSize, minimum));
                    if (candidate.getState() != AudioRecord.STATE_INITIALIZED) {
                        candidate.release();
                        continue;
                    }
                    try {
                        AudioManager audioManager = (AudioManager) ApplicationLoader.applicationContext
                                .getSystemService(Context.AUDIO_SERVICE);
                        if (audioManager != null) {
                            for (android.media.AudioDeviceInfo device : audioManager.getDevices(AudioManager.GET_DEVICES_INPUTS)) {
                                if (device.getType() == android.media.AudioDeviceInfo.TYPE_BUILTIN_MIC) {
                                    candidate.setPreferredDevice(device);
                                    break;
                                }
                            }
                        }
                    } catch (Throwable ignored) {}
                    if (wideMicrophoneCapture) {
                        com.th3nekit.finegram.camera.CameraXAudioCapture.requestWideCapture(candidate);
                    }
                    candidate.startRecording();
                    if (candidate.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING) {
                        if (source != configuredSource) {
                            FileLog.d("InstantCamera: audio source " + configuredSource
                                    + " unavailable, using " + source);
                        }
                        return candidate;
                    }
                } catch (Throwable t) {
                    FileLog.e("InstantCamera: AudioRecord source " + source + " failed", t);
                }
                if (candidate != null) {
                    try { candidate.release(); } catch (Throwable ignored) {}
                }
            }
            return null;
        }

        private MediaFormat createVideoEncoderFormat(boolean highProfile) {
            MediaFormat format = MediaFormat.createVideoFormat(
                    VIDEO_MIME_TYPE, videoWidth, videoHeight);
            format.setInteger(
                    MediaFormat.KEY_COLOR_FORMAT,
                    MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
            format.setInteger(MediaFormat.KEY_BIT_RATE, videoBitrate);
            format.setInteger(MediaFormat.KEY_FRAME_RATE, frameRate);
            format.setInteger(
                    MediaFormat.KEY_I_FRAME_INTERVAL, IFRAME_INTERVAL);
            if (highProfile) {

                format.setInteger(
                        MediaFormat.KEY_PROFILE,
                        MediaCodecInfo.CodecProfileLevel.AVCProfileHigh);
            }
            return format;
        }

        private boolean supportsHighProfile(MediaCodec encoder, MediaFormat format) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N || encoder == null) {
                return false;
            }
            try {
                MediaCodecInfo.CodecCapabilities capabilities =
                        encoder.getCodecInfo().getCapabilitiesForType(VIDEO_MIME_TYPE);
                boolean advertised = false;
                if (capabilities.profileLevels != null) {
                    for (MediaCodecInfo.CodecProfileLevel profileLevel
                            : capabilities.profileLevels) {
                        if (profileLevel != null
                                && profileLevel.profile
                                == MediaCodecInfo.CodecProfileLevel.AVCProfileHigh) {
                            advertised = true;
                            break;
                        }
                    }
                }
                return advertised && capabilities.isFormatSupported(format);
            } catch (Throwable error) {
                FileLog.e("InstantCamera: unable to query AVC High Profile", error);
                return false;
            }
        }

        private boolean supportsConfiguredFrameRate(MediaCodec encoder) {
            if (encoder == null || frameRate <= DEFAULT_FRAME_RATE) {
                return true;
            }
            try {
                MediaCodecInfo.CodecCapabilities capabilities =
                        encoder.getCodecInfo().getCapabilitiesForType(VIDEO_MIME_TYPE);
                MediaCodecInfo.VideoCapabilities videoCapabilities =
                        capabilities.getVideoCapabilities();
                return videoCapabilities == null
                        || videoCapabilities.areSizeAndRateSupported(
                                videoWidth, videoHeight, frameRate);
            } catch (Throwable error) {

                FileLog.e("InstantCamera: unable to query AVC frame-rate support", error);
                return true;
            }
        }

        private MediaCodec createConfiguredVideoEncoder() throws Exception {
            MediaCodec candidate = MediaCodec.createEncoderByType(VIDEO_MIME_TYPE);
            if (!supportsConfiguredFrameRate(candidate)) {
                FileLog.d("InstantCamera: AVC encoder does not advertise "
                        + videoWidth + "x" + videoHeight + "@" + frameRate
                        + ", using " + DEFAULT_FRAME_RATE + " fps");
                frameRate = DEFAULT_FRAME_RATE;
            }
            MediaFormat highFormat = createVideoEncoderFormat(true);
            boolean useHighProfile = supportsHighProfile(candidate, highFormat);
            MediaFormat selectedFormat =
                    useHighProfile ? highFormat : createVideoEncoderFormat(false);
            try {
                candidate.configure(
                        selectedFormat, null, null,
                        MediaCodec.CONFIGURE_FLAG_ENCODE);
                return candidate;
            } catch (Exception preferredError) {
                try {
                    candidate.release();
                } catch (Throwable ignored) {
                }
                if (!useHighProfile) {
                    throw preferredError;
                }
                FileLog.e(
                        "InstantCamera: AVC High Profile rejected, retrying platform profile",
                        preferredError);
                candidate = MediaCodec.createEncoderByType(VIDEO_MIME_TYPE);
                try {
                    candidate.configure(
                            createVideoEncoderFormat(false), null, null,
                            MediaCodec.CONFIGURE_FLAG_ENCODE);
                    return candidate;
                } catch (Exception fallbackError) {
                    try {
                        candidate.release();
                    } catch (Throwable ignored) {
                    }
                    fallbackError.addSuppressed(preferredError);
                    throw fallbackError;
                }
            }
        }

        private void startConfiguredVideoEncoder() throws Exception {
            int requestedFrameRate = frameRate;
            try {
                videoEncoder = createConfiguredVideoEncoder();
                surface = videoEncoder.createInputSurface();
                videoEncoder.start();
            } catch (Exception preferredError) {
                if (surface != null) {
                    try {
                        surface.release();
                    } catch (Throwable ignored) {
                    }
                    surface = null;
                }
                if (videoEncoder != null) {
                    try {
                        videoEncoder.release();
                    } catch (Throwable ignored) {
                    }
                    videoEncoder = null;
                }
                if (requestedFrameRate <= DEFAULT_FRAME_RATE) {
                    throw preferredError;
                }

                FileLog.e("InstantCamera: AVC " + requestedFrameRate
                        + " fps start failed, retrying " + DEFAULT_FRAME_RATE,
                        preferredError);
                frameRate = DEFAULT_FRAME_RATE;
                try {
                    videoEncoder = createConfiguredVideoEncoder();
                    surface = videoEncoder.createInputSurface();
                    videoEncoder.start();
                } catch (Exception fallbackError) {
                    fallbackError.addSuppressed(preferredError);
                    throw fallbackError;
                }
            }
        }

        private void configureAudioEncoder() throws Exception {
            try {
                startAudioEncoder();
            } catch (Exception error) {
                if (audioEncoder != null) {
                    try { audioEncoder.release(); } catch (Exception ignored) {}
                    audioEncoder = null;
                }
                throw error;
            }
        }

        private void startAudioEncoder() throws Exception {
            MediaFormat audioFormat = new MediaFormat();
            audioFormat.setString(MediaFormat.KEY_MIME, AUDIO_MIME_TYPE);
            audioFormat.setInteger(MediaFormat.KEY_SAMPLE_RATE, audioSampleRate);
            audioFormat.setInteger(MediaFormat.KEY_CHANNEL_COUNT, audioOutputChannels);
            audioFormat.setInteger(MediaFormat.KEY_BIT_RATE, MessagesController.getInstance(currentAccount).roundAudioBitrate * 1024 * audioOutputChannels);
            audioFormat.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, 2048 * AudioBufferInfo.MAX_SAMPLES);
            audioEncoder = MediaCodec.createEncoderByType(AUDIO_MIME_TYPE);
            audioEncoder.configure(audioFormat, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            audioEncoder.start();
        }

        private void prepareEncoder(boolean fromPause) {
            setBluetoothScoOn(true);

            try {
                if (!fromPause) {
                    wideMicrophoneCapture = useCameraX && (com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE.getNewCameraAudio() && com.th3nekit.finegram.core.configs.FinegramChatsConfig.INSTANCE.getRecordMultiMic());
                    audioInputChannels = wideMicrophoneCapture ? 2 : 1;
                    audioChannelsLocked = false;
                }
                int recordBufferSize = AudioRecord.getMinBufferSize(audioSampleRate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
                if (recordBufferSize <= 0) {
                    recordBufferSize = 3584;
                }
                int bufferSize = 2048 * 24;
                if (bufferSize < recordBufferSize) {
                    bufferSize = ((recordBufferSize / 2048) + 1) * 2048 * 2;
                }
                buffers.clear();
                for (int a = 0; a < 3; a++) {
                    buffers.add(new AudioBufferInfo());
                }

                if (fromPause) {
                    prevVideoLast = videoLast + videoLastDt;
                    prevAudioLast = audioLast + audioLastDt;
                    firstVideoFrameSincePause = true;
                } else {
                    prevVideoLast = -1;
                    prevAudioLast = -1;
                    currentTimestamp = 0;
                }
                lastTimestamp = -1;
                lastCommittedFrameRealtimeNanos = 0;
                audioStartTime = -1;
                audioFirst = -1;
                videoFirst = -1;
                videoLast = -1;
                videoDiff = -1;
                audioLast = -1;
                audioDiff = -1;
                skippedFirst = false;
                skippedTime = 0;

                audioRecorder = createStartedAudioRecorder(
                        wideMicrophoneCapture ? MediaRecorder.AudioSource.CAMCORDER
                                : com.th3nekit.finegram.chats.AudioEnhance.INSTANCE.getAudioSource(), bufferSize);
                if (audioRecorder == null) {
                    throw new IllegalStateException("No usable AudioRecord source");
                }
                if (BuildVars.LOGS_ENABLED) {
                    FileLog.d("InstantCamera initied audio record with channels " + audioRecorder.getChannelCount() + " sample rate = " + audioRecorder.getSampleRate() + " bufferSize = " + bufferSize);
                }
                pauseRecorder = false;

                audioBufferInfo = new MediaCodec.BufferInfo();
                videoBufferInfo = new MediaCodec.BufferInfo();

                configureAudioEncoder();
                audioChannelsLocked = true;

                firstEncode = true;
                startConfiguredVideoEncoder();

                if (!fromPause) {
                    boolean isSdCard = ImageLoader.isSdCardPath(videoFile);
                    fileToWrite = videoFile;
                    if (isSdCard) {
                        try {
                            fileToWrite = new File(ApplicationLoader.getFilesDirFixed(), "camera_tmp.mp4");
                            if (fileToWrite.exists()) {
                                fileToWrite.delete();
                            }
                            writingToDifferentFile = true;
                        } catch (Throwable e) {
                            FileLog.e(e);
                            fileToWrite = videoFile;
                            writingToDifferentFile = false;
                        }
                    }
                    Mp4Movie movie = new Mp4Movie();
                    movie.setCacheFile(fileToWrite);
                    movie.setRotation(0);
                    movie.setSize(videoWidth, videoHeight);
                    mediaMuxer = new MP4Builder().createMovie(movie, isSecretChat, false);
                    mediaMuxer.setAllowSyncFiles(allowSendingWhileRecording = SharedConfig.deviceIsHigh());
                }

            } catch (Exception ioe) {
                throw new RuntimeException(ioe);
            }

            if (eglDisplay != EGL14.EGL_NO_DISPLAY) {
                throw new RuntimeException("EGL already set up");
            }

            eglDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
            if (eglDisplay == EGL14.EGL_NO_DISPLAY) {
                throw new RuntimeException("unable to get EGL14 display");
            }
            int[] version = new int[2];
            if (!EGL14.eglInitialize(eglDisplay, version, 0, version, 1)) {
                eglDisplay = null;
                throw new RuntimeException("unable to initialize EGL14");
            }

            if (eglContext == EGL14.EGL_NO_CONTEXT) {
                int renderableType = EGL14.EGL_OPENGL_ES2_BIT;

                int[] attribList = {
                        EGL14.EGL_RED_SIZE, 8,
                        EGL14.EGL_GREEN_SIZE, 8,
                        EGL14.EGL_BLUE_SIZE, 8,
                        EGL14.EGL_ALPHA_SIZE, 8,
                        EGL14.EGL_RENDERABLE_TYPE, renderableType,
                        0x3142, 1,
                        EGL14.EGL_NONE
                };
                android.opengl.EGLConfig[] configs = new android.opengl.EGLConfig[1];
                int[] numConfigs = new int[1];
                if (!EGL14.eglChooseConfig(eglDisplay, attribList, 0, configs, 0, configs.length, numConfigs, 0)) {
                    throw new RuntimeException("Unable to find a suitable EGLConfig");
                }

                int[] attrib2_list = {
                        EGL14.EGL_CONTEXT_CLIENT_VERSION, 2,
                        EGL14.EGL_NONE
                };
                eglContext = EGL14.eglCreateContext(eglDisplay, configs[0], sharedEglContext, attrib2_list, 0);
                eglConfig = configs[0];
            }

            int[] values = new int[1];
            EGL14.eglQueryContext(eglDisplay, eglContext, EGL14.EGL_CONTEXT_CLIENT_VERSION, values, 0);

            if (eglSurface != EGL14.EGL_NO_SURFACE) {
                throw new IllegalStateException("surface already created");
            }

            int[] surfaceAttribs = {
                    EGL14.EGL_NONE
            };
            eglSurface = EGL14.eglCreateWindowSurface(eglDisplay, eglConfig, surface, surfaceAttribs, 0);
            if (eglSurface == null) {
                throw new RuntimeException("surface was null");
            }

            if (!EGL14.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
                if (BuildVars.LOGS_ENABLED) {
                    FileLog.e("eglMakeCurrent failed " + GLUtils.getEGLErrorString(EGL14.eglGetError()));
                }
                throw new RuntimeException("eglMakeCurrent failed");
            }
            GLES20.glBlendFunc(GLES20.GL_SRC_ALPHA, GLES20.GL_ONE_MINUS_SRC_ALPHA);

            if (overlayHelper != null) {
                overlayHelper.destroy();
                overlayHelper = null;
            }
            overlayHelper = new InstantCameraVideoEncoderOverlayHelper(videoWidth, videoHeight);

            String vertexShaderSource, fragmentShaderSource;
            if (overlayHelper != null) {
                vertexShaderSource = VERTEX_SHADER;
                fragmentShaderSource = createFragmentShaderV2(previewSize[0], useCameraX);
            } else if (useCameraX || useCamera2) {
                vertexShaderSource = AndroidUtilities.readRes(R.raw.instant_lanczos_vert);
                fragmentShaderSource = AndroidUtilities.readRes(R.raw.instant_lanczos_frag_oes);
            } else {
                vertexShaderSource = VERTEX_SHADER;
                fragmentShaderSource = createFragmentShader(previewSize[0]);
            }
            int vertexShader = loadShader(GLES20.GL_VERTEX_SHADER, vertexShaderSource);
            int fragmentShader = loadShader(GLES20.GL_FRAGMENT_SHADER, fragmentShaderSource);
            if (vertexShader != 0 && fragmentShader != 0) {
                drawProgram = GLES20.glCreateProgram();
                GLES20.glAttachShader(drawProgram, vertexShader);
                GLES20.glAttachShader(drawProgram, fragmentShader);
                GLES20.glLinkProgram(drawProgram);
                int[] linkStatus = new int[1];
                GLES20.glGetProgramiv(drawProgram, GLES20.GL_LINK_STATUS, linkStatus, 0);
                if (linkStatus[0] == 0) {
                    GLES20.glDeleteProgram(drawProgram);
                    drawProgram = 0;
                } else {
                    positionHandle = GLES20.glGetAttribLocation(drawProgram, "aPosition");
                    textureHandle = GLES20.glGetAttribLocation(drawProgram, "aTextureCoord");
                    previewSizeHandle = GLES20.glGetUniformLocation(drawProgram, "preview");
                    resolutionHandle = GLES20.glGetUniformLocation(drawProgram, "resolution");
                    alphaHandle = GLES20.glGetUniformLocation(drawProgram, "alpha");
                    vertexMatrixHandle = GLES20.glGetUniformLocation(drawProgram, "uMVPMatrix");
                    textureMatrixHandle = GLES20.glGetUniformLocation(drawProgram, "uSTMatrix");
                    texelSizeHandle = GLES20.glGetUniformLocation(drawProgram, "texelSize");
                    switchBlurHandle = GLES20.glGetUniformLocation(drawProgram, "switchBlur");
                }
            }
            if (useCameraX) {
                int snapshotVertexShader = loadShader(GLES20.GL_VERTEX_SHADER, VERTEX_SHADER);
                int snapshotFragmentShader = loadShader(GLES20.GL_FRAGMENT_SHADER,
                        FRAGMENT_SNAPSHOT_SHADER);
                if (snapshotVertexShader != 0 && snapshotFragmentShader != 0) {
                    snapshotProgram = GLES20.glCreateProgram();
                    GLES20.glAttachShader(snapshotProgram, snapshotVertexShader);
                    GLES20.glAttachShader(snapshotProgram, snapshotFragmentShader);
                    GLES20.glLinkProgram(snapshotProgram);
                    int[] linkStatus = new int[1];
                    GLES20.glGetProgramiv(snapshotProgram, GLES20.GL_LINK_STATUS, linkStatus, 0);
                    if (linkStatus[0] == 0) {
                        GLES20.glDeleteProgram(snapshotProgram);
                        snapshotProgram = 0;
                    } else {
                        snapshotPositionHandle = GLES20.glGetAttribLocation(snapshotProgram, "aPosition");
                        snapshotTextureHandle = GLES20.glGetAttribLocation(snapshotProgram, "aTextureCoord");
                        snapshotVertexMatrixHandle = GLES20.glGetUniformLocation(snapshotProgram, "uMVPMatrix");
                        snapshotTextureMatrixHandle = GLES20.glGetUniformLocation(snapshotProgram, "uSTMatrix");
                        snapshotAlphaHandle = GLES20.glGetUniformLocation(snapshotProgram, "alpha");
                        snapshotTexelSizeHandle = GLES20.glGetUniformLocation(snapshotProgram, "texelSize");
                        snapshotSwitchBlurHandle = GLES20.glGetUniformLocation(snapshotProgram, "switchBlur");
                    }
                }
                if (snapshotProgram == 0) {
                    throw new IllegalStateException("Unable to create CameraX snapshot encoder shader");
                }
            }

            Thread thread = new Thread(recorderRunnable);
            thread.setPriority(Thread.MAX_PRIORITY);
            AndroidUtilities.runOnUIThread(() -> {
                if (cancelled || startupFailed || InstantCameraView.this.videoEncoder != this) return;
                if (!com.th3nekit.finegram.core.configs.FinegramChatsConfig.INSTANCE.getDisableVibration()) {
                    try {
                        performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP, HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING);
                    } catch (Exception ignore) {}
                }
                AndroidUtilities.lockOrientation(delegate.getParentActivity());
                recordPlusTime = fromPause ? recordedTime : 0;
                recordStartTime = SystemClock.elapsedRealtime();
                recording = true;
                legacyZoom = 0f;
                updateFlash();
                invalidate();
                NotificationCenter.getInstance(currentAccount).postNotificationName(NotificationCenter.recordStarted, recordingGuid, false);
            });

            thread.start();
            encoderPrepared = true;
        }

        public Surface getInputSurface() {
            return surface;
        }

        private void didWriteData(File file, long availableSize, boolean last) {
            if (videoConvertFirstWrite) {
                FileLoader.getInstance(currentAccount).uploadFile(file.toString(), isSecretChat, false, 1, ConnectionsManager.FileTypeVideo, false);
                videoConvertFirstWrite = false;
                if (last) {
                    FileLoader.getInstance(currentAccount).checkUploadNewDataAvailable(file.toString(), isSecretChat, availableSize, last ? file.length() : 0);
                }
            } else {
                FileLoader.getInstance(currentAccount).checkUploadNewDataAvailable(file.toString(), isSecretChat, availableSize, last ? file.length() : 0);
            }
        }

        public void drainEncoder(boolean endOfStream) throws Exception {
            if (endOfStream) {
                videoEncoder.signalEndOfInputStream();
            }

            ByteBuffer[] encoderOutputBuffers = null;
            while (true) {

                long dequeueTimeoutUs = endOfStream || frameRate <= DEFAULT_FRAME_RATE
                        ? 10000L : 0L;
                int encoderStatus = videoEncoder.dequeueOutputBuffer(
                        videoBufferInfo, dequeueTimeoutUs);
                if (encoderStatus == MediaCodec.INFO_TRY_AGAIN_LATER) {
                    if (!endOfStream || pauseRecorder) {
                        break;
                    }
                } else if (encoderStatus == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                } else if (encoderStatus == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                    MediaFormat newFormat = videoEncoder.getOutputFormat();
                    if (videoTrackIndex == -5) {
                        videoTrackIndex = mediaMuxer.addTrack(newFormat, false);
                        if (newFormat.containsKey(MediaFormat.KEY_PREPEND_HEADER_TO_SYNC_FRAMES) && newFormat.getInteger(MediaFormat.KEY_PREPEND_HEADER_TO_SYNC_FRAMES) == 1) {
                            ByteBuffer spsBuff = newFormat.getByteBuffer("csd-0");
                            ByteBuffer ppsBuff = newFormat.getByteBuffer("csd-1");
                            prependHeaderSize = (spsBuff != null ? spsBuff.remaining() : 0)
                                    + (ppsBuff != null ? ppsBuff.remaining() : 0);
                        }
                    }
                } else if (encoderStatus >= 0) {
                    ByteBuffer encodedData;
                    encodedData = videoEncoder.getOutputBuffer(encoderStatus);
                    if (encodedData == null) {
                        throw new RuntimeException("encoderOutputBuffer " + encoderStatus + " was null");
                    }
                    if (videoBufferInfo.size > 1) {
                        if ((videoBufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) == 0) {
                            if (prependHeaderSize != 0 && (videoBufferInfo.flags & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0) {
                                videoBufferInfo.offset += prependHeaderSize;
                                videoBufferInfo.size -= prependHeaderSize;
                            }
                            if (firstEncode && (videoBufferInfo.flags & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0) {
                                if (videoBufferInfo.size > 100) {
                                    encodedData.position(videoBufferInfo.offset);
                                    byte[] temp = new byte[100];
                                    encodedData.get(temp);
                                    int nalCount = 0;
                                    for (int a = 0; a < temp.length - 4; a++) {
                                        if (temp[a] == 0 && temp[a + 1] == 0 && temp[a + 2] == 0 && temp[a + 3] == 1) {
                                            nalCount++;
                                            if (nalCount > 1) {
                                                videoBufferInfo.offset += a;
                                                videoBufferInfo.size -= a;
                                                break;
                                            }
                                        }
                                    }
                                }
                                firstEncode = false;
                            }
                            if (WRITE_TO_FILE_IN_BACKGROUND) {
                                MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();
                                bufferInfo.size = videoBufferInfo.size;
                                bufferInfo.offset = videoBufferInfo.offset;
                                bufferInfo.flags = videoBufferInfo.flags;
                                bufferInfo.presentationTimeUs = videoBufferInfo.presentationTimeUs;
                                ByteBuffer byteBuffer = AndroidUtilities.cloneByteBuffer(encodedData);
                                fileWriteQueue.postRunnable(() -> {
                                    long availableSize = 0;
                                    try {
                                        availableSize = mediaMuxer.writeSampleData(videoTrackIndex, byteBuffer, bufferInfo, true);
                                        hasWrittenVideoSample = true;
                                    } catch (Exception e) {
                                        e.printStackTrace();
                                    }
                                    if (availableSize != 0 && !writingToDifferentFile && allowSendingWhileRecording) {
                                        didWriteData(videoFile, availableSize, false);
                                    }
                                });
                            } else {
                                long availableSize = mediaMuxer.writeSampleData(videoTrackIndex, encodedData, videoBufferInfo, true);
                                hasWrittenVideoSample = true;
                                if (availableSize != 0 && !writingToDifferentFile && allowSendingWhileRecording) {
                                    didWriteData(videoFile, availableSize, false);
                                }
                            }
                        } else if (videoTrackIndex == -5) {
                            byte[] csd = new byte[videoBufferInfo.size];
                            encodedData.limit(videoBufferInfo.offset + videoBufferInfo.size);
                            encodedData.position(videoBufferInfo.offset);
                            encodedData.get(csd);
                            ByteBuffer sps = null;
                            ByteBuffer pps = null;
                            for (int a = videoBufferInfo.size - 1; a >= 0; a--) {
                                if (a > 3) {
                                    if (csd[a] == 1 && csd[a - 1] == 0 && csd[a - 2] == 0 && csd[a - 3] == 0) {
                                        sps = ByteBuffer.allocate(a - 3);
                                        pps = ByteBuffer.allocate(videoBufferInfo.size - (a - 3));
                                        sps.put(csd, 0, a - 3).position(0);
                                        pps.put(csd, a - 3, videoBufferInfo.size - (a - 3)).position(0);
                                        break;
                                    }
                                } else {
                                    break;
                                }
                            }

                            MediaFormat newFormat = MediaFormat.createVideoFormat("video/avc", videoWidth, videoHeight);
                            if (sps != null && pps != null) {
                                newFormat.setByteBuffer("csd-0", sps);
                                newFormat.setByteBuffer("csd-1", pps);
                            }
                            videoTrackIndex = mediaMuxer.addTrack(newFormat, false);
                        }
                    }
                    videoEncoder.releaseOutputBuffer(encoderStatus, false);
                    if ((videoBufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                        break;
                    }
                }
            }

            while (true) {
                int encoderStatus = audioEncoder.dequeueOutputBuffer(audioBufferInfo, 0);
                if (encoderStatus == MediaCodec.INFO_TRY_AGAIN_LATER) {
                    if (!endOfStream || !running && sendWhenDone == ENCODER_SEND_CANCEL || pauseRecorder) {
                        break;
                    }
                } else if (encoderStatus == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                } else if (encoderStatus == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                    MediaFormat newFormat = audioEncoder.getOutputFormat();
                    if (audioTrackIndex == -5) {
                        audioTrackIndex = mediaMuxer.addTrack(newFormat, true);
                    }
                } else if (encoderStatus >= 0) {
                    ByteBuffer encodedData = audioEncoder.getOutputBuffer(encoderStatus);
                    if (encodedData == null) {
                        throw new RuntimeException("encoderOutputBuffer " + encoderStatus + " was null");
                    }
                    if ((audioBufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
                        audioBufferInfo.size = 0;
                    }
                    if (audioBufferInfo.size != 0) {
                        if (WRITE_TO_FILE_IN_BACKGROUND) {
                            MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();
                            bufferInfo.size = audioBufferInfo.size;
                            bufferInfo.offset = audioBufferInfo.offset;
                            bufferInfo.flags = audioBufferInfo.flags;
                            bufferInfo.presentationTimeUs = audioBufferInfo.presentationTimeUs;
                            ByteBuffer byteBuffer = AndroidUtilities.cloneByteBuffer(encodedData);
                            fileWriteQueue.postRunnable(() -> {
                                long availableSize = 0;
                                try {
                                    availableSize = mediaMuxer.writeSampleData(audioTrackIndex, byteBuffer, bufferInfo, false);
                                } catch (Exception e) {
                                    e.printStackTrace();
                                }
                                if (availableSize != 0 && !writingToDifferentFile && allowSendingWhileRecording) {
                                    didWriteData(videoFile, availableSize, false);
                                }
                            });
                            if (audioEncoder != null) {
                                audioEncoder.releaseOutputBuffer(encoderStatus, false);
                            }
                        } else {
                            long availableSize = mediaMuxer.writeSampleData(audioTrackIndex, encodedData, audioBufferInfo, false);
                            if (availableSize != 0 && !writingToDifferentFile && allowSendingWhileRecording) {
                                didWriteData(videoFile, availableSize, false);
                            }
                            if (audioEncoder != null) {
                                audioEncoder.releaseOutputBuffer(encoderStatus, false);
                            }
                        }
                    } else if (audioEncoder != null) {
                        audioEncoder.releaseOutputBuffer(encoderStatus, false);
                    }
                    if ((audioBufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                        break;
                    }
                }
            }
        }

        @Override
        protected void finalize() throws Throwable {
            if (fileWriteQueue != null) {
                fileWriteQueue.recycle();
                fileWriteQueue = null;
            }
            if (overlayHelper != null) {
                overlayHelper.destroy();
                overlayHelper = null;
            }
            try {
                if (eglDisplay != EGL14.EGL_NO_DISPLAY) {
                    EGL14.eglMakeCurrent(eglDisplay, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT);
                    EGL14.eglDestroyContext(eglDisplay, eglContext);
                    EGL14.eglReleaseThread();
                    EGL14.eglTerminate(eglDisplay);
                    eglDisplay = EGL14.EGL_NO_DISPLAY;
                    eglContext = EGL14.EGL_NO_CONTEXT;
                    eglConfig = null;
                }
            } finally {
                super.finalize();
            }
        }
    }

    private String createFragmentShader(Size previewSize) {
        if (SharedConfig.deviceIsLow() || !allowBigSizeCamera() || previewSize != null && Math.max(previewSize.getHeight(), previewSize.getWidth()) * 0.7f < MessagesController.getInstance(currentAccount).roundVideoSize) {
            return "#extension GL_OES_EGL_image_external : require\n" +
                    "precision highp float;\n" +
                    "varying vec2 vTextureCoord;\n" +
                    "uniform float alpha;\n" +
                    "uniform vec2 preview;\n" +
                    "uniform vec2 resolution;\n" +
                    "uniform samplerExternalOES sTexture;\n" +
                    "void main() {\n" +
                    "   vec4 textColor = texture2D(sTexture, vTextureCoord);\n" +
                    "   vec2 coord = resolution * 0.5;\n" +
                    "   float radius = 0.51 * resolution.x;\n" +
                    "   float d = length(coord - gl_FragCoord.xy) - radius;\n" +
                    "   float t = clamp(d, 0.0, 1.0);\n" +
                    "   vec3 color = mix(textColor.rgb, vec3(1, 1, 1), t);\n" +
                    "   gl_FragColor = vec4(color * alpha, alpha);\n" +
                    "}\n";
        }

        return "#extension GL_OES_EGL_image_external : require\n" +
                "precision highp float;\n" +
                "varying vec2 vTextureCoord;\n" +
                "uniform vec2 resolution;\n" +
                "uniform vec2 preview;\n" +
                "uniform float alpha;\n" +

                "uniform samplerExternalOES sTexture;\n" +
                "void main() {\n" +
                "   vec2 coord = resolution * 0.5;\n" +
                "   float radius = 0.51 * resolution.x;\n" +
                "   float d = length(coord - gl_FragCoord.xy) - radius;\n" +
                "   float t = clamp(d, 0.0, 1.0);\n" +
                "   if (t == 0.0) {\n" +
                "       vec2 c_textureSize = preview;\n" +
                "       vec2 c_onePixel = (1.0 / c_textureSize);\n" +
                "       vec2 uv = vTextureCoord;\n" +
                "       vec2 pixel = uv * c_textureSize + 0.5;\n" +

                "       vec2 frac = fract(pixel);\n" +
                "       pixel = (floor(pixel) / c_textureSize) - vec2(c_onePixel);\n" +

                "       vec4 tl = texture2D(sTexture, pixel + vec2(0.0         , 0.0));\n" +
                "       vec4 tr = texture2D(sTexture, pixel + vec2(c_onePixel.x, 0.0));\n" +
                "       vec4 bl = texture2D(sTexture, pixel + vec2(0.0         , c_onePixel.y));\n" +
                "       vec4 br = texture2D(sTexture, pixel + vec2(c_onePixel.x, c_onePixel.y));\n" +

                "       vec4 x1 = mix(tl, tr, frac.x);\n" +
                "       vec4 x2 = mix(bl, br, frac.x);\n" +
                "       gl_FragColor = mix(x1, x2, frac.y) * alpha;" +
                "   } else {\n" +
                "       gl_FragColor = vec4(1, 1, 1, alpha);\n" +
                "   }\n" +
                "}\n";
    }

    private String createFragmentShaderV2(Size previewSize, boolean cameraXTransition) {
        if (SharedConfig.deviceIsLow() || !allowBigSizeCamera() || previewSize != null && Math.max(previewSize.getHeight(), previewSize.getWidth()) * 0.7f < MessagesController.getInstance(currentAccount).roundVideoSize) {
            if (!cameraXTransition) {
                return "#extension GL_OES_EGL_image_external : require\n" +
                        "precision highp float;\n" +
                        "varying vec2 vTextureCoord;\n" +
                        "uniform float alpha;\n" +
                        "uniform vec2 preview;\n" +
                        "uniform vec2 resolution;\n" +
                        "uniform samplerExternalOES sTexture;\n" +
                        "void main() {\n" +
                        "   vec4 textColor = texture2D(sTexture, vTextureCoord);\n" +
                        "   gl_FragColor = vec4(textColor.rgb * alpha, alpha);\n" +
                        "}\n";
            }
            return "#extension GL_OES_EGL_image_external : require\n" +
                    "precision highp float;\n" +
                    "varying vec2 vTextureCoord;\n" +
                    "uniform float alpha;\n" +
                    "uniform vec2 preview;\n" +
                    "uniform vec2 resolution;\n" +
                    "uniform vec2 texelSize;\n" +
                    "uniform float switchBlur;\n" +
                    "uniform samplerExternalOES sTexture;\n" +
                    "void main() {\n" +
                    "   float transition = smoothstep(0.0, 1.0, switchBlur);\n" +
                    "   vec2 uv = vTextureCoord;\n" +
                    "   vec4 textColor = texture2D(sTexture, uv);\n" +
                    "   if (switchBlur > 0.001) {\n" +
                    "       vec2 d = texelSize * (2.0 * transition);\n" +
                    "       vec2 radial = (uv - vec2(0.5)) * (0.012 * transition);\n" +
                    "       textColor = textColor * 0.52\n" +
                    "           + texture2D(sTexture, uv + vec2(d.x, 0.0)) * 0.09\n" +
                    "           + texture2D(sTexture, uv - vec2(d.x, 0.0)) * 0.09\n" +
                    "           + texture2D(sTexture, uv + vec2(0.0, d.y)) * 0.09\n" +
                    "           + texture2D(sTexture, uv - vec2(0.0, d.y)) * 0.09\n" +
                    "           + texture2D(sTexture, uv + radial) * 0.06\n" +
                    "           + texture2D(sTexture, uv - radial) * 0.06;\n" +
                    "   }\n" +
                    "   gl_FragColor = vec4(textColor.rgb * alpha, alpha);\n" +
                    "}\n";
        }
        if (!cameraXTransition) {
            return "#extension GL_OES_EGL_image_external : require\n" +
                    "precision highp float;\n" +
                    "varying vec2 vTextureCoord;\n" +
                    "uniform vec2 resolution;\n" +
                    "uniform vec2 preview;\n" +
                    "uniform float alpha;\n" +
                    "uniform samplerExternalOES sTexture;\n" +
                    "void main() {\n" +
                    "   vec2 c_textureSize = preview;\n" +
                    "   vec2 c_onePixel = (1.0 / c_textureSize);\n" +
                    "   vec2 uv = vTextureCoord;\n" +
                    "   vec2 pixel = uv * c_textureSize + 0.5;\n" +
                    "   vec2 frac = fract(pixel);\n" +
                    "   pixel = (floor(pixel) / c_textureSize) - vec2(c_onePixel);\n" +
                    "   vec4 tl = texture2D(sTexture, pixel + vec2(0.0, 0.0));\n" +
                    "   vec4 tr = texture2D(sTexture, pixel + vec2(c_onePixel.x, 0.0));\n" +
                    "   vec4 bl = texture2D(sTexture, pixel + vec2(0.0, c_onePixel.y));\n" +
                    "   vec4 br = texture2D(sTexture, pixel + vec2(c_onePixel.x, c_onePixel.y));\n" +
                    "   vec4 x1 = mix(tl, tr, frac.x);\n" +
                    "   vec4 x2 = mix(bl, br, frac.x);\n" +
                    "   gl_FragColor = mix(x1, x2, frac.y) * alpha;\n" +
                    "}\n";
        }
        return "#extension GL_OES_EGL_image_external : require\n" +
                "precision highp float;\n" +
                "varying vec2 vTextureCoord;\n" +
                "uniform vec2 resolution;\n" +
                "uniform vec2 preview;\n" +
                "uniform float alpha;\n" +
                "uniform vec2 texelSize;\n" +
                "uniform float switchBlur;\n" +

                "uniform samplerExternalOES sTexture;\n" +
                "void main() {\n" +
                "   vec2 c_textureSize = preview;\n" +
                "   vec2 c_onePixel = (1.0 / c_textureSize);\n" +
                "   float transition = smoothstep(0.0, 1.0, switchBlur);\n" +
                "   vec2 uv = vTextureCoord;\n" +
                "   vec2 pixel = uv * c_textureSize + 0.5;\n" +
                "   vec2 frac = fract(pixel);\n" +
                "   pixel = (floor(pixel) / c_textureSize) - vec2(c_onePixel);\n" +
                "   vec4 tl = texture2D(sTexture, pixel + vec2(0.0         , 0.0));\n" +
                "   vec4 tr = texture2D(sTexture, pixel + vec2(c_onePixel.x, 0.0));\n" +
                "   vec4 bl = texture2D(sTexture, pixel + vec2(0.0         , c_onePixel.y));\n" +
                "   vec4 br = texture2D(sTexture, pixel + vec2(c_onePixel.x, c_onePixel.y));\n" +
                "   vec4 x1 = mix(tl, tr, frac.x);\n" +
                "   vec4 x2 = mix(bl, br, frac.x);\n" +
                "   vec4 color = mix(x1, x2, frac.y);\n" +
                "   if (switchBlur > 0.001) {\n" +
                "       vec2 d = texelSize * (2.0 * transition);\n" +
                "       vec2 radial = (uv - vec2(0.5)) * (0.012 * transition);\n" +
                "       color = color * 0.52\n" +
                "           + texture2D(sTexture, uv + vec2(d.x, 0.0)) * 0.09\n" +
                "           + texture2D(sTexture, uv - vec2(d.x, 0.0)) * 0.09\n" +
                "           + texture2D(sTexture, uv + vec2(0.0, d.y)) * 0.09\n" +
                "           + texture2D(sTexture, uv - vec2(0.0, d.y)) * 0.09\n" +
                "           + texture2D(sTexture, uv + radial) * 0.06\n" +
                "           + texture2D(sTexture, uv - radial) * 0.06;\n" +
                "   }\n" +
                "   gl_FragColor = color * alpha;\n" +
                "}\n";
    }

    public class InstantViewCameraContainer extends InstantCameraViewBase.InstantViewCameraContainer {

        ImageReceiver imageReceiver;
        float imageProgress;

        public InstantViewCameraContainer(Context context) {
            super(context);
            InstantCameraView.this.setWillNotDraw(false);
        }

        @Override
        public void setImageReceiver(ImageReceiver imageReceiver) {
            if (this.imageReceiver == null) {
                imageProgress = 0;
            }
            this.imageReceiver = imageReceiver;
            invalidate();
        }

        @Override
        protected void dispatchDraw(Canvas canvas) {
            super.dispatchDraw(canvas);
            if (imageProgress != 1f) {
                imageProgress += AndroidUtilities.screenRefreshTime / 250.0f;
                if (imageProgress > 1f) {
                    imageProgress = 1f;
                }
                invalidate();
            }
            if (imageReceiver != null) {
                canvas.save();
                if (imageReceiver.getImageWidth() != textureViewSize) {
                    float s = textureViewSize / imageReceiver.getImageWidth();
                    canvas.scale(s, s);
                }
                canvas.translate(-imageReceiver.getImageX(), -imageReceiver.getImageY());
                float oldAlpha = imageReceiver.getAlpha();
                imageReceiver.setAlpha(imageProgress);
                imageReceiver.draw(canvas);
                imageReceiver.setAlpha(oldAlpha);
                canvas.restore();
            }
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        if (ev.getAction() == MotionEvent.ACTION_DOWN && ev.getY() > getMeasuredHeight() - getPaddingBottom()) {
            return false;
        }

        if (ev.getAction() == MotionEvent.ACTION_DOWN && delegate != null) {
            if (videoPlayer != null) {
                boolean mute = !videoPlayer.isMuted();
                videoPlayer.setMute(mute);
                if (muteAnimation != null) {
                    muteAnimation.cancel();
                }
                muteAnimation = new AnimatorSet();
                muteAnimation.playTogether(
                        ObjectAnimator.ofFloat(muteImageView, View.ALPHA, mute ? 1.0f : 0.0f),
                        ObjectAnimator.ofFloat(muteImageView, View.SCALE_X, mute ? 1.0f : 0.5f),
                        ObjectAnimator.ofFloat(muteImageView, View.SCALE_Y, mute ? 1.0f : 0.5f));
                muteAnimation.addListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        if (animation.equals(muteAnimation)) {
                            muteAnimation = null;
                        }
                    }
                });
                muteAnimation.setDuration(180);
                muteAnimation.setInterpolator(new DecelerateInterpolator());
                muteAnimation.start();
            } else {

            }
        }

        if (useCameraX && videoPlayer == null && !isRoundCameraXZoomReady()) {
            cancelRoundZoomGesture();
            return true;
        }
        if (ev.getActionMasked() == MotionEvent.ACTION_DOWN || ev.getActionMasked() == MotionEvent.ACTION_POINTER_DOWN) {
            if (maybePinchToZoomTouchMode && !isInPinchToZoomTouchMode && ev.getPointerCount() == 2 && finishZoomTransition == null && recording) {
                pinchStartDistance = (float) Math.hypot(ev.getX(1) - ev.getX(0), ev.getY(1) - ev.getY(0));

                pinchScale = 1f;
                cameraXPinchStartRatio = currentRoundZoomRatio();
                roundZoomGestureFilter.reset(pinchStartDistance / AndroidUtilities.density, ev.getEventTime());

                pointerId1 = ev.getPointerId(0);
                pointerId2 = ev.getPointerId(1);
                isInPinchToZoomTouchMode = true;

                singleZoomMaybe = false;
                singleZoomActive = false;
            }
            if (ev.getActionMasked() == MotionEvent.ACTION_DOWN) {
                AndroidUtilities.rectTmp.set(cameraContainer.getX(), cameraContainer.getY(), cameraContainer.getX() + cameraContainer.getMeasuredWidth(), cameraContainer.getY() + cameraContainer.getMeasuredHeight());
                maybePinchToZoomTouchMode = AndroidUtilities.rectTmp.contains(ev.getX(), ev.getY());

                if (maybePinchToZoomTouchMode && recording && finishZoomTransition == null) {
                    singleZoomMaybe = true;
                    singleZoomActive = false;
                    singleZoomStartY = ev.getY();
                    singleZoomStartRatio = currentRoundZoomRatio();
                    roundZoomGestureFilter.reset(0, ev.getEventTime());
                }
            }
            return true;
        } else if (ev.getActionMasked() == MotionEvent.ACTION_MOVE && isInPinchToZoomTouchMode) {
            int index1 = -1;
            int index2 = -1;
            for (int i = 0; i < ev.getPointerCount(); i++) {
                if (pointerId1 == ev.getPointerId(i)) {
                    index1 = i;
                }
                if (pointerId2 == ev.getPointerId(i)) {
                    index2 = i;
                }
            }
            if (index1 == -1 || index2 == -1) {
                isInPinchToZoomTouchMode = false;

                finishZoom();
                return false;
            }
            float span = (float) Math.hypot(ev.getX(index2) - ev.getX(index1), ev.getY(index2) - ev.getY(index1));
            if (pinchStartDistance < AndroidUtilities.dp(24)) {

                pinchStartDistance = span;
                cameraXPinchStartRatio = currentRoundZoomRatio();
                roundZoomGestureFilter.reset(span / AndroidUtilities.density, ev.getEventTime());
                return true;
            }
            float filteredSpan = roundZoomGestureFilter.update(span / AndroidUtilities.density, ev.getEventTime())
                    * AndroidUtilities.density;
            pinchScale = filteredSpan / pinchStartDistance;
            if (useCameraX) {
                float min = videoMessagesHelper.getMinZoomRatio();
                float max = videoMessagesHelper.getMaxZoomRatio();
                float ratio = cameraXPinchStartRatio * pinchScale;
                requestSmoothRoundZoom(Math.max(min, Math.min(max, ratio)));
            } else if (useCamera2) {
                if (camera2SessionCurrent != null) {
                    float zoom = Utilities.clamp(cameraXPinchStartRatio * pinchScale, camera2SessionCurrent.getMaxZoom(), camera2SessionCurrent.getMinZoom());
                    requestSmoothRoundZoom(zoom);
                }
            } else {
                float zoom = Math.min(1f, Math.max(0, (1f + cameraXPinchStartRatio) * pinchScale - 1f));
                requestSmoothRoundZoom(zoom);
            }
        } else if (ev.getActionMasked() == MotionEvent.ACTION_MOVE && singleZoomMaybe
                && !isInPinchToZoomTouchMode && ev.getPointerCount() == 1) {
            float dy = singleZoomStartY - ev.getY();
            if (!singleZoomActive && Math.abs(dy) > AndroidUtilities.dp(8)) {
                singleZoomActive = true;

                singleZoomStartY = ev.getY();
                singleZoomStartRatio = currentRoundZoomRatio();
                roundZoomGestureFilter.reset(0, ev.getEventTime());
                dy = 0;
            }
            if (singleZoomActive) {
                applySingleDragZoom(roundZoomGestureFilter.update(dy / AndroidUtilities.density,
                        ev.getEventTime()) * AndroidUtilities.density);
            }
        } else if (ev.getActionMasked() == MotionEvent.ACTION_UP
                || ev.getActionMasked() == MotionEvent.ACTION_CANCEL
                || (ev.getActionMasked() == MotionEvent.ACTION_POINTER_UP && checkPointerIds(ev))) {
            if (isInPinchToZoomTouchMode) {
                isInPinchToZoomTouchMode = false;
                finishZoom();
            }

            singleZoomMaybe = false;
            singleZoomActive = false;
        }
        return true;
    }

    private void cancelRoundZoomGesture() {
        singleZoomMaybe = false;
        singleZoomActive = false;
        maybePinchToZoomTouchMode = false;
        isInPinchToZoomTouchMode = false;
        pointerId1 = pointerId2 = -1;
    }

    private float currentRoundZoomRatio() {
        if (roundZoomSpringRunning && roundZoomSession == roundZoomSessionIdentity()) {
            if (roundZoomControlMotion) {
                sampleRoundZoomAtInput();
                return roundZoomMotion.value();
            }
            return roundZoomSpring.target() - (useCameraX || useCamera2 ? 0f : 1f);
        }
        if (useCameraX) {
            return videoMessagesHelper.getZoomRatio();
        } else if (useCamera2) {
            return camera2SessionCurrent != null ? camera2SessionCurrent.getZoom() : 1f;
        }
        return legacyZoom;
    }

    private void applySingleDragZoom(float dyPx) {
        float travel = Math.max(AndroidUtilities.dp(160), getMeasuredHeight() * 0.42f);
        if (useCameraX) {
            float min = videoMessagesHelper.getMinZoomRatio();
            float max = videoMessagesHelper.getMaxZoomRatio();
            float ratio = singleZoomStartRatio * (float) Math.pow(max / min, dyPx / travel);
            requestSmoothRoundZoom(Math.max(min, Math.min(max, ratio)));
        } else if (useCamera2) {
            if (camera2SessionCurrent == null) return;
            float min = camera2SessionCurrent.getMinZoom();
            float max = camera2SessionCurrent.getMaxZoom();
            if (max <= min) return;
            float ratio = singleZoomStartRatio * (float) Math.pow(max / min, dyPx / travel);
            ratio = Utilities.clamp(ratio, max, min);
            requestSmoothRoundZoom(ratio);
        } else {
            if (cameraSession == null) return;
            float v = singleZoomStartRatio + (dyPx / travel);
            v = Utilities.clamp(v, 1f, 0f);
            requestSmoothRoundZoom(v);
        }
    }

    ValueAnimator finishZoomTransition;

    private final com.th3nekit.finegram.camera.RoundZoomSpring roundZoomSpring =
            new com.th3nekit.finegram.camera.RoundZoomSpring();
    private final com.th3nekit.finegram.camera.RoundZoomMotion roundZoomMotion =
            new com.th3nekit.finegram.camera.RoundZoomMotion();
    private boolean roundZoomControlMotion;
    private final com.th3nekit.finegram.camera.RoundZoomGestureFilter roundZoomGestureFilter =
            new com.th3nekit.finegram.camera.RoundZoomGestureFilter();
    private boolean roundZoomSpringRunning;
    private Object roundZoomSession;
    private CameraXSurfaceSession roundZoomBindingSession;
    private int roundZoomBindingGeneration = -1;
    private Object roundZoomBindingToken;
    private long roundZoomFrameTime;
    private final android.view.Choreographer.FrameCallback roundZoomFrame = new android.view.Choreographer.FrameCallback() {
        @Override public void doFrame(long frameTimeNanos) {
            if (!roundZoomSpringRunning) return;
            if (!recording || cancelled || roundZoomSession != roundZoomSessionIdentity()
                    || (useCameraX || useCamera2) && !isRoundCameraZoomReady()) {
                stopRoundZoomSpring();
                return;
            }

            long now = Math.max(frameTimeNanos, roundZoomFrameTime);
            if (useCameraX || useCamera2) {
                float min = roundZoomMin(), max = roundZoomMax();
                if (roundZoomControlMotion) roundZoomMotion.setBounds(min, max);
                else roundZoomSpring.setBounds(min, max);
            }
            double elapsed = roundZoomFrameTime == 0 ? 0 : (now - roundZoomFrameTime) / 1_000_000_000.0;
            float value = roundZoomControlMotion ? roundZoomMotion.step(elapsed) : roundZoomSpring.step(elapsed);
            roundZoomFrameTime = now;
            if (useCameraX) {
                requestRoundCameraXZoom(value);
            } else if (useCamera2) {
                camera2SessionCurrent.setZoom(Utilities.clamp(value,
                        camera2SessionCurrent.getMaxZoom(), camera2SessionCurrent.getMinZoom()));
            } else {
                legacyZoom = Utilities.clamp(value - 1f, 1f, 0f);
                cameraSession.setZoom(legacyZoom);
            }
            if (roundZoomControl != null && roundZoomControl.matchesSession(roundZoomSession)
                    && (useCamera2 || useCameraX && videoMessagesHelper.isZoomShortcutsReady())) {
                roundZoomControl.setValue(roundZoomDisplayedValue(), roundZoomTarget());
            }
            if (roundZoomControlMotion ? roundZoomMotion.settled() : roundZoomSpring.settled()) {
                roundZoomSpringRunning = false;
            } else {
                android.view.Choreographer.getInstance().postFrameCallback(this);
            }
        }
    };

    private Object roundZoomSessionIdentity() {
        if (!useCameraX) return useCamera2 ? camera2SessionCurrent : cameraSession;
        CameraXSurfaceSession current = videoMessagesHelper.getCurrentSession();
        if (current == null) return null;
        int generation = current.getSurfaceRequestGeneration();
        if (current != roundZoomBindingSession || generation != roundZoomBindingGeneration
                || roundZoomBindingToken == null) {
            roundZoomBindingSession = current;
            roundZoomBindingGeneration = generation;
            roundZoomBindingToken = new Object();
        }
        return roundZoomBindingToken;
    }

    private void stopRoundZoomSpring() {
        android.view.Choreographer.getInstance().removeFrameCallback(roundZoomFrame);
        roundZoomSpringRunning = false;
        roundZoomSession = null;
        roundZoomControlMotion = false;
    }

    private float roundZoomDisplayedValue() {
        return roundZoomControlMotion ? roundZoomMotion.intent() : roundZoomSpring.value();
    }

    private float roundZoomTarget() {
        return roundZoomControlMotion ? roundZoomMotion.target() : roundZoomSpring.target();
    }

    private boolean isRoundCameraXZoomReady() {
        return useCameraX && recording && !cancelled && cameraReady && cameraThread != null
                && !cameraXSingleSwitchAwaitingBind
                && !flipAnimationInProgress
                && videoMessagesHelper.isZoomReady()
                && RoundZoomScale.valid(videoMessagesHelper.getMinZoomRatio(), videoMessagesHelper.getMaxZoomRatio());
    }

    private boolean isRoundZoomControlReady() {
        return isRoundCameraZoomReady() && roundZoomControl != null
                && roundZoomControl.matchesSession(roundZoomSessionIdentity());
    }

    private boolean isRoundCameraZoomReady() {
        return useCameraX ? isRoundCameraXZoomReady()
                : useCamera2 && recording && !cancelled && cameraReady && cameraThread != null
                && !flipAnimationInProgress && camera2SessionCurrent != null
                && camera2SessionCurrent.isInitiated()
                && RoundZoomScale.valid(camera2SessionCurrent.getMinZoom(), camera2SessionCurrent.getMaxZoom());
    }

    private float roundZoomMin() {
        return useCameraX ? videoMessagesHelper.getMinZoomRatio()
                : camera2SessionCurrent != null ? camera2SessionCurrent.getMinZoom() : 1f;
    }

    private float roundZoomMax() {
        return useCameraX ? videoMessagesHelper.getMaxZoomRatio()
                : camera2SessionCurrent != null ? camera2SessionCurrent.getMaxZoom() : 1f;
    }

    private float roundZoomObserved() {
        if (useCameraX) {
            CameraXSurfaceSession session = videoMessagesHelper.getCurrentSession();
            return session != null ? session.getObservedZoomRatio() : 1f;
        }
        return camera2SessionCurrent != null ? camera2SessionCurrent.getZoom() : 1f;
    }

    private void sampleRoundZoomAtInput() {
        if (!roundZoomSpringRunning || roundZoomSession != roundZoomSessionIdentity()) return;
        long now = System.nanoTime();
        if (roundZoomFrameTime != 0 && now > roundZoomFrameTime) {
            double elapsed = (now - roundZoomFrameTime) / 1_000_000_000.0;
            if (roundZoomControlMotion) roundZoomMotion.step(elapsed);
            else roundZoomSpring.step(elapsed);
        }
        roundZoomFrameTime = now;
    }

    private void updateRoundZoomBounds() {
        if (useCameraX) {
            roundZoomSpring.setBounds(videoMessagesHelper.getMinZoomRatio(), videoMessagesHelper.getMaxZoomRatio());
        } else if (useCamera2 && camera2SessionCurrent != null) {
            roundZoomSpring.setBounds(camera2SessionCurrent.getMinZoom(), camera2SessionCurrent.getMaxZoom());
        } else {
            roundZoomSpring.setBounds(1f, 2f);
        }
    }

    private void requestRoundZoomControl(float ratio, boolean preset) {
        if (!isRoundZoomControlReady() || !Float.isFinite(ratio)) return;
        Object session = roundZoomSessionIdentity();
        ratio = RoundZoomScale.clamp(ratio, roundZoomMin(), roundZoomMax());
        sampleRoundZoomAtInput();
        roundZoomMotion.setBounds(roundZoomMin(), roundZoomMax());
        if (!roundZoomControlMotion || roundZoomSession != session) {
            float start = roundZoomSession == session ? roundZoomSpring.value()
                    : roundZoomObserved();
            roundZoomMotion.reset(start);
        }
        roundZoomControlMotion = true;
        roundZoomSession = session;
        if (!SharedConfig.animationsEnabled()) roundZoomMotion.reset(ratio);
        else if (preset) roundZoomMotion.preset(ratio);
        else roundZoomMotion.drag(ratio);
        roundZoomFrameTime = System.nanoTime();
        if (!roundZoomSpringRunning) {
            roundZoomSpringRunning = true;
            android.view.Choreographer.getInstance().postFrameCallback(roundZoomFrame);
        }
        roundZoomControl.setValue(roundZoomMotion.intent(), roundZoomMotion.target());
    }

    private void requestSmoothRoundZoom(float target) {
        if (!recording || cancelled || Float.isNaN(target) || Float.isInfinite(target)) return;
        if ((useCameraX || useCamera2) && !isRoundCameraZoomReady()) return;
        Object session = roundZoomSessionIdentity();
        if (session == null) return;
        if (useCameraX || useCamera2) {
            target = RoundZoomScale.clamp(target, roundZoomMin(), roundZoomMax());
        }
        float previousTarget = useCameraX || useCamera2 ? currentRoundZoomRatio() : target;
        sampleRoundZoomAtInput();
        updateRoundZoomBounds();
        float offset = useCameraX || useCamera2 ? 0f : 1f;
        if (roundZoomControlMotion || !roundZoomSpringRunning || roundZoomSession != session) {
            float startRatio = roundZoomSession == session
                    ? (roundZoomControlMotion ? roundZoomMotion.value() : roundZoomSpring.value())
                    : currentRoundZoomRatio() + offset;
            stopRoundZoomSpring();
            roundZoomSpring.reset(startRatio);
            roundZoomSession = session;
            roundZoomFrameTime = System.nanoTime();
            roundZoomSpringRunning = true;
            android.view.Choreographer.getInstance().postFrameCallback(roundZoomFrame);
        }
        roundZoomSpring.target(target + offset);
        if ((useCameraX || useCamera2) && roundZoomControl != null && roundZoomControl.matchesSession(session)) {
            roundZoomControl.onExternalGestureZoom(previousTarget, target);
        }
    }

    public void finishZoom() {

        cameraXPinchStartRatio = currentRoundZoomRatio();
    }

    public interface Delegate {

        View getFragmentView();
        void sendMedia(MediaController.PhotoEntry entry, VideoEditedInfo videoEditedInfo, boolean notify, int scheduleDate, int scheduleRepeatPeriod, boolean b1, long stars);
        Activity getParentActivity();
        int getClassGuid();
        long getDialogId();

        default boolean isSecretChat() {
            return false;
        }

        default boolean isInScheduleMode() {
            return false;
        }
    }
}

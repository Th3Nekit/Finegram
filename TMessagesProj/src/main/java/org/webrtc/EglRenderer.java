/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.SurfaceTexture;
import android.opengl.GLES20;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import androidx.annotation.Nullable;
import android.view.Surface;

import org.telegram.messenger.FileLog;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class EglRenderer implements VideoSink {
  private static final String TAG = "EglRenderer";
  private static final long LOG_INTERVAL_SEC = 4;

  public boolean firstFrameRendered;

  public interface FrameListener { void onFrame(Bitmap frame); }

  public interface ErrorCallback {

    void onGlOutOfMemory();
  }

  private static class FrameListenerAndParams {
    public final FrameListener listener;
    public final float scale;
    public final RendererCommon.GlDrawer drawer;
    public final boolean applyFpsReduction;

    public FrameListenerAndParams(FrameListener listener, float scale,
        RendererCommon.GlDrawer drawer, boolean applyFpsReduction) {
      this.listener = listener;
      this.scale = scale;
      this.drawer = drawer;
      this.applyFpsReduction = applyFpsReduction;
    }
  }

  private class EglSurfaceCreation implements Runnable {
    private Object surface;
    private final boolean background;

    public EglSurfaceCreation(boolean background) {
      this.background = background;
    }

    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void setSurface(Object surface) {
      this.surface = surface;
    }

    @Override

    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void run() {
      if (surface != null && eglBase != null && (background ? !eglBase.hasBackgroundSurface() : !eglBase.hasSurface())) {
        if (surface instanceof Surface) {
          eglBase.createSurface((Surface) surface);
        } else if (surface instanceof SurfaceTexture) {
          if (background) {
            eglBase.createBackgroundSurface((SurfaceTexture) surface);
          } else {
            eglBase.createSurface((SurfaceTexture) surface);
          }

        } else {
          throw new IllegalStateException("Invalid surface: " + surface);
        }
        if (!background) {
          eglBase.makeCurrent();

          GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 1);
        } else {
          eglBase.makeBackgroundCurrent();

          GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 1);
          if (eglBase.hasSurface()) {
            eglBase.makeCurrent();
          }
        }
      }
    }
  }

  private static class HandlerWithExceptionCallback extends Handler {
    private final Runnable exceptionCallback;

    public HandlerWithExceptionCallback(Looper looper, Runnable exceptionCallback) {
      super(looper);
      this.exceptionCallback = exceptionCallback;
    }

    @Override
    public void dispatchMessage(Message msg) {
      try {
        super.dispatchMessage(msg);
      } catch (Exception e) {
        Logging.e(TAG, "Exception on EglRenderer thread", e);
        org.telegram.messenger.FileLog.e(e);
        exceptionCallback.run();

      }
    }
  }

  protected final String name;

  private final Object handlerLock = new Object();
  @Nullable private Handler renderThreadHandler;

  private final ArrayList<FrameListenerAndParams> frameListeners = new ArrayList<>();

  private volatile ErrorCallback errorCallback;

  private final Object fpsReductionLock = new Object();

  private long nextFrameTimeNs;

  private long minRenderPeriodNs;

  @Nullable private EglBase eglBase;
  private final VideoFrameDrawer frameDrawer;
  @Nullable private RendererCommon.GlDrawer drawer;
  private boolean usePresentationTimeStamp;
  private final Matrix drawMatrix = new Matrix();

  private final Object frameLock = new Object();
  @Nullable private VideoFrame pendingFrame;

  private final Object layoutLock = new Object();
  private float layoutAspectRatio;

  private boolean mirrorHorizontally;

  private boolean mirrorVertically;

  private int rotation;

  private int framesReceived;

  private int framesDropped;

  private int framesRendered;

  private long renderTimeNs;

  private long renderSwapBufferTimeNs;

  private final GlTextureFrameBuffer bitmapTextureFramebuffer =
      new GlTextureFrameBuffer(GLES20.GL_RGBA);

  private final EglSurfaceCreation eglSurfaceCreationRunnable = new EglSurfaceCreation(false);
  private final EglSurfaceCreation eglSurfaceBackgroundCreationRunnable = new EglSurfaceCreation(true);

  public EglRenderer(String name) {
    this(name, new VideoFrameDrawer());
  }

  public EglRenderer(String name, VideoFrameDrawer videoFrameDrawer) {
    this.name = name;
    this.frameDrawer = videoFrameDrawer;
  }

  public void init(@Nullable final EglBase.Context sharedContext, final int[] configAttributes,
      RendererCommon.GlDrawer drawer, boolean usePresentationTimeStamp) {
    synchronized (handlerLock) {
      if (renderThreadHandler != null) {
        throw new IllegalStateException(name + "Already initialized");
      }
      logD("Initializing EglRenderer");
      this.drawer = drawer;
      this.usePresentationTimeStamp = usePresentationTimeStamp;
      firstFrameRendered = false;

      final HandlerThread renderThread = new HandlerThread(name + "EglRenderer");
      renderThread.start();
      renderThreadHandler =
          new HandlerWithExceptionCallback(renderThread.getLooper(), new Runnable() {
            @Override
            public void run() {
              synchronized (handlerLock) {
                renderThreadHandler = null;
              }
            }
          });

      renderThreadHandler.post(() -> {

        if (sharedContext == null) {
          logD("EglBase10.create context");
          eglBase = EglBase.createEgl10(configAttributes);
        } else {
          logD("EglBase.create shared context");
          eglBase = EglBase.create(sharedContext, configAttributes);
        }
      });
      renderThreadHandler.post(eglSurfaceCreationRunnable);
    }
  }

  public void init(@Nullable final EglBase.Context sharedContext, final int[] configAttributes,
      RendererCommon.GlDrawer drawer) {
    init(sharedContext, configAttributes, drawer,                                 false);
  }

  public void createEglSurface(Surface surface) {
    createEglSurfaceInternal(surface, false);
  }

  public void createEglSurface(SurfaceTexture surfaceTexture) {
    createEglSurfaceInternal(surfaceTexture, false);
  }

  public void createBackgroundSurface(SurfaceTexture surface) {
    createEglSurfaceInternal(surface, true);
  }

  private void createEglSurfaceInternal(Object surface, boolean background) {
    if (background) {
      eglSurfaceBackgroundCreationRunnable.setSurface(surface);
      synchronized (handlerLock) {
        if (renderThreadHandler != null) {
          renderThreadHandler.post(eglSurfaceBackgroundCreationRunnable);
        } else {
          FileLog.d("can't create background surface. render thread is null");
        }
      }

    } else {
      eglSurfaceCreationRunnable.setSurface(surface);
      postToRenderThread(eglSurfaceCreationRunnable);
    }
  }

  public void release() {
    logD("Releasing.");
    final CountDownLatch eglCleanupBarrier = new CountDownLatch(1);
    synchronized (handlerLock) {
      if (renderThreadHandler == null) {
        logD("Already released");
        return;
      }

      renderThreadHandler.postAtFrontOfQueue(() -> {

        synchronized (EglBase.lock) {
          GLES20.glUseProgram(               0);
        }
        if (drawer != null) {
          drawer.release();
          drawer = null;
        }
        frameDrawer.release();
        bitmapTextureFramebuffer.release();
        if (eglBase != null) {
          logD("eglBase detach and release.");
          eglBase.detachCurrent();
          eglBase.release();
          eglBase = null;
        }
        frameListeners.clear();
        eglCleanupBarrier.countDown();
      });
      final Looper renderLooper = renderThreadHandler.getLooper();

      renderThreadHandler.post(() -> {
        logD("Quitting render thread.");
        renderLooper.quit();
      });

      renderThreadHandler = null;
    }

    ThreadUtils.awaitUninterruptibly(eglCleanupBarrier);
    synchronized (frameLock) {
      if (pendingFrame != null) {
        pendingFrame.release();
        pendingFrame = null;
      }
    }
    logD("Releasing done.");
  }

  public void printStackTrace() {
    synchronized (handlerLock) {
      final Thread renderThread =
          (renderThreadHandler == null) ? null : renderThreadHandler.getLooper().getThread();
      if (renderThread != null) {
        final StackTraceElement[] renderStackTrace = renderThread.getStackTrace();
        if (renderStackTrace.length > 0) {
          logW("EglRenderer stack trace:");
          for (StackTraceElement traceElem : renderStackTrace) {
            logW(traceElem.toString());
          }
        }
      }
    }
  }

  public void setMirror(final boolean mirror) {
    logD("setMirrorHorizontally: " + mirror);
    synchronized (layoutLock) {
      this.mirrorHorizontally = mirror;
    }
  }

  public void setMirrorVertically(final boolean mirrorVertically) {
    logD("setMirrorVertically: " + mirrorVertically);
    synchronized (layoutLock) {
      this.mirrorVertically = mirrorVertically;
    }
  }

  public void setLayoutAspectRatio(float layoutAspectRatio) {
    if (this.layoutAspectRatio != layoutAspectRatio) {
      synchronized (layoutLock) {
        this.layoutAspectRatio = layoutAspectRatio;
      }
    }
  }

  public void setFpsReduction(float fps) {
    logD("setFpsReduction: " + fps);
    synchronized (fpsReductionLock) {
      final long previousRenderPeriodNs = minRenderPeriodNs;
      if (fps <= 0) {
        minRenderPeriodNs = Long.MAX_VALUE;
      } else {
        minRenderPeriodNs = (long) (TimeUnit.SECONDS.toNanos(1) / fps);
      }
      if (minRenderPeriodNs != previousRenderPeriodNs) {

        nextFrameTimeNs = System.nanoTime();
      }
    }
  }

  public void disableFpsReduction() {
    setFpsReduction(Float.POSITIVE_INFINITY          );
  }

  public void pauseVideo() {
    setFpsReduction(0          );
  }

  public void addFrameListener(final FrameListener listener, final float scale) {
    addFrameListener(listener, scale, null, false                        );
  }

  public void addFrameListener(
      final FrameListener listener, final float scale, final RendererCommon.GlDrawer drawerParam) {
    addFrameListener(listener, scale, drawerParam, false                        );
  }

  public void addFrameListener(final FrameListener listener, final float scale,
      @Nullable final RendererCommon.GlDrawer drawerParam, final boolean applyFpsReduction) {
    postToRenderThread(() -> {
      final RendererCommon.GlDrawer listenerDrawer = drawerParam == null ? drawer : drawerParam;
      frameListeners.add(
          new FrameListenerAndParams(listener, scale, listenerDrawer, applyFpsReduction));
    });
  }

  public void removeFrameListener(final FrameListener listener) {
    final CountDownLatch latch = new CountDownLatch(1);
    synchronized (handlerLock) {
      if (renderThreadHandler == null) {
        return;
      }
      if (Thread.currentThread() == renderThreadHandler.getLooper().getThread()) {
        throw new RuntimeException("removeFrameListener must not be called on the render thread.");
      }
      postToRenderThread(() -> {
        latch.countDown();
        final Iterator<FrameListenerAndParams> iter = frameListeners.iterator();
        while (iter.hasNext()) {
          if (iter.next().listener == listener) {
            iter.remove();
          }
        }
      });
    }
    ThreadUtils.awaitUninterruptibly(latch);
  }

  public void setErrorCallback(ErrorCallback errorCallback) {
    this.errorCallback = errorCallback;
  }

  @Override
  public void onFrame(VideoFrame frame) {
    final boolean dropOldFrame;
    synchronized (handlerLock) {
      if (renderThreadHandler == null) {
        logD("Dropping frame - Not initialized or already released.");
        return;
      }
      synchronized (frameLock) {
        dropOldFrame = (pendingFrame != null);
        if (dropOldFrame) {
          pendingFrame.release();
        }
        pendingFrame = frame;
        pendingFrame.retain();
        renderThreadHandler.post(this ::renderFrameOnRenderThread);
      }
    }
  }

  public void setRotation(int value) {
    synchronized (layoutLock) {
      rotation = value;
    }
  }

  public void releaseEglSurface(final Runnable completionCallback, boolean background) {

    eglSurfaceCreationRunnable.setSurface(null              );
    synchronized (handlerLock) {
      if (renderThreadHandler != null) {
        renderThreadHandler.removeCallbacks(eglSurfaceCreationRunnable);
        renderThreadHandler.postAtFrontOfQueue(() -> {
          if (eglBase != null) {
            eglBase.detachCurrent();
            eglBase.releaseSurface(background);
          }
          if (completionCallback != null) {
            completionCallback.run();
          }
        });
        return;
      }
    }
    if (completionCallback != null) {
      completionCallback.run();
    }
  }

  private void postToRenderThread(Runnable runnable) {
    synchronized (handlerLock) {
      if (renderThreadHandler != null) {
        renderThreadHandler.post(runnable);
      }
    }
  }

  private void clearSurfaceOnRenderThread(float r, float g, float b, float a) {
    if (eglBase != null && eglBase.hasSurface()) {
      logD("clearSurface");
      GLES20.glClearColor(r, g, b, a);
      GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
      eglBase.swapBuffers(false);
    }
    if (eglBase != null && eglBase.hasBackgroundSurface()) {
      eglBase.makeBackgroundCurrent();
      logD("clearSurface in background");
      GLES20.glClearColor(r, g, b, a);
      GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
      eglBase.swapBuffers(true);
      eglBase.makeCurrent();
    }
  }

  public void clearImage() {
    clearImage(0          , 0            , 0           , 0            );
    firstFrameRendered = false;
  }

  public void clearImage(final float r, final float g, final float b, final float a) {
    synchronized (handlerLock) {
      if (renderThreadHandler == null) {
        return;
      }
      renderThreadHandler.postAtFrontOfQueue(() -> clearSurfaceOnRenderThread(r, g, b, a));
    }
  }

  public void getTexture(GlGenericDrawer.TextureCallback callback) {
    synchronized (handlerLock) {
      try {
        if (renderThreadHandler != null) {
          renderThreadHandler.post(() -> frameDrawer.getRenderBufferBitmap(drawer, rotation, callback));
        }
      } catch (Exception e) {
        FileLog.e(e);
      }
    }
  }

  private void renderFrameOnRenderThread() {

    final VideoFrame frame;
    synchronized (frameLock) {
      if (pendingFrame == null) {
        return;
      }
      frame = pendingFrame;
      pendingFrame = null;
    }
    if (eglBase == null || !eglBase.hasSurface()) {
      logD("Dropping frame - No surface");
      frame.release();
      return;
    }

    final boolean shouldRenderFrame;
    synchronized (fpsReductionLock) {
      if (minRenderPeriodNs == Long.MAX_VALUE) {

        shouldRenderFrame = false;
      } else if (minRenderPeriodNs <= 0) {

        shouldRenderFrame = true;
      } else {
        final long currentTimeNs = System.nanoTime();
        if (currentTimeNs < nextFrameTimeNs) {
          shouldRenderFrame = false;
        } else {
          nextFrameTimeNs += minRenderPeriodNs;

          nextFrameTimeNs = Math.max(nextFrameTimeNs, currentTimeNs);
          shouldRenderFrame = true;
        }
      }
    }

    final long startTimeNs = System.nanoTime();

    boolean rotate = Math.abs(rotation) == 90 || Math.abs(rotation) == 270;
    final float frameAspectRatio = (rotate ? frame.getRotatedHeight() : frame.getRotatedWidth()) / (float) (rotate ? frame.getRotatedWidth() : frame.getRotatedHeight());
    final float drawnAspectRatio;
    synchronized (layoutLock) {
      drawnAspectRatio = layoutAspectRatio != 0f ? layoutAspectRatio : frameAspectRatio;
    }

    final float scaleX;
    final float scaleY;

    if (frameAspectRatio > drawnAspectRatio) {
      scaleX = drawnAspectRatio / frameAspectRatio;
      scaleY = 1f;
    } else {
      scaleX = 1f;
      scaleY = frameAspectRatio / drawnAspectRatio;
    }

    drawMatrix.reset();
    drawMatrix.preTranslate(0.5f, 0.5f);
    drawMatrix.preRotate(rotation);
    drawMatrix.preScale(mirrorHorizontally ? -1f : 1f, mirrorVertically ? -1f : 1f);
    drawMatrix.preScale(scaleX, scaleY);
    drawMatrix.preTranslate(-0.5f, -0.5f);

    try {
      if (shouldRenderFrame) {
        frameDrawer.drawFrame(frame, drawer, drawMatrix, 0                , 0                ,
                eglBase.surfaceWidth(), eglBase.surfaceHeight(), rotate, false);

        if (eglBase.hasBackgroundSurface()) {
          eglBase.makeBackgroundCurrent();

          frameDrawer.drawFrame(frame, drawer, drawMatrix, 0, 0,
                  eglBase.surfaceWidth(), eglBase.surfaceHeight(), rotate, true);

          if (usePresentationTimeStamp) {
            eglBase.swapBuffers(frame.getTimestampNs(), true);
          } else {
            eglBase.swapBuffers(true);
          }
          eglBase.makeCurrent();
        }

        final long swapBuffersStartTimeNs = System.nanoTime();
        if (usePresentationTimeStamp) {
          eglBase.swapBuffers(frame.getTimestampNs(), false);
        } else {
          eglBase.swapBuffers(false);
        }

        if (!firstFrameRendered) {
          firstFrameRendered = true;
          onFirstFrameRendered();
        }
      }

      notifyCallbacks(frame, shouldRenderFrame);
    } catch (GlUtil.GlOutOfMemoryException e) {
      logE("Error while drawing frame", e);
      final ErrorCallback errorCallback = this.errorCallback;
      if (errorCallback != null) {
        errorCallback.onGlOutOfMemory();
      }

      drawer.release();
      frameDrawer.release();
      bitmapTextureFramebuffer.release();

    } finally {
      frame.release();
    }
  }

  protected void onFirstFrameRendered() {

  }

  private void notifyCallbacks(VideoFrame frame, boolean wasRendered) {
    if (frameListeners.isEmpty())
      return;

    drawMatrix.reset();
    drawMatrix.preTranslate(0.5f, 0.5f);
    drawMatrix.preRotate(rotation);
    drawMatrix.preScale(mirrorHorizontally ? -1f : 1f, mirrorVertically ? -1f : 1f);
    drawMatrix.preScale(1f, -1f);
    drawMatrix.preTranslate(-0.5f, -0.5f);

    Iterator<FrameListenerAndParams> it = frameListeners.iterator();
    while (it.hasNext()) {
      FrameListenerAndParams listenerAndParams = it.next();
      if (!wasRendered && listenerAndParams.applyFpsReduction) {
        continue;
      }
      it.remove();

      final int scaledWidth = (int) (listenerAndParams.scale * frame.getRotatedWidth());
      final int scaledHeight = (int) (listenerAndParams.scale * frame.getRotatedHeight());

      if (scaledWidth == 0 || scaledHeight == 0) {
        listenerAndParams.listener.onFrame(null);
        continue;
      }

      bitmapTextureFramebuffer.setSize(scaledWidth, scaledHeight);

      GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, bitmapTextureFramebuffer.getFrameBufferId());
      GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER, GLES20.GL_COLOR_ATTACHMENT0,
          GLES20.GL_TEXTURE_2D, bitmapTextureFramebuffer.getTextureId(), 0);

      GLES20.glClearColor(0          , 0            , 0           , 0            );
      GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
      frameDrawer.drawFrame(frame, listenerAndParams.drawer, drawMatrix, 0                ,
          0                , scaledWidth, scaledHeight, false, false);

      final ByteBuffer bitmapBuffer = ByteBuffer.allocateDirect(scaledWidth * scaledHeight * 4);
      GLES20.glViewport(0, 0, scaledWidth, scaledHeight);
      GLES20.glReadPixels(
          0, 0, scaledWidth, scaledHeight, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, bitmapBuffer);

      GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
      GlUtil.checkNoGLES2Error("EglRenderer.notifyCallbacks");

      final Bitmap bitmap = Bitmap.createBitmap(scaledWidth, scaledHeight, Bitmap.Config.ARGB_8888);
      bitmap.copyPixelsFromBuffer(bitmapBuffer);
      listenerAndParams.listener.onFrame(bitmap);
    }
  }

  private void logE(String string, Throwable e) {
    Logging.e(TAG, name + string, e);
  }

  private void logD(String string) {
    Logging.d(TAG, name + string);
  }

  private void logW(String string) {
    Logging.w(TAG, name + string);
  }
}

/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.content.Context;
import android.content.res.Resources.NotFoundException;
import android.graphics.Point;
import android.os.Looper;
import android.util.AttributeSet;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.util.Log;

public class SurfaceViewRenderer extends SurfaceView
    implements SurfaceHolder.Callback, VideoSink, RendererCommon.RendererEvents {
  private static final String TAG = "SurfaceViewRenderer";

  private final String resourceName;
  private final RendererCommon.VideoLayoutMeasure videoLayoutMeasure =
      new RendererCommon.VideoLayoutMeasure();
  private final SurfaceEglRenderer eglRenderer;

  private RendererCommon.RendererEvents rendererEvents;

  private int rotatedFrameWidth;
  private int rotatedFrameHeight;
  private boolean enableFixedSize;
  private int surfaceWidth;
  private int surfaceHeight;

  public SurfaceViewRenderer(Context context) {
    super(context);
    this.resourceName = getResourceName();
    eglRenderer = new SurfaceEglRenderer(resourceName);
    getHolder().addCallback(this);
    getHolder().addCallback(eglRenderer);
  }

  public SurfaceViewRenderer(Context context, AttributeSet attrs) {
    super(context, attrs);
    this.resourceName = getResourceName();
    eglRenderer = new SurfaceEglRenderer(resourceName);
    getHolder().addCallback(this);
    getHolder().addCallback(eglRenderer);
  }

  public void init(EglBase.Context sharedContext, RendererCommon.RendererEvents rendererEvents) {
    init(sharedContext, rendererEvents, EglBase.CONFIG_PLAIN, new GlRectDrawer());
  }

  public void init(final EglBase.Context sharedContext,
      RendererCommon.RendererEvents rendererEvents, final int[] configAttributes,
      RendererCommon.GlDrawer drawer) {
    ThreadUtils.checkIsOnMainThread();
    this.rendererEvents = rendererEvents;
    rotatedFrameWidth = 0;
    rotatedFrameHeight = 0;
    eglRenderer.init(sharedContext, this                     , configAttributes, drawer);
  }

  public void release() {
    eglRenderer.release();
  }

  public void addFrameListener(
      EglRenderer.FrameListener listener, float scale, RendererCommon.GlDrawer drawerParam) {
    eglRenderer.addFrameListener(listener, scale, drawerParam);
  }

  public void addFrameListener(EglRenderer.FrameListener listener, float scale) {
    eglRenderer.addFrameListener(listener, scale);
  }

  public void removeFrameListener(EglRenderer.FrameListener listener) {
    eglRenderer.removeFrameListener(listener);
  }

  public void setEnableHardwareScaler(boolean enabled) {
    ThreadUtils.checkIsOnMainThread();
    enableFixedSize = enabled;
    updateSurfaceSize();
  }

  public void setMirror(final boolean mirror) {
    eglRenderer.setMirror(mirror);
  }

  public void setScalingType(RendererCommon.ScalingType scalingType) {
    ThreadUtils.checkIsOnMainThread();
    videoLayoutMeasure.setScalingType(scalingType);
    requestLayout();
  }

  public void setScalingType(RendererCommon.ScalingType scalingTypeMatchOrientation,
      RendererCommon.ScalingType scalingTypeMismatchOrientation) {
    ThreadUtils.checkIsOnMainThread();
    videoLayoutMeasure.setScalingType(scalingTypeMatchOrientation, scalingTypeMismatchOrientation);
    requestLayout();
  }

  public void setFpsReduction(float fps) {
    eglRenderer.setFpsReduction(fps);
  }

  public void disableFpsReduction() {
    eglRenderer.disableFpsReduction();
  }

  public void pauseVideo() {
    eglRenderer.pauseVideo();
  }

  @Override
  public void onFrame(VideoFrame frame) {
    eglRenderer.onFrame(frame);
  }

  @Override
  protected void onMeasure(int widthSpec, int heightSpec) {
    ThreadUtils.checkIsOnMainThread();
    Point size =
        videoLayoutMeasure.measure(true, widthSpec, heightSpec, rotatedFrameWidth, rotatedFrameHeight);
    setMeasuredDimension(size.x, size.y);
    logD("onMeasure(). New size: " + size.x + "x" + size.y);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    ThreadUtils.checkIsOnMainThread();
    eglRenderer.setLayoutAspectRatio((right - left) / (float) (bottom - top));
    updateSurfaceSize();
  }

  private void updateSurfaceSize() {
    ThreadUtils.checkIsOnMainThread();
    if (enableFixedSize && rotatedFrameWidth != 0 && rotatedFrameHeight != 0 && getWidth() != 0
        && getHeight() != 0) {
      final float layoutAspectRatio = getWidth() / (float) getHeight();
      final float frameAspectRatio = rotatedFrameWidth / (float) rotatedFrameHeight;
      final int drawnFrameWidth;
      final int drawnFrameHeight;
      if (frameAspectRatio > layoutAspectRatio) {
        drawnFrameWidth = (int) (rotatedFrameHeight * layoutAspectRatio);
        drawnFrameHeight = rotatedFrameHeight;
      } else {
        drawnFrameWidth = rotatedFrameWidth;
        drawnFrameHeight = (int) (rotatedFrameWidth / layoutAspectRatio);
      }

      final int width = Math.min(getWidth(), drawnFrameWidth);
      final int height = Math.min(getHeight(), drawnFrameHeight);
      logD("updateSurfaceSize. Layout size: " + getWidth() + "x" + getHeight() + ", frame size: "
          + rotatedFrameWidth + "x" + rotatedFrameHeight + ", requested surface size: " + width
          + "x" + height + ", old surface size: " + surfaceWidth + "x" + surfaceHeight);
      if (width != surfaceWidth || height != surfaceHeight) {
        surfaceWidth = width;
        surfaceHeight = height;
        getHolder().setFixedSize(width, height);
      }
    } else {
      surfaceWidth = surfaceHeight = 0;
      getHolder().setSizeFromLayout();
    }
  }

  @Override
  public void surfaceCreated(final SurfaceHolder holder) {
    ThreadUtils.checkIsOnMainThread();
    surfaceWidth = surfaceHeight = 0;
    updateSurfaceSize();
  }

  @Override
  public void surfaceDestroyed(SurfaceHolder holder) {}

  @Override
  public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {}

  private String getResourceName() {
    try {
      return getResources().getResourceEntryName(getId());
    } catch (NotFoundException e) {
      return "";
    }
  }

  public void clearImage() {
    eglRenderer.clearImage();
  }

  @Override
  public void onFirstFrameRendered() {
    if (rendererEvents != null) {
      rendererEvents.onFirstFrameRendered();
    }
  }

  @Override
  public void onFrameResolutionChanged(int videoWidth, int videoHeight, int rotation) {
    if (rendererEvents != null) {
      rendererEvents.onFrameResolutionChanged(videoWidth, videoHeight, rotation);
    }
    int rotatedWidth = rotation == 0 || rotation == 180 ? videoWidth : videoHeight;
    int rotatedHeight = rotation == 0 || rotation == 180 ? videoHeight : videoWidth;

    postOrRun(() -> {
      rotatedFrameWidth = rotatedWidth;
      rotatedFrameHeight = rotatedHeight;
      updateSurfaceSize();
      requestLayout();
    });
  }

  private void postOrRun(Runnable r) {
    if (Thread.currentThread() == Looper.getMainLooper().getThread()) {
      r.run();
    } else {
      post(r);
    }
  }

  private void logD(String string) {
    Logging.d(TAG, resourceName + ": " + string);
  }
}

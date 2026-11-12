/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.view.SurfaceHolder;
import java.util.concurrent.CountDownLatch;

public class SurfaceEglRenderer extends EglRenderer implements SurfaceHolder.Callback {
  private static final String TAG = "SurfaceEglRenderer";

  private RendererCommon.RendererEvents rendererEvents;

  private final Object layoutLock = new Object();
  private boolean isRenderingPaused;
  private boolean isFirstFrameRendered;
  private int rotatedFrameWidth;
  private int rotatedFrameHeight;
  private int frameRotation;

  public SurfaceEglRenderer(String name) {
    super(name);
  }

  public void init(final EglBase.Context sharedContext,
      RendererCommon.RendererEvents rendererEvents, final int[] configAttributes,
      RendererCommon.GlDrawer drawer) {
    ThreadUtils.checkIsOnMainThread();
    this.rendererEvents = rendererEvents;
    synchronized (layoutLock) {
      isFirstFrameRendered = false;
      rotatedFrameWidth = 0;
      rotatedFrameHeight = 0;
      frameRotation = 0;
    }
    super.init(sharedContext, configAttributes, drawer);
  }

  @Override
  public void init(final EglBase.Context sharedContext, final int[] configAttributes,
      RendererCommon.GlDrawer drawer) {
    init(sharedContext, null                     , configAttributes, drawer);
  }

  @Override
  public void setFpsReduction(float fps) {
    synchronized (layoutLock) {
      isRenderingPaused = fps == 0f;
    }
    super.setFpsReduction(fps);
  }

  @Override
  public void disableFpsReduction() {
    synchronized (layoutLock) {
      isRenderingPaused = false;
    }
    super.disableFpsReduction();
  }

  @Override
  public void pauseVideo() {
    synchronized (layoutLock) {
      isRenderingPaused = true;
    }
    super.pauseVideo();
  }

  @Override
  public void onFrame(VideoFrame frame) {
    updateFrameDimensionsAndReportEvents(frame);
    super.onFrame(frame);
  }

  @Override
  public void surfaceCreated(final SurfaceHolder holder) {
    ThreadUtils.checkIsOnMainThread();
    createEglSurface(holder.getSurface());
  }

  @Override
  public void surfaceDestroyed(SurfaceHolder holder) {
    ThreadUtils.checkIsOnMainThread();
    final CountDownLatch completionLatch = new CountDownLatch(1);
    releaseEglSurface(completionLatch::countDown, false);
    ThreadUtils.awaitUninterruptibly(completionLatch);
  }

  @Override
  public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    ThreadUtils.checkIsOnMainThread();
    logD("surfaceChanged: format: " + format + " size: " + width + "x" + height);
  }

  private void updateFrameDimensionsAndReportEvents(VideoFrame frame) {
    synchronized (layoutLock) {
      if (isRenderingPaused) {
        return;
      }
      if (!isFirstFrameRendered) {
        isFirstFrameRendered = true;
        logD("Reporting first rendered frame.");
        if (rendererEvents != null) {
          rendererEvents.onFirstFrameRendered();
        }
      }
      if (rotatedFrameWidth != frame.getRotatedWidth()
          || rotatedFrameHeight != frame.getRotatedHeight()
          || frameRotation != frame.getRotation()) {
        logD("Reporting frame resolution changed to " + frame.getBuffer().getWidth() + "x"
            + frame.getBuffer().getHeight() + " with rotation " + frame.getRotation());
        if (rendererEvents != null) {
          rendererEvents.onFrameResolutionChanged(
              frame.getBuffer().getWidth(), frame.getBuffer().getHeight(), frame.getRotation());
        }
        rotatedFrameWidth = frame.getRotatedWidth();
        rotatedFrameHeight = frame.getRotatedHeight();
        frameRotation = frame.getRotation();
      }
    }
  }

  private void logD(String string) {
    Logging.d(TAG, name + ": " + string);
  }
}

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

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import androidx.annotation.Nullable;
import android.view.Surface;

import org.telegram.messenger.FileLog;

@TargetApi(21)
public class ScreenCapturerAndroid implements VideoCapturer, VideoSink {
  private static final int DISPLAY_FLAGS =
      DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC | DisplayManager.VIRTUAL_DISPLAY_FLAG_PRESENTATION;

  private static final int VIRTUAL_DISPLAY_DPI = 400;

  private final Intent mediaProjectionPermissionResultData;
  private final MediaProjection.Callback mediaProjectionCallback;

  private int width;
  private int height;
  @Nullable private VirtualDisplay virtualDisplay;
  @Nullable private SurfaceTextureHelper surfaceTextureHelper;
  @Nullable private CapturerObserver capturerObserver;
  private long numCapturedFrames;
  @Nullable private MediaProjection mediaProjection;
  private boolean isDisposed;
  @Nullable private MediaProjectionManager mediaProjectionManager;

  public ScreenCapturerAndroid(Intent mediaProjectionPermissionResultData,
                               MediaProjection.Callback mediaProjectionCallback) {
    this.mediaProjectionPermissionResultData = mediaProjectionPermissionResultData;
    this.mediaProjectionCallback = mediaProjectionCallback;
  }

  private void checkNotDisposed() {
    if (isDisposed) {
      throw new RuntimeException("capturer is disposed.");
    }
  }

  @Nullable
  public MediaProjection getMediaProjection() {
    return mediaProjection;
  }

  @Override

  @SuppressWarnings("NoSynchronizedMethodCheck")
  public synchronized void initialize(final SurfaceTextureHelper surfaceTextureHelper,
                                      final Context applicationContext, final CapturerObserver capturerObserver) {
    checkNotDisposed();

    if (capturerObserver == null) {
      throw new RuntimeException("capturerObserver not set.");
    }
    this.capturerObserver = capturerObserver;

    if (surfaceTextureHelper == null) {
      throw new RuntimeException("surfaceTextureHelper not set.");
    }
    this.surfaceTextureHelper = surfaceTextureHelper;

    mediaProjectionManager = (MediaProjectionManager) applicationContext.getSystemService(
        Context.MEDIA_PROJECTION_SERVICE);
  }

  @Override

  @SuppressWarnings("NoSynchronizedMethodCheck")
  public synchronized void startCapture(
      final int width, final int height, final int ignoredFramerate) {
    if (mediaProjection != null || mediaProjectionManager == null) {
      return;
    }
    try {
      checkNotDisposed();

      this.width = width;
      this.height = height;

      mediaProjection = mediaProjectionManager.getMediaProjection(
          Activity.RESULT_OK, mediaProjectionPermissionResultData);

      mediaProjection.registerCallback(mediaProjectionCallback, surfaceTextureHelper.getHandler());

      createVirtualDisplay();
      capturerObserver.onCapturerStarted(true);
      surfaceTextureHelper.startListening(ScreenCapturerAndroid.this);
    } catch (Throwable e) {
      mediaProjectionCallback.onStop();
      FileLog.e(e);
    }
  }

  @Override

  @SuppressWarnings("NoSynchronizedMethodCheck")
  public synchronized void stopCapture() {
    checkNotDisposed();
    ThreadUtils.invokeAtFrontUninterruptibly(surfaceTextureHelper.getHandler(), () -> {
      surfaceTextureHelper.stopListening();
      capturerObserver.onCapturerStopped();

      if (virtualDisplay != null) {
        virtualDisplay.release();
        virtualDisplay = null;
      }

      if (mediaProjection != null) {

        mediaProjection.unregisterCallback(mediaProjectionCallback);
        mediaProjection.stop();
        mediaProjection = null;
      }
    });
  }

  @Override

  @SuppressWarnings("NoSynchronizedMethodCheck")
  public synchronized void dispose() {
    isDisposed = true;
  }

  @Override

  @SuppressWarnings("NoSynchronizedMethodCheck")
  public synchronized void changeCaptureFormat(
      final int width, final int height, final int ignoredFramerate) {
    checkNotDisposed();

    this.width = width;
    this.height = height;

    if (virtualDisplay == null) {

      return;
    }

    ThreadUtils.invokeAtFrontUninterruptibly(surfaceTextureHelper.getHandler(), () -> {
      virtualDisplay.release();
      createVirtualDisplay();
    });
  }

  private void createVirtualDisplay() {
    surfaceTextureHelper.setTextureSize(width, height);
    try {
      virtualDisplay = mediaProjection.createVirtualDisplay("WebRTC_ScreenCapture", width, height,
          VIRTUAL_DISPLAY_DPI, DISPLAY_FLAGS, new Surface(surfaceTextureHelper.getSurfaceTexture()),
          null               , null                       );
    } catch (Throwable e) {
      FileLog.e(e);
    }
  }

  @Override
  public void onFrame(VideoFrame frame) {
    numCapturedFrames++;
    capturerObserver.onFrameCaptured(frame);
  }

  @Override
  public boolean isScreencast() {
    return true;
  }

  public long getNumCapturedFrames() {
    return numCapturedFrames;
  }
}

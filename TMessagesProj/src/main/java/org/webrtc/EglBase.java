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

import android.graphics.SurfaceTexture;
import androidx.annotation.Nullable;
import android.view.Surface;
import java.util.ArrayList;
import javax.microedition.khronos.egl.EGL10;

public interface EglBase {

  public interface Context {
    public final static long NO_CONTEXT = 0;

    long getNativeEglContext();
  }

  public static final Object lock = new Object();

  public static final int EGL_OPENGL_ES2_BIT = 4;
  public static final int EGL_OPENGL_ES3_BIT = 0x40;

  public static final int EGL_RECORDABLE_ANDROID = 0x3142;

  public static ConfigBuilder configBuilder() {
    return new ConfigBuilder();
  }

  public static class ConfigBuilder {
    private int openGlesVersion = 2;
    private boolean hasAlphaChannel;
    private boolean supportsPixelBuffer;
    private boolean isRecordable;

    public ConfigBuilder setOpenGlesVersion(int version) {
      if (version < 1 || version > 3) {
        throw new IllegalArgumentException("OpenGL ES version " + version + " not supported");
      }
      this.openGlesVersion = version;
      return this;
    }

    public ConfigBuilder setHasAlphaChannel(boolean hasAlphaChannel) {
      this.hasAlphaChannel = hasAlphaChannel;
      return this;
    }

    public ConfigBuilder setSupportsPixelBuffer(boolean supportsPixelBuffer) {
      this.supportsPixelBuffer = supportsPixelBuffer;
      return this;
    }

    public ConfigBuilder setIsRecordable(boolean isRecordable) {
      this.isRecordable = isRecordable;
      return this;
    }

    public int[] createConfigAttributes() {
      ArrayList<Integer> list = new ArrayList<>();
      list.add(EGL10.EGL_RED_SIZE);
      list.add(8);
      list.add(EGL10.EGL_GREEN_SIZE);
      list.add(8);
      list.add(EGL10.EGL_BLUE_SIZE);
      list.add(8);
      if (hasAlphaChannel) {
        list.add(EGL10.EGL_ALPHA_SIZE);
        list.add(8);
      }
      if (openGlesVersion == 2 || openGlesVersion == 3) {
        list.add(EGL10.EGL_RENDERABLE_TYPE);
        list.add(openGlesVersion == 3 ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_ES2_BIT);
      }
      if (supportsPixelBuffer) {
        list.add(EGL10.EGL_SURFACE_TYPE);
        list.add(EGL10.EGL_PBUFFER_BIT);
      }
      if (isRecordable) {
        list.add(EGL_RECORDABLE_ANDROID);
        list.add(1);
      }
      list.add(EGL10.EGL_NONE);

      final int[] res = new int[list.size()];
      for (int i = 0; i < list.size(); ++i) {
        res[i] = list.get(i);
      }
      return res;
    }
  }

  public static final int[] CONFIG_PLAIN = configBuilder().createConfigAttributes();
  public static final int[] CONFIG_RGBA =
      configBuilder().setHasAlphaChannel(true).createConfigAttributes();
  public static final int[] CONFIG_PIXEL_BUFFER =
      configBuilder().setSupportsPixelBuffer(true).createConfigAttributes();
  public static final int[] CONFIG_PIXEL_RGBA_BUFFER = configBuilder()
                                                           .setHasAlphaChannel(true)
                                                           .setSupportsPixelBuffer(true)
                                                           .createConfigAttributes();
  public static final int[] CONFIG_RECORDABLE =
      configBuilder().setIsRecordable(true).createConfigAttributes();

  static int getOpenGlesVersionFromConfig(int[] configAttributes) {
    for (int i = 0; i < configAttributes.length - 1; ++i) {
      if (configAttributes[i] == EGL10.EGL_RENDERABLE_TYPE) {
        switch (configAttributes[i + 1]) {
          case EGL_OPENGL_ES2_BIT:
            return 2;
          case EGL_OPENGL_ES3_BIT:
            return 3;
          default:
            return 1;
        }
      }
    }

    return 1;
  }

  public static EglBase create(@Nullable Context sharedContext, int[] configAttributes) {
    if (sharedContext == null) {
      return EglBase14Impl.isEGL14Supported() ? createEgl14(configAttributes)
                                              : createEgl10(configAttributes);
    } else if (sharedContext instanceof EglBase14.Context) {
      return createEgl14((EglBase14.Context) sharedContext, configAttributes);
    } else if (sharedContext instanceof EglBase10.Context) {
      return createEgl10((EglBase10.Context) sharedContext, configAttributes);
    }
    throw new IllegalArgumentException("Unrecognized Context");
  }

  public static EglBase create() {
    return create(null                    , CONFIG_PLAIN);
  }

  public static EglBase create(Context sharedContext) {
    return create(sharedContext, CONFIG_PLAIN);
  }

  public static EglBase10 createEgl10(int[] configAttributes) {
    return new EglBase10Impl(                     null, configAttributes);
  }

  public static EglBase10 createEgl10(EglBase10.Context sharedContext, int[] configAttributes) {
    return new EglBase10Impl(
        sharedContext == null ? null : sharedContext.getRawContext(), configAttributes);
  }

  public static EglBase10 createEgl10(
      javax.microedition.khronos.egl.EGLContext sharedContext, int[] configAttributes) {
    return new EglBase10Impl(sharedContext, configAttributes);
  }

  public static EglBase14 createEgl14(int[] configAttributes) {
    return new EglBase14Impl(                     null, configAttributes);
  }

  public static EglBase14 createEgl14(EglBase14.Context sharedContext, int[] configAttributes) {
    return new EglBase14Impl(
        sharedContext == null ? null : sharedContext.getRawContext(), configAttributes);
  }

  public static EglBase14 createEgl14(
      android.opengl.EGLContext sharedContext, int[] configAttributes) {
    return new EglBase14Impl(sharedContext, configAttributes);
  }

  void createSurface(Surface surface);

  void createSurface(SurfaceTexture surfaceTexture);

  void createDummyPbufferSurface();

  void createPbufferSurface(int width, int height);

  Context getEglBaseContext();

  boolean hasSurface();

  int surfaceWidth();

  int surfaceHeight();

  void releaseSurface(boolean background);

  void release();

  void makeCurrent();

  void detachCurrent();

  void swapBuffers(boolean background);

  void swapBuffers(long presentationTimeStampNs,boolean background);

  void createBackgroundSurface(SurfaceTexture surface);

  void makeBackgroundCurrent();

  boolean hasBackgroundSurface();
}

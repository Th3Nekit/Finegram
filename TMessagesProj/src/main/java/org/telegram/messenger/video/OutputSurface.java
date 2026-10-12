/*
 * This is the source code of Telegram for Android v. 6.x.x.
 * It is licensed under GNU GPL v. 2 or later.
 * You should have received a copy of the license in this archive (see LICENSE).
 *
 * Copyright Nikolai Kudashov, 2013-2020.
 */

package org.telegram.messenger.video;

import android.graphics.SurfaceTexture;
import android.opengl.EGL14;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLSurface;
import android.opengl.GLES20;
import android.os.Handler;
import android.view.Surface;

import org.telegram.messenger.FileLog;
import org.telegram.messenger.MediaController;
import org.telegram.messenger.VideoEditedInfo;
import org.telegram.ui.Stories.recorder.StoryEntry;

import java.util.ArrayList;
import java.util.concurrent.CountDownLatch;

import javax.microedition.khronos.egl.EGL10;

public class OutputSurface implements SurfaceTexture.OnFrameAvailableListener {

    private static final int EGL_OPENGL_ES2_BIT = 4;
    private static final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
    private EGL10 mEGL;
    private EGLDisplay mEGLDisplay = null;
    private EGLContext mEGLContext = null;
    private EGLSurface mEGLSurface = null;
    private SurfaceTexture mSurfaceTexture;
    private Surface mSurface;
    private final Object mFrameSyncObject = new Object();
    private boolean mFrameAvailable;
    private TextureRenderer mTextureRender;

    public OutputSurface(MediaController.SavedFilterState savedFilterState, String imagePath, String paintPath, String blurPath, ArrayList<VideoEditedInfo.MediaEntity> mediaEntities, MediaController.CropState cropState, int w, int h, int originalW, int originalH, int rotation, float fps, boolean photo, Integer gradientTopColor, Integer gradientBottomColor, StoryEntry.HDRInfo hdrInfo, MediaCodecVideoConvertor.ConvertVideoParams params) {
        mTextureRender = new TextureRenderer(savedFilterState, imagePath, paintPath, blurPath, mediaEntities, cropState, w, h, originalW, originalH, rotation, fps, photo, gradientTopColor, gradientBottomColor, hdrInfo, params);
        mTextureRender.surfaceCreated();
        mSurfaceTexture = new SurfaceTexture(mTextureRender.getTextureId());
        mSurfaceTexture.setOnFrameAvailableListener(this);
        mSurface = new Surface(mSurfaceTexture);
    }

    public void release() {

        if (mTextureRender != null) {
            mTextureRender.release();
        }
        mSurface.release();
        mEGLDisplay = null;
        mEGLContext = null;
        mEGLSurface = null;
        mEGL = null;
        mTextureRender = null;
        mSurface = null;
        mSurfaceTexture = null;
    }

    public Surface getSurface() {
        return mSurface;
    }

    public void awaitNewImage() {
        final int TIMEOUT_MS = 2500;
        synchronized (mFrameSyncObject) {
            while (!mFrameAvailable) {
                try {
                    mFrameSyncObject.wait(TIMEOUT_MS);
                    if (!mFrameAvailable) {
                        throw new RuntimeException("Surface frame wait timed out");
                    }
                } catch (InterruptedException ie) {
                    throw new RuntimeException(ie);
                }
            }
            mFrameAvailable = false;
        }
        mSurfaceTexture.updateTexImage();
    }

    public void drawImage(long time) {
        mTextureRender.drawFrame(mSurfaceTexture, time);
    }

    @Override
    public void onFrameAvailable(SurfaceTexture st) {
        synchronized (mFrameSyncObject) {
            if (mFrameAvailable) {
                throw new RuntimeException("mFrameAvailable already set, frame could be dropped");
            }
            mFrameAvailable = true;
            mFrameSyncObject.notifyAll();
        }
    }

    private void checkEglError(String msg) {
        if (EGL14.eglGetError() != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("EGL error encountered (see log) at: " + msg);
        }
    }

    public void changeFragmentShader(String fragmentExternalShader, String fragmentShader, boolean is300) {
        mTextureRender.changeFragmentShader(fragmentExternalShader, fragmentShader, is300);
    }

    public boolean supportsEXTYUV() {
        try {
            String extensions = GLES20.glGetString(GLES20.GL_EXTENSIONS);
            return extensions.contains("GL_EXT_YUV_target");
        } catch (Exception e) {
            FileLog.e(e);
        }
        return false;
    }
}

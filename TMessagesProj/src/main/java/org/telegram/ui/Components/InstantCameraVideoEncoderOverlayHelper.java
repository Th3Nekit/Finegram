package org.telegram.ui.Components;

import android.graphics.Bitmap;
import android.opengl.GLES20;
import android.opengl.GLUtils;

import androidx.annotation.RawRes;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.FileLog;
import org.telegram.messenger.R;

import com.th3nekit.finegram.core.configs.FinegramCameraConfig;

import org.telegram.utils.recorder.round.RoundVideoWatermark;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

public class InstantCameraVideoEncoderOverlayHelper {
    private final static int DOWNSCALED_WIDTH = 48, DOWNSCALED_HEIGHT = 48;

    private static final int TEXTURE_INDEX_FULL_SAMPLE = 0;
    private static final int TEXTURE_INDEX_DOWN_SAMPLE = 1;
    private static final int TEXTURE_INDEX_DOWN_SAMPLE_TMP = 2;
    private static final int TEXTURE_INDEX_WATERMARK = 3;

    private static final int VERTEX_BUFFER_NORMAL_POSITION = 0;
    private static final int TEXTURE_BUFFER_NORMAL_POSITION = 0;
    private static final int TEXTURE_BUFFER_Y_REVERSE_POSITION = 8;

    private final int videoWidth, videoHeight;

    private final Program programRenderTexture = new Program(R.raw.round_blur_stage_0_frag);
    private final BlurProgram programRenderBlur = new BlurProgram();
    private final MixProgram programRenderMixed = new MixProgram();

    private final FloatBuffer attributeVertexBuffer;
    private final FloatBuffer attributeTextureBuffer;

    private final boolean watermarkEnabled;

    private final int[] glFrameBuffers = new int[1];
    private final int[] glTextures;
    private final com.th3nekit.finegram.camera.roundfx.FGRoundFxRenderer roundFx =
            new com.th3nekit.finegram.camera.roundfx.FGRoundFxRenderer(
                    com.th3nekit.finegram.camera.roundfx.FGRoundFx.shared(), false);

    public InstantCameraVideoEncoderOverlayHelper(int width, int height) {
        this.videoWidth = width;
        this.videoHeight = height;
        watermarkEnabled = FinegramCameraConfig.INSTANCE.getVideoMessageWatermark();
        glTextures = new int[watermarkEnabled ? 4 : 3];

        final float[] texData = new float[16];
        setTextureCords(texData, TEXTURE_BUFFER_NORMAL_POSITION, 0, 1, 1, 0);
        setTextureCords(texData, TEXTURE_BUFFER_Y_REVERSE_POSITION, 0, 0, 1, 1);

        final float[] verData = new float[12];
        setVertexCords(verData, VERTEX_BUFFER_NORMAL_POSITION, -1, 1, 1, -1);

        GLES20.glGenTextures(glTextures.length, glTextures, 0);
        for (int i = 0; i < glTextures.length; i++) {
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, glTextures[i]);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, i < 2 || i == TEXTURE_INDEX_WATERMARK ? GLES20.GL_LINEAR : GLES20.GL_NEAREST);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, i < 2 || i == TEXTURE_INDEX_WATERMARK ? GLES20.GL_LINEAR : GLES20.GL_NEAREST);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);

            if (i == TEXTURE_INDEX_WATERMARK) {
                Bitmap bitmap = RoundVideoWatermark.createBitmap(Math.min(width, height));
                try {
                    GLUtils.texImage2D(GLES20.GL_TEXTURE_2D, 0, bitmap, 0);
                } finally {
                    bitmap.recycle();
                }
            } else  {
                GLES20.glTexImage2D(
                        GLES20.GL_TEXTURE_2D,
                        0,
                        GLES20.GL_RGBA,
                        i == 0 ? videoWidth : DOWNSCALED_WIDTH,
                        i == 0 ? videoHeight : DOWNSCALED_HEIGHT,
                        0,
                        GLES20.GL_RGBA,
                        GLES20.GL_UNSIGNED_BYTE,
                        null
                );
            }
        }
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
        GLES20.glGenFramebuffers(1, glFrameBuffers, 0);

        attributeVertexBuffer = ByteBuffer.allocateDirect(verData.length * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
        attributeVertexBuffer.put(verData).position(0);

        attributeTextureBuffer = ByteBuffer.allocateDirect(texData.length * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
        attributeTextureBuffer.put(texData).position(0);
    }

    public void bind() {
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, glFrameBuffers[0]);
        GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER, GLES20.GL_COLOR_ATTACHMENT0, GLES20.GL_TEXTURE_2D, glTextures[TEXTURE_INDEX_FULL_SAMPLE], 0);
        GLES20.glViewport(0, 0, videoWidth, videoHeight);
    }

    public void render() {
        final int fullTexture = roundFx.process(glTextures[TEXTURE_INDEX_FULL_SAMPLE], videoWidth, videoHeight);
        GLES20.glDisable(GLES20.GL_BLEND);

        {
            final Program program = programRenderTexture;

            GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER, GLES20.GL_COLOR_ATTACHMENT0, GLES20.GL_TEXTURE_2D, glTextures[TEXTURE_INDEX_DOWN_SAMPLE], 0);
            GLES20.glViewport(0, 0, DOWNSCALED_WIDTH, DOWNSCALED_HEIGHT);

            GLES20.glUseProgram(program.program);
            GLES20.glVertexAttribPointer(program.attributePositionHandle, 3, GLES20.GL_FLOAT, false, 12, attributeVertexBuffer.position(VERTEX_BUFFER_NORMAL_POSITION));
            GLES20.glEnableVertexAttribArray(program.attributePositionHandle);
            GLES20.glVertexAttribPointer(program.attributeTextureHandle, 2, GLES20.GL_FLOAT, false, 8, attributeTextureBuffer.position(TEXTURE_BUFFER_NORMAL_POSITION));
            GLES20.glEnableVertexAttribArray(program.attributeTextureHandle);

            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, fullTexture);

            GLES20.glUniform1i(program.uniformTextureHandle, 0);

            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
            GLES20.glDisableVertexAttribArray(program.attributeTextureHandle);
            GLES20.glDisableVertexAttribArray(program.attributePositionHandle);
            GLES20.glUseProgram(0);
        }

        for (int a = 0; a < 2; a++) {
            final BlurProgram program = programRenderBlur;

            GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER, GLES20.GL_COLOR_ATTACHMENT0, GLES20.GL_TEXTURE_2D, glTextures[a == 0 ? TEXTURE_INDEX_DOWN_SAMPLE_TMP : TEXTURE_INDEX_DOWN_SAMPLE], 0);
            GLES20.glViewport(0, 0, DOWNSCALED_WIDTH, DOWNSCALED_HEIGHT);

            GLES20.glUseProgram(program.program);
            GLES20.glVertexAttribPointer(program.attributePositionHandle, 3, GLES20.GL_FLOAT, false, 12, attributeVertexBuffer.position(VERTEX_BUFFER_NORMAL_POSITION));
            GLES20.glEnableVertexAttribArray(program.attributePositionHandle);
            GLES20.glVertexAttribPointer(program.attributeTextureHandle, 2, GLES20.GL_FLOAT, false, 8, attributeTextureBuffer.position(TEXTURE_BUFFER_NORMAL_POSITION));
            GLES20.glEnableVertexAttribArray(program.attributeTextureHandle);

            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, glTextures[a == 0 ? TEXTURE_INDEX_DOWN_SAMPLE : TEXTURE_INDEX_DOWN_SAMPLE_TMP]);

            GLES20.glUniform1i(program.uniformTextureHandle, 0);
            GLES20.glUniform2f(program.uniformOffsetHandle, a == 0 ? 1f / DOWNSCALED_WIDTH : 0f, a == 1 ? 1f / DOWNSCALED_HEIGHT : 0f);

            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
            GLES20.glDisableVertexAttribArray(program.attributeTextureHandle);
            GLES20.glDisableVertexAttribArray(program.attributePositionHandle);
            GLES20.glUseProgram(0);
        }

        {
            final MixProgram program = programRenderMixed;

            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
            GLES20.glViewport(0, 0, videoWidth, videoHeight);

            GLES20.glUseProgram(program.program);
            GLES20.glVertexAttribPointer(program.attributePositionHandle, 3, GLES20.GL_FLOAT, false, 12, attributeVertexBuffer.position(VERTEX_BUFFER_NORMAL_POSITION));
            GLES20.glEnableVertexAttribArray(program.attributePositionHandle);
            GLES20.glVertexAttribPointer(program.attributeTextureHandle, 2, GLES20.GL_FLOAT, false, 8, attributeTextureBuffer.position(TEXTURE_BUFFER_NORMAL_POSITION));
            GLES20.glEnableVertexAttribArray(program.attributeTextureHandle);

            GLES20.glActiveTexture(GLES20.GL_TEXTURE1);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, glTextures[TEXTURE_INDEX_DOWN_SAMPLE]);
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, fullTexture);

            GLES20.glUniform1i(program.uniformTextureHandle, 0);
            GLES20.glUniform1i(program.uniformBlurredTextureHandle, 1);
            GLES20.glUniform2f(program.uniformHalfResolutionHandle, videoWidth / 2f, videoHeight / 2f);

            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

            GLES20.glActiveTexture(GLES20.GL_TEXTURE1);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
            GLES20.glDisableVertexAttribArray(program.attributeTextureHandle);
            GLES20.glDisableVertexAttribArray(program.attributePositionHandle);
            GLES20.glUseProgram(0);
        }

        if (watermarkEnabled) {
            final Program program = programRenderTexture;
            GLES20.glEnable(GLES20.GL_BLEND);
            GLES20.glBlendFunc(GLES20.GL_ONE, GLES20.GL_ONE_MINUS_SRC_ALPHA);
            GLES20.glUseProgram(program.program);
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            GLES20.glVertexAttribPointer(program.attributePositionHandle, 3, GLES20.GL_FLOAT, false, 12, attributeVertexBuffer.position(VERTEX_BUFFER_NORMAL_POSITION));
            GLES20.glEnableVertexAttribArray(program.attributePositionHandle);
            GLES20.glVertexAttribPointer(program.attributeTextureHandle, 2, GLES20.GL_FLOAT, false, 8, attributeTextureBuffer.position(TEXTURE_BUFFER_Y_REVERSE_POSITION));
            GLES20.glEnableVertexAttribArray(program.attributeTextureHandle);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, glTextures[TEXTURE_INDEX_WATERMARK]);
            GLES20.glUniform1i(program.uniformTextureHandle, 0);
            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
            GLES20.glDisableVertexAttribArray(program.attributeTextureHandle);
            GLES20.glDisableVertexAttribArray(program.attributePositionHandle);
            GLES20.glUseProgram(0);
            GLES20.glBlendFunc(GLES20.GL_SRC_ALPHA, GLES20.GL_ONE_MINUS_SRC_ALPHA);
            GLES20.glDisable(GLES20.GL_BLEND);
        }
    }

    public void destroy() {
        roundFx.release();
        programRenderTexture.destroy();
        programRenderBlur.destroy();
        programRenderMixed.destroy();

        GLES20.glDeleteTextures(glTextures.length, glTextures, 0);
        GLES20.glDeleteFramebuffers(1, glFrameBuffers, 0);
    }

    private static class MixProgram extends Program {
        final int uniformBlurredTextureHandle;
        final int uniformHalfResolutionHandle;

        public MixProgram() {
            super(R.raw.round_blur_stage_2_frag);
            uniformBlurredTextureHandle = GLES20.glGetUniformLocation(program, "bTexture");
            uniformHalfResolutionHandle = GLES20.glGetUniformLocation(program, "center");
        }
    }

    private static class BlurProgram extends Program {
        final int uniformOffsetHandle;

        public BlurProgram() {
            super(R.raw.round_blur_stage_1_frag);
            uniformOffsetHandle = GLES20.glGetUniformLocation(program, "texOffset");
        }
    }

    private static class Program {
        final int program;
        final int vertexShader;
        final int fragmentShader;

        final int attributePositionHandle;
        final int attributeTextureHandle;
        final int uniformTextureHandle;

        public Program(@RawRes int fragmentShaderRes) {
            vertexShader = createShader(GLES20.GL_VERTEX_SHADER, R.raw.round_blur_vert);
            fragmentShader = createShader(GLES20.GL_FRAGMENT_SHADER, fragmentShaderRes);

            program = createProgram(vertexShader, fragmentShader);
            attributePositionHandle = GLES20.glGetAttribLocation(program, "aPosition");
            attributeTextureHandle = GLES20.glGetAttribLocation(program, "aTextureCoord");
            uniformTextureHandle = GLES20.glGetUniformLocation(program, "sTexture");
        }

        public void destroy() {
            GLES20.glDeleteProgram(program);
            GLES20.glDeleteShader(vertexShader);
            GLES20.glDeleteShader(fragmentShader);
        }
    }

    private static int createShader(int type, @RawRes int shaderRes) {
        final int shader = GLES20.glCreateShader(type);
        if (shader == 0) {
            return 0;
        }

        GLES20.glShaderSource(shader, AndroidUtilities.readRes(shaderRes));
        GLES20.glCompileShader(shader);

        final int[] status = new int[1];
        GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, status, 0);
        if (status[0] == 0) {
            String err = GLES20.glGetShaderInfoLog(shader);
            FileLog.e("GlUtils: compile shader error: " + err);
            GLES20.glDeleteShader(shader);
            return 0;
        }

        return shader;
    }

    private static int createProgram(int vertexShader, int fragmentShader) {
        final int program = GLES20.glCreateProgram();
        GLES20.glAttachShader(program, vertexShader);
        GLES20.glAttachShader(program, fragmentShader);
        GLES20.glLinkProgram(program);

        int[] linkStatus = new int[1];
        GLES20.glGetProgramiv(program, GLES20.GL_LINK_STATUS, linkStatus, 0);
        if (linkStatus[0] == 0) {
            GLES20.glDeleteProgram(program);
            return 0;
        }

        return program;
    }

    private static void setVertexCords(float[] buffer, int offset, float left, float top, float right, float bottom) {
        buffer[offset    ] = left;
        buffer[offset + 1] = bottom;
        buffer[offset + 2] = 0f;

        buffer[offset + 3] = right;
        buffer[offset + 4] = bottom;
        buffer[offset + 5] = 0f;

        buffer[offset + 6] = left;
        buffer[offset + 7] = top;
        buffer[offset + 8] = 0f;

        buffer[offset + 9] = right;
        buffer[offset + 10] = top;
        buffer[offset + 11] = 0f;
    }

    private static void setTextureCords(float[] buffer, int offset, float left, float top, float right, float bottom) {
        buffer[offset    ] = left;
        buffer[offset + 1] = bottom;

        buffer[offset + 2] = right;
        buffer[offset + 3] = bottom;

        buffer[offset + 4] = left;
        buffer[offset + 5] = top;

        buffer[offset + 6] = right;
        buffer[offset + 7] = top;
    }
}

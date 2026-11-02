                                 

package com.th3nekit.finegram.camera;

import android.opengl.GLES20;

import org.telegram.messenger.FileLog;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

public final class CameraXFrameCrossfade {
    public static final class Handoff {
        public final Object owner;
        public final int sourceGeneration;
        public final long submittedAtMs;
        private int confirmedTargetGeneration;
        private volatile long confirmedTargetTimestampNanos;

        public Handoff(Object owner, int sourceGeneration, long submittedAtMs) {
            this.owner = owner;
            this.sourceGeneration = sourceGeneration;
            this.submittedAtMs = submittedAtMs;
        }

        public boolean isValidFor(Object owner, long nowMs) {
            return owner != null && this.owner == owner && submittedAtMs >= 0
                    && nowMs >= submittedAtMs && nowMs - submittedAtMs <= HANDOFF_MAX_AGE_MS;
        }

        public synchronized void confirmTargetFrame(long timestampNanos, int generation) {
            if (timestampNanos <= 0 || generation < sourceGeneration
                    || confirmedTargetTimestampNanos != 0) return;
            confirmedTargetGeneration = generation;
            confirmedTargetTimestampNanos = timestampNanos;
        }
    }

    private static final long HANDOFF_MAX_AGE_MS = 2500;
    private static final long HANDOFF_DURATION_MS = 160;
    private static final long HANDOFF_WAIT_MS = 1000;
    private static final String VERTEX = "attribute vec2 position; varying vec2 uv;"
            + "void main(){uv=position*0.5+0.5;gl_Position=vec4(position,0.0,1.0);}";
    private static final String FRAGMENT = "precision mediump float; varying vec2 uv;"
            + "uniform sampler2D image; uniform float opacity;"
            + "void main(){gl_FragColor=vec4(texture2D(image,uv).rgb,opacity);}";
    private final FloatBuffer vertices = ByteBuffer.allocateDirect(32)
            .order(ByteOrder.nativeOrder()).asFloatBuffer();
    private final int[] textures = new int[2];
    private final int[] framebuffer = new int[1];
    private final int[] savedFramebuffer = new int[1];
    private final int[] savedViewport = new int[4];
    private final int[] blend = new int[4];
    private final int[] status = new int[1];
    private final int[] retainedState = new int[10];
    private int program;
    private int position;
    private int opacity;
    private int width;
    private int height;
    private int previous = -1;
    private int lastPresented = -1;
    private int target;
    private Object owner;
    private int generation;
    private long previousTimestamp;
    private long timestamp;
    private boolean blending;
    private boolean failed;
    private Handoff handoff;
    private boolean handoffConsumed;
    private boolean handoffWaiting;
    private long handoffStartedAtMs = -1;
    private long handoffLastFrameMs;
    private int handoffGeneration;
    private final boolean[] checkedTargets = new boolean[2];

    public CameraXFrameCrossfade() {
        vertices.put(new float[]{-1f, -1f, 1f, -1f, -1f, 1f, 1f, 1f}).position(0);
    }

    public boolean drawRetainedFrame(Object owner, Handoff request, long nowMs, int width, int height) {
        if (failed || program == 0 || previous < 0 || lastPresented < 0 || this.owner != owner
                || width <= 0 || height <= 0 || width != this.width || height != this.height
                || request == null || !request.isValidFor(owner, nowMs)) return false;
        boolean activeHandoff = request == handoff && handoffStartedAtMs >= 0;
        if (activeHandoff ? generation != handoffGeneration : generation != request.sourceGeneration) return false;
        if (request == handoff) {
            if ((handoffConsumed && !activeHandoff)
                    || ((handoffWaiting || activeHandoff) && nowMs < handoffLastFrameMs)) return false;
        } else if (handoff != null && (request.submittedAtMs < handoff.submittedAtMs
                || (request.submittedAtMs == handoff.submittedAtMs
                && (request.owner != handoff.owner || request.sourceGeneration <= handoff.sourceGeneration)))) {
            return false;
        }
        if (nowMs - request.submittedAtMs >= HANDOFF_WAIT_MS) {
            updateHandoff(request);
            resetHistory();
            return false;
        }
        GLES20.glGetIntegerv(GLES20.GL_FRAMEBUFFER_BINDING, retainedState, 0);
        GLES20.glGetIntegerv(GLES20.GL_VIEWPORT, retainedState, 1);
        GLES20.glGetIntegerv(GLES20.GL_CURRENT_PROGRAM, retainedState, 5);
        GLES20.glGetIntegerv(GLES20.GL_ACTIVE_TEXTURE, retainedState, 6);
        GLES20.glGetIntegerv(GLES20.GL_ARRAY_BUFFER_BINDING, retainedState, 9);
        GLES20.glGetVertexAttribiv(position, GLES20.GL_VERTEX_ATTRIB_ARRAY_ENABLED, retainedState, 8);
        boolean retainedBlend = GLES20.glIsEnabled(GLES20.GL_BLEND);
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glGetIntegerv(GLES20.GL_TEXTURE_BINDING_2D, retainedState, 7);
        try {
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, retainedState[0]);
            GLES20.glViewport(retainedState[1], retainedState[2], retainedState[3], retainedState[4]);
            GLES20.glDisable(GLES20.GL_BLEND);
            GLES20.glUseProgram(program);
            GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, 0);
            vertices.position(0);
            GLES20.glVertexAttribPointer(position, 2, GLES20.GL_FLOAT, false, 8, vertices);
            GLES20.glEnableVertexAttribArray(position);
            draw(textures[lastPresented], 1f);
            if (GLES20.glGetError() != GLES20.GL_NO_ERROR) {
                throw new IllegalStateException("Retained camera frame draw failed");
            }
            return true;
        } catch (RuntimeException e) {
            release();
            failed = true;
            FileLog.e(e);
            return false;
        } finally {
            if (retainedState[8] == 0) GLES20.glDisableVertexAttribArray(position);
            GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, retainedState[9]);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, retainedState[7]);
            GLES20.glActiveTexture(retainedState[6]);
            GLES20.glUseProgram(retainedState[5]);
            if (retainedBlend) GLES20.glEnable(GLES20.GL_BLEND);
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, retainedState[0]);
            GLES20.glViewport(retainedState[1], retainedState[2], retainedState[3], retainedState[4]);
        }
    }

    public boolean begin(int width, int height, long timestamp, Object owner, int generation) {
        return begin(width, height, timestamp, owner, generation, 1f);
    }

    public boolean begin(int width, int height, long timestamp, Object owner, int generation, float strength) {
        return begin(width, height, timestamp, owner, generation, strength, null, 0);
    }

    public boolean begin(int width, int height, long timestamp, Object owner, int generation,
                         float strength, Handoff request, long nowMs) {
        updateHandoff(request);
        if (failed || width <= 0 || height <= 0 || timestamp <= 0 || owner == null) {
            if (!failed && width == this.width && height == this.height && width > 0 && height > 0
                    && timestamp <= 0 && previous >= 0 && this.owner == owner
                    && request != null && request == handoff && !handoffConsumed
                    && request.isValidFor(owner, nowMs) && this.generation == request.sourceGeneration
                    && generation >= request.sourceGeneration
                    && (!handoffWaiting || nowMs >= handoffLastFrameMs)) return false;
            resetHistory();
            return false;
        }
        GLES20.glGetIntegerv(GLES20.GL_FRAMEBUFFER_BINDING, savedFramebuffer, 0);
        GLES20.glGetIntegerv(GLES20.GL_VIEWPORT, savedViewport, 0);
        blending = GLES20.glIsEnabled(GLES20.GL_BLEND);
        GLES20.glGetIntegerv(GLES20.GL_BLEND_SRC_RGB, blend, 0);
        GLES20.glGetIntegerv(GLES20.GL_BLEND_DST_RGB, blend, 1);
        GLES20.glGetIntegerv(GLES20.GL_BLEND_SRC_ALPHA, blend, 2);
        GLES20.glGetIntegerv(GLES20.GL_BLEND_DST_ALPHA, blend, 3);
        try {
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            if (this.width != width || this.height != height || program == 0) {
                release();
                this.width = width;
                this.height = height;
                createProgram();
                GLES20.glGenTextures(2, textures, 0);
                for (int texture : textures) {
                    GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, texture);
                    GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
                    GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
                    GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
                    GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
                    GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA,
                            width, height, 0, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, null);
                }
                GLES20.glGenFramebuffers(1, framebuffer, 0);
            }
            boolean validRequest = request != null && request == handoff && request.isValidFor(owner, nowMs);
            if (!validRequest || ((handoffStartedAtMs >= 0 || handoffWaiting)
                    && (this.owner != owner || nowMs < handoffLastFrameMs))
                    || (handoffStartedAtMs >= 0
                    && (generation != handoffGeneration || timestamp < previousTimestamp))) {
                finishHandoff();
            }
            handoffWaiting = false;
            boolean bridge = false;
            if (validRequest && !handoffConsumed) {
                if (previous < 0 || this.owner != owner || this.generation != request.sourceGeneration
                        || generation < request.sourceGeneration) {
                    finishHandoff();
                } else {
                    long confirmedTimestamp = request.confirmedTargetTimestampNanos;
                    boolean confirmed = confirmedTimestamp > 0
                            && request.confirmedTargetGeneration == generation && timestamp >= confirmedTimestamp;
                    bridge = generation > request.sourceGeneration;
                    if (bridge || confirmed || nowMs - request.submittedAtMs >= HANDOFF_WAIT_MS) {
                        handoffConsumed = true;
                        handoffStartedAtMs = nowMs;
                        handoffGeneration = generation;
                    } else {
                        handoffWaiting = true;
                    }
                }
            }
            if (this.owner != owner || (!bridge && (this.generation != generation
                    || timestamp < previousTimestamp))) {
                resetHistory();
            }
            if (handoffStartedAtMs >= 0 || handoffWaiting) handoffLastFrameMs = nowMs;
            this.owner = owner;
            this.generation = generation;
            this.timestamp = timestamp;
            target = previous == 0 ? 1 : 0;
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, framebuffer[0]);
            GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER, GLES20.GL_COLOR_ATTACHMENT0,
                    GLES20.GL_TEXTURE_2D, textures[target], 0);
            if (!checkedTargets[target]) {
                if (GLES20.glCheckFramebufferStatus(GLES20.GL_FRAMEBUFFER) != GLES20.GL_FRAMEBUFFER_COMPLETE) {
                    throw new IllegalStateException("Camera frame crossfade framebuffer is incomplete");
                }
                checkedTargets[target] = true;
            }
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
            GLES20.glViewport(0, 0, width, height);
            GLES20.glClearColor(0f, 0f, 0f, 1f);
            GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
            if (GLES20.glGetError() != GLES20.GL_NO_ERROR) {
                throw new IllegalStateException("Camera frame crossfade GL setup failed");
            }
            return true;
        } catch (RuntimeException e) {
            release();
            failed = true;
            restoreTarget();
            FileLog.e(e);
            return false;
        }
    }

    public void end(float strength) {
        GLES20.glUseProgram(program);
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        vertices.position(0);
        GLES20.glVertexAttribPointer(position, 2, GLES20.GL_FLOAT, false, 8, vertices);
        GLES20.glEnableVertexAttribArray(position);
        boolean handingOff = handoffStartedAtMs >= 0;
        if (handoffWaiting || handingOff || previous < 0 || timestamp != previousTimestamp) {
            float weight = handoffWaiting ? 1f : handingOff
                    ? Math.max(0f, 1f - (handoffLastFrameMs - handoffStartedAtMs) / (float) HANDOFF_DURATION_MS)
                    : previous < 0 ? 0f : historyWeight(strength, timestamp - previousTimestamp);
            if (weight > 0f) {
                GLES20.glEnable(GLES20.GL_BLEND);
                GLES20.glBlendFuncSeparate(GLES20.GL_SRC_ALPHA, GLES20.GL_ONE_MINUS_SRC_ALPHA,
                        GLES20.GL_ONE, GLES20.GL_ONE_MINUS_SRC_ALPHA);
                draw(textures[previous], weight);
            }
            lastPresented = target;
            if ((!handingOff && !handoffWaiting) || weight <= 0f) {
                previous = target;
                if (handingOff) finishHandoff();
            }
            previousTimestamp = timestamp;
        }
        restoreTarget();
        GLES20.glDisable(GLES20.GL_BLEND);
        draw(textures[lastPresented], 1f);
        GLES20.glDisableVertexAttribArray(position);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
        GLES20.glUseProgram(0);
        GLES20.glBlendFuncSeparate(blend[0], blend[1], blend[2], blend[3]);
        if (blending) GLES20.glEnable(GLES20.GL_BLEND);
    }

    public static float historyWeight(float strength, long deltaNanos) {
        if (Float.isNaN(strength) || Float.isInfinite(strength)
                || deltaNanos <= 0 || deltaNanos > 200_000_000L) return 0f;
        double weight = Math.max(0f, Math.min(CameraXRoundLensTransition.MAX_STRENGTH, strength));
        return (float) Math.min(CameraXRoundLensTransition.MAX_STRENGTH,
                Math.pow(weight, deltaNanos / 16_666_667.0));
    }

    public void resetHistory() {
        finishHandoff();
        previous = -1;
        lastPresented = -1;
        previousTimestamp = 0;
        owner = null;
    }

    private void updateHandoff(Handoff request) {
        if (request == handoff) return;
        finishHandoff();
        if (request == null) return;
        if (handoff != null && (request.submittedAtMs < handoff.submittedAtMs
                || (request.submittedAtMs == handoff.submittedAtMs
                && (request.owner != handoff.owner || request.sourceGeneration <= handoff.sourceGeneration)))) {
            return;
        }
        handoff = request;
        handoffConsumed = false;
    }

    private void finishHandoff() {
        if (lastPresented >= 0) previous = lastPresented;
        handoffStartedAtMs = -1;
        handoffWaiting = false;
        handoffConsumed = true;
    }

    public void release() {
        resetHistory();
        GLES20.glDeleteTextures(2, textures, 0);
        textures[0] = textures[1] = 0;
        checkedTargets[0] = checkedTargets[1] = false;
        if (framebuffer[0] != 0) GLES20.glDeleteFramebuffers(1, framebuffer, 0);
        framebuffer[0] = 0;
        if (program != 0) GLES20.glDeleteProgram(program);
        program = 0;
        width = height = 0;
    }

    private void restoreTarget() {
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, savedFramebuffer[0]);
        GLES20.glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
    }

    private void draw(int texture, float alpha) {
        GLES20.glUniform1f(opacity, alpha);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, texture);
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
    }

    private int shader(int type, String source) {
        int shader = GLES20.glCreateShader(type);
        GLES20.glShaderSource(shader, source);
        GLES20.glCompileShader(shader);
        GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, status, 0);
        if (status[0] == 0) {
            String error = GLES20.glGetShaderInfoLog(shader);
            GLES20.glDeleteShader(shader);
            throw new IllegalStateException(error);
        }
        return shader;
    }

    private void createProgram() {
        int vertex = 0;
        int fragment = 0;
        try {
            vertex = shader(GLES20.GL_VERTEX_SHADER, VERTEX);
            fragment = shader(GLES20.GL_FRAGMENT_SHADER, FRAGMENT);
            program = GLES20.glCreateProgram();
            GLES20.glAttachShader(program, vertex);
            GLES20.glAttachShader(program, fragment);
            GLES20.glLinkProgram(program);
            GLES20.glGetProgramiv(program, GLES20.GL_LINK_STATUS, status, 0);
            if (status[0] == 0) throw new IllegalStateException(GLES20.glGetProgramInfoLog(program));
            position = GLES20.glGetAttribLocation(program, "position");
            opacity = GLES20.glGetUniformLocation(program, "opacity");
            GLES20.glUseProgram(program);
            GLES20.glUniform1i(GLES20.glGetUniformLocation(program, "image"), 0);
            GLES20.glUseProgram(0);
        } finally {
            if (vertex != 0) GLES20.glDeleteShader(vertex);
            if (fragment != 0) GLES20.glDeleteShader(fragment);
        }
    }
}

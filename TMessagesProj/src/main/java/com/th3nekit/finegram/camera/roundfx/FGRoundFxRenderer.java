package com.th3nekit.finegram.camera.roundfx;

import android.opengl.GLES20;
import android.os.SystemClock;

import org.telegram.messenger.FileLog;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

public final class FGRoundFxRenderer {

    private static final String VERTEX =
            "attribute vec2 aPos;\n" +
            "varying vec2 vUv;\n" +
            "void main() {\n" +
            "   vUv = aPos * 0.5 + 0.5;\n" +
            "   gl_Position = vec4(aPos, 0.0, 1.0);\n" +
            "}\n";

    private static final String HEADER =
            "#ifdef GL_FRAGMENT_PRECISION_HIGH\n" +
            "precision highp float;\n" +
            "#else\n" +
            "precision mediump float;\n" +
            "#endif\n" +
            "varying vec2 vUv;\n" +
            "uniform sampler2D sTex;\n";

    private static final String MASK =
            "uniform sampler2D sMask;\n" +
            "uniform float uMaskMix;\n" +
            "uniform float uPresence;\n" +
            "uniform vec2 uMaskShift;\n" +
            "float fallbackMask(vec2 uv) {\n" +
            "   vec2 a = vec2(0.5, -0.30);\n" +
            "   vec2 b = vec2(0.5, 0.56);\n" +
            "   vec2 pa = uv - a;\n" +
            "   vec2 ba = b - a;\n" +
            "   float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);\n" +
            "   float d = length((pa - ba * h) * vec2(1.0, 0.9));\n" +
            "   return 1.0 - smoothstep(0.19, 0.34, d);\n" +
            "}\n" +
            "float rawMask(vec2 uv) {\n" +
            "   return mix(fallbackMask(uv), smoothstep(0.22, 0.62, texture2D(sMask, uv - uMaskShift).r), uMaskMix);\n" +
            "}\n" +
            "float subjectMask(vec2 uv) {\n" +
            "   return mix(1.0, rawMask(uv), uPresence);\n" +
            "}\n";

    private static final String COPY_FRAGMENT = HEADER +
            "void main() {\n" +
            "   gl_FragColor = texture2D(sTex, vUv);\n" +
            "}\n";

    private static final String DOWN_FRAGMENT = HEADER + MASK +
            "uniform vec2 uTexel;\n" +
            "void main() {\n" +
            "   vec3 c = texture2D(sTex, vUv + vec2(-uTexel.x, -uTexel.y)).rgb;\n" +
            "   c += texture2D(sTex, vUv + vec2(uTexel.x, -uTexel.y)).rgb;\n" +
            "   c += texture2D(sTex, vUv + vec2(-uTexel.x, uTexel.y)).rgb;\n" +
            "   c += texture2D(sTex, vUv + vec2(uTexel.x, uTexel.y)).rgb;\n" +
            "   c *= 0.25;\n" +
            "   float bg = 1.0 - subjectMask(vUv);\n" +
            "   float l = dot(c, vec3(0.299, 0.587, 0.114));\n" +
            "   float a = bg * mix(0.42, 1.0, smoothstep(0.70, 0.97, l));\n" +
            "   gl_FragColor = vec4(c * a, a);\n" +
            "}\n";

    private static final String BLUR_FRAGMENT = HEADER +
            "uniform vec2 uDir;\n" +
            "void main() {\n" +
            "   vec4 s = texture2D(sTex, vUv) * 0.2270270270;\n" +
            "   s += texture2D(sTex, vUv + uDir * 1.3846153846) * 0.3162162162;\n" +
            "   s += texture2D(sTex, vUv - uDir * 1.3846153846) * 0.3162162162;\n" +
            "   s += texture2D(sTex, vUv + uDir * 3.2307692308) * 0.0702702703;\n" +
            "   s += texture2D(sTex, vUv - uDir * 3.2307692308) * 0.0702702703;\n" +
            "   gl_FragColor = s;\n" +
            "}\n";

    private static final String COMPOSITE_FRAGMENT = HEADER + MASK +
            "uniform sampler2D sBlur;\n" +
            "uniform float uBlur;\n" +
            "uniform float uLight;\n" +
            "uniform vec2 uRim;\n" +
            "void main() {\n" +
            "   vec4 scene = texture2D(sTex, vUv);\n" +
            "   vec3 c = scene.rgb;\n" +
            "   float s = subjectMask(vUv);\n" +
            "   if (uBlur > 0.001) {\n" +
            "       vec4 b = texture2D(sBlur, vUv);\n" +
            "       vec3 bg = min(b.rgb / max(b.a, 0.004), vec3(1.0));\n" +
            "       float w = uBlur * (1.0 - s) * smoothstep(0.004, 0.05, b.a);\n" +
            "       c = mix(c, bg, w);\n" +
            "   }\n" +
            "   if (uLight > 0.001) {\n" +
            "       vec3 lum = vec3(0.299, 0.587, 0.114);\n" +
            "       float l = dot(c, lum);\n" +
            "       vec2 fz = (vUv - vec2(0.5, 0.66)) * vec2(1.0, 1.2);\n" +
            "       float face = exp(-dot(fz, fz) * 5.0);\n" +
            "       vec3 subj = c + (1.0 - c) * (0.16 * (1.0 - l) * (1.0 - l) * (0.35 + 0.65 * face));\n" +
            "       vec2 kd = (vUv - vec2(0.30, 0.80)) * vec2(1.0, 1.15);\n" +
            "       float key = exp(-dot(kd, kd) * 3.2);\n" +
            "       subj *= 1.0 + 0.16 * key;\n" +
            "       subj *= vec3(1.035, 1.0, 0.95);\n" +
            "       subj = clamp(subj, 0.0, 1.0);\n" +
            "       subj = mix(subj, subj * subj * (3.0 - 2.0 * subj), 0.24);\n" +
            "       float sl = dot(subj, lum);\n" +
            "       subj = mix(vec3(sl), subj, 1.10);\n" +
            "       float edge = 1.0 - min(min(rawMask(vUv + vec2(uRim.x, 0.0)), rawMask(vUv - vec2(uRim.x, 0.0))),\n" +
            "               min(rawMask(vUv + vec2(0.0, uRim.y)), rawMask(vUv - vec2(0.0, uRim.y))));\n" +
            "       float rim = s * edge * uPresence * smoothstep(0.45, 0.92, vUv.y);\n" +
            "       subj += vec3(1.0, 0.96, 0.90) * rim * 0.10;\n" +
            "       vec3 back = c * 0.80;\n" +
            "       float bl = dot(back, lum);\n" +
            "       back = mix(vec3(bl), back, 0.82) * vec3(0.96, 0.99, 1.05);\n" +
            "       vec2 q = vUv - 0.5;\n" +
            "       float vig = 1.0 - 0.30 * smoothstep(0.10, 0.50, dot(q, q) * 2.0);\n" +
            "       vec3 graded = mix(back, subj, s) * mix(1.0, vig, 1.0 - s * 0.6);\n" +
            "       c = mix(c, clamp(graded, 0.0, 1.0), uLight);\n" +
            "   }\n" +
            "   float n = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));\n" +
            "   c += (n - 0.5) / 255.0;\n" +
            "   gl_FragColor = vec4(c, scene.a);\n" +
            "}\n";

    private final FGRoundFx fx;
    private final boolean feedSegmentation;
    private final FloatBuffer quad;

    private Program copy;
    private Program down;
    private Program blur;
    private Program composite;
    private boolean broken;

    private int sceneFbo;
    private int sceneTex;
    private int sceneW;
    private int sceneH;

    private int outFbo;
    private int outTex;
    private int outW;
    private int outH;

    private final int[] smallFbo = new int[2];
    private final int[] smallTex = new int[2];
    private int smallW;
    private int smallH;

    private int segFbo;
    private int segTex;

    private int maskTex;
    private int maskSeq = -1;
    private ByteBuffer maskUpload;

    private final int[] saved = new int[5];
    private boolean frameOpen;

    public FGRoundFxRenderer(FGRoundFx fx, boolean feedSegmentation) {
        this.fx = fx;
        this.feedSegmentation = feedSegmentation;
        quad = ByteBuffer.allocateDirect(8 * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
        quad.put(new float[]{-1f, -1f, 1f, -1f, -1f, 1f, 1f, 1f}).position(0);
    }

    public boolean prepare(int width, int height) {
        if (broken || width <= 0 || height <= 0 || !fx.isActive(SystemClock.uptimeMillis())) {
            return false;
        }
        try {
            return ensurePrograms() && ensureScene(width, height);
        } catch (Throwable t) {
            fail(t);
            return false;
        }
    }

    public int sceneFramebuffer() {
        return sceneFbo;
    }

    public void finish(int targetFbo, int vx, int vy, int vw, int vh) {
        long now = SystemClock.uptimeMillis();
        boolean blend = GLES20.glIsEnabled(GLES20.GL_BLEND);
        try {
            GLES20.glDisable(GLES20.GL_BLEND);
            GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, 0);
            if (feedSegmentation) {
                feed(now);
            }
            render(sceneTex, sceneW, sceneH, targetFbo, vx, vy, vw, vh, now);
        } catch (Throwable t) {
            fail(t);
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, targetFbo);
            GLES20.glViewport(vx, vy, vw, vh);
        } finally {
            if (blend) {
                GLES20.glEnable(GLES20.GL_BLEND);
            }
        }
    }

    public boolean beginFrame(int width, int height) {
        frameOpen = false;
        if (!prepare(width, height)) {
            return false;
        }
        GLES20.glGetIntegerv(GLES20.GL_FRAMEBUFFER_BINDING, saved, 0);
        GLES20.glGetIntegerv(GLES20.GL_VIEWPORT, saved, 1);
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, sceneFbo);
        GLES20.glViewport(0, 0, width, height);
        frameOpen = true;
        return true;
    }

    public void endFrame() {
        if (!frameOpen) {
            return;
        }
        frameOpen = false;
        finish(saved[0], saved[1], saved[2], saved[3], saved[4]);
    }

    public int process(int sourceTexture, int width, int height) {
        if (broken || width <= 0 || height <= 0) {
            return sourceTexture;
        }
        long now = SystemClock.uptimeMillis();
        if (!fx.isActive(now)) {
            return sourceTexture;
        }
        int[] state = new int[5];
        boolean blend = GLES20.glIsEnabled(GLES20.GL_BLEND);
        GLES20.glGetIntegerv(GLES20.GL_FRAMEBUFFER_BINDING, state, 0);
        GLES20.glGetIntegerv(GLES20.GL_VIEWPORT, state, 1);
        try {
            if (!ensurePrograms() || !ensureOut(width, height)) {
                return sourceTexture;
            }
            GLES20.glDisable(GLES20.GL_BLEND);
            GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, 0);
            render(sourceTexture, width, height, outFbo, 0, 0, width, height, now);
            return outTex;
        } catch (Throwable t) {
            fail(t);
            return sourceTexture;
        } finally {
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, state[0]);
            GLES20.glViewport(state[1], state[2], state[3], state[4]);
            if (blend) {
                GLES20.glEnable(GLES20.GL_BLEND);
            }
        }
    }

    private void fail(Throwable t) {
        FileLog.e(t);
        broken = true;
    }

    private void feed(long now) {
        if (!fx.wantsSegmentation(now)) {
            return;
        }
        if (segFbo == 0) {
            int[] ids = createTarget(FGRoundFx.MASK_SIZE, FGRoundFx.MASK_SIZE);
            segTex = ids[0];
            segFbo = ids[1];
        }
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, segFbo);
        GLES20.glViewport(0, 0, FGRoundFx.MASK_SIZE, FGRoundFx.MASK_SIZE);
        copy.use(quad);
        bindTexture(0, sceneTex);
        GLES20.glUniform1i(copy.sTex, 0);
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
        copy.done();
        GLES20.glPixelStorei(GLES20.GL_PACK_ALIGNMENT, 4);
        GLES20.glReadPixels(0, 0, FGRoundFx.MASK_SIZE, FGRoundFx.MASK_SIZE,
                GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, fx.frameBuffer());
        fx.submitFrame(now);
    }

    private void render(int source, int width, int height, int targetFbo, int vx, int vy, int vw, int vh, long now) {
        float blurAmount = fx.blurAmount(now);
        float lightAmount = fx.lightAmount(now);
        float maskMix = fx.maskMix();
        float presence = fx.presence();
        float shiftX = fx.maskShiftX(now);
        float shiftY = fx.maskShiftY(now);
        updateMask();
        float base = Math.min(width, height);
        if (blurAmount > 0.001f) {
            ensureSmall(width, height);
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, smallFbo[0]);
            GLES20.glViewport(0, 0, smallW, smallH);
            down.use(quad);
            bindTexture(0, source);
            bindTexture(1, maskTex);
            GLES20.glUniform1i(down.sTex, 0);
            GLES20.glUniform1i(down.sMask, 1);
            GLES20.glUniform1f(down.uMaskMix, maskMix);
            GLES20.glUniform1f(down.uPresence, presence);
            GLES20.glUniform2f(down.uMaskShift, shiftX, shiftY);
            GLES20.glUniform2f(down.uTexel, 0.25f / smallW, 0.25f / smallH);
            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
            down.done();

            float unit = fx.radius() / 1.62f * base;
            for (int pass = 0; pass < 3; pass++) {
                blurPass(smallTex[0], smallFbo[1], unit / width, 0f);
                blurPass(smallTex[1], smallFbo[0], 0f, unit / height);
                unit *= 0.5f;
            }
        }
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, targetFbo);
        GLES20.glViewport(vx, vy, vw, vh);
        composite.use(quad);
        bindTexture(0, source);
        bindTexture(1, maskTex);
        bindTexture(2, blurAmount > 0.001f ? smallTex[0] : 0);
        GLES20.glUniform1i(composite.sTex, 0);
        GLES20.glUniform1i(composite.sMask, 1);
        GLES20.glUniform1i(composite.sBlur, 2);
        GLES20.glUniform1f(composite.uMaskMix, maskMix);
        GLES20.glUniform1f(composite.uPresence, presence);
        GLES20.glUniform2f(composite.uMaskShift, shiftX, shiftY);
        GLES20.glUniform1f(composite.uBlur, blurAmount);
        GLES20.glUniform1f(composite.uLight, lightAmount);
        float rim = 0.012f * base;
        GLES20.glUniform2f(composite.uRim, rim / width, rim / height);
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
        composite.done();
        bindTexture(2, 0);
        bindTexture(1, 0);
        bindTexture(0, 0);
    }

    private void blurPass(int sourceTexture, int targetFbo, float dx, float dy) {
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, targetFbo);
        GLES20.glViewport(0, 0, smallW, smallH);
        blur.use(quad);
        bindTexture(0, sourceTexture);
        GLES20.glUniform1i(blur.sTex, 0);
        GLES20.glUniform2f(blur.uDir, dx, dy);
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
        blur.done();
    }

    private static void bindTexture(int unit, int texture) {
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + unit);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, texture);
        if (unit != 0) {
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        }
    }

    private void updateMask() {
        if (maskTex == 0) {
            int[] id = new int[1];
            GLES20.glGenTextures(1, id, 0);
            maskTex = id[0];
            maskUpload = ByteBuffer.allocateDirect(FGRoundFx.MASK_SIZE * FGRoundFx.MASK_SIZE).order(ByteOrder.nativeOrder());
            GLES20.glActiveTexture(GLES20.GL_TEXTURE1);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, maskTex);
            setParams();
            GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 1);
            GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_LUMINANCE, FGRoundFx.MASK_SIZE, FGRoundFx.MASK_SIZE,
                    0, GLES20.GL_LUMINANCE, GLES20.GL_UNSIGNED_BYTE, maskUpload);
            GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 4);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            maskSeq = 0;
        }
        int seq = fx.maskSeq();
        if (seq == maskSeq) {
            return;
        }
        maskSeq = fx.copyMask(maskUpload);
        GLES20.glActiveTexture(GLES20.GL_TEXTURE1);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, maskTex);
        GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 1);
        GLES20.glTexSubImage2D(GLES20.GL_TEXTURE_2D, 0, 0, 0, FGRoundFx.MASK_SIZE, FGRoundFx.MASK_SIZE,
                GLES20.GL_LUMINANCE, GLES20.GL_UNSIGNED_BYTE, maskUpload);
        GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 4);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
    }

    private boolean ensurePrograms() {
        if (composite != null) {
            return true;
        }
        try {
            copy = new Program(COPY_FRAGMENT);
            down = new Program(DOWN_FRAGMENT);
            blur = new Program(BLUR_FRAGMENT);
            composite = new Program(COMPOSITE_FRAGMENT);
            return true;
        } catch (Throwable t) {
            releasePrograms();
            fail(t);
            return false;
        }
    }

    private boolean ensureScene(int width, int height) {
        if (sceneFbo != 0 && sceneW == width && sceneH == height) {
            return true;
        }
        deleteTarget(sceneTex, sceneFbo);
        int[] ids = createTarget(width, height);
        sceneTex = ids[0];
        sceneFbo = ids[1];
        sceneW = width;
        sceneH = height;
        return sceneFbo != 0;
    }

    private boolean ensureOut(int width, int height) {
        if (outFbo != 0 && outW == width && outH == height) {
            return true;
        }
        deleteTarget(outTex, outFbo);
        int[] ids = createTarget(width, height);
        outTex = ids[0];
        outFbo = ids[1];
        outW = width;
        outH = height;
        return outFbo != 0;
    }

    private void ensureSmall(int width, int height) {
        int w = Math.max(48, Math.min(256, Math.round(width / 4f)));
        int h = Math.max(48, Math.min(256, Math.round(w * (height / (float) width))));
        if (smallFbo[0] != 0 && w == smallW && h == smallH) {
            return;
        }
        for (int i = 0; i < 2; i++) {
            deleteTarget(smallTex[i], smallFbo[i]);
            int[] ids = createTarget(w, h);
            smallTex[i] = ids[0];
            smallFbo[i] = ids[1];
        }
        smallW = w;
        smallH = h;
    }

    private static void setParams() {
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
    }

    private static int[] createTarget(int width, int height) {
        int[] texture = new int[1];
        int[] framebuffer = new int[1];
        int[] previous = new int[1];
        GLES20.glGetIntegerv(GLES20.GL_FRAMEBUFFER_BINDING, previous, 0);
        GLES20.glGenTextures(1, texture, 0);
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, texture[0]);
        setParams();
        GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA, width, height, 0,
                GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, null);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
        GLES20.glGenFramebuffers(1, framebuffer, 0);
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, framebuffer[0]);
        GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER, GLES20.GL_COLOR_ATTACHMENT0,
                GLES20.GL_TEXTURE_2D, texture[0], 0);
        int status = GLES20.glCheckFramebufferStatus(GLES20.GL_FRAMEBUFFER);
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, previous[0]);
        if (status != GLES20.GL_FRAMEBUFFER_COMPLETE) {
            GLES20.glDeleteFramebuffers(1, framebuffer, 0);
            GLES20.glDeleteTextures(1, texture, 0);
            throw new IllegalStateException("round fx framebuffer " + status);
        }
        return new int[]{texture[0], framebuffer[0]};
    }

    private static void deleteTarget(int texture, int framebuffer) {
        if (framebuffer != 0) {
            GLES20.glDeleteFramebuffers(1, new int[]{framebuffer}, 0);
        }
        if (texture != 0) {
            GLES20.glDeleteTextures(1, new int[]{texture}, 0);
        }
    }

    private void releasePrograms() {
        if (copy != null) copy.release();
        if (down != null) down.release();
        if (blur != null) blur.release();
        if (composite != null) composite.release();
        copy = down = blur = composite = null;
    }

    public void release() {
        try {
            releasePrograms();
            deleteTarget(sceneTex, sceneFbo);
            deleteTarget(outTex, outFbo);
            deleteTarget(segTex, segFbo);
            for (int i = 0; i < 2; i++) {
                deleteTarget(smallTex[i], smallFbo[i]);
                smallTex[i] = smallFbo[i] = 0;
            }
            if (maskTex != 0) {
                GLES20.glDeleteTextures(1, new int[]{maskTex}, 0);
            }
        } catch (Throwable t) {
            FileLog.e(t);
        }
        sceneTex = sceneFbo = outTex = outFbo = segTex = segFbo = maskTex = 0;
        sceneW = sceneH = outW = outH = smallW = smallH = 0;
        maskSeq = -1;
        frameOpen = false;
    }

    private static final class Program {
        final int id;
        final int aPos;
        final int sTex;
        final int sMask;
        final int sBlur;
        final int uTexel;
        final int uDir;
        final int uMaskMix;
        final int uPresence;
        final int uBlur;
        final int uLight;
        final int uRim;
        final int uMaskShift;
        private final int vertex;
        private final int fragment;
        private final int[] attribState = new int[1];

        Program(String fragmentSource) {
            vertex = compile(GLES20.GL_VERTEX_SHADER, VERTEX);
            fragment = compile(GLES20.GL_FRAGMENT_SHADER, fragmentSource);
            id = GLES20.glCreateProgram();
            GLES20.glAttachShader(id, vertex);
            GLES20.glAttachShader(id, fragment);
            GLES20.glLinkProgram(id);
            int[] status = new int[1];
            GLES20.glGetProgramiv(id, GLES20.GL_LINK_STATUS, status, 0);
            if (status[0] == 0) {
                String log = GLES20.glGetProgramInfoLog(id);
                GLES20.glDeleteProgram(id);
                GLES20.glDeleteShader(vertex);
                GLES20.glDeleteShader(fragment);
                throw new IllegalStateException("round fx link: " + log);
            }
            aPos = GLES20.glGetAttribLocation(id, "aPos");
            sTex = GLES20.glGetUniformLocation(id, "sTex");
            sMask = GLES20.glGetUniformLocation(id, "sMask");
            sBlur = GLES20.glGetUniformLocation(id, "sBlur");
            uTexel = GLES20.glGetUniformLocation(id, "uTexel");
            uDir = GLES20.glGetUniformLocation(id, "uDir");
            uMaskMix = GLES20.glGetUniformLocation(id, "uMaskMix");
            uPresence = GLES20.glGetUniformLocation(id, "uPresence");
            uBlur = GLES20.glGetUniformLocation(id, "uBlur");
            uLight = GLES20.glGetUniformLocation(id, "uLight");
            uRim = GLES20.glGetUniformLocation(id, "uRim");
            uMaskShift = GLES20.glGetUniformLocation(id, "uMaskShift");
        }

        private static int compile(int type, String source) {
            int shader = GLES20.glCreateShader(type);
            GLES20.glShaderSource(shader, source);
            GLES20.glCompileShader(shader);
            int[] status = new int[1];
            GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, status, 0);
            if (status[0] == 0) {
                String log = GLES20.glGetShaderInfoLog(shader);
                GLES20.glDeleteShader(shader);
                throw new IllegalStateException("round fx shader: " + log);
            }
            return shader;
        }

        void use(FloatBuffer quad) {
            GLES20.glUseProgram(id);
            attribState[0] = 0;
            GLES20.glGetVertexAttribiv(aPos, GLES20.GL_VERTEX_ATTRIB_ARRAY_ENABLED, attribState, 0);
            quad.position(0);
            GLES20.glVertexAttribPointer(aPos, 2, GLES20.GL_FLOAT, false, 8, quad);
            GLES20.glEnableVertexAttribArray(aPos);
        }

        void done() {
            if (attribState[0] == 0) {
                GLES20.glDisableVertexAttribArray(aPos);
            }
            GLES20.glUseProgram(0);
        }

        void release() {
            GLES20.glDeleteProgram(id);
            GLES20.glDeleteShader(vertex);
            GLES20.glDeleteShader(fragment);
        }
    }
}

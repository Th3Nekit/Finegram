package com.th3nekit.finegram.camera.roundfx;

import android.content.Context;
import android.os.SystemClock;

import com.google.android.gms.tasks.Tasks;
import com.google.android.gms.tflite.client.TfLiteInitializationOptions;
import com.google.android.gms.tflite.gpu.support.TfLiteGpu;
import com.google.android.gms.tflite.java.TfLite;
import com.th3nekit.finegram.core.configs.FinegramCameraConfig;

import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.FileLog;
import org.tensorflow.lite.InterpreterApi;
import org.tensorflow.lite.gpu.GpuDelegateFactory;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

public final class FGRoundFx {

    public static final int MASK_SIZE = 256;
    private static final int PIXELS = MASK_SIZE * MASK_SIZE;
    private static final String MODEL_ASSET = "models/fg_selfie_seg.tflite";
    private static final long FADE_MS = 320;
    private static final long FRAME_INTERVAL_MS = 8;
    private static final long MAX_EXTRAPOLATION_MS = 220;
    private static final float MAX_SHIFT = 0.07f;

    private static final ExecutorService WORKER = Executors.newSingleThreadExecutor(runnable -> {
        Thread thread = new Thread(runnable, "FGRoundSeg");
        thread.setDaemon(true);
        return thread;
    });

    private static volatile int engineState;
    private static InterpreterApi interpreter;
    private static ByteBuffer modelInput;
    private static ByteBuffer modelOutput;
    private static float[] inputPixels;
    private static float[] outputPixels;
    private static byte[] framePixels;

    private final Fade blur = new Fade();
    private final Fade light = new Fade();
    private final Object maskLock = new Object();
    private final byte[] mask = new byte[PIXELS];
    private final byte[] maskWork = new byte[PIXELS];
    private final float[] smooth = new float[PIXELS];
    private final AtomicBoolean busy = new AtomicBoolean();
    private final ByteBuffer frame = ByteBuffer.allocateDirect(PIXELS * 4).order(ByteOrder.nativeOrder());

    private volatile int maskSeq;
    private volatile float presence;
    private volatile long readyAt;
    private volatile int generation;
    private volatile float radius = radiusFor(1);
    private volatile long lastSubmit;
    private volatile long frameTime;
    private volatile long maskTime;
    private volatile float velX;
    private volatile float velY;
    private float lastCx;
    private float lastCy;
    private long lastCentroidTime;
    private boolean hasSmooth;

    private static FGRoundFx shared;

    public static synchronized FGRoundFx shared() {
        if (shared == null) {
            shared = new FGRoundFx();
        }
        return shared;
    }

    private FGRoundFx() {
        syncFromConfig();
    }

    public void startSession() {
        generation++;
        readyAt = 0;
        syncFromConfig();
        try {
            WORKER.execute(() -> hasSmooth = false);
        } catch (Throwable ignored) {
        }
    }

    public static float radiusFor(int strength) {
        if (strength <= 0) {
            return 0.014f;
        }
        return strength == 1 ? 0.024f : 0.038f;
    }

    public void syncFromConfig() {
        FinegramCameraConfig config = FinegramCameraConfig.INSTANCE;
        blur.jump(config.getRoundBackgroundBlur() ? 1f : 0f);
        light.jump(config.getRoundStudioLight() ? 1f : 0f);
        radius = radiusFor(config.getRoundBlurStrength());
    }

    public boolean isBlurEnabled() {
        return blur.target() > 0.5f;
    }

    public boolean isLightEnabled() {
        return light.target() > 0.5f;
    }

    public boolean isEnabled() {
        return isBlurEnabled() || isLightEnabled();
    }

    public void setBlurEnabled(boolean enabled) {
        FinegramCameraConfig.INSTANCE.setRoundBackgroundBlur(enabled);
        blur.animateTo(enabled ? 1f : 0f, SystemClock.uptimeMillis());
    }

    public void setLightEnabled(boolean enabled) {
        FinegramCameraConfig.INSTANCE.setRoundStudioLight(enabled);
        light.animateTo(enabled ? 1f : 0f, SystemClock.uptimeMillis());
    }

    public void setStrength(int strength) {
        FinegramCameraConfig.INSTANCE.setRoundBlurStrength(strength);
        radius = radiusFor(strength);
    }

    public boolean isActive(long now) {
        return isEnabled() || blur.value(now) > 0.001f || light.value(now) > 0.001f;
    }

    private float readiness(long now) {
        long ready = readyAt;
        if (engineState < 0 && ready == 0) {
            readyAt = ready = now;
        }
        if (ready == 0) {
            return 0f;
        }
        float t = (now - ready) / (float) FADE_MS;
        if (t >= 1f) {
            return 1f;
        }
        return t <= 0f ? 0f : t * t * (3f - 2f * t);
    }

    public float blurAmount(long now) {
        return blur.value(now) * readiness(now);
    }

    public float lightAmount(long now) {
        return light.value(now) * readiness(now);
    }

    public float radius() {
        return radius;
    }

    public float maskMix() {
        return engineState > 0 && maskSeq > 0 ? 1f : 0f;
    }

    public float presence() {
        return maskMix() > 0f ? presence : 1f;
    }

    public int maskSeq() {
        return maskSeq;
    }

    public int copyMask(ByteBuffer target) {
        synchronized (maskLock) {
            target.position(0);
            target.put(mask);
            target.position(0);
            return maskSeq;
        }
    }

    public boolean wantsSegmentation(long now) {
        return engineState >= 0 && isEnabled() && !busy.get() && now - lastSubmit >= FRAME_INTERVAL_MS;
    }

    public ByteBuffer frameBuffer() {
        frame.position(0);
        return frame;
    }

    public void submitFrame(long now) {
        if (!busy.compareAndSet(false, true)) {
            return;
        }
        lastSubmit = now;
        frameTime = now;
        final int gen = generation;
        try {
            WORKER.execute(() -> segment(gen));
        } catch (Throwable t) {
            busy.set(false);
        }
    }

    public void stop() {
        generation++;
        try {
            WORKER.execute(() -> {
                hasSmooth = false;
                lastCentroidTime = 0;
                velX = velY = 0f;
                closeEngine();
            });
        } catch (Throwable ignored) {
        }
    }

    private void segment(int gen) {
        try {
            if (gen != generation) {
                return;
            }
            final long captured = frameTime;
            if (!ensureEngine()) {
                if (readyAt == 0) {
                    readyAt = SystemClock.uptimeMillis();
                }
                return;
            }
            frame.position(0);
            frame.get(framePixels, 0, PIXELS * 4);
            frame.position(0);
            final float[] in = inputPixels;
            final byte[] src = framePixels;
            final float scale = 1f / 255f;
            int o = 0;
            for (int y = MASK_SIZE - 1; y >= 0; y--) {
                int i = y * MASK_SIZE * 4;
                for (int x = 0; x < MASK_SIZE; x++, i += 4) {
                    in[o++] = (src[i] & 0xFF) * scale;
                    in[o++] = (src[i + 1] & 0xFF) * scale;
                    in[o++] = (src[i + 2] & 0xFF) * scale;
                }
            }
            modelInput.rewind();
            modelInput.asFloatBuffer().put(in);
            modelOutput.rewind();
            interpreter.run(modelInput, modelOutput);
            modelOutput.rewind();
            modelOutput.asFloatBuffer().get(outputPixels);
            if (gen != generation) {
                return;
            }
            publish(outputPixels, captured);
        } catch (Throwable t) {
            FileLog.e(t);
            engineState = -1;
            closeEngine();
        } finally {
            busy.set(false);
        }
    }

    public float maskShiftX(long now) {
        return shift(velX, now);
    }

    public float maskShiftY(long now) {
        return shift(velY, now);
    }

    private float shift(float velocity, long now) {
        long time = maskTime;
        if (time == 0 || velocity == 0f) {
            return 0f;
        }
        long latency = Math.max(0, Math.min(MAX_EXTRAPOLATION_MS, now - time));
        float value = velocity * latency;
        return Math.max(-MAX_SHIFT, Math.min(MAX_SHIFT, value));
    }

    private void publish(float[] out, long captured) {
        final float k = hasSmooth ? 0.86f : 1f;
        double sum = 0;
        double mass = 0;
        double sumX = 0;
        double sumY = 0;
        for (int r = 0; r < MASK_SIZE; r++) {
            int src = r * MASK_SIZE;
            int dst = (MASK_SIZE - 1 - r) * MASK_SIZE;
            for (int x = 0; x < MASK_SIZE; x++) {
                float v = out[src + x];
                if (!(v >= 0f)) {
                    v = 0f;
                } else if (v > 1f) {
                    v = 1f;
                }
                sum += v;
                float s = smooth[dst + x];
                s += (v - s) * k;
                smooth[dst + x] = s;
                maskWork[dst + x] = (byte) (int) (s * 255f + 0.5f);
                if (v > 0.5f) {
                    mass += v;
                    sumX += v * x;
                    sumY += v * (MASK_SIZE - 1 - r);
                }
            }
        }
        if (mass > PIXELS * 0.01f) {
            float cx = (float) (sumX / mass + 0.5) / MASK_SIZE;
            float cy = (float) (sumY / mass + 0.5) / MASK_SIZE;
            long dt = captured - lastCentroidTime;
            if (lastCentroidTime != 0 && dt >= 8 && dt <= 400) {
                float vx = (cx - lastCx) / dt;
                float vy = (cy - lastCy) / dt;
                float limit = 0.0025f;
                vx = Math.max(-limit, Math.min(limit, vx));
                vy = Math.max(-limit, Math.min(limit, vy));
                velX = velX * 0.35f + vx * 0.65f;
                velY = velY * 0.35f + vy * 0.65f;
            } else {
                velX = velY = 0f;
            }
            lastCx = cx;
            lastCy = cy;
            lastCentroidTime = captured;
        } else {
            velX = velY = 0f;
            lastCentroidTime = 0;
        }
        float mean = (float) (sum / PIXELS);
        float target = mean <= 0.012f ? 0f : mean >= 0.045f ? 1f : (mean - 0.012f) / 0.033f;
        presence = hasSmooth ? presence + (target - presence) * 0.22f : target;
        hasSmooth = true;
        synchronized (maskLock) {
            System.arraycopy(maskWork, 0, mask, 0, PIXELS);
            maskTime = captured;
            maskSeq++;
        }
        if (readyAt == 0) {
            readyAt = SystemClock.uptimeMillis();
        }
    }

    private static boolean ensureEngine() {
        if (engineState < 0) {
            return false;
        }
        if (interpreter != null) {
            return true;
        }
        try {
            Context context = ApplicationLoader.applicationContext;
            boolean gpu = false;
            try {
                TfLiteInitializationOptions init = TfLiteInitializationOptions.builder()
                        .setEnableGpuDelegateSupport(true)
                        .build();
                Tasks.await(TfLite.initialize(context, init), 20, TimeUnit.SECONDS);
                gpu = Boolean.TRUE.equals(Tasks.await(TfLiteGpu.isGpuDelegateAvailable(context), 10, TimeUnit.SECONDS));
            } catch (Throwable t) {
                FileLog.e(t);
                Tasks.await(TfLite.initialize(context), 20, TimeUnit.SECONDS);
            }
            InterpreterApi created = null;
            if (gpu) {
                try {
                    created = InterpreterApi.create(loadModel(context), new InterpreterApi.Options()
                            .setRuntime(InterpreterApi.Options.TfLiteRuntime.FROM_SYSTEM_ONLY)
                            .addDelegateFactory(new GpuDelegateFactory()));
                } catch (Throwable t) {
                    FileLog.e(t);
                    created = null;
                }
            }
            if (created == null) {
                created = InterpreterApi.create(loadModel(context), new InterpreterApi.Options()
                        .setRuntime(InterpreterApi.Options.TfLiteRuntime.FROM_SYSTEM_ONLY)
                        .setNumThreads(Math.max(2, Math.min(4, Runtime.getRuntime().availableProcessors() - 2))));
            }
            if (created.getInputTensor(0).numElements() != PIXELS * 3
                    || created.getOutputTensor(0).numElements() != PIXELS) {
                created.close();
                throw new IllegalStateException("unexpected segmentation model shape");
            }
            modelInput = ByteBuffer.allocateDirect(PIXELS * 3 * 4).order(ByteOrder.nativeOrder());
            modelOutput = ByteBuffer.allocateDirect(PIXELS * 4).order(ByteOrder.nativeOrder());
            if (inputPixels == null) {
                inputPixels = new float[PIXELS * 3];
                outputPixels = new float[PIXELS];
                framePixels = new byte[PIXELS * 4];
            }
            interpreter = created;
            engineState = 1;
            return true;
        } catch (Throwable t) {
            FileLog.e(t);
            engineState = -1;
            return false;
        }
    }

    private static void closeEngine() {
        InterpreterApi current = interpreter;
        interpreter = null;
        modelInput = null;
        modelOutput = null;
        if (current != null) {
            try {
                current.close();
            } catch (Throwable ignored) {
            }
        }
        if (engineState > 0) {
            engineState = 0;
        }
    }

    private static ByteBuffer loadModel(Context context) throws Exception {
        try (InputStream stream = context.getAssets().open(MODEL_ASSET)) {
            ByteArrayOutputStream out = new ByteArrayOutputStream(262144);
            byte[] chunk = new byte[16384];
            int read;
            while ((read = stream.read(chunk)) > 0) {
                out.write(chunk, 0, read);
            }
            byte[] bytes = out.toByteArray();
            ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.length).order(ByteOrder.nativeOrder());
            buffer.put(bytes);
            buffer.rewind();
            return buffer;
        }
    }

    private static final class Fade {
        private float from;
        private float to;
        private long start;

        synchronized void jump(float value) {
            from = to = value;
            start = 0;
        }

        synchronized void animateTo(float value, long now) {
            from = value(now);
            to = value;
            start = now;
        }

        synchronized float target() {
            return to;
        }

        synchronized float value(long now) {
            if (start == 0 || from == to) {
                return to;
            }
            float t = (now - start) / (float) FADE_MS;
            if (t >= 1f) {
                return to;
            }
            if (t <= 0f) {
                return from;
            }
            t = t * t * (3f - 2f * t);
            return from + (to - from) * t;
        }
    }
}

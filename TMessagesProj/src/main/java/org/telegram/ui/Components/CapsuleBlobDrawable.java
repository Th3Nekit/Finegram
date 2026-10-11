package org.telegram.ui.Components;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.animation.TimeInterpolator;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.graphics.ColorUtils;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ChatObject;
import org.telegram.messenger.LiteMode;
import org.telegram.messenger.utils.Choreographer60FpsContent;
import org.telegram.messenger.voip.VoIPService;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.Theme;

import java.util.Random;

public class CapsuleBlobDrawable extends Drawable {

    public static float MAX_SPEED = 8.2f;
    public static float MIN_SPEED = 0.8f;

    public static final int STATE_UNMUTE = 0;
    public static final int STATE_MUTE = 1;
    public static final int STATE_CONNECTING = 2;
    public static final int STATE_MUTED_BY_ADMIN = 3;

    private static final float ANIMATION_SPEED_WAVE_HUGE = 0.65f;
    private static final float ANIMATION_SPEED_WAVE_SMALL = 0.45f;
    private static final float animationSpeed = 1f - ANIMATION_SPEED_WAVE_HUGE;
    private static final float animationSpeedTiny = 1f - ANIMATION_SPEED_WAVE_SMALL;

    private static final TimeInterpolator SMOOTHER = t -> t * t * t * (t * (t * 6f - 15f) + 10f);

    private static final float WAVE_IDLE_FRACTION = 0f;

    private static final float BREATH_AMPLITUDE_FALLOFF = 0.7f;

    private static final int SMOOTHING_ITERATIONS = 2;

    private float cornerRadius = dp(18);
    private float targetSpacing = dp(22);
    private float curvatureBoost = 2.4f;

    private float waveDepth = dp(12f);
    private float breathDepth = dp(1.5f);
    private float breathPeriodMs = 3600f;
    private float neighborCoherence = 0.25f;

    private float halfWidth;
    private float halfHeight;
    private float radius;
    private float innerX;
    private float innerY;
    private float straightH;
    private float straightV;
    private float weightedArc;
    private float weightedTotal;
    private float perimeter;

    private final Layer big;
    private final Layer small;
    private float breathPhase;
    private long lastFrameTime;
    private boolean running;

    private float amplitude;
    private float animateToAmplitude;
    private float animateAmplitudeDiff;

    private final Path path = new Path();
    private final float[] sample = new float[4];

    public CapsuleBlobDrawable() {
        big = new Layer();
        big.speedScale = 1f;
        big.phaseOffset = 0f;
        big.pushMin = dp(0.5f);
        big.pushMax = dp(8.5f);
        big.waveScale = 1f;
        big.breathScale = 1f;
        big.baseAlpha = 61;

        small = new Layer();
        small.speedScale = 0.82f;
        small.phaseOffset = 0.6f;
        small.pushMin = dp(0);
        small.pushMax = dp(4.25f);
        small.waveScale = 0.55f;
        small.breathScale = 0.55f;
        small.baseAlpha = 128;

        setState(STATE_UNMUTE);
    }

    private int colorState = -1;
    private int currentColor;
    private int fromColor;
    private int toColor;
    private float colorProgress = 1f;
    private static final float COLOR_TRANSITION_MS = 250f;

    public void setState(int state) {
        setState(state, false);
    }

    public void setState(int state, boolean animated) {
        if (state == colorState && colorProgress >= 1f) {
            return;
        }
        colorState = state;
        int target = resolveStateColor(state);
        if (animated && currentColor != 0 && LiteMode.isEnabled(LiteMode.FLAG_CALLS_ANIMATIONS)) {
            fromColor = currentColor;
            toColor = target;
            colorProgress = 0f;
        } else {
            colorProgress = 1f;
            applyColor(target);
        }
        invalidateSelf();
    }

    public void updateState(boolean animated) {
        VoIPService voIPService = VoIPService.getSharedInstance();
        if (voIPService == null) {
            return;
        }
        int callState = voIPService.getCallState();
        if (!voIPService.isSwitchingStream()
                && (callState == VoIPService.STATE_WAIT_INIT
                || callState == VoIPService.STATE_WAIT_INIT_ACK
                || callState == VoIPService.STATE_CREATING
                || callState == VoIPService.STATE_RECONNECTING)) {
            setState(STATE_CONNECTING, animated);
        } else if (voIPService.groupCall != null) {
            TLRPC.GroupCallParticipant participant =
                    voIPService.groupCall.participants.get(voIPService.getSelfId());
            if (participant != null && !participant.can_self_unmute && participant.muted
                    && !ChatObject.canManageCalls(voIPService.getChat())
                    || voIPService.groupCall.call.rtmp_stream) {
                voIPService.setMicMute(true, false, false);
                setState(STATE_MUTED_BY_ADMIN, animated);
            } else {
                setState(voIPService.isMicMute() ? STATE_MUTE : STATE_UNMUTE, animated);
            }
        } else {
            setState(voIPService.isMicMute() ? STATE_MUTE : STATE_UNMUTE, animated);
        }
    }

    public void updateColors() {
        if (colorState >= 0) {
            colorProgress = 1f;
            applyColor(resolveStateColor(colorState));
        }
    }

    private static int resolveStateColor(int state) {
        switch (state) {
            case STATE_UNMUTE:
                return ColorUtils.blendARGB(
                        Theme.getColor(Theme.key_voipgroup_topPanelGreen1),
                        Theme.getColor(Theme.key_voipgroup_topPanelGreen2), 0.5f);
            case STATE_MUTE:
                return ColorUtils.blendARGB(
                        Theme.getColor(Theme.key_voipgroup_topPanelBlue1),
                        Theme.getColor(Theme.key_voipgroup_topPanelBlue2), 0.5f);
            case STATE_MUTED_BY_ADMIN:
                return ColorUtils.blendARGB(
                        ColorUtils.blendARGB(
                                Theme.getColor(Theme.key_voipgroup_mutedByAdminGradient),
                                Theme.getColor(Theme.key_voipgroup_mutedByAdminGradient2), 0.5f),
                        Theme.getColor(Theme.key_voipgroup_mutedByAdminGradient3), 0.5f);
            case STATE_CONNECTING:
            default:
                return Theme.getColor(Theme.key_voipgroup_topPanelGray);
        }
    }

    public void setColor(int color) {
        colorProgress = 1f;
        applyColor(color);
        invalidateSelf();
    }

    private void applyColor(int color) {
        currentColor = color;
        big.color = color;
        small.color = color;
        big.applyColor();
        small.applyColor();
    }

    public void setCornerRadius(float radiusPx) {
        cornerRadius = radiusPx;
        rebuildGeometry();
    }

    public void setTargetSpacing(float spacingPx) {
        targetSpacing = spacingPx;
        rebuildGeometry();
    }

    public void setCurvatureBoost(float boost) {
        curvatureBoost = Math.max(1f, boost);
        rebuildGeometry();
    }

    public void setWaveDepth(float depthPx) {
        waveDepth = depthPx;
        rebuildGeometry();
    }

    public int getRequiredInset() {
        return (int) maxOutwardExcursion() + dp(1);
    }

    private float maxOutwardExcursion() {
        float bigOff = big.pushMax + breathDepth * big.breathScale + waveDepth * big.waveScale;
        float smallOff = small.pushMax + breathDepth * small.breathScale + waveDepth * small.waveScale;
        return Math.max(bigOff, smallOff);
    }

    public void setNeighborCoherence(float coherence) {
        neighborCoherence = Math.max(0f, Math.min(1f, coherence));
    }

    public void setBreathing(float depthPx, float periodMs) {
        breathDepth = depthPx;
        breathPeriodMs = Math.max(1f, periodMs);
        rebuildGeometry();
    }

    private final Runnable mInvalidateSelf = () -> {
        if (LiteMode.isEnabled(LiteMode.FLAG_CALLS_ANIMATIONS)) {
            invalidateSelf();
        }
    };

    public void start() {
        if (running) {
            return;
        }
        running = true;
        lastFrameTime = SystemClock.elapsedRealtime();
        Choreographer60FpsContent.getInstance().addFrameCallback(mInvalidateSelf, 60);
    }

    public void stop() {
        if (!running) {
            return;
        }
        running = false;
        Choreographer60FpsContent.getInstance().removeFrameCallback(mInvalidateSelf);
    }

    public boolean isRunning() {
        return running;
    }

    public void setAmplitude(float value) {
        setAmplitude(value, false);
    }

    public void setAmplitude(float value, boolean isBig) {
        animateToAmplitude = value;
        if (!LiteMode.isEnabled(LiteMode.FLAG_CALLS_ANIMATIONS)) {
            return;
        }
        float diff = animateToAmplitude - amplitude;
        if (isBig) {
            animateAmplitudeDiff = diff / (100f + (diff > 0 ? 300f : 500f) * animationSpeed);
        } else {
            animateAmplitudeDiff = diff / (100f + (diff > 0 ? 400f : 500f) * animationSpeedTiny);
        }
    }

    @Override
    protected void onBoundsChange(@NonNull Rect bounds) {
        super.onBoundsChange(bounds);
        rebuildGeometry();
    }

    private void rebuildGeometry() {
        Rect b = getBounds();
        if (b.isEmpty()) {
            return;
        }

        float inset = maxOutwardExcursion() + dp(1);
        float maxInset = Math.min(b.width(), b.height()) / 2f - dp(2);
        if (inset > maxInset) {
            inset = Math.max(0f, maxInset);
        }
        halfWidth = b.width() / 2f - inset;
        halfHeight = b.height() / 2f - inset;
        if (halfWidth < 1f || halfHeight < 1f) {
            return;
        }
        radius = Math.min(cornerRadius, Math.min(halfWidth, halfHeight));

        innerX = halfWidth - radius;
        innerY = halfHeight - radius;
        straightH = 2f * innerX;
        straightV = 2f * innerY;

        weightedArc = curvatureBoost * (float) (Math.PI / 2.0) * radius;
        weightedTotal = 2f * straightH + 2f * straightV + 4f * weightedArc;
        perimeter = 2f * straightH + 2f * straightV + (float) (2.0 * Math.PI) * radius;

        int n = Math.max(12, Math.min(80, Math.round(perimeter / targetSpacing)));
        if (n != big.count()) {
            big.resize(n);
            small.resize(n);
        }
    }

    @Override
    public void draw(@NonNull Canvas canvas) {
        Rect b = getBounds();
        if (b.isEmpty() || halfWidth < 1f) {
            return;
        }

        long now = SystemClock.elapsedRealtime();
        boolean animating = running || colorProgress < 1f;
        long dt = animating ? Math.min(40L, Math.max(0L, now - lastFrameTime)) : 0L;
        lastFrameTime = now;

        boolean lite = LiteMode.isEnabled(LiteMode.FLAG_CALLS_ANIMATIONS);
        if (lite && dt > 0) {
            updateAmplitude(dt);
            breathPhase += (dt / breathPeriodMs) * (float) (2.0 * Math.PI);
            big.update(amplitude);
            small.update(amplitude);
        }

        if (colorProgress < 1f && dt > 0) {
            colorProgress += dt / COLOR_TRANSITION_MS;
            if (colorProgress > 1f) {
                colorProgress = 1f;
            }
            applyColor(ColorUtils.blendARGB(fromColor, toColor, colorProgress));
        }

        float cX = b.exactCenterX();
        float cY = b.exactCenterY();

        float breathFade = 1f - BREATH_AMPLITUDE_FALLOFF * amplitude;
        float breathBig = breathDepth * big.breathScale * breathFade
                * (0.5f + 0.5f * (float) Math.sin(breathPhase));
        float breathSmall = breathDepth * small.breathScale * breathFade
                * (0.5f + 0.5f * (float) Math.sin(breathPhase + small.phaseOffset));

        drawLayer(canvas, big, cX, cY, breathBig);
        drawLayer(canvas, small, cX, cY, breathSmall);

        if (colorProgress < 1f) {
            invalidateSelf();
        }
    }

    private void drawLayer(Canvas canvas, Layer layer, float cX, float cY, float breath) {
        int n = layer.count();
        if (n == 0) {
            return;
        }
        float push = layer.pushMin + (layer.pushMax - layer.pushMin) * amplitude;
        float wave = waveDepth * layer.waveScale * (WAVE_IDLE_FRACTION + (1f - WAVE_IDLE_FRACTION) * amplitude);

        float[] dEff = layer.depthEff;
        float[] dTmp = layer.depthTmp;
        float[] oEff = layer.offsetEff;
        for (int i = 0; i < n; i++) {
            float pr = SMOOTHER.getInterpolation(layer.progress[i]);
            dEff[i] = layer.depth[i] * (1f - pr) + layer.depthNext[i] * pr;
            oEff[i] = layer.offset[i] * (1f - pr) + layer.offsetNext[i] * pr;
        }

        if (neighborCoherence > 0f) {
            for (int it = 0; it < SMOOTHING_ITERATIONS; it++) {
                for (int i = 0; i < n; i++) {
                    int p = i == 0 ? n - 1 : i - 1;
                    int q = i + 1 == n ? 0 : i + 1;
                    float avg = (dEff[p] + dEff[q]) * 0.5f;
                    dTmp[i] = dEff[i] + neighborCoherence * (avg - dEff[i]);
                }
                float[] swap = dEff;
                dEff = dTmp;
                dTmp = swap;
            }
        }

        for (int i = 0; i < n; i++) {
            pointAt((float) i / n + oEff[i], sample);

            float nx = sample[3];
            float ny = -sample[2];

            float off = push + breath + dEff[i] * wave;

            layer.px[i] = cX + sample[0] + nx * off;
            layer.py[i] = cY + sample[1] + ny * off;
            layer.tx[i] = sample[2];
            layer.ty[i] = sample[3];
        }

        path.rewind();
        path.moveTo(layer.px[0], layer.py[0]);
        for (int i = 0; i < n; i++) {
            int j = i + 1 < n ? i + 1 : 0;
            float dx = layer.px[j] - layer.px[i];
            float dy = layer.py[j] - layer.py[i];
            float arm = (float) Math.sqrt(dx * dx + dy * dy) / 3f;
            path.cubicTo(
                    layer.px[i] + layer.tx[i] * arm, layer.py[i] + layer.ty[i] * arm,
                    layer.px[j] - layer.tx[j] * arm, layer.py[j] - layer.ty[j] * arm,
                    layer.px[j], layer.py[j]
            );
        }
        path.close();
        canvas.drawPath(path, layer.paint);
    }

    private void pointAt(float f, float[] out) {
        float t = (f - (float) Math.floor(f)) * weightedTotal;

        if (t < straightH) {
            out[0] = -innerX + t;
            out[1] = -halfHeight;
            out[2] = 1f;
            out[3] = 0f;
            return;
        }
        t -= straightH;

        if (t < weightedArc) {
            arc(innerX, -innerY, -(float) (Math.PI / 2.0) + t / (curvatureBoost * radius), out);
            return;
        }
        t -= weightedArc;

        if (t < straightV) {
            out[0] = halfWidth;
            out[1] = -innerY + t;
            out[2] = 0f;
            out[3] = 1f;
            return;
        }
        t -= straightV;

        if (t < weightedArc) {
            arc(innerX, innerY, t / (curvatureBoost * radius), out);
            return;
        }
        t -= weightedArc;

        if (t < straightH) {
            out[0] = innerX - t;
            out[1] = halfHeight;
            out[2] = -1f;
            out[3] = 0f;
            return;
        }
        t -= straightH;

        if (t < weightedArc) {
            arc(-innerX, innerY, (float) (Math.PI / 2.0) + t / (curvatureBoost * radius), out);
            return;
        }
        t -= weightedArc;

        if (t < straightV) {
            out[0] = -halfWidth;
            out[1] = innerY - t;
            out[2] = 0f;
            out[3] = -1f;
            return;
        }
        t -= straightV;

        arc(-innerX, -innerY, (float) Math.PI + t / (curvatureBoost * radius), out);
    }

    private void arc(float centerX, float centerY, float angle, float[] out) {
        float cos = (float) Math.cos(angle);
        float sin = (float) Math.sin(angle);
        out[0] = centerX + radius * cos;
        out[1] = centerY + radius * sin;
        out[2] = -sin;
        out[3] = cos;
    }

    private void updateAmplitude(long dt) {
        if (animateToAmplitude != amplitude) {
            amplitude += animateAmplitudeDiff * dt;
            if (animateAmplitudeDiff > 0) {
                if (amplitude > animateToAmplitude) {
                    amplitude = animateToAmplitude;
                }
            } else {
                if (amplitude < animateToAmplitude) {
                    amplitude = animateToAmplitude;
                }
            }
        }
    }

    private int mAlpha = 255;

    @Override
    public void setAlpha(int alpha) {
        if (mAlpha != alpha) {
            mAlpha = alpha;
            big.setLayerAlpha(alpha);
            small.setLayerAlpha(alpha);
            invalidateSelf();
        }
    }

    @Override
    public int getAlpha() {
        return mAlpha;
    }

    @Override
    public void setColorFilter(@Nullable ColorFilter colorFilter) {
        big.paint.setColorFilter(colorFilter);
        small.paint.setColorFilter(colorFilter);
        invalidateSelf();
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    private static final class Layer {
        float speedScale;
        float phaseOffset;
        float pushMin;
        float pushMax;
        float waveScale;
        float breathScale;
        int baseAlpha;
        int color = 0xFF534AB7;

        final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        final Random random = new Random();

        float[] depth;
        float[] depthNext;
        float[] offset;
        float[] offsetNext;
        float[] progress;
        float[] speed;

        float[] px;
        float[] py;
        float[] tx;
        float[] ty;

        float[] depthEff;
        float[] depthTmp;
        float[] offsetEff;

        private int n;
        private static final float WALK_STEP = 0.35f;
        private static final float MAX_JITTER = 0.18f;

        int count() {
            return n;
        }

        private int mAlpha = 255;

        void applyColor() {
            paint.setColor(color);
            paint.setAlpha(baseAlpha * mAlpha / 255);
        }

        void setLayerAlpha(int alpha) {
            mAlpha = alpha;
            applyColor();
        }

        void resize(int newN) {
            n = newN;
            depth = new float[n];
            depthNext = new float[n];
            offset = new float[n];
            offsetNext = new float[n];
            progress = new float[n];
            speed = new float[n];
            px = new float[n];
            py = new float[n];
            tx = new float[n];
            ty = new float[n];
            depthEff = new float[n];
            depthTmp = new float[n];
            offsetEff = new float[n];
            for (int i = 0; i < n; i++) {
                depth[i] = random.nextFloat();
                offset[i] = (random.nextFloat() - 0.5f) * 2f * MAX_JITTER / n;
                next(i);
                progress[i] = random.nextFloat();
            }
            applyColor();
        }

        void next(int i) {
            float jitterLimit = MAX_JITTER / n;
            float d = depth[i] + (random.nextFloat() - 0.5f) * 2f * WALK_STEP;
            depthNext[i] = clamp(d, 0f, 1f);
            float o = offset[i] + (random.nextFloat() - 0.5f) * 2f * jitterLimit * WALK_STEP;
            offsetNext[i] = clamp(o, -jitterLimit, jitterLimit);
            speed[i] = (0.017f + 0.003f * random.nextFloat()) * speedScale;
        }

        void update(float amplitude) {
            for (int i = 0; i < n; i++) {
                progress[i] += (speed[i] * MIN_SPEED) + amplitude * speed[i] * MAX_SPEED;
                if (progress[i] >= 1f) {
                    progress[i] = 0f;
                    depth[i] = depthNext[i];
                    offset[i] = offsetNext[i];
                    next(i);
                }
            }
        }

        static float clamp(float v, float min, float max) {
            return v < min ? min : (v > max ? max : v);
        }
    }
}
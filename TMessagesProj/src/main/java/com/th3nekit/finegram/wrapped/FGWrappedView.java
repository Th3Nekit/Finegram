package com.th3nekit.finegram.wrapped;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Camera;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.DashPathEffect;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PathMeasure;
import android.graphics.RadialGradient;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.SweepGradient;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Build;
import android.os.SystemClock;
import android.text.Layout;
import android.text.StaticLayout;
import android.text.TextPaint;
import android.text.TextUtils;
import android.view.DisplayCutout;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.WindowInsets;

import androidx.core.content.FileProvider;
import androidx.core.graphics.ColorUtils;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.FileLoader;
import org.telegram.messenger.FileLog;
import org.telegram.messenger.ImageLocation;
import org.telegram.messenger.ImageReceiver;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.MediaController;
import org.telegram.messenger.MessageObject;
import org.telegram.messenger.R;
import org.telegram.messenger.UserObject;
import org.telegram.messenger.Utilities;
import org.telegram.tgnet.TLObject;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.Components.AvatarDrawable;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.CubicBezierInterpolator;

import java.io.File;
import java.io.FileOutputStream;
import java.text.NumberFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Random;

@SuppressLint("ViewConstructor")
public class FGWrappedView extends View {

    private static final float W = 360f;
    private static final float H = 640f;
    private static final long PAGE_MS = 7000;
    private static final long TRANSITION_MS = 560;

    private static final int P_INTRO = 0;
    private static final int P_TOTAL = 1;
    private static final int P_TOP = 2;
    private static final int P_BEST = 3;
    private static final int P_CLOCK = 4;
    private static final int P_STICKER = 6;
    private static final int P_MEDIA = 7;
    private static final int P_CALLS = 8;
    private static final int P_WORD = 9;
    private static final int P_SUMMARY = 10;
    private static final int P_EMPTY = 11;

    private static final int[][] PALETTES = {
            {0xff3a0ca3, 0xff7209b7, 0xfff72585},
            {0xff0b486b, 0xfff56217, 0xffffd36e},
            {0xff1f4068, 0xff162447, 0xff4cc9f0},
            {0xffb5179e, 0xff3a0ca3, 0xffff9ad5},
            {0xff0f0c29, 0xff302b63, 0xff7f7fd5},
            {0xff11998e, 0xff0b5d55, 0xff38ef7d},
            {0xff11998e, 0xff0b5d55, 0xff38ef7d},
            {0xfffc466b, 0xff3f5efb, 0xffffc3a0},
            {0xffc94b4b, 0xff4b134f, 0xffff8a65},
            {0xff000428, 0xff004e92, 0xff6dd5ed},
            {0xff141e30, 0xff243b55, 0xffc77dff},
            {0xff232526, 0xff414345, 0xff8e9eab},
    };

    private static final int[] CONFETTI = {0xffffd166, 0xffef476f, 0xff06d6a0, 0xff118ab2, 0xffffffff, 0xffc77dff, 0xffff9f1c};

    private static Typeface displayFace;
    private static Typeface headFace;
    private static Typeface bodyFace;

    private final Runnable onClose;
    private final int account;
    private FGWrappedStats stats;
    private float progress;
    private Runnable cancel;
    private final ArrayList<Integer> pages = new ArrayList<>();
    private int index;
    private int previousIndex = -1;
    private int direction = 1;
    private long pageStart;
    private long transitionStart;
    private long pausedAt;
    private long pausedTotal;
    private final long born = SystemClock.uptimeMillis();
    private long lastFrame;
    private boolean attached;
    private boolean ghost;
    private final ArrayList<RectF> cutouts = new ArrayList<>();
    private final Path halo = new Path();
    private final Path haloPiece = new Path();
    private final PathMeasure haloMeasure = new PathMeasure();
    private final float[] haloPos = new float[2];
    private final RectF haloBounds = new RectF();
    private final RectF arcRect = new RectF();
    private final ArrayList<float[]> spans = new ArrayList<>();
    private float haloLength;
    private float barY;
    private float closeX;
    private float closeY;

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint strokePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint blobPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint shaderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final TextPaint displayPaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
    private final TextPaint headPaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
    private final TextPaint bodyPaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
    private final RectF rect = new RectF();
    private final Path path = new Path();
    private final Path cutPath = new Path();
    private final Matrix matrix = new Matrix();
    private final Camera camera = new Camera();
    private final HashMap<String, StaticLayout> layouts = new HashMap<>();
    private final float[] twinkle = new float[60 * 4];

    private final ArrayList<ImageReceiver> avatars = new ArrayList<>();
    private ImageReceiver bestAvatar;
    private ImageReceiver callAvatar;
    private ImageReceiver stickerImage;
    private final ArrayList<ImageReceiver> receivers = new ArrayList<>();

    private final ArrayList<Particle> particles = new ArrayList<>();
    private final ArrayList<float[]> rings = new ArrayList<>();
    private final Paint sparkPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final HashSet<String> fired = new HashSet<>();
    private final Random random = new Random();

    private float downX;
    private float downY;
    private long downTime;
    private boolean holding;
    private boolean sharing;
    private final RectF shareButton = new RectF();
    private final RectF saveButton = new RectF();

    private static final class Particle {
        float x;
        float y;
        float vx;
        float vy;
        float rotation;
        float spin;
        float size;
        float life;
        float age;
        int color;
        int shape;
    }

    public FGWrappedView(Context context, int account, int period, Runnable onClose) {
        super(context);
        this.account = account;
        this.onClose = onClose;
        loadFaces();
        displayPaint.setTypeface(displayFace);
        displayPaint.setColor(Color.WHITE);
        headPaint.setTypeface(headFace);
        headPaint.setColor(Color.WHITE);
        bodyPaint.setTypeface(bodyFace);
        bodyPaint.setColor(Color.WHITE);
        strokePaint.setStyle(Paint.Style.STROKE);
        sparkPaint.setStyle(Paint.Style.STROKE);
        sparkPaint.setStrokeCap(Paint.Cap.ROUND);
        Random seeded = new Random(1601);
        for (int i = 0; i < twinkle.length; i += 4) {
            twinkle[i] = seeded.nextFloat();
            twinkle[i + 1] = seeded.nextFloat();
            twinkle[i + 2] = 0.6f + seeded.nextFloat() * 1.6f;
            twinkle[i + 3] = seeded.nextFloat() * 6.28f;
        }
        pages.add(P_INTRO);
        cancel = FGWrappedStats.collect(account, period, new FGWrappedStats.Listener() {
            @Override
            public void onProgress(float value) {
                progress = value;
                invalidate();
            }

            @Override
            public void onDone(FGWrappedStats result) {
                onStats(result);
            }
        });
        pageStart = SystemClock.uptimeMillis();
    }

    private static void loadFaces() {
        if (displayFace != null) {
            return;
        }
        Typeface fallback = AndroidUtilities.bold();
        displayFace = face("fonts/fg_unbounded_extrabold.ttf", fallback);
        headFace = face("fonts/fg_onest_semibold.ttf", fallback);
        bodyFace = face("fonts/fg_onest_medium.ttf", Typeface.DEFAULT);
    }

    private static Typeface face(String asset, Typeface fallback) {
        try {
            Typeface typeface = AndroidUtilities.getTypeface(asset);
            return typeface != null ? typeface : fallback;
        } catch (Throwable t) {
            return fallback;
        }
    }

    private void onStats(FGWrappedStats result) {
        stats = result;
        pages.clear();
        if (result.total <= 0) {
            pages.add(P_EMPTY);
        } else {
            pages.add(P_INTRO);
            pages.add(P_TOTAL);
            if (result.top.size() >= 2) {
                pages.add(P_TOP);
            }
            if (!result.top.isEmpty()) {
                pages.add(P_BEST);
            }
            if (result.sampled >= 20) {
                pages.add(P_CLOCK);
            }
            if (result.sticker != null) {
                pages.add(P_STICKER);
            }
            if (result.voice + result.rounds + result.photos > 0) {
                pages.add(P_MEDIA);
            }
            if (result.calls > 0 && result.callSeconds >= 60) {
                pages.add(P_CALLS);
            }
            if (result.word != null) {
                pages.add(P_WORD);
            }
            pages.add(P_SUMMARY);
        }
        for (FGWrappedStats.Person person : result.top) {
            ImageReceiver receiver = createReceiver();
            AvatarDrawable drawable = new AvatarDrawable();
            drawable.setInfo(account, person.peer);
            receiver.setForUserOrChat(person.peer, drawable);
            avatars.add(receiver);
        }
        if (!result.top.isEmpty()) {
            bestAvatar = createReceiver();
            AvatarDrawable drawable = new AvatarDrawable();
            drawable.setInfo(account, result.top.get(0).peer);
            bestAvatar.setForUserOrChat(result.top.get(0).peer, drawable);
        }
        if (result.callPeer != null) {
            callAvatar = createReceiver();
            AvatarDrawable drawable = new AvatarDrawable();
            drawable.setInfo(account, result.callPeer);
            callAvatar.setForUserOrChat(result.callPeer, drawable);
        }
        if (result.sticker != null) {
            stickerImage = createReceiver();
            TLRPC.Document document = result.sticker;
            TLRPC.PhotoSize thumb = FileLoader.getClosestPhotoSizeWithSize(document.thumbs, 90);
            ImageLocation thumbLocation = thumb == null ? null : ImageLocation.getForDocument(thumb, document);
            Object parent = result.stickerParent != null ? result.stickerParent : document;
            boolean animated = MessageObject.isAnimatedStickerDocument(document, true) || MessageObject.isVideoStickerDocument(document);
            stickerImage.setImage(ImageLocation.getForDocument(document), "360_360", thumbLocation, null, null, 0,
                    animated ? null : "webp", parent, 1);
            stickerImage.setAutoRepeat(1);
            stickerImage.setAllowStartLottieAnimation(true);
            stickerImage.setAllowStartAnimation(true);
            stickerImage.startAnimation();
        }
        index = 0;
        previousIndex = -1;
        pageStart = now();
        transitionStart = pageStart;
        fired.clear();
        invalidate();
    }

    private ImageReceiver createReceiver() {
        ImageReceiver receiver = new ImageReceiver(this);
        receiver.setInvalidateAll(true);
        receivers.add(receiver);
        if (attached) {
            receiver.onAttachedToWindow();
        }
        return receiver;
    }

    @Override
    public WindowInsets onApplyWindowInsets(WindowInsets insets) {
        requestLayout();
        return super.onApplyWindowInsets(insets);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        readCutouts();
    }

    private void readCutouts() {
        cutouts.clear();
        halo.rewind();
        haloLength = 0f;
        int width = getWidth();
        int height = getHeight();
        if (width <= 0 || height <= 0) {
            layoutChrome(width);
            return;
        }
        if (Build.VERSION.SDK_INT >= 28) {
            WindowInsets insets = getRootWindowInsets();
            DisplayCutout cutout = insets == null ? null : insets.getDisplayCutout();
            if (cutout != null) {
                int[] location = new int[2];
                getLocationInWindow(location);
                RectF precise = null;
                if (Build.VERSION.SDK_INT >= 31) {
                    Path shape = cutout.getCutoutPath();
                    if (shape != null && !shape.isEmpty()) {
                        RectF bounds = new RectF();
                        shape.computeBounds(bounds, true);
                        bounds.offset(-location[0], -location[1]);
                        if (bounds.width() > 0 && bounds.height() > 0 && bounds.bottom < height * 0.2f && bounds.width() < width * 0.6f) {
                            precise = bounds;
                        }
                    }
                }
                for (Rect r : cutout.getBoundingRects()) {
                    if (r == null || r.isEmpty()) {
                        continue;
                    }
                    RectF f = new RectF(r);
                    f.offset(-location[0], -location[1]);
                    if (f.bottom <= 0 || f.top > height * 0.2f) {
                        continue;
                    }
                    if (precise != null && RectF.intersects(precise, f)) {
                        f.set(precise);
                        precise = null;
                    }
                    cutouts.add(f);
                }
            }
        }
        if (cutouts.size() == 1) {
            buildHalo(cutouts.get(0));
        }
        layoutChrome(width);
    }

    private void buildHalo(RectF cut) {
        float pad = AndroidUtilities.dp(4);
        float w = cut.width();
        float h = cut.height();
        boolean edge = cut.top <= AndroidUtilities.dp(2);
        if (edge && w >= h * 1.35f) {
            float l = cut.left - pad;
            float r = cut.right + pad;
            float b = cut.bottom + pad;
            float cx = cut.centerX();
            float span = r - l;
            halo.moveTo(l, 0);
            halo.cubicTo(l + span * 0.07f, b * 0.78f, cx - span * 0.32f, b, cx, b);
            halo.cubicTo(cx + span * 0.32f, b, r - span * 0.07f, b * 0.78f, r, 0);
            haloBounds.set(l, 0, r, b);
        } else {
            float cx = cut.centerX();
            float cy;
            float rx;
            float ry;
            if (edge && w < h * 1.35f) {
                float radius = w / 2f + pad;
                cy = cut.bottom - w / 2f;
                rx = radius;
                ry = radius;
            } else {
                cy = cut.centerY();
                rx = w / 2f + pad;
                ry = h / 2f + pad;
            }
            if (rx >= ry) {
                float r = ry;
                float top = cy - r;
                float bottom = cy + r;
                float left = cx - rx;
                float right = cx + rx;
                halo.moveTo(cx, top);
                halo.lineTo(right - r, top);
                arcRect.set(right - 2 * r, top, right, bottom);
                halo.arcTo(arcRect, -90, 180, false);
                halo.lineTo(left + r, bottom);
                arcRect.set(left, top, left + 2 * r, bottom);
                halo.arcTo(arcRect, 90, 180, false);
                halo.lineTo(cx, top);
                haloBounds.set(left, top, right, bottom);
            } else {
                arcRect.set(cx - rx, cy - ry, cx + rx, cy + ry);
                halo.addArc(arcRect, -90, 359.9f);
                haloBounds.set(arcRect);
            }
        }
        haloMeasure.setPath(halo, false);
        haloLength = haloMeasure.getLength();
    }

    private void layoutChrome(int width) {
        float gap = AndroidUtilities.dp(7);
        float side = AndroidUtilities.dp(10);
        barY = AndroidUtilities.dp(15.5f);
        if (!cutouts.isEmpty()) {
            RectF first = haloLength > 0 ? haloBounds : cutouts.get(0);
            barY = Math.max(AndroidUtilities.dp(12), Math.min(AndroidUtilities.dp(56), first.centerY()));
        }
        spans.clear();
        spans.add(new float[]{side, width - side});
        for (RectF cut : cutouts) {
            RectF zone = haloLength > 0 && RectF.intersects(cut, haloBounds) ? haloBounds : cut;
            if (barY < zone.top - AndroidUtilities.dp(6) || barY > zone.bottom + AndroidUtilities.dp(6)) {
                continue;
            }
            float from = zone.left - gap;
            float to = zone.right + gap;
            ArrayList<float[]> next = new ArrayList<>();
            for (float[] span : spans) {
                if (to <= span[0] || from >= span[1]) {
                    next.add(span);
                    continue;
                }
                if (from > span[0]) {
                    next.add(new float[]{span[0], from});
                }
                if (to < span[1]) {
                    next.add(new float[]{to, span[1]});
                }
            }
            spans.clear();
            spans.addAll(next);
        }
        float total = 0f;
        for (float[] span : spans) {
            total += span[1] - span[0];
        }
        if (total < width * 0.45f) {
            float bottom = 0f;
            for (RectF cut : cutouts) {
                bottom = Math.max(bottom, cut.bottom);
            }
            barY = bottom + AndroidUtilities.dp(10);
            spans.clear();
            spans.add(new float[]{side, width - side});
        }
        closeX = width - AndroidUtilities.dp(26);
        closeY = barY + AndroidUtilities.dp(24);
        float reach = AndroidUtilities.dp(22);
        for (RectF cut : cutouts) {
            if (closeX + reach > cut.left && closeX - reach < cut.right && closeY - reach < cut.bottom && closeY + reach > cut.top) {
                closeY = cut.bottom + reach;
            }
        }
    }

    private void track(Canvas canvas, float from, float to, float h, int color) {
        if (to - from <= 0.5f) {
            return;
        }
        paint.setColor(color);
        float offset = 0f;
        for (float[] span : spans) {
            float length = span[1] - span[0];
            float a = Math.max(from, offset);
            float b = Math.min(to, offset + length);
            if (b - a > 0.5f) {
                rect.set(span[0] + a - offset, barY - h / 2f, span[0] + b - offset, barY + h / 2f);
                canvas.drawRoundRect(rect, h / 2f, h / 2f, paint);
            }
            offset += length;
        }
    }

    private void drawHalo(Canvas canvas, float fill, long t) {
        if (haloLength <= 0f) {
            return;
        }
        float seconds = (SystemClock.uptimeMillis() - born) / 1000f;
        float pulse = stats != null ? Math.max(0f, 1f - (t - transitionStart) / 700f) : 0f;
        strokePaint.setStrokeCap(Paint.Cap.ROUND);
        strokePaint.setStrokeWidth(AndroidUtilities.dp(2));
        strokePaint.setColor(0x40ffffff);
        canvas.drawPath(halo, strokePaint);
        if (pulse > 0f) {
            strokePaint.setStrokeWidth(AndroidUtilities.dp(2) + AndroidUtilities.dp(9) * pulse);
            strokePaint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, (int) (110 * pulse)));
            canvas.drawPath(halo, strokePaint);
        }
        if (stats == null) {
            float head = (seconds * 0.6f) % 1f;
            haloPiece.rewind();
            haloMeasure.getSegment(head * haloLength, Math.min(haloLength, (head + 0.25f) * haloLength), haloPiece, true);
            strokePaint.setStrokeWidth(AndroidUtilities.dp(2.4f));
            strokePaint.setColor(Color.WHITE);
            canvas.drawPath(haloPiece, strokePaint);
            return;
        }
        if (fill <= 0f) {
            return;
        }
        float breathe = fill >= 1f ? 0.65f + 0.35f * (float) Math.sin(seconds * 2.2f) : 1f;
        haloPiece.rewind();
        haloMeasure.getSegment(0, haloLength * Math.min(1f, fill), haloPiece, true);
        strokePaint.setStrokeWidth(AndroidUtilities.dp(7));
        strokePaint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, (int) (60 * breathe)));
        canvas.drawPath(haloPiece, strokePaint);
        strokePaint.setStrokeWidth(AndroidUtilities.dp(2.4f));
        strokePaint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, (int) (255 * Math.max(0.75f, breathe))));
        canvas.drawPath(haloPiece, strokePaint);
        if (fill < 1f && haloMeasure.getPosTan(haloLength * fill, haloPos, null)) {
            paint.setColor(0x55ffffff);
            canvas.drawCircle(haloPos[0], haloPos[1], AndroidUtilities.dp(5), paint);
            paint.setColor(Color.WHITE);
            canvas.drawCircle(haloPos[0], haloPos[1], AndroidUtilities.dp(2.6f), paint);
        }
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        attached = true;
        for (ImageReceiver receiver : receivers) {
            receiver.onAttachedToWindow();
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        attached = false;
        for (ImageReceiver receiver : receivers) {
            receiver.onDetachedFromWindow();
        }
        if (cancel != null) {
            cancel.run();
            cancel = null;
        }
    }

    private long now() {
        if (holding) {
            return pausedAt - pausedTotal;
        }
        return SystemClock.uptimeMillis() - pausedTotal;
    }

    private int page() {
        return pages.get(Math.max(0, Math.min(index, pages.size() - 1)));
    }

    private void go(int delta) {
        if (stats == null) {
            return;
        }
        int next = Math.max(0, index + delta);
        if (next >= pages.size()) {
            if (onClose != null && page() != P_SUMMARY) {
                onClose.run();
            }
            return;
        }
        if (next == index) {
            pageStart = now();
            fired.clear();
            invalidate();
            return;
        }
        direction = next > index ? 1 : -1;
        previousIndex = index;
        index = next;
        pageStart = now();
        transitionStart = pageStart;
        fired.clear();
        particles.clear();
        rings.clear();
        try {
            performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP, HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING);
        } catch (Throwable ignored) {
        }
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        long t = now();
        if (stats != null && page() != P_SUMMARY && page() != P_EMPTY && !holding && t - pageStart >= PAGE_MS) {
            go(1);
            t = now();
        }
        long real = SystemClock.uptimeMillis();
        float dt = lastFrame == 0 ? 0f : Math.min(0.05f, (real - lastFrame) / 1000f);
        lastFrame = real;
        if (!holding) {
            stepParticles(dt);
        }
        drawFrame(canvas, getWidth(), getHeight(), t, false);
        invalidate();
    }

    private void drawFrame(Canvas canvas, int width, int height, long t, boolean export) {
        int current = stats == null ? -1 : page();
        float transition = export ? 1f : Math.min(1f, (t - transitionStart) / (float) TRANSITION_MS);
        float eased = CubicBezierInterpolator.EASE_OUT_QUINT.getInterpolation(transition);
        int[] palette = palette(current);
        if (!export && previousIndex >= 0 && transition < 1f) {
            int[] from = palette(pages.get(previousIndex));
            palette = new int[]{
                    ColorUtils.blendARGB(from[0], palette[0], eased),
                    ColorUtils.blendARGB(from[1], palette[1], eased),
                    ColorUtils.blendARGB(from[2], palette[2], eased)};
        }
        float seconds = export ? 4f : (t - born) / 1000f;
        drawBackground(canvas, width, height, palette, seconds);
        float scale = Math.min(width / W, height / H);
        canvas.save();
        canvas.translate((width - W * scale) / 2f, (height - H * scale) / 2f);
        canvas.scale(scale, scale);
        if (stats == null) {
            drawLoading(canvas, seconds);
        } else if (export) {
            drawPage(canvas, current, 30f, 1f, true);
        } else {
            if (previousIndex >= 0 && transition < 1f) {
                canvas.save();
                float s = 1f - 0.12f * eased;
                canvas.translate(-direction * W * 0.32f * eased, 0);
                canvas.scale(s, s, W / 2f, H / 2f);
                ghost = true;
                drawPage(canvas, pages.get(previousIndex), 30f, 1f - eased, false);
                ghost = false;
                canvas.restore();
            }
            canvas.save();
            float s = 0.9f + 0.1f * eased;
            canvas.translate(direction * W * 0.38f * (1f - eased), 0);
            canvas.scale(s, s, W / 2f, H / 2f);
            drawPage(canvas, current, (t - pageStart) / 1000f, eased, false);
            canvas.restore();
            drawParticles(canvas);
        }
        canvas.restore();
        if (!export) {
            drawChrome(canvas, width, height, t);
        }
    }

    private void drawPage(Canvas canvas, int page, float time, float alpha, boolean export) {
        if (alpha <= 0.002f) {
            return;
        }
        switch (page) {
            case P_INTRO:
                drawIntro(canvas, time, alpha);
                break;
            case P_TOTAL:
                drawTotal(canvas, time, alpha);
                break;
            case P_TOP:
                drawTop(canvas, time, alpha);
                break;
            case P_BEST:
                drawBest(canvas, time, alpha);
                break;
            case P_CLOCK:
                drawClock(canvas, time, alpha);
                break;
            case P_STICKER:
                drawSticker(canvas, time, alpha);
                break;
            case P_MEDIA:
                drawMedia(canvas, time, alpha);
                break;
            case P_CALLS:
                drawCalls(canvas, time, alpha);
                break;
            case P_WORD:
                drawWord(canvas, time, alpha);
                break;
            case P_SUMMARY:
                drawSummary(canvas, time, alpha, export);
                break;
            default:
                drawEmpty(canvas, time, alpha);
                break;
        }
    }

    private int[] palette(int page) {
        if (page < 0) {
            return PALETTES[0];
        }
        return PALETTES[Math.min(page, PALETTES.length - 1)];
    }

    private void drawBackground(Canvas canvas, int width, int height, int[] palette, float s) {
        shaderPaint.setAlpha(255);
        shaderPaint.setShader(new LinearGradient(0, 0, width * 0.4f, height, palette[0], palette[1], Shader.TileMode.CLAMP));
        canvas.drawRect(0, 0, width, height, shaderPaint);
        shaderPaint.setShader(null);
        float base = Math.max(width, height);
        for (int i = 0; i < 3; i++) {
            float phase = s * (0.2f + i * 0.08f) + i * 2.1f;
            float cx = width * (0.5f + 0.45f * (float) Math.sin(phase * 1.3f + i));
            float cy = height * (0.5f + 0.4f * (float) Math.cos(phase + i * 1.7f));
            float r = base * (0.45f + 0.1f * (float) Math.sin(phase * 0.7f));
            int color = i == 1 ? Color.WHITE : palette[2];
            blobPaint.setShader(new RadialGradient(cx, cy, r, ColorUtils.setAlphaComponent(color, i == 1 ? 30 : 80),
                    ColorUtils.setAlphaComponent(color, 0), Shader.TileMode.CLAMP));
            canvas.drawCircle(cx, cy, r, blobPaint);
        }
        blobPaint.setShader(null);
        float unit = Math.min(width, height) / 360f;
        for (int i = 0; i < twinkle.length; i += 4) {
            float a = 0.15f + 0.6f * (0.5f + 0.5f * (float) Math.sin(s * twinkle[i + 2] + twinkle[i + 3]));
            paint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, (int) (255 * a * 0.7f)));
            canvas.drawCircle(twinkle[i] * width, twinkle[i + 1] * height, unit * (0.8f + twinkle[i + 2] * 0.6f), paint);
        }
    }

    private void drawChrome(Canvas canvas, int width, int height, long t) {
        if (spans.isEmpty()) {
            layoutChrome(width);
        }
        int count = stats == null ? 1 : pages.size();
        float gap = AndroidUtilities.dp(4);
        float h = AndroidUtilities.dp(3);
        float total = 0f;
        for (float[] span : spans) {
            total += span[1] - span[0];
        }
        float segment = (total - gap * (count - 1)) / count;
        float current = 0f;
        for (int i = 0; i < count; i++) {
            float from = i * (segment + gap);
            float fill;
            if (stats == null) {
                fill = progress;
            } else if (i < index) {
                fill = 1f;
            } else if (i > index) {
                fill = 0f;
            } else {
                fill = page() == P_SUMMARY || page() == P_EMPTY ? 1f : Math.min(1f, (t - pageStart) / (float) PAGE_MS);
            }
            if (stats == null || i == index) {
                current = fill;
            }
            track(canvas, from, from + segment, h, 0x44ffffff);
            if (fill > 0f) {
                track(canvas, from, from + segment * fill, h, Color.WHITE);
            }
        }
        drawHalo(canvas, current, t);
        float arm = AndroidUtilities.dp(7);
        strokePaint.setColor(Color.WHITE);
        strokePaint.setStrokeWidth(AndroidUtilities.dp(2.2f));
        strokePaint.setStrokeCap(Paint.Cap.ROUND);
        canvas.drawLine(closeX - arm, closeY - arm, closeX + arm, closeY + arm, strokePaint);
        canvas.drawLine(closeX - arm, closeY + arm, closeX + arm, closeY - arm, strokePaint);
    }

    private static float ease(float time, float delay, float duration) {
        float t = (time - delay) / duration;
        if (t <= 0f) {
            return 0f;
        }
        if (t >= 1f) {
            return 1f;
        }
        return CubicBezierInterpolator.EASE_OUT_QUINT.getInterpolation(t);
    }

    private static float spring(float time, float delay, float duration) {
        float t = (time - delay) / duration;
        if (t <= 0f) {
            return 0f;
        }
        if (t >= 1f) {
            return 1f;
        }
        float s = 1.9f;
        float x = t - 1f;
        return 1f + (s + 1f) * x * x * x + s * x * x;
    }

    private boolean once(String key, float time, float at) {
        if (!ghost && time >= at && !fired.contains(key)) {
            fired.add(key);
            return true;
        }
        return false;
    }

    private void burst(float x, float y, int count, float speed, boolean confetti) {
        burst(x, y, count, speed, confetti, 0f);
    }

    private void burst(float x, float y, int count, float speed, boolean confetti, float spawn) {
        for (int i = 0; i < count; i++) {
            Particle p = new Particle();
            double angle = random.nextDouble() * Math.PI * 2;
            float cos = (float) Math.cos(angle);
            float sin = (float) Math.sin(angle);
            float v = speed * (0.45f + random.nextFloat() * 0.75f);
            float r = spawn * (0.6f + random.nextFloat() * 0.4f);
            p.x = x + cos * r;
            p.y = y + sin * r * 0.6f;
            p.vx = cos * v;
            p.vy = sin * v - (confetti ? speed * 0.45f : 0f);
            p.rotation = random.nextFloat() * 360f;
            p.spin = (random.nextFloat() - 0.5f) * 720f;
            p.size = confetti ? 4f + random.nextFloat() * 5f : 1.6f + random.nextFloat() * 2f;
            p.life = confetti ? 2.2f + random.nextFloat() * 1.4f : 0.7f + random.nextFloat() * 0.6f;
            p.color = confetti ? CONFETTI[random.nextInt(CONFETTI.length)] : (random.nextInt(4) == 0 ? 0xffffd166 : Color.WHITE);
            p.shape = confetti ? random.nextInt(3) : 3;
            particles.add(p);
        }
    }

    private void shockwave(float x, float y, float radius) {
        rings.add(new float[]{x, y, 0f, 0.75f, radius, 7f});
        rings.add(new float[]{x, y, -0.12f, 0.9f, radius * 1.35f, 3f});
    }

    private void blast(float x, float y, float spawn, float scale) {
        burst(x, y, (int) (80 * scale), 640f * scale, false, spawn);
        burst(x, y, (int) (40 * scale), 470f * scale, true, spawn * 0.5f);
        shockwave(x, y, 190f * scale);
        try {
            performHapticFeedback(HapticFeedbackConstants.LONG_PRESS, HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING);
        } catch (Throwable ignored) {
        }
    }

    private float odometerEnd(long value, float delay) {
        String target = number(value);
        int digits = 0;
        for (int i = 0; i < target.length(); i++) {
            if (Character.isDigit(target.charAt(i))) {
                digits++;
            }
        }
        digits = Math.max(1, digits);
        return delay + (digits - 1) * 0.07f + 1.5f + digits * 0.12f;
    }

    private float punch(float time, float at) {
        float t = (time - at) / 0.42f;
        if (t <= 0f || t >= 1f) {
            return 1f;
        }
        return 1f + 0.16f * (float) Math.sin(t * Math.PI) * (1f - t * 0.5f);
    }

    private void flash(Canvas canvas, float x, float y, float time, float at, float alpha) {
        float t = (time - at) / 0.5f;
        if (t <= 0f || t >= 1f) {
            return;
        }
        float k = (1f - t) * (1f - t);
        shaderPaint.setAlpha(255);
        shaderPaint.setShader(new RadialGradient(x, y, 120 + 120 * t, ColorUtils.setAlphaComponent(Color.WHITE, (int) (150 * k * alpha)), 0, Shader.TileMode.CLAMP));
        canvas.drawCircle(x, y, 120 + 120 * t, shaderPaint);
        shaderPaint.setShader(null);
    }

    private void stepParticles(float dt) {
        for (int i = particles.size() - 1; i >= 0; i--) {
            Particle p = particles.get(i);
            p.age += dt;
            if (p.age >= p.life) {
                particles.remove(i);
                continue;
            }
            boolean spark = p.shape == 3;
            p.vy += (spark ? 150f : 520f) * dt;
            float drag = spark ? 0.955f : 0.985f;
            p.vx *= Math.pow(drag, dt * 60f);
            p.vy *= Math.pow(drag, dt * 60f);
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.rotation += p.spin * dt;
        }
        for (int i = rings.size() - 1; i >= 0; i--) {
            float[] ring = rings.get(i);
            ring[2] += dt;
            if (ring[2] >= ring[3]) {
                rings.remove(i);
            }
        }
    }

    private void drawParticles(Canvas canvas) {
        for (float[] ring : rings) {
            if (ring[2] <= 0f) {
                continue;
            }
            float t = ring[2] / ring[3];
            float e = 1f - (1f - t) * (1f - t) * (1f - t);
            strokePaint.setStrokeWidth(Math.max(0.8f, ring[5] * (1f - t)));
            strokePaint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, (int) (220 * (1f - t))));
            canvas.drawCircle(ring[0], ring[1], 24 + ring[4] * e, strokePaint);
        }
        for (Particle p : particles) {
            float fade = Math.max(0f, 1f - Math.max(0f, (p.age - p.life * 0.65f) / (p.life * 0.35f)));
            if (p.shape == 3) {
                int color = ColorUtils.setAlphaComponent(p.color, (int) (255 * fade));
                sparkPaint.setColor(color);
                sparkPaint.setStrokeWidth(p.size);
                canvas.drawLine(p.x, p.y, p.x - p.vx * 0.045f, p.y - p.vy * 0.045f, sparkPaint);
                paint.setColor(color);
                canvas.drawCircle(p.x, p.y, p.size * 0.9f, paint);
                continue;
            }
            paint.setColor(ColorUtils.setAlphaComponent(p.color, (int) (255 * fade)));
            canvas.save();
            canvas.translate(p.x, p.y);
            canvas.rotate(p.rotation);
            if (p.shape == 0) {
                float squash = (float) Math.abs(Math.cos(Math.toRadians(p.rotation * 1.7f)));
                rect.set(-p.size, -p.size * 0.45f * squash - 0.5f, p.size, p.size * 0.45f * squash + 0.5f);
                canvas.drawRect(rect, paint);
            } else if (p.shape == 1) {
                canvas.drawCircle(0, 0, p.size * 0.55f, paint);
            } else {
                path.rewind();
                path.moveTo(0, -p.size * 0.7f);
                path.lineTo(p.size * 0.6f, p.size * 0.5f);
                path.lineTo(-p.size * 0.6f, p.size * 0.5f);
                path.close();
                canvas.drawPath(path, paint);
            }
            canvas.restore();
        }
    }

    private StaticLayout layout(CharSequence text, TextPaint paint, float size, float width) {
        String key = text + "|" + size + "|" + width + "|" + paint.hashCode();
        StaticLayout cached = layouts.get(key);
        if (cached != null) {
            return cached;
        }
        paint.setTextSize(size);
        StaticLayout layout = StaticLayout.Builder.obtain(text, 0, text.length(), paint, (int) Math.ceil(width))
                .setAlignment(Layout.Alignment.ALIGN_CENTER)
                .setMaxLines(4)
                .setEllipsize(TextUtils.TruncateAt.END)
                .setLineSpacing(0, 1.08f)
                .build();
        layouts.put(key, layout);
        return layout;
    }

    private float text(Canvas canvas, CharSequence text, TextPaint paint, float size, float y, float width, float appear, float alpha) {
        if (text == null || appear <= 0f || alpha <= 0f) {
            return 0f;
        }
        StaticLayout layout = layout(text, paint, size, width);
        paint.setTextSize(size);
        int restore = paint.getAlpha();
        paint.setAlpha((int) (255 * Math.min(1f, appear) * alpha));
        canvas.save();
        canvas.translate((W - width) / 2f, y + (1f - Math.min(1f, appear)) * 18f);
        layout.draw(canvas);
        canvas.restore();
        paint.setAlpha(restore);
        return layout.getHeight();
    }

    private float fit(TextPaint paint, String text, float size, float maxWidth) {
        paint.setTextSize(size);
        float width = paint.measureText(text);
        if (width > maxWidth && width > 0) {
            size = size * maxWidth / width;
            paint.setTextSize(size);
        }
        return size;
    }

    private void stagger(Canvas canvas, String text, TextPaint paint, float size, float cx, float baseline, float maxWidth,
                         float time, float delay, float step, float alpha) {
        if (text == null || text.isEmpty() || alpha <= 0f) {
            return;
        }
        fit(paint, text, size, maxWidth);
        float total = paint.measureText(text);
        float x = cx - total / 2f;
        int restore = paint.getAlpha();
        for (int i = 0; i < text.length(); ) {
            int end = i + Character.charCount(text.codePointAt(i));
            String ch = text.substring(i, end);
            float advance = paint.measureText(ch);
            float e = spring(time, delay + i * step, 0.7f);
            float a = Math.min(1f, ease(time, delay + i * step, 0.4f));
            if (a > 0f) {
                paint.setAlpha((int) (255 * a * alpha));
                canvas.save();
                canvas.translate(x + advance / 2f, baseline + (1f - e) * size * 0.55f);
                float scale = 0.6f + 0.4f * e;
                canvas.scale(scale, scale);
                canvas.drawText(ch, -advance / 2f, 0, paint);
                canvas.restore();
            }
            x += advance;
            i = end;
        }
        paint.setAlpha(restore);
    }

    private String number(long value) {
        NumberFormat format = NumberFormat.getIntegerInstance(Locale.US);
        format.setGroupingUsed(true);
        return format.format(value).replace(',', ' ').replace(' ', ' ').replace(' ', ' ');
    }

    private float odometer(Canvas canvas, long value, TextPaint paint, float size, float x, float baseline, float maxWidth,
                           float time, float delay, float alpha, int align) {
        String target = number(value);
        paint.setTextSize(size);
        float digit = 0f;
        for (int d = 0; d <= 9; d++) {
            digit = Math.max(digit, paint.measureText(String.valueOf(d)));
        }
        float spaceWidth = digit * 0.42f;
        float total = 0f;
        for (int i = 0; i < target.length(); i++) {
            total += Character.isDigit(target.charAt(i)) ? digit : spaceWidth;
        }
        if (total > maxWidth) {
            float k = maxWidth / total;
            size *= k;
            digit *= k;
            spaceWidth *= k;
            total = maxWidth;
            paint.setTextSize(size);
        }
        Paint.FontMetrics metrics = paint.getFontMetrics();
        float top = baseline + metrics.ascent * 0.92f;
        float bottom = baseline + metrics.descent;
        float line = bottom - top;
        float left = align == 0 ? x - total / 2f : align > 0 ? x - total : x;
        int restore = paint.getAlpha();
        int digits = 0;
        for (int i = 0; i < target.length(); i++) {
            if (Character.isDigit(target.charAt(i))) {
                digits++;
            }
        }
        int digitIndex = 0;
        for (int i = 0; i < target.length(); i++) {
            char c = target.charAt(i);
            if (!Character.isDigit(c)) {
                left += spaceWidth;
                continue;
            }
            int goal = c - '0';
            int spins = digitIndex + 1;
            float e = ease(time, delay + digitIndex * 0.07f, 1.5f + spins * 0.12f);
            float position = e * (goal + spins * 10f);
            int base = (int) Math.floor(position);
            float frac = position - base;
            canvas.save();
            canvas.clipRect(left - 2, top - 4, left + digit + 2, bottom + 4);
            for (int k = 0; k < (frac > 0.001f ? 2 : 1); k++) {
                int shown = ((base + k) % 10 + 10) % 10;
                float offset = (k - frac) * line;
                float fade = 1f - Math.min(1f, Math.abs(offset) / line);
                paint.setAlpha((int) (255 * alpha * (0.25f + 0.75f * fade)));
                String s = String.valueOf(shown);
                float w = paint.measureText(s);
                canvas.drawText(s, left + (digit - w) / 2f, baseline + offset, paint);
            }
            canvas.restore();
            left += digit;
            digitIndex++;
        }
        paint.setAlpha(restore);
        return total;
    }

    private static String plural(int key, long count) {
        String[] forms = LocaleController.getString(key).split("\\|");
        if (forms.length < 3) {
            return forms[0];
        }
        String lang = LocaleController.getInstance().getCurrentLocale().getLanguage();
        if ("ru".equals(lang) || "uk".equals(lang) || "be".equals(lang)) {
            long n = Math.abs(count) % 100;
            long n1 = n % 10;
            if (n > 10 && n < 20) {
                return forms[2];
            }
            if (n1 > 1 && n1 < 5) {
                return forms[1];
            }
            if (n1 == 1) {
                return forms[0];
            }
            return forms[2];
        }
        return count == 1 ? forms[0] : forms[1];
    }

    private String periodName() {
        if (stats.isYear()) {
            return String.valueOf(stats.year);
        }
        Calendar calendar = Calendar.getInstance();
        calendar.clear();
        calendar.set(stats.year, stats.month, 1);
        return new SimpleDateFormat("LLLL", LocaleController.getInstance().getCurrentLocale()).format(calendar.getTime());
    }

    private static String nameOf(TLObject peer) {
        if (peer instanceof TLRPC.User) {
            return UserObject.getUserName((TLRPC.User) peer);
        }
        if (peer instanceof TLRPC.Chat) {
            return ((TLRPC.Chat) peer).title;
        }
        return "";
    }

    private void glassCard(Canvas canvas, RectF bounds, float radius, float alpha) {
        paint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, (int) (34 * alpha)));
        canvas.drawRoundRect(bounds, radius, radius, paint);
        strokePaint.setStrokeWidth(1.2f);
        strokePaint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, (int) (60 * alpha)));
        canvas.drawRoundRect(bounds, radius, radius, strokePaint);
    }

    private void rays(Canvas canvas, float cx, float cy, float radius, float rotation, int count, float alpha) {
        paint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, (int) (22 * alpha)));
        float step = 360f / count;
        for (int i = 0; i < count; i += 2) {
            double a1 = Math.toRadians(rotation + i * step);
            double a2 = Math.toRadians(rotation + (i + 1) * step);
            path.rewind();
            path.moveTo(cx, cy);
            path.lineTo(cx + (float) Math.cos(a1) * radius, cy + (float) Math.sin(a1) * radius);
            path.lineTo(cx + (float) Math.cos(a2) * radius, cy + (float) Math.sin(a2) * radius);
            path.close();
            canvas.drawPath(path, paint);
        }
    }

    private void avatar(Canvas canvas, ImageReceiver receiver, float cx, float cy, float size, float alpha) {
        if (receiver == null || size <= 1f) {
            return;
        }
        receiver.setRoundRadius((int) (size / 2f));
        receiver.setImageCoords(cx - size / 2f, cy - size / 2f, size, size);
        receiver.setAlpha(alpha);
        receiver.draw(canvas);
    }

    private void drawLoading(Canvas canvas, float time) {
        float cx = W / 2f;
        float cy = H / 2f - 30;
        strokePaint.setStrokeWidth(7);
        strokePaint.setStrokeCap(Paint.Cap.ROUND);
        strokePaint.setColor(0x33ffffff);
        canvas.drawCircle(cx, cy, 52, strokePaint);
        strokePaint.setColor(Color.WHITE);
        strokePaint.setShader(new SweepGradient(cx, cy, new int[]{0x00ffffff, Color.WHITE}, null));
        canvas.save();
        canvas.rotate((time * 260f) % 360f, cx, cy);
        rect.set(cx - 52, cy - 52, cx + 52, cy + 52);
        canvas.drawArc(rect, 0, 60 + 280 * progress, false, strokePaint);
        canvas.restore();
        strokePaint.setShader(null);
        float pulse = 1f + 0.05f * (float) Math.sin(time * 4f);
        canvas.save();
        canvas.scale(pulse, pulse, cx, cy);
        displayPaint.setTextSize(22);
        String percent = Math.round(progress * 100) + "%";
        float w = displayPaint.measureText(percent);
        displayPaint.setAlpha(255);
        canvas.drawText(percent, cx - w / 2f, cy + 8, displayPaint);
        canvas.restore();
        text(canvas, LocaleController.getString(R.string.FG_Wrapped_Loading), bodyPaint, 17, cy + 85, 300, 1f, 1f);
    }

    private void brandPill(Canvas canvas, float cy, float alpha) {
        headPaint.setTextSize(11);
        headPaint.setLetterSpacing(0.22f);
        String brand = "FINEGRAM RECAP";
        float w = headPaint.measureText(brand);
        rect.set(W / 2f - w / 2f - 14, cy - 13, W / 2f + w / 2f + 14, cy + 13);
        glassCard(canvas, rect, 13, alpha);
        headPaint.setAlpha((int) (255 * alpha));
        canvas.drawText(brand, W / 2f - w / 2f, cy + 4, headPaint);
        headPaint.setAlpha(255);
        headPaint.setLetterSpacing(0f);
    }

    private void drawIntro(Canvas canvas, float time, float alpha) {
        rays(canvas, W / 2f, 270, 520, time * 9f, 24, ease(time, 0f, 1.2f) * alpha);
        brandPill(canvas, 132, ease(time, 0.1f, 0.8f) * alpha);
        String hello = LocaleController.getString(R.string.FG_Wrapped_IntroWord);
        stagger(canvas, hello, headPaint, 28, W / 2f, 225, 320, time, 0.3f, 0.05f, alpha);
        String title = stats.isYear() ? String.valueOf(stats.year) : periodName();
        float pulse = 1f + 0.025f * (float) Math.sin(time * 2.4f);
        canvas.save();
        canvas.scale(pulse, pulse, W / 2f, 280);
        stagger(canvas, title.toUpperCase(LocaleController.getInstance().getCurrentLocale()), displayPaint, 50, W / 2f, 300, 330,
                time, 0.55f, 0.07f, alpha);
        canvas.restore();
        text(canvas, LocaleController.getString(R.string.FG_Wrapped_IntroSub), bodyPaint, 20, 325, 300, ease(time, 1.3f, 0.8f), alpha);
        if (once("intro", time, 1.0f)) {
            burst(W / 2f, 280, 90, 420f, true);
            burst(W / 2f, 280, 60, 600f, false, 60f);
            shockwave(W / 2f, 280, 210f);
        }
        float hint = ease(time, 2.2f, 0.8f) * (0.55f + 0.45f * (float) Math.sin(time * 3.2f));
        text(canvas, LocaleController.getString(R.string.FG_Wrapped_TapToStart), bodyPaint, 14, 575, 300, hint, alpha);
    }

    private void drawTotal(Canvas canvas, float time, float alpha) {
        text(canvas, LocaleController.getString(R.string.FG_Wrapped_SentTitle), headPaint, 22, 150, 300, ease(time, 0.1f, 0.7f), alpha);
        float landed = odometerEnd(stats.total, 0.3f);
        float glow = ease(time, landed, 0.6f) * (0.5f + 0.5f * (float) Math.sin(time * 3f));
        flash(canvas, W / 2f, 250, time, landed, alpha);
        if (glow > 0f) {
            shaderPaint.setAlpha(255);
            shaderPaint.setShader(new RadialGradient(W / 2f, 250, 170, ColorUtils.setAlphaComponent(Color.WHITE, (int) (60 * glow * alpha)), 0, Shader.TileMode.CLAMP));
            canvas.drawCircle(W / 2f, 250, 170, shaderPaint);
            shaderPaint.setShader(null);
        }
        float kick = punch(time, landed);
        canvas.save();
        canvas.scale(kick, kick, W / 2f, 250);
        odometer(canvas, stats.total, displayPaint, 66, W / 2f, 275, 320, time, 0.3f, alpha * ease(time, 0.2f, 0.4f), 0);
        canvas.restore();
        if (once("total", time, landed)) {
            blast(W / 2f, 250, 70f, 1f);
        }
        stagger(canvas, plural(R.string.FG_Wrapped_W_Messages, stats.total), headPaint, 26, W / 2f, 330, 320, time, 1.4f, 0.04f, alpha);
        text(canvas, LocaleController.formatString(R.string.FG_Wrapped_InChats, stats.chats, plural(R.string.FG_Wrapped_W_ChatsIn, stats.chats)),
                bodyPaint, 18, 352, 300, ease(time, 1.9f, 0.7f), alpha);
        if (stats.sampled > 0 && stats.sampledChars > 0) {
            long chars = stats.sampledChars * stats.total / Math.max(1, stats.sampled);
            long pagesCount = Math.max(1, chars / 1800);
            float e = spring(time, 2.6f, 0.8f);
            if (e > 0f) {
                canvas.save();
                canvas.translate(0, (1f - e) * 40f);
                rect.set(40, 420, W - 40, 490);
                glassCard(canvas, rect, 22, Math.min(1f, e) * alpha);
                text(canvas, LocaleController.formatString(R.string.FG_Wrapped_Book, number(pagesCount), plural(R.string.FG_Wrapped_W_Pages, pagesCount)),
                        bodyPaint, 16, 433, 250, 1f, Math.min(1f, e) * alpha);
                canvas.restore();
            }
        }
    }

    private void drawTop(Canvas canvas, float time, float alpha) {
        stagger(canvas, LocaleController.getString(R.string.FG_Wrapped_TopTitle), headPaint, 25, W / 2f, 120, 320, time, 0.05f, 0.025f, alpha);
        int max = Math.max(1, stats.top.get(0).count);
        for (int i = 0; i < stats.top.size(); i++) {
            FGWrappedStats.Person person = stats.top.get(i);
            float delay = 0.45f + i * 0.2f;
            float slide = spring(time, delay, 0.8f);
            if (slide <= 0f) {
                continue;
            }
            float a = Math.min(1f, ease(time, delay, 0.4f)) * alpha;
            float y = 190 + i * 82;
            canvas.save();
            canvas.translate((1f - slide) * 220f, 0);
            rect.set(18, y - 34, W - 18, y + 34);
            glassCard(canvas, rect, 24, a);
            displayPaint.setTextSize(18);
            displayPaint.setAlpha((int) (255 * a));
            canvas.drawText(String.valueOf(i + 1), 34, y + 7, displayPaint);
            displayPaint.setAlpha(255);
            float pop = spring(time, delay + 0.15f, 0.7f);
            avatar(canvas, i < avatars.size() ? avatars.get(i) : null, 84, y, 46 * pop, a);
            headPaint.setTextSize(16);
            headPaint.setAlpha((int) (255 * a));
            CharSequence name = TextUtils.ellipsize(nameOf(person.peer), headPaint, 170, TextUtils.TruncateAt.END);
            canvas.drawText(name, 0, name.length(), 118, y - 6, headPaint);
            headPaint.setAlpha(255);
            float bar = ease(time, delay + 0.3f, 1.4f) * (person.count / (float) max);
            rect.set(118, y + 6, 118 + 118 * bar, y + 14);
            shaderPaint.setShader(new LinearGradient(118, 0, 236, 0, Color.WHITE, ColorUtils.setAlphaComponent(Color.WHITE, 140), Shader.TileMode.CLAMP));
            shaderPaint.setAlpha((int) (255 * a));
            canvas.drawRoundRect(rect, 4, 4, shaderPaint);
            shaderPaint.setShader(null);
            headPaint.setTextSize(15);
            odometer(canvas, person.count, headPaint, 15, W - 34, y + 14, 64, time, delay + 0.3f, a, 1);
            canvas.restore();
        }
    }

    private void drawBest(Canvas canvas, float time, float alpha) {
        FGWrappedStats.Person person = stats.top.get(0);
        stagger(canvas, LocaleController.getString(stats.isYear() ? R.string.FG_Wrapped_BestYear : R.string.FG_Wrapped_BestMonth),
                headPaint, 24, W / 2f, 105, 320, time, 0.05f, 0.03f, alpha);
        float cx = W / 2f;
        float cy = 265;
        float pop = spring(time, 0.4f, 0.9f);
        float glow = 0.5f + 0.5f * (float) Math.sin(time * 2.6f);
        float appear = Math.min(1f, ease(time, 0.4f, 0.6f)) * alpha;
        for (int i = 3; i >= 1; i--) {
            paint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, (int) ((16 + 12 * glow) * appear / i)));
            canvas.drawCircle(cx, cy, 78 * pop + i * 16 + glow * 8, paint);
        }
        strokePaint.setStrokeWidth(2.4f);
        strokePaint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, (int) (170 * appear)));
        strokePaint.setPathEffect(new DashPathEffect(new float[]{10, 9}, 0));
        canvas.save();
        canvas.rotate(time * 40f, cx, cy);
        canvas.drawCircle(cx, cy, 96 * pop, strokePaint);
        canvas.restore();
        strokePaint.setPathEffect(null);
        for (int i = 0; i < 10; i++) {
            double angle = time * (0.6f + i * 0.09f) * (i % 2 == 0 ? 1 : -1) + i * 0.63;
            float r = (112 + (i % 3) * 14) * pop;
            float s = 2.2f + (i % 4) * 0.8f;
            paint.setColor(ColorUtils.setAlphaComponent(i % 3 == 0 ? 0xffffd166 : Color.WHITE, (int) (220 * appear)));
            canvas.drawCircle(cx + (float) Math.cos(angle) * r, cy + (float) Math.sin(angle) * r, s, paint);
        }
        avatar(canvas, bestAvatar, cx, cy, 150 * pop, appear);
        if (once("best", time, 0.85f)) {
            burst(cx, cy, 70, 380f, true);
            burst(cx, cy, 50, 560f, false, 80f);
            shockwave(cx, cy, 170f);
        }
        stagger(canvas, nameOf(person.peer), displayPaint, 24, W / 2f, 415, 320, time, 1.0f, 0.04f, alpha);
        float countWidth = odometer(canvas, person.count, displayPaint, 30, W / 2f, 465, 200, time, 1.3f, alpha * ease(time, 1.2f, 0.4f), 0);
        text(canvas, plural(R.string.FG_Wrapped_W_Messages, person.count), bodyPaint, 17, 475, 300, ease(time, 1.8f, 0.6f), alpha);
        String share;
        if (person.count * 2 > stats.total) {
            share = LocaleController.getString(R.string.FG_Wrapped_BestHalf);
        } else {
            share = LocaleController.formatString(R.string.FG_Wrapped_BestShare, Math.max(2, Math.round(stats.total / (float) person.count)));
        }
        float e = spring(time, 2.3f, 0.8f);
        if (e > 0f && countWidth > 0f) {
            canvas.save();
            canvas.translate(0, (1f - e) * 30f);
            rect.set(36, 512, W - 36, 566);
            glassCard(canvas, rect, 20, Math.min(1f, e) * alpha);
            text(canvas, share, bodyPaint, 15, 523, 270, 1f, Math.min(1f, e) * alpha);
            canvas.restore();
        }
    }

    private int peakHour() {
        int best = 0;
        for (int i = 1; i < 24; i++) {
            if (stats.hours[i] > stats.hours[best]) {
                best = i;
            }
        }
        return best;
    }

    private int persona() {
        int night = 0;
        int morning = 0;
        int day = 0;
        int evening = 0;
        for (int i = 0; i < 24; i++) {
            if (i < 5) {
                night += stats.hours[i];
            } else if (i < 11) {
                morning += stats.hours[i];
            } else if (i < 18) {
                day += stats.hours[i];
            } else {
                evening += stats.hours[i];
            }
        }
        int total = Math.max(1, night + morning + day + evening);
        if (night * 100 / total >= 18) {
            return 0;
        }
        if (morning * 100 / total >= 30) {
            return 1;
        }
        return evening >= day ? 3 : 2;
    }

    private void personaIcon(Canvas canvas, int persona, float cx, float cy, float size, float time, float alpha) {
        int a = (int) (255 * alpha);
        if (persona == 0) {
            path.rewind();
            path.addCircle(cx, cy, size * 0.42f, Path.Direction.CW);
            cutPath.rewind();
            cutPath.addCircle(cx + size * 0.2f, cy - size * 0.14f, size * 0.36f, Path.Direction.CW);
            path.op(cutPath, Path.Op.DIFFERENCE);
            paint.setColor(ColorUtils.setAlphaComponent(0xfffff3c4, a));
            canvas.drawPath(path, paint);
            for (int i = 0; i < 3; i++) {
                float tw = 0.5f + 0.5f * (float) Math.sin(time * 3f + i * 2f);
                paint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, (int) (a * tw)));
                canvas.drawCircle(cx + size * (0.3f + i * 0.12f), cy + size * (-0.4f + i * 0.28f), size * 0.04f, paint);
            }
            return;
        }
        int sun = persona == 3 ? 0xffff9f1c : 0xffffd166;
        canvas.save();
        canvas.rotate(time * 30f, cx, cy);
        strokePaint.setStrokeWidth(size * 0.07f);
        strokePaint.setStrokeCap(Paint.Cap.ROUND);
        strokePaint.setColor(ColorUtils.setAlphaComponent(sun, a));
        for (int i = 0; i < 8; i++) {
            double angle = Math.toRadians(i * 45);
            canvas.drawLine(cx + (float) Math.cos(angle) * size * 0.42f, cy + (float) Math.sin(angle) * size * 0.42f,
                    cx + (float) Math.cos(angle) * size * 0.56f, cy + (float) Math.sin(angle) * size * 0.56f, strokePaint);
        }
        canvas.restore();
        paint.setColor(ColorUtils.setAlphaComponent(sun, a));
        canvas.drawCircle(cx, cy, size * 0.3f, paint);
        if (persona == 1 || persona == 3) {
            paint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, (int) (a * 0.85f)));
            rect.set(cx - size * 0.62f, cy + size * 0.12f, cx + size * 0.62f, cy + size * 0.2f);
            canvas.drawRoundRect(rect, size * 0.04f, size * 0.04f, paint);
        }
    }

    private void drawClock(Canvas canvas, float time, float alpha) {
        stagger(canvas, LocaleController.getString(R.string.FG_Wrapped_ClockTitle), headPaint, 25, W / 2f, 100, 320, time, 0.05f, 0.03f, alpha);
        float cx = W / 2f;
        float cy = 280;
        float inner = 66;
        float outer = 146;
        int max = 1;
        for (int value : stats.hours) {
            max = Math.max(max, value);
        }
        int peak = peakHour();
        float sweepTime = Math.max(0f, time - 0.3f);
        float sweep = Math.min(360f, sweepTime / 1.6f * 360f);
        if (sweepTime > 0f) {
            float radar = (sweepTime < 1.6f ? 1f : 0.45f) * Math.min(1f, sweepTime * 4f) * alpha;
            shaderPaint.setAlpha((int) (255 * radar));
            shaderPaint.setShader(new SweepGradient(cx, cy, new int[]{0x00ffffff, 0x00ffffff, 0x55ffffff}, new float[]{0f, 0.75f, 1f}));
            canvas.save();
            canvas.rotate(sweepTime / 1.6f * 360f - 90f, cx, cy);
            canvas.drawCircle(cx, cy, outer + 8, shaderPaint);
            canvas.restore();
            shaderPaint.setShader(null);
        }
        strokePaint.setStrokeCap(Paint.Cap.ROUND);
        for (int i = 0; i < 24; i++) {
            float angleDeg = i * 15;
            float appear = sweep >= angleDeg ? spring(time, 0.3f + angleDeg / 360f * 1.6f, 0.6f) : 0f;
            if (appear <= 0f) {
                continue;
            }
            float v = stats.hours[i] / (float) max;
            float length = (outer - inner) * Math.max(0.05f, v) * appear;
            double angle = Math.toRadians(angleDeg - 90);
            float cos = (float) Math.cos(angle);
            float sin = (float) Math.sin(angle);
            if (i == peak) {
                float pulse = 0.5f + 0.5f * (float) Math.sin(time * 4f);
                strokePaint.setStrokeWidth(16);
                strokePaint.setColor(ColorUtils.setAlphaComponent(0xffffd166, (int) ((60 + 60 * pulse) * alpha)));
                canvas.drawLine(cx + cos * inner, cy + sin * inner, cx + cos * (inner + length), cy + sin * (inner + length), strokePaint);
            }
            strokePaint.setStrokeWidth(9);
            strokePaint.setColor(ColorUtils.setAlphaComponent(i == peak ? 0xffffd166 : Color.WHITE, (int) ((i == peak ? 255 : 150) * alpha)));
            canvas.drawLine(cx + cos * inner, cy + sin * inner, cx + cos * (inner + length), cy + sin * (inner + length), strokePaint);
        }
        bodyPaint.setTextSize(11);
        bodyPaint.setAlpha((int) (180 * alpha));
        for (int h = 0; h < 24; h += 6) {
            double angle = Math.toRadians(h * 15 - 90);
            String label = String.format(Locale.US, "%02d", h);
            float tw = bodyPaint.measureText(label);
            canvas.drawText(label, cx + (float) Math.cos(angle) * (inner - 16) - tw / 2, cy + (float) Math.sin(angle) * (inner - 16) + 4, bodyPaint);
        }
        bodyPaint.setAlpha(255);
        int persona = persona();
        float iconPop = spring(time, 1.9f, 0.8f);
        personaIcon(canvas, persona, cx, cy, 52 * iconPop, time, Math.min(1f, iconPop) * alpha);
        int[] titles = {R.string.FG_Wrapped_Owl, R.string.FG_Wrapped_Lark, R.string.FG_Wrapped_Day, R.string.FG_Wrapped_Evening};
        stagger(canvas, LocaleController.getString(titles[persona]), displayPaint, 24, W / 2f, 485, 320, time, 2.1f, 0.04f, alpha);
        text(canvas, LocaleController.formatString(R.string.FG_Wrapped_Peak, String.format(Locale.US, "%02d:00", peak)),
                bodyPaint, 17, 505, 300, ease(time, 2.6f, 0.7f), alpha);
    }

    private void drawSticker(Canvas canvas, float time, float alpha) {
        stagger(canvas, LocaleController.getString(R.string.FG_Wrapped_StickerTitle), headPaint, 25, W / 2f, 105, 320, time, 0.05f, 0.03f, alpha);
        float cy = 290;
        rays(canvas, W / 2f, cy, 340, -time * 14f, 20, ease(time, 0.3f, 1f) * alpha);
        float drop = spring(time, 0.35f, 1.0f);
        float size = 220;
        float floatY = (float) Math.sin(time * 2.2f) * 6f;
        float wobble = (float) Math.sin(time * 1.8f) * 5f;
        if (drop > 0f && stickerImage != null) {
            canvas.save();
            canvas.translate(0, -(1f - drop) * 380f + floatY);
            canvas.rotate(wobble, W / 2f, cy);
            paint.setColor(ColorUtils.setAlphaComponent(Color.BLACK, (int) (40 * alpha)));
            rect.set(W / 2f - 70, cy + 112, W / 2f + 70, cy + 126);
            canvas.drawOval(rect, paint);
            stickerImage.setImageCoords(W / 2f - size / 2, cy - size / 2, size, size);
            stickerImage.setAlpha(Math.min(1f, drop) * alpha);
            stickerImage.draw(canvas);
            canvas.restore();
        }
        if (once("sticker", time, 1.0f)) {
            burst(W / 2f, cy + 60, 50, 420f, false, 40f);
            shockwave(W / 2f, cy + 110, 150f);
        }
        float stickerLanded = odometerEnd(stats.stickerCount, 1.2f);
        float stickerKick = punch(time, stickerLanded);
        canvas.save();
        canvas.scale(stickerKick, stickerKick, W / 2f, 475);
        float width = odometer(canvas, stats.stickerCount, displayPaint, 40, W / 2f, 490, 200, time, 1.2f, alpha * ease(time, 1.1f, 0.3f), 0);
        canvas.restore();
        if (once("stickerCount", time, stickerLanded)) {
            burst(W / 2f, 475, 36, 420f, false, 30f);
        }
        if (width > 0f) {
            text(canvas, plural(R.string.FG_Wrapped_W_Times, stats.stickerCount), bodyPaint, 18, 500, 300, ease(time, 1.6f, 0.6f), alpha);
        }
    }

    private void mediaIcon(Canvas canvas, int kind, float cx, float cy, float size, float alpha) {
        int a = (int) (255 * alpha);
        paint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, (int) (a * 0.22f)));
        canvas.drawCircle(cx, cy, size / 2f, paint);
        paint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, a));
        strokePaint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, a));
        strokePaint.setStrokeWidth(size * 0.07f);
        strokePaint.setStrokeCap(Paint.Cap.ROUND);
        float u = size / 2f;
        if (kind == 0) {
            rect.set(cx - u * 0.22f, cy - u * 0.55f, cx + u * 0.22f, cy + u * 0.12f);
            canvas.drawRoundRect(rect, u * 0.22f, u * 0.22f, paint);
            rect.set(cx - u * 0.4f, cy - u * 0.25f, cx + u * 0.4f, cy + u * 0.32f);
            canvas.drawArc(rect, 0, 180, false, strokePaint);
            canvas.drawLine(cx, cy + u * 0.32f, cx, cy + u * 0.5f, strokePaint);
        } else if (kind == 1) {
            strokePaint.setStrokeWidth(size * 0.06f);
            canvas.drawCircle(cx, cy, u * 0.5f, strokePaint);
            path.rewind();
            path.moveTo(cx - u * 0.14f, cy - u * 0.22f);
            path.lineTo(cx + u * 0.24f, cy);
            path.lineTo(cx - u * 0.14f, cy + u * 0.22f);
            path.close();
            canvas.drawPath(path, paint);
        } else {
            rect.set(cx - u * 0.52f, cy - u * 0.38f, cx + u * 0.52f, cy + u * 0.38f);
            strokePaint.setStrokeWidth(size * 0.06f);
            canvas.drawRoundRect(rect, u * 0.12f, u * 0.12f, strokePaint);
            canvas.drawCircle(cx + u * 0.22f, cy - u * 0.12f, u * 0.08f, paint);
            path.rewind();
            path.moveTo(cx - u * 0.42f, cy + u * 0.3f);
            path.lineTo(cx - u * 0.1f, cy - u * 0.05f);
            path.lineTo(cx + u * 0.12f, cy + u * 0.16f);
            path.lineTo(cx + u * 0.24f, cy + u * 0.06f);
            path.lineTo(cx + u * 0.44f, cy + u * 0.3f);
            path.close();
            canvas.drawPath(path, paint);
        }
    }

    private void drawMedia(Canvas canvas, float time, float alpha) {
        stagger(canvas, LocaleController.getString(R.string.FG_Wrapped_MediaTitle), headPaint, 25, W / 2f, 100, 320, time, 0.05f, 0.025f, alpha);
        int[][] rows = {
                {stats.voice, R.string.FG_Wrapped_W_Voice, 0},
                {stats.rounds, R.string.FG_Wrapped_W_Rounds, 1},
                {stats.photos, R.string.FG_Wrapped_W_Photos, 2},
        };
        float y = 175;
        int shown = 0;
        for (int[] row : rows) {
            if (row[0] <= 0) {
                continue;
            }
            float delay = 0.4f + shown * 0.3f;
            float e = spring(time, delay, 0.8f);
            if (e > 0f) {
                float a = Math.min(1f, e) * alpha;
                canvas.save();
                canvas.translate(0, (1f - e) * 120f);
                rect.set(24, y, W - 24, y + 112);
                glassCard(canvas, rect, 28, a);
                float bounce = 1f + 0.06f * (float) Math.sin(time * 3f + shown);
                canvas.save();
                canvas.scale(bounce, bounce, 82, y + 56);
                mediaIcon(canvas, row[2], 82, y + 56, 62, a);
                canvas.restore();
                odometer(canvas, row[0], displayPaint, 36, 132, y + 62, 180, time, delay + 0.15f, a, -1);
                bodyPaint.setTextSize(15);
                bodyPaint.setAlpha((int) (220 * a));
                canvas.drawText(plural(row[1], row[0]), 134, y + 88, bodyPaint);
                bodyPaint.setAlpha(255);
                canvas.restore();
            }
            y += 128;
            shown++;
        }
    }

    private void drawCalls(Canvas canvas, float time, float alpha) {
        stagger(canvas, LocaleController.getString(R.string.FG_Wrapped_CallsTitle), headPaint, 25, W / 2f, 105, 320, time, 0.05f, 0.03f, alpha);
        float cx = W / 2f;
        float cy = 255;
        for (int i = 0; i < 4; i++) {
            float phase = ((time + i * 0.45f) % 1.8f) / 1.8f;
            float r = 50 + phase * 150;
            strokePaint.setStrokeWidth(3);
            strokePaint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, (int) (120 * (1f - phase) * alpha * ease(time, 0.2f, 0.6f))));
            canvas.drawCircle(cx, cy, r, strokePaint);
        }
        int minutes = Math.max(1, stats.callSeconds / 60);
        long value;
        String unit;
        if (minutes >= 120) {
            value = minutes / 60;
            unit = plural(R.string.FG_Wrapped_W_Hours, value);
        } else {
            value = minutes;
            unit = plural(R.string.FG_Wrapped_W_Minutes, minutes);
        }
        float callsLanded = odometerEnd(value, 0.3f);
        flash(canvas, cx, 260, time, callsLanded, alpha);
        float callsKick = punch(time, callsLanded);
        canvas.save();
        canvas.scale(callsKick, callsKick, cx, 260);
        odometer(canvas, value, displayPaint, 70, cx, 285, 320, time, 0.3f, alpha * ease(time, 0.2f, 0.4f), 0);
        canvas.restore();
        if (once("calls", time, callsLanded)) {
            blast(cx, 260, 60f, 0.8f);
        }
        stagger(canvas, unit, headPaint, 26, cx, 335, 320, time, 1.2f, 0.04f, alpha);
        text(canvas, number(stats.calls) + " " + plural(R.string.FG_Wrapped_W_Calls, stats.calls), bodyPaint, 17, 352, 300, ease(time, 1.6f, 0.6f), alpha);
        if (stats.callPeer != null && callAvatar != null) {
            float pop = spring(time, 2.0f, 0.8f);
            float a = Math.min(1f, pop) * alpha;
            strokePaint.setStrokeWidth(3);
            strokePaint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, (int) (200 * a)));
            canvas.drawCircle(cx, 440, 44 * pop, strokePaint);
            avatar(canvas, callAvatar, cx, 440, 80 * pop, a);
            text(canvas, LocaleController.formatString(R.string.FG_Wrapped_CallsLongest, nameOf(stats.callPeer)), bodyPaint, 16, 495, 300,
                    ease(time, 2.3f, 0.6f), alpha);
        }
    }

    private void drawWord(Canvas canvas, float time, float alpha) {
        stagger(canvas, LocaleController.getString(R.string.FG_Wrapped_WordTitle), headPaint, 25, W / 2f, 140, 320, time, 0.05f, 0.03f, alpha);
        String word = stats.word.toUpperCase(LocaleController.getInstance().getCurrentLocale());
        float size = fit(displayPaint, word, 52, 310);
        float total = displayPaint.measureText(word);
        float x = W / 2f - total / 2f;
        float baseline = 310;
        Random scatter = new Random(word.hashCode());
        float glow = ease(time, 1.8f, 0.6f) * (0.5f + 0.5f * (float) Math.sin(time * 3f));
        if (glow > 0f) {
            shaderPaint.setAlpha(255);
            shaderPaint.setShader(new RadialGradient(W / 2f, baseline - size * 0.35f, 180, ColorUtils.setAlphaComponent(Color.WHITE, (int) (55 * glow * alpha)), 0, Shader.TileMode.CLAMP));
            canvas.drawCircle(W / 2f, baseline - size * 0.35f, 180, shaderPaint);
            shaderPaint.setShader(null);
        }
        for (int i = 0; i < word.length(); ) {
            int end = i + Character.charCount(word.codePointAt(i));
            String ch = word.substring(i, end);
            float advance = displayPaint.measureText(ch);
            float sx = (scatter.nextFloat() - 0.5f) * 320f;
            float sy = (scatter.nextFloat() - 0.5f) * 420f;
            float sr = (scatter.nextFloat() - 0.5f) * 220f;
            float e = spring(time, 0.35f + i * 0.06f, 1.0f);
            float a = Math.min(1f, ease(time, 0.3f + i * 0.06f, 0.4f)) * alpha;
            if (a > 0f) {
                canvas.save();
                canvas.translate(x + advance / 2f + sx * (1f - e), baseline + sy * (1f - e));
                canvas.rotate(sr * (1f - e));
                displayPaint.setAlpha((int) (255 * a));
                canvas.drawText(ch, -advance / 2f, 0, displayPaint);
                canvas.restore();
            }
            x += advance;
            i = end;
        }
        displayPaint.setAlpha(255);
        if (once("word", time, 1.6f)) {
            blast(W / 2f, baseline - size * 0.3f, 60f, 0.9f);
        }
        text(canvas, LocaleController.formatString(R.string.FG_Wrapped_WordSub, number(stats.wordCount), plural(R.string.FG_Wrapped_W_Times, stats.wordCount)),
                bodyPaint, 18, 360, 300, ease(time, 1.9f, 0.6f), alpha);
    }

    private void drawEmpty(Canvas canvas, float time, float alpha) {
        float pop = spring(time, 0.1f, 0.8f);
        strokePaint.setStrokeWidth(4);
        strokePaint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, (int) (200 * Math.min(1f, pop) * alpha)));
        canvas.drawCircle(W / 2f, 240, 46 * pop, strokePaint);
        text(canvas, LocaleController.getString(R.string.FG_Wrapped_Empty), headPaint, 22, 310, 300, ease(time, 0.3f, 0.8f), alpha);
    }

    private void drawSummary(Canvas canvas, float time, float alpha, boolean export) {
        float flip = export ? 1f : spring(time, 0.1f, 1.1f);
        float cardBottom = export ? 600 : 540;
        float cx = W / 2f;
        float cy = (60 + cardBottom) / 2f;
        canvas.save();
        camera.save();
        camera.rotateY(-80f * (1f - flip));
        camera.getMatrix(matrix);
        camera.restore();
        matrix.preTranslate(-cx, -cy);
        matrix.postTranslate(cx, cy);
        canvas.concat(matrix);
        float a = Math.min(1f, Math.max(0f, flip)) * alpha;
        rect.set(22, 60, W - 22, cardBottom);
        glassCard(canvas, rect, 30, a);
        float shine = export ? -1f : ((time - 1.2f) % 4.5f) / 1.1f;
        if (shine >= 0f && shine <= 1f) {
            canvas.save();
            canvas.clipRect(22, 60, W - 22, cardBottom);
            float sx = -120 + shine * (W + 240);
            shaderPaint.setAlpha((int) (255 * a));
            shaderPaint.setShader(new LinearGradient(sx - 60, 0, sx + 60, 0, new int[]{0x00ffffff, 0x40ffffff, 0x00ffffff}, null, Shader.TileMode.CLAMP));
            canvas.save();
            canvas.rotate(18, sx, cy);
            canvas.drawRect(sx - 60, 0, sx + 60, H, shaderPaint);
            canvas.restore();
            shaderPaint.setShader(null);
            canvas.restore();
        }
        String title = stats.isYear()
                ? LocaleController.formatString(R.string.FG_Wrapped_SummaryYear, stats.year)
                : LocaleController.formatString(R.string.FG_Wrapped_SummaryMonth, periodName());
        text(canvas, title, headPaint, 19, 84, 300, 1f, a);
        odometer(canvas, stats.total, displayPaint, 46, cx, 172, 290, export ? 30f : time, 0.5f, a, 0);
        text(canvas, plural(R.string.FG_Wrapped_W_Messages, stats.total), bodyPaint, 15, 182, 300, 1f, a);
        int people = Math.min(3, avatars.size());
        float spacing = 94;
        float start = cx - (people - 1) * spacing / 2f;
        for (int i = 0; i < people; i++) {
            float pop = export ? 1f : spring(time, 0.6f + i * 0.12f, 0.7f);
            float x = start + i * spacing;
            avatar(canvas, avatars.get(i), x, 248, 56 * pop, a);
            bodyPaint.setTextSize(12);
            bodyPaint.setAlpha((int) (220 * a));
            CharSequence name = TextUtils.ellipsize(nameOf(stats.top.get(i).peer), bodyPaint, 86, TextUtils.TruncateAt.END);
            float tw = bodyPaint.measureText(name, 0, name.length());
            canvas.drawText(name, 0, name.length(), x - tw / 2, 296, bodyPaint);
            bodyPaint.setAlpha(255);
        }
        String[][] cells = new String[4][];
        int cell = 0;
        if (stats.sampled >= 20) {
            int[] titles = {R.string.FG_Wrapped_Owl, R.string.FG_Wrapped_Lark, R.string.FG_Wrapped_Day, R.string.FG_Wrapped_Evening};
            cells[cell++] = new String[]{String.format(Locale.US, "%02d:00", peakHour()), LocaleController.getString(titles[persona()])};
        }
        cells[cell++] = new String[]{number(stats.chats), plural(R.string.FG_Wrapped_W_ChatsIn, stats.chats)};
        if (stats.voice + stats.rounds > 0 && cell < 4) {
            cells[cell++] = new String[]{number(stats.voice + stats.rounds), LocaleController.getString(R.string.FG_Wrapped_VoiceAndRounds)};
        }
        if (stats.callSeconds >= 60 && cell < 4) {
            cells[cell++] = new String[]{number(Math.max(1, stats.callSeconds / 60)), plural(R.string.FG_Wrapped_W_Minutes, Math.max(1, stats.callSeconds / 60)) + " " + LocaleController.getString(R.string.FG_Wrapped_InCalls)};
        }
        if (stats.word != null && cell < 4) {
            cells[cell++] = new String[]{stats.word, LocaleController.getString(R.string.FG_Wrapped_WordTitle)};
        }
        float colW = (W - 60) / 2f;
        for (int i = 0; i < cell; i++) {
            float ccx = 30 + colW * (i % 2) + colW / 2f;
            float top = 318 + (i / 2) * 96;
            float pop = export ? 1f : spring(time, 0.9f + i * 0.12f, 0.7f);
            if (pop <= 0f) {
                continue;
            }
            float ca = Math.min(1f, pop) * a;
            canvas.save();
            canvas.scale(pop, pop, ccx, top + 40);
            rect.set(ccx - colW / 2f + 6, top, ccx + colW / 2f - 6, top + 84);
            paint.setColor(ColorUtils.setAlphaComponent(Color.WHITE, (int) (22 * ca)));
            canvas.drawRoundRect(rect, 20, 20, paint);
            fit(displayPaint, cells[i][0], 20, colW - 28);
            float vw = displayPaint.measureText(cells[i][0]);
            displayPaint.setAlpha((int) (255 * ca));
            canvas.drawText(cells[i][0], ccx - vw / 2f, top + 40, displayPaint);
            displayPaint.setAlpha(255);
            canvas.restore();
            canvas.save();
            canvas.translate(ccx - W / 2f, 0);
            text(canvas, cells[i][1], bodyPaint, 12, top + 50, colW - 28, 1f, ca);
            canvas.restore();
        }
        brandPill(canvas, cardBottom - 22, a);
        canvas.restore();
        if (!export) {
            if (once("summary", time, 0.9f)) {
                burst(cx, 160, 110, 460f, true);
                shockwave(cx, 160, 200f);
            }
            float b = ease(time, 1.4f, 0.8f);
            drawButton(canvas, shareButton, 22, 560, W / 2f - 28, LocaleController.getString(R.string.FG_Wrapped_Share), b * alpha, true);
            drawButton(canvas, saveButton, W / 2f + 6, 560, W - 22 - (W / 2f + 6), LocaleController.getString(R.string.FG_Wrapped_Save), b * alpha, false);
        }
    }

    private void drawButton(Canvas canvas, RectF bounds, float x, float y, float width, String label, float alpha, boolean primary) {
        bounds.set(x, y, x + width, y + 48);
        paint.setColor(primary ? ColorUtils.setAlphaComponent(Color.WHITE, (int) (255 * alpha)) : ColorUtils.setAlphaComponent(Color.WHITE, (int) (60 * alpha)));
        canvas.drawRoundRect(bounds, 24, 24, paint);
        headPaint.setTextSize(16);
        int color = headPaint.getColor();
        headPaint.setColor(primary ? 0xff1d1d2b : Color.WHITE);
        headPaint.setAlpha((int) (255 * alpha));
        float tw = headPaint.measureText(label);
        canvas.drawText(label, bounds.centerX() - tw / 2, bounds.centerY() + 6, headPaint);
        headPaint.setColor(color);
    }

    private boolean hitSummary(float x, float y, RectF bounds) {
        float scale = Math.min(getWidth() / W, getHeight() / H);
        float vx = (x - (getWidth() - W * scale) / 2f) / scale;
        float vy = (y - (getHeight() - H * scale) / 2f) / scale;
        return bounds.contains(vx, vy);
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                downX = event.getX();
                downY = event.getY();
                downTime = SystemClock.uptimeMillis();
                holding = true;
                pausedAt = SystemClock.uptimeMillis();
                return true;
            case MotionEvent.ACTION_MOVE:
                return true;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL: {
                if (holding) {
                    holding = false;
                    pausedTotal += SystemClock.uptimeMillis() - pausedAt;
                }
                if (event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
                    return true;
                }
                float dx = event.getX() - downX;
                float dy = event.getY() - downY;
                int slop = ViewConfiguration.get(getContext()).getScaledTouchSlop();
                if (dy > AndroidUtilities.dp(110) && Math.abs(dx) < dy) {
                    if (onClose != null) {
                        onClose.run();
                    }
                    return true;
                }
                if (Math.abs(dx) > AndroidUtilities.dp(60) && Math.abs(dx) > Math.abs(dy)) {
                    go(dx < 0 ? 1 : -1);
                    return true;
                }
                if (Math.abs(dx) > slop || Math.abs(dy) > slop || SystemClock.uptimeMillis() - downTime > 350) {
                    return true;
                }
                float x = event.getX();
                float y = event.getY();
                if (x > closeX - AndroidUtilities.dp(30) && y > closeY - AndroidUtilities.dp(30) && y < closeY + AndroidUtilities.dp(30)) {
                    if (onClose != null) {
                        onClose.run();
                    }
                    return true;
                }
                if (stats != null && page() == P_SUMMARY) {
                    if (hitSummary(x, y, shareButton)) {
                        export(true);
                        return true;
                    }
                    if (hitSummary(x, y, saveButton)) {
                        export(false);
                        return true;
                    }
                }
                go(x < getWidth() / 3f ? -1 : 1);
                return true;
            }
        }
        return super.onTouchEvent(event);
    }

    private void export(boolean share) {
        if (sharing || stats == null) {
            return;
        }
        sharing = true;
        Bitmap bitmap;
        try {
            bitmap = Bitmap.createBitmap(1080, 1920, Bitmap.Config.ARGB_8888);
            drawFrame(new Canvas(bitmap), 1080, 1920, now(), true);
        } catch (Throwable t) {
            FileLog.e(t);
            sharing = false;
            return;
        }
        final Bitmap result = bitmap;
        Utilities.globalQueue.postRunnable(() -> {
            File file = new File(AndroidUtilities.getCacheDir(), "finegram_wrapped_" + System.currentTimeMillis() + ".png");
            boolean ok = false;
            try (FileOutputStream out = new FileOutputStream(file)) {
                ok = result.compress(Bitmap.CompressFormat.PNG, 100, out);
            } catch (Throwable t) {
                FileLog.e(t);
            }
            result.recycle();
            final boolean saved = ok;
            AndroidUtilities.runOnUIThread(() -> {
                sharing = false;
                if (!saved) {
                    return;
                }
                if (share) {
                    try {
                        Uri uri = FileProvider.getUriForFile(getContext(), ApplicationLoader.getApplicationId() + ".provider", file);
                        Intent intent = new Intent(Intent.ACTION_SEND);
                        intent.setType("image/png");
                        intent.putExtra(Intent.EXTRA_STREAM, uri);
                        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                        getContext().startActivity(Intent.createChooser(intent, LocaleController.getString(R.string.FG_Wrapped_Share)));
                    } catch (Throwable t) {
                        FileLog.e(t);
                    }
                } else {
                    MediaController.saveFile(file.getAbsolutePath(), getContext(), 0, null, "image/png", uri -> AndroidUtilities.runOnUIThread(() -> {
                        try {
                            if (getParent() instanceof android.widget.FrameLayout) {
                                BulletinFactory.of((android.widget.FrameLayout) getParent(), null)
                                        .createSimpleBulletin(R.raw.ic_download, LocaleController.getString(R.string.FG_Wrapped_Saved)).show();
                            }
                        } catch (Throwable ignored) {
                        }
                    }));
                }
            });
        });
    }
}

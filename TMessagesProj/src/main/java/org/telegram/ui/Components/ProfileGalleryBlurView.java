package org.telegram.ui.Components;

import static org.telegram.messenger.AndroidUtilities.dp;
import static org.telegram.ui.Components.ProfileMetaballView.profileBlurQueue;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapShader;
import android.graphics.BlendMode;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ComposeShader;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.RecordingCanvas;
import android.graphics.RenderEffect;
import android.graphics.RenderNode;
import android.graphics.Shader;
import android.os.Build;
import android.view.View;

import androidx.annotation.RequiresApi;
import androidx.viewpager.widget.ViewPager;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ImageReceiver;
import org.telegram.messenger.SharedConfig;
import org.telegram.messenger.Utilities;
import org.telegram.ui.ProfileActivity;

public class ProfileGalleryBlurView extends View {

    private boolean usingRenderNode = Build.VERSION.SDK_INT >= 31;
    private final Object lock = new Object();

    private final ProfileMetaballView.BlurBitmapHolder[] nextFrame = new ProfileMetaballView.BlurBitmapHolder[3];
    private final ProfileMetaballView.BlurBitmapHolder[] currentFrame = new ProfileMetaballView.BlurBitmapHolder[3];

    private volatile boolean isBluring = false;
    private volatile int frameGeneration;

    private final Paint[] paints = new Paint[]{
            new Paint(),
            new Paint(),
    };

    private ProfileGalleryView view;
    public int actionSize;
    public int size;

    private final Runnable blurTask = this::doBlur;
    private final Runnable invalidateTask = this::updateContent;

    private int currentPosition = -1;
    private int offset;

    private int frameWidth, frameHeight;
    private boolean loopInvalidate = false;

    private boolean sizeChanged = false;
    private boolean needNewFrame = false;

    private RenderNode blurNode;
    private float renderNodeBlurRadius;
    private int renderNodeActionSize = -1;
    private final android.graphics.Matrix continuationMatrix = new android.graphics.Matrix();
    private final float[] continuationPoints = new float[4];
    private static final float MIRROR_STRETCH = 1.5f;
    private RenderNode openingNode;
    private final Paint openingShadePaint = new Paint();
    private float openingNodeBlurRadius;
    private int openingNodeActionSize = -1;

    private final SizeNotifierFrameLayout.IViewWithInvalidateCallback[] listeners =
            new SizeNotifierFrameLayout.IViewWithInvalidateCallback[3];

    private final AnimatedFloat alpha = new AnimatedFloat(this, 0, 350, CubicBezierInterpolator.DEFAULT);

    private final ViewPager.OnPageChangeListener listener = new ViewPager.OnPageChangeListener() {
        @Override
        public void onPageScrolled(int position, float positionOffset, int positionOffsetPixels) {
            if (!usingRenderNode) {
                if (Math.abs(position - currentPosition) == 1) {
                    if (position > currentPosition) {
                        swap(0, 1, 1);
                    } else if (position < currentPosition) {
                        swap(1, 0, 0);
                        swap(2, 0, -1);
                    }
                }
            }
            int oldPosition = currentPosition;
            int oldOffset = offset;

            currentPosition = position;
            offset = positionOffsetPixels;

            if (oldPosition != currentPosition || oldOffset != offset) {
                updateContent();
            }
        }

        @Override
        public void onPageSelected(int position) {
        }

        @Override
        public void onPageScrollStateChanged(int state) {

        }
    };

    public ProfileGalleryBlurView(Context context) {
        super(context);
        alpha.set(1f, true);

        usingRenderNode &= SharedConfig.useNewBlur;
        if (usingRenderNode) {
            setLayerType(View.LAYER_TYPE_HARDWARE, null);
            return;
        }

        setLayerType(View.LAYER_TYPE_SOFTWARE, paints[0]);
        setLayerType(View.LAYER_TYPE_SOFTWARE, paints[1]);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), (usingRenderNode ? getMirrorLead() : size) + actionSize);
    }

    public void restartAlpha() {
        if (usingRenderNode) {
            alpha.set(0f, true);
            invalidate();
        }
    }

    public boolean isUsingRenderNode() {
        return usingRenderNode;
    }

    private void swap(int from, int to, int clear) {
        synchronized (lock) {
            ProfileMetaballView.BlurBitmapHolder tmp2 = nextFrame[from];
            nextFrame[from] = nextFrame[to];
            nextFrame[to] = tmp2;

            tmp2 = currentFrame[from];
            currentFrame[from] = currentFrame[to];
            currentFrame[to] = tmp2;

            if (from == 2) {
                if (currentFrame[to] != null && currentFrame[to].hasContent) {
                    applyShader(currentFrame[to].bitmap, to);
                }
            } else {
                Paint tmp = paints[from];
                paints[from] = paints[to];
                paints[to] = tmp;
            }

            if (clear != -1) {
                paints[clear].setShader(null);
                if (nextFrame[clear] != null && !nextFrame[clear].isBusy) {
                    nextFrame[clear].clear();
                }
            }
        }
    }

    private void updateContent() {
        needNewFrame = true;
        postInvalidateOnAnimation();
        if (mirrorTarget != null) {
            mirrorTarget.invalidate();
        }
    }

    public void notifyUpdateSize() {
        sizeChanged = true;
        postInvalidateOnAnimation();
    }

    public void setSize(int size) {
        if (this.actionSize != size) {
            invalidate();
        }
        this.actionSize = size;
        this.size = (int) (dp(64) * 1.5f);
    }

    public void setView(ProfileGalleryView view) {
        destroy();
        this.view = view;
        currentPosition = view.getCurrentItem();
        offset = 0;
        view.addOnPageChangeListener(listener);
    }

    public void destroy() {
        frameGeneration++;
        if (view != null) {
            view.removeOnPageChangeListener(listener);
            view = null;
        }
        isBluring = false;
        profileBlurQueue.cancelRunnable(blurTask);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            if (blurNode != null) {
                blurNode.discardDisplayList();
                blurNode = null;
            }
            if (openingNode != null) {
                openingNode.discardDisplayList();
                openingNode = null;
            }
        }
        synchronized (lock) {
            for (int i = 0; i < 3; i++) {
                if (nextFrame[i] != null) {
                    nextFrame[i].recycle();
                    nextFrame[i] = null;
                }
                if (currentFrame[i] != null) {
                    currentFrame[i].recycle();
                    currentFrame[i] = null;
                }
                if (listeners[i] != null) {
                    listeners[i].listenInvalidate(null);
                    listeners[i] = null;
                }
            }
            paints[0].setShader(null);
            paints[1].setShader(null);
        }
    }

    private void doBlur() {
        final int generation = frameGeneration;
        boolean invalidateNeeded = false;
        ProfileMetaballView.BlurBitmapHolder[] holders;
        synchronized (lock) {
            holders = nextFrame.clone();
        }
        for (ProfileMetaballView.BlurBitmapHolder holder : holders) {
            synchronized (lock) {
                if (holder == null || holder.destroying || !holder.hasContent ||
                        generation != frameGeneration || indexOf(holder) < 0) continue;
                holder.lock();
            }
            try {
                Bitmap bitmap = holder.bitmap;
                Utilities.stackBlurBitmap(bitmap, Math.max(10, bitmap.getWidth() / 180) * (hasTallActions() ? 2 : 1));
                synchronized (lock) {
                    int index = indexOf(holder);
                    if (generation != frameGeneration || index < 0 || holder.destroying) continue;
                    ProfileMetaballView.BlurBitmapHolder target = currentFrame[index];
                    if (target == null || !target.canUse(holder)) {
                        if (target != null) target.recycle();
                        currentFrame[index] = target = new ProfileMetaballView.BlurBitmapHolder(holder);
                    }
                    target.clear();
                    target.canvas.drawBitmap(bitmap, 0, 0, null);
                    target.ready();
                    applyShader(target.bitmap, index);
                    invalidateNeeded = true;
                }
            } finally {
                synchronized (lock) {
                    holder.clear();
                    holder.unlock();
                }
            }
        }
        if (generation != frameGeneration) return;
        if (invalidateNeeded && isBluring && view != null) postInvalidateOnAnimation();
        if (isBluring && (loopInvalidate || needNewFrame)) {
            AndroidUtilities.runOnUIThread(() -> {
                if (generation != frameGeneration || !isBluring || view == null) return;
                if (captureNextFrame()) profileBlurQueue.postRunnable(blurTask);
                else isBluring = false;
            });
        } else {
            isBluring = false;
        }
    }

    private int indexOf(ProfileMetaballView.BlurBitmapHolder next) {
        for (int i = 0; i < nextFrame.length; i++) {
            if (nextFrame[i] == next) return i;
        }
        return -1;
    }

    private boolean captureNextFrame() {
        synchronized (lock) {
            return captureNextFrameInternal();
        }
    }

    private boolean captureNextFrameInternal() {
        if (view == null || view.isZooming()) return false;

        int viewportWidth = view.getMeasuredWidth();

        int w = (int) (viewportWidth / 6.0f);
        int h = (int) (size / 6.0f);
        if (w <= 0 || h <= 0) {
            return false;
        }
        frameWidth = w;
        frameHeight = h;

        int checkCount = sizeChanged && !needNewFrame ? 1 : nextFrame.length;

        needNewFrame = sizeChanged = false;
        for (int i = 0; i < checkCount; i++) {
            if (listeners[i] != null) {
                listeners[i].listenInvalidate(null);
            }

            ProfileMetaballView.BlurBitmapHolder nextFrameHolder = nextFrame[i];
            if (nextFrameHolder == null || !nextFrameHolder.canUse(w, h)) {
                if (nextFrameHolder != null) nextFrameHolder.recycle();

                nextFrame[i] = new ProfileMetaballView.BlurBitmapHolder(w, h);
            }

            if (nextFrame[i].isBusy) {
                if (checkCount == 1) {
                    sizeChanged = true;
                } else {
                    needNewFrame = true;
                }
            }
        }

        final View v1 = view.getItemViewAt(currentPosition);
        drawView(v1, 0);

        if (checkCount == 1) {
            if (listeners[0] != null) {
                listeners[0].listenInvalidate(invalidateTask);
            }
            return !sizeChanged;
        }

        final View v2 = view.getItemViewAt(currentPosition + 1);
        drawView(v2, 1);

        if (offset == 0) {
            final View v3 = view.getItemViewAt(currentPosition - 1);
            drawView(v3, 2);
        }

        for (int i = 0; i < listeners.length; i++) {
            if (listeners[i] != null) {
                listeners[i].listenInvalidate(invalidateTask);
            }
        }

        loopInvalidate = (v1 != null && listeners[0] == null) ||
                (offset != 0 && v2 != null && listeners[1] == null);

        return true;
    }

    private void drawView(View view, int index) {
        ProfileMetaballView.BlurBitmapHolder frame = nextFrame[index];
        if (view != null && frame != null && !frame.isBusy && !frame.destroying) {
            Canvas canvas = frame.canvas;
            int checkpoint = canvas.save();
            try {
                canvas.scale(1.0f / 6.0f, 1.0f / 6.0f);
                canvas.translate(0, size - view.getMeasuredHeight());
                view.draw(canvas);
                frame.ready();
            } finally {
                canvas.restoreToCount(checkpoint);
            }
        }
        if (index == 0 || (offset != 0 && index == 1)) {
            if (view instanceof SizeNotifierFrameLayout.IViewWithInvalidateCallback) {
                listeners[index] = (SizeNotifierFrameLayout.IViewWithInvalidateCallback) view;
            } else {
                listeners[index] = null;
            }
        }
    }

    private void applyShader(Bitmap bitmap, int i) {
        if (i < 0 || i >= 2 || size <= 0 || bitmap == null || bitmap.isRecycled()) {
            return;
        }


        LinearGradient alphaGradient = new LinearGradient(
                0, 0, 0, size / 6f,
                new int[]{Color.TRANSPARENT, Color.WHITE},
                new float[]{0f, AndroidUtilities.dpf2(56) / size},
                Shader.TileMode.CLAMP
        );
        BitmapShader bitmapShader = new BitmapShader(
                bitmap,
                Shader.TileMode.MIRROR,
                Shader.TileMode.MIRROR
        );
        ComposeShader composed = new ComposeShader(
                bitmapShader,
                alphaGradient,
                PorterDuff.Mode.DST_IN
        );
        paints[i].setShader(composed);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (view != null) draw(canvas, null, view.getMeasuredWidth(), view.getMeasuredHeight(), false, 0f, 1f);
    }

    public void draw(Canvas canvas, ProfileActivity.AvatarImageView avatarImageView, float width, float height, boolean translate, float fraction, float alpha) {
        if (view == null || width <= 0f || height <= 0f || !Float.isFinite(width) || !Float.isFinite(height) ||
                !Float.isFinite(fraction) || !Float.isFinite(alpha)) return;
        int checkpoint = canvas.save();
        try {
            drawInternal(canvas, avatarImageView, width, height, translate, Math.max(0f, Math.min(1f, fraction)), Math.max(0f, Math.min(1f, alpha)));
        } finally {
            canvas.restoreToCount(checkpoint);
        }
    }

    private void drawInternal(Canvas canvas, ProfileActivity.AvatarImageView avatarImageView, float width, float height, boolean translate, float fraction, float alpha) {
        if (view == null || !view.isAttachedToWindow() || view.getVisibility() == GONE) {
            return;
        }

        if (usingRenderNode && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (canvas.isHardwareAccelerated()) {
                if (avatarImageView == null && getVisibility() == View.VISIBLE && getAlpha() > 0f) {
                    drawRenderNode(canvas, width);
                } else if (avatarImageView != null) {
                    drawOpeningRenderNode(avatarImageView, canvas, width, height, fraction, alpha);
                }
                return;
            } else if (avatarImageView == null && !AndroidUtilities.makingGlobalBlurBitmap) {
                usingRenderNode = false;
                setLayerType(View.LAYER_TYPE_SOFTWARE, paints[0]);
                setLayerType(View.LAYER_TYPE_SOFTWARE, paints[1]);
            } else {
                return;
            }
        }
        boolean shouldCapture = needNewFrame || sizeChanged || loopInvalidate ||
                (paints[0].getShader() == null && paints[1].getShader() == null && !isBluring);
        if (shouldCapture) {
            boolean newFrame = captureNextFrame();

            if (!isBluring && newFrame) {
                isBluring = true;
                profileBlurQueue.cancelRunnable(blurTask);
                profileBlurQueue.postRunnable(blurTask);
            }
        }

        if (paints[0].getShader() == null && paints[1].getShader() == null) {
            return;
        }

        synchronized (lock) {
            if (frameWidth <= 0 || frameHeight <= 0) return;
            float scale = width / frameWidth;
            if (translate) {
                canvas.translate(0f, -scale * frameHeight);
            }
            canvas.scale(scale, scale);
            float hAction = actionSize / scale;

            if (paints[0].getShader() != null) {
                canvas.save();
                canvas.translate(-offset / scale, 0f);

                canvas.save();
                canvas.scale(1f, 2f, 0, frameHeight);
                canvas.drawRect(0, frameHeight, frameWidth, frameHeight + hAction, paints[0]);
                canvas.restore();

                paints[0].setAlpha((int) (0xFF * alpha));
                canvas.drawRect(0, frameHeight * fraction, frameWidth, frameHeight, paints[0]);
                paints[0].setAlpha(0xFF);
                canvas.restore();
            }

            if (offset != 0 && paints[1].getShader() != null) {
                canvas.save();
                canvas.translate((-offset + width) / scale, 0f);

                canvas.save();
                canvas.scale(1f, 2f, 0, frameHeight);
                canvas.drawRect(0, frameHeight, frameWidth, frameHeight + hAction, paints[1]);
                canvas.restore();

                paints[1].setAlpha((int) (0xFF * alpha));
                canvas.drawRect(0, frameHeight * fraction, frameWidth, frameHeight, paints[1]);
                paints[1].setAlpha(0xFF);
                canvas.restore();
            }
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.S)
    private void initRenderNode() {
        if (blurNode == null) {
            blurNode = new RenderNode("profileBlurNode");
            renderNodeBlurRadius = 0f;
        }
        final float r = getBlurRadius() * (hasTallActions() ? 3f : 1f);
        if (renderNodeBlurRadius != r || renderNodeActionSize != actionSize) {
            renderNodeBlurRadius = r;
            renderNodeActionSize = actionSize;
            final float scale = getRenderNodeScale();
            final int lead = getMirrorLead();
            final float total = Math.max(1, lead + actionSize);
            final float in = lead / total;
            final float out = Math.min(1f, (lead + dp(72)) / total);
            LinearGradient renderNodeAlphaGradient = new LinearGradient(
                    0, 0, 0, total / scale,
                    new int[]{Color.TRANSPARENT, 0x0FFFFFFF, 0x52FFFFFF, 0xB3FFFFFF, Color.WHITE, 0x80FFFFFF, Color.TRANSPARENT},
                    new float[]{0f, in * .3f, in * .6f, in * .85f, in, (in + out) / 2f, out},
                    Shader.TileMode.CLAMP
            );

            blurNode.setRenderEffect(RenderEffect.createBlendModeEffect(
                    RenderEffect.createBlurEffect(r, r, Shader.TileMode.CLAMP),
                    RenderEffect.createShaderEffect(renderNodeAlphaGradient),
                    BlendMode.DST_IN
            ));
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.S)
    private void initOpeningNode() {
        if (openingNode == null) {
            openingNode = new RenderNode("profileOpeningBlurNode");
            openingNodeBlurRadius = 0f;
        }
        final float r = getBlurRadius() * (hasTallActions() ? 3f : 1f);
        if (openingNodeBlurRadius != r || openingNodeActionSize != actionSize) {
            openingNodeBlurRadius = r;
            openingNodeActionSize = actionSize;
            final float scale = getRenderNodeScale();
            final float total = Math.max(1, size + actionSize);
            final float fadeIn = Math.min(.45f, AndroidUtilities.dpf2(56) / total);
            final float fadeOut = Math.max(fadeIn, 1f - actionSize * .75f / total);
            final float fadeLength = 1f - fadeOut;
            final LinearGradient gradient = new LinearGradient(
                    0, 0, 0, total / scale,
                    new int[]{Color.TRANSPARENT, Color.WHITE, Color.WHITE, 0xD7FFFFFF, 0x80FFFFFF, 0x28FFFFFF, Color.TRANSPARENT},
                    new float[]{0f, fadeIn, fadeOut, fadeOut + fadeLength * .25f, fadeOut + fadeLength * .5f, fadeOut + fadeLength * .75f, 1f},
                    Shader.TileMode.CLAMP
            );
            openingNode.setRenderEffect(RenderEffect.createBlendModeEffect(
                    RenderEffect.createBlurEffect(r, r, Shader.TileMode.CLAMP),
                    RenderEffect.createShaderEffect(gradient),
                    BlendMode.DST_IN
            ));
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.S)
    public void drawRenderNode(Canvas canvas, float width) {
        initRenderNode();

        if (listeners[0] != null) listeners[0].listenInvalidate(null);
        if (listeners[1] != null) listeners[1].listenInvalidate(null);

        float scale = getRenderNodeScale();
        final int lead = getMirrorLead();
        blurNode.setPosition(0, 0, (int) Math.ceil(width / scale), (int) Math.ceil((lead + actionSize) / scale));
        RecordingCanvas recordingCanvas = blurNode.beginRecording();
        try {
        recordingCanvas.scale(1f / scale, 1f / scale);

        recordingCanvas.save();
        recordingCanvas.translate(-offset, 0);
        drawViewWithRenderNode(recordingCanvas, 0);
        recordingCanvas.restore();

        if (offset != 0) {
            recordingCanvas.save();
            recordingCanvas.translate(-offset + width, 0f);
            drawViewWithRenderNode(recordingCanvas, 1);
            recordingCanvas.restore();
        }

        } finally {
            blurNode.endRecording();
        }
        blurNode.setAlpha(alpha.set(1f));

        canvas.save();
        canvas.clipRect(0, 0, width, lead + actionSize);
        canvas.scale(scale, scale);
        canvas.drawRenderNode(blurNode);
        canvas.restore();
    }

    private void drawViewWithRenderNode(Canvas recordingCanvas, int index) {
        View drawingView = view.getItemViewAt(currentPosition + index);

        if (drawingView != null) {
            final int w = drawingView.getMeasuredWidth();
            final float photoBottom = drawingView.getMeasuredHeight() - actionSize;
            final float seam = getMirrorLead();
            final float total = seam + actionSize;
            recordingCanvas.save();
            recordingCanvas.clipRect(0, 0, w, seam);
            recordingCanvas.translate(0, seam - photoBottom);
            drawingView.draw(recordingCanvas);
            recordingCanvas.restore();

            recordingCanvas.save();
            recordingCanvas.clipRect(0, seam, w, total);
            recordingCanvas.translate(0, seam);
            recordingCanvas.scale(1f, -MIRROR_STRETCH);
            recordingCanvas.translate(0, -photoBottom);
            drawingView.draw(recordingCanvas);
            recordingCanvas.restore();
        }

        if (drawingView instanceof SizeNotifierFrameLayout.IViewWithInvalidateCallback) {
            listeners[index] = (SizeNotifierFrameLayout.IViewWithInvalidateCallback) drawingView;
            listeners[index].listenInvalidate(invalidateTask);
        } else {
            listeners[index] = null;
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.S)
    public void drawOpeningRenderNode(ProfileActivity.AvatarImageView avatarImageView, Canvas canvas, float width, float height, float fraction, float alpha) {
        if (view == null || avatarImageView == null || view.getMeasuredWidth() <= 0 || width <= 0f || height <= 0f ||
                !Float.isFinite(width) || !Float.isFinite(height) || !Float.isFinite(fraction) || !Float.isFinite(alpha)) return;
        int checkpoint = canvas.save();
        try {
            drawOpeningRenderNodeInternal(avatarImageView, canvas, width, height,
                    Math.max(0f, Math.min(1f, fraction)), Math.max(0f, Math.min(1f, alpha)));
        } finally {
            canvas.restoreToCount(checkpoint);
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.S)
    private void drawOpeningRenderNodeInternal(ProfileActivity.AvatarImageView avatarImageView, Canvas canvas, float width, float height, float fraction, float alpha) {
        fraction = 1f - fraction;
        float openingScale = width / view.getMeasuredWidth();
        float croppedSize = size * fraction;
        float scaledSize = size * fraction * openingScale;
        float scale = getRenderNodeScale() * openingScale;
        initOpeningNode();

        openingNode.setPosition(0, 0, (int) Math.ceil(width / scale), (int) Math.ceil((croppedSize + actionSize) / scale));
        final RecordingCanvas recordingCanvas = openingNode.beginRecording();
        try {
        recordingCanvas.scale(1f / scale, 1f / scale);

        final ImageReceiver imageReceiver = avatarImageView.animatedEmojiDrawable != null
                ? avatarImageView.animatedEmojiDrawable.getImageReceiver()
                : avatarImageView.imageReceiver;

        drawOpeningImageRenderNode(imageReceiver, recordingCanvas, scaledSize, height);
        if (avatarImageView.drawForeground && avatarImageView.foregroundAlpha > 0f) {
            drawOpeningImageRenderNode(avatarImageView.foregroundImageReceiver, recordingCanvas, scaledSize, height);
        }

        } finally {
            openingNode.endRecording();
        }
        openingNode.setAlpha(alpha);

        canvas.translate(0f, -scaledSize);
        canvas.scale(scale, scale);
        canvas.drawRenderNode(openingNode);
    }

    private void drawOpeningImageRenderNode(
            ImageReceiver imageReceiver,
            Canvas recordingCanvas,
            float scaledSize,
            float height
    ) {
        if (imageReceiver == null) {
            return;
        }
        int oldR = imageReceiver.getRoundRadius()[0];
        imageReceiver.setRoundRadius(0);

        recordingCanvas.save();
        recordingCanvas.translate(0, scaledSize - height);
        imageReceiver.draw(recordingCanvas);
        recordingCanvas.restore();

        recordingCanvas.save();
        recordingCanvas.clipRect(0, scaledSize, imageReceiver.getImageX2() + 1, scaledSize + actionSize + height);
        recordingCanvas.translate(0, scaledSize);
        recordingCanvas.scale(1f, -MIRROR_STRETCH);
        recordingCanvas.translate(0, -height);
        imageReceiver.draw(recordingCanvas);
        recordingCanvas.restore();

        final float shadeTop = scaledSize * .2f;
        final float shadeBottom = Math.max(scaledSize + 1, scaledSize + actionSize);
        openingShadePaint.setShader(new LinearGradient(0, shadeTop, 0, shadeBottom,
                new int[]{0x00000000, 0x6B000000, 0x4D000000},
                new float[]{0f, (scaledSize - shadeTop) / (shadeBottom - shadeTop), 1f},
                Shader.TileMode.CLAMP));
        recordingCanvas.drawRect(0, shadeTop, imageReceiver.getImageX2() + 1, shadeBottom, openingShadePaint);

        imageReceiver.setRoundRadius(oldR);
    }

    @Override
    public void setAlpha(float alpha) {
        super.setAlpha(alpha);
        if (alpha != 0f && usingRenderNode) {
            invalidate();
        }
    }

    public static int getMirrorLead() {
        return dp(220);
    }

    private static LinearGradient seamShadeShader;

    public static Shader getSeamShade(float seamY) {
        if (seamShadeShader == null) {
            final int lead = getMirrorLead();
            final float a = -lead;
            final float b = dp(200);
            seamShadeShader = new LinearGradient(0, a, 0, b,
                    new int[]{0x00000000, 0x14000000, 0x40000000, 0x6B000000, 0x5C000000, 0x4D000000},
                    new float[]{0f, (lead * .45f) / (b - a), (lead * .8f) / (b - a), lead / (b - a), (lead + dp(80)) / (b - a), 1f},
                    Shader.TileMode.CLAMP);
        }
        final android.graphics.Matrix m = new android.graphics.Matrix();
        m.setTranslate(0, seamY);
        seamShadeShader.setLocalMatrix(m);
        return seamShadeShader;
    }

    private static final class MirrorEntry {
        Object key;
        float height;
        Bitmap bitmap;
        BitmapShader shader;
    }

    private final MirrorEntry[] mirrorCache = new MirrorEntry[3];
    private int mirrorCacheNext;
    private View mirrorTarget;
    private final Paint mirrorPaint = new Paint(Paint.FILTER_BITMAP_FLAG);
    private final Paint mirrorShadePaint = new Paint();
    private final android.graphics.Matrix mirrorMatrix = new android.graphics.Matrix();

    public void setMirrorTarget(View target) {
        mirrorTarget = target;
    }

    private MirrorEntry getMirror(View item) {
        if (!(item instanceof BackupImageView)) {
            return null;
        }
        final ImageReceiver receiver = ((BackupImageView) item).getImageReceiver();
        final Bitmap source = receiver.getBitmap();
        final float photoBottom = item.getMeasuredHeight() - actionSize;
        final Object key = receiver.getImageKey() != null ? receiver.getImageKey() : source;
        if (key == null) {
            return null;
        }
        for (MirrorEntry entry : mirrorCache) {
            if (entry != null && entry.height == photoBottom && key.equals(entry.key)) {
                return entry;
            }
        }
        if (source == null || source.isRecycled()) {
            return null;
        }
        final float dstW = item.getMeasuredWidth();
        if (dstW <= 0 || photoBottom <= 0) {
            return null;
        }
        try {
            Bitmap readable = source;
            if (Build.VERSION.SDK_INT >= 26 && source.getConfig() == Bitmap.Config.HARDWARE) {
                readable = source.copy(Bitmap.Config.ARGB_8888, false);
                if (readable == null) {
                    return null;
                }
            }
            final float imageX = receiver.getImageX(), imageY = receiver.getImageY();
            final float imageW = receiver.getImageWidth() > 0 ? receiver.getImageWidth() : dstW;
            final float imageH = receiver.getImageHeight() > 0 ? receiver.getImageHeight() : photoBottom;
            final float bw = readable.getWidth(), bh = readable.getHeight();
            final float k = Math.max(imageW / bw, imageH / bh);
            final float srcX = (bw - imageW / k) / 2f + (0 - imageX) / k;
            final float srcY = (bh - imageH / k) / 2f + (0 - imageY) / k;
            final android.graphics.Rect src = new android.graphics.Rect(
                    (int) Math.max(0, srcX), (int) Math.max(0, srcY),
                    (int) Math.min(bw, srcX + dstW / k), (int) Math.min(bh, srcY + photoBottom / k));
            if (src.width() <= 0 || src.height() <= 0) {
                return null;
            }
            final int outW = 36;
            final int outH = Math.max(8, Math.round(outW * photoBottom / dstW));
            final Bitmap out = Bitmap.createBitmap(outW, outH, Bitmap.Config.ARGB_8888);
            final Canvas c = new Canvas(out);
            c.scale(1f, -1f, 0, outH / 2f);
            c.drawBitmap(readable, src, new android.graphics.Rect(0, 0, outW, outH), new Paint(Paint.FILTER_BITMAP_FLAG));
            if (readable != source) {
                readable.recycle();
            }
            Utilities.stackBlurBitmap(out, 5);
            Utilities.stackBlurBitmap(out, 5);
            final MirrorEntry entry = new MirrorEntry();
            entry.key = key;
            entry.height = photoBottom;
            entry.bitmap = out;
            entry.shader = new BitmapShader(out, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP);
            mirrorCache[mirrorCacheNext] = entry;
            mirrorCacheNext = (mirrorCacheNext + 1) % mirrorCache.length;
            return entry;
        } catch (Throwable e) {
            return null;
        }
    }

    private MirrorEntry lastMirror;

    public boolean drawMirrorBackground(Canvas canvas, View target, float progress) {
        if (view == null || progress <= 0f || !isAttachedToWindow() || getVisibility() != VISIBLE || getAlpha() <= 0f
                || view.getVisibility() != VISIBLE || !view.isAttachedToWindow() || getWidth() <= 0) {
            return false;
        }
        MirrorEntry first = getMirror(view.getItemViewAt(currentPosition));
        if (first == null) {
            first = lastMirror;
        } else {
            lastMirror = first;
        }
        if (first == null) {
            return false;
        }
        MirrorEntry second = null;
        if (offset != 0) {
            second = getMirror(view.getItemViewAt(currentPosition + 1));
            if (second == null) {
                second = first;
            }
        }
        continuationMatrix.reset();
        transformMatrixToGlobal(continuationMatrix);
        target.transformMatrixToLocal(continuationMatrix);
        continuationPoints[0] = 0;
        continuationPoints[1] = getHeight() - actionSize;
        continuationPoints[2] = getWidth();
        continuationPoints[3] = getHeight();
        continuationMatrix.mapPoints(continuationPoints);
        final float left = continuationPoints[0];
        final float right = continuationPoints[2];
        final float seamY = continuationPoints[1];
        final float headerBottom = continuationPoints[3];
        if (right <= left) {
            return false;
        }
        final float scaleX = (right - left) / getWidth();
        final int a = (int) (0xFF * Math.min(1f, progress) * getAlpha());
        final float shift = offset * scaleX;
        drawMirrorPage(canvas, target, first, left - shift, right - left, seamY, scaleX, a);
        if (second != null) {
            drawMirrorPage(canvas, target, second, right - shift, right - left, seamY, scaleX, a);
        }
        mirrorShadePaint.setShader(getSeamShade(seamY));
        mirrorShadePaint.setAlpha(a);
        canvas.drawRect(0, Math.max(seamY, headerBottom), target.getWidth(), target.getHeight(), mirrorShadePaint);
        return true;
    }

    private void drawMirrorPage(Canvas canvas, View target, MirrorEntry entry, float x, float width, float seamY, float scaleX, int alpha) {
        final float l = Math.max(0, x);
        final float r = Math.min(target.getWidth(), x + width);
        if (r <= l) {
            return;
        }
        final float tile = entry.height * scaleX * MIRROR_STRETCH;
        mirrorMatrix.setScale(width / entry.bitmap.getWidth(), tile / entry.bitmap.getHeight());
        mirrorMatrix.postTranslate(x, seamY);
        entry.shader.setLocalMatrix(mirrorMatrix);
        mirrorPaint.setShader(entry.shader);
        mirrorPaint.setAlpha(alpha);
        canvas.drawRect(l, seamY, r, target.getHeight(), mirrorPaint);
    }

    private float getRenderNodeScale() {
        return dp(1);
    }

    private boolean hasTallActions() {
        return actionSize > dp(110);
    }

    private float getBlurRadius() {
        switch (SharedConfig.getDevicePerformanceClass()) {
            case SharedConfig.PERFORMANCE_CLASS_HIGH:
                return 20;
            case SharedConfig.PERFORMANCE_CLASS_AVERAGE:
                return 12;
            default:
            case SharedConfig.PERFORMANCE_CLASS_LOW:
                return 8;
        }
    }
}

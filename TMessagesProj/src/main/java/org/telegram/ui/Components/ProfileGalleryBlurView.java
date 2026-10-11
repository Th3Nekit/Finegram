package org.telegram.ui.Components;

import static org.telegram.messenger.AndroidUtilities.dp;
import static org.telegram.ui.Components.ProfileMetaballView.profileBlurQueue;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapShader;
import android.graphics.BlendMode;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
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

    private ProfileActionsView actionsView;
    private ProfileSuggestionView suggestionView;
    private ProfileMusicView musicView;
    private boolean shouldBlurActions;
    private RenderNode blurNode, actionsBlurNode;

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
        setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), size + actionSize);
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

    public void setActionsView(ProfileActionsView actionsView) {
        this.actionsView = actionsView;
    }

    public void setSuggestionView(ProfileSuggestionView suggestionView) {
        this.suggestionView = suggestionView;
    }

    public void setMusicView(ProfileMusicView musicView) {
        this.musicView = musicView;
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
            if (actionsBlurNode != null) {
                actionsBlurNode.discardDisplayList();
                actionsBlurNode = null;
            }
        }
        actionsView = null;
        suggestionView = null;
        musicView = null;
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
                Utilities.stackBlurBitmap(bitmap, Math.max(10, bitmap.getWidth() / 180));
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
        if (actionsView != null) {
            actionsView.drawingBlur(false);
        }
        if (suggestionView != null) {
            suggestionView.drawingBlur(false);
        }
        if (musicView != null) {
            musicView.drawingBlur(false);
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
            float scale = getRenderNodeScale();
            blurNode = new RenderNode("profileBlurNode");
            LinearGradient renderNodeAlphaGradient = new LinearGradient(
                    0, 0, 0, size / scale,
                    new int[]{Color.TRANSPARENT, Color.WHITE},
                    new float[]{0f, AndroidUtilities.dpf2(56) / size},
                    Shader.TileMode.CLAMP
            );

            float r = getBlurRadius();
            blurNode.setRenderEffect(RenderEffect.createBlendModeEffect(
                    RenderEffect.createBlurEffect(r, r, Shader.TileMode.CLAMP),
                    RenderEffect.createShaderEffect(renderNodeAlphaGradient),
                    BlendMode.DST_IN
            ));
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.S)
    private void initActionsRenderNode() {
        if (actionsView == null && suggestionView == null && musicView == null) {
            shouldBlurActions = false;
            return;
        }

        if (actionsBlurNode == null) {
            actionsBlurNode = new RenderNode("profileActionsBlurNode");

            ColorMatrix colorMatrix = new ColorMatrix();
            AndroidUtilities.adjustSaturationColorMatrix(colorMatrix, .65f);
            AndroidUtilities.multiplyBrightnessColorMatrix(colorMatrix, .5f);

            actionsBlurNode.setRenderEffect(

                RenderEffect.createColorFilterEffect(new ColorMatrixColorFilter(colorMatrix))
                 );
        }
        shouldBlurActions = true;
    }

    @RequiresApi(api = Build.VERSION_CODES.S)
    public void drawRenderNode(Canvas canvas, float width) {
        initRenderNode();

        if (listeners[0] != null) listeners[0].listenInvalidate(null);
        if (listeners[1] != null) listeners[1].listenInvalidate(null);

        float scale = getRenderNodeScale();
        blurNode.setPosition(0, 0, (int) (width / scale), (int) ((size + actionSize) / scale));
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
        canvas.scale(scale, scale);
        canvas.drawRenderNode(blurNode);
        canvas.restore();

        if (getVisibility() == View.VISIBLE && getAlpha() > 0f) {
            captureActionsBlurRenderNode(width, null, 1f, size);
        }
    }

    private void drawViewWithRenderNode(Canvas recordingCanvas, int index) {
        View drawingView = view.getItemViewAt(currentPosition + index);

        if (drawingView != null) {
            int h = drawingView.getMeasuredHeight();
            recordingCanvas.save();
            recordingCanvas.translate(0, size - h);
            drawingView.draw(recordingCanvas);
            recordingCanvas.restore();

            recordingCanvas.save();
            recordingCanvas.scale(1f, -1f);
            recordingCanvas.translate(0f, -h - size);
            recordingCanvas.scale(1f, 2f, 0, h);
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
        initRenderNode();

        blurNode.setPosition(0, 0, (int) (width / scale), (int) ((croppedSize + actionSize) / scale));
        final RecordingCanvas recordingCanvas = blurNode.beginRecording();
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
            blurNode.endRecording();
        }
        blurNode.setAlpha(alpha);

        canvas.translate(0f, -scaledSize);
        canvas.scale(scale, scale);
        canvas.drawRenderNode(blurNode);

        captureActionsBlurRenderNode(width, avatarImageView, openingScale, croppedSize);
    }

    @RequiresApi(api = Build.VERSION_CODES.S)
    private void captureActionsBlurRenderNode(float width, ProfileActivity.AvatarImageView avatarImageView, float openingScale, float size) {
        initActionsRenderNode();

        if (!shouldBlurActions) {
            if (actionsView != null) {
                actionsView.drawingBlur(false);
            }
            if (suggestionView != null) {
                suggestionView.drawingBlur(false);
            }
            if (musicView != null) {
                musicView.drawingBlur(false);
            }
            return;
        }

        float outScale = 8f;
        float scale = getRenderNodeScale() * openingScale * outScale;
        actionsBlurNode.setPosition(0, 0, (int) Math.ceil(width / scale), (int) ((size + actionSize) / scale));
        RecordingCanvas recordingCanvas = actionsBlurNode.beginRecording();
        recordingCanvas.scale(1f / outScale, 1f / outScale);
        recordingCanvas.drawRenderNode(blurNode);
        actionsBlurNode.endRecording();
        actionsBlurNode.setAlpha(alpha.set(1f));

        if (actionsView != null) {
            if (avatarImageView != null) {
                actionsView.drawingBlur(actionsBlurNode, avatarImageView, scale / openingScale, -size);
            } else {
                actionsView.drawingBlur(actionsBlurNode, null, scale, -size);
            }
        }
        if (suggestionView != null) {
            if (avatarImageView != null) {
                suggestionView.drawingBlur(actionsBlurNode, avatarImageView, scale / openingScale, -size);
            } else {
                suggestionView.drawingBlur(actionsBlurNode, null, scale, -size);
            }
        }
        if (musicView != null) {
            if (avatarImageView != null) {
                musicView.drawingBlur(actionsBlurNode, avatarImageView, scale / openingScale, -size + dp(22));
            } else {
                musicView.drawingBlur(actionsBlurNode, null, scale, -size + dp(22));
            }
        }
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
        recordingCanvas.scale(1f, -1f);
        recordingCanvas.translate(0f, -height - scaledSize);
        recordingCanvas.scale(1f, 2f, 0, height);
        imageReceiver.draw(recordingCanvas);
        recordingCanvas.restore();

        imageReceiver.setRoundRadius(oldR);
    }

    @Override
    public void setAlpha(float alpha) {
        super.setAlpha(alpha);
        if (alpha != 0f && usingRenderNode) {
            invalidate();
        }
    }

    private float getRenderNodeScale() {
        return dp(1);
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
package org.telegram.ui.Components;

import static org.telegram.messenger.AndroidUtilities.dp;
import static org.telegram.messenger.LocaleController.getString;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.RenderNode;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.PowerManager;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.graphics.ColorUtils;

import com.th3nekit.finegram.core.ui.MD3ListAdapter;
import com.th3nekit.finegram.core.ui.ProfileMusicLayout;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.FileLoader;
import org.telegram.messenger.FileLog;
import org.telegram.messenger.ImageLocation;
import org.telegram.messenger.ImageReceiver;
import org.telegram.messenger.LiteMode;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.MediaController;
import org.telegram.messenger.MessageObject;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.R;
import org.telegram.messenger.SharedConfig;
import org.telegram.messenger.UserConfig;
import org.telegram.messenger.Utilities;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.ProfileActivity;

import java.lang.ref.WeakReference;

public class ProfileMusicView extends FrameLayout implements NotificationCenter.NotificationCenterDelegate {
    private final Theme.ResourcesProvider resourcesProvider;
    private final int currentAccount;
    private final PowerManager powerManager;
    private final TextView titleView, authorView;
    private final ImageView playButton;
    private final PlayPauseDrawable playPause = new PlayPauseDrawable(22);
    private final ImageReceiver cover = new ImageReceiver(this);
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint edgePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint sheenPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint artworkPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final Paint washPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint buttonRimPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF rect = new RectF();
    private final RectF trackRect = new RectF();
    private final RectF artworkRect = new RectF();
    private final Rect visibleRect = new Rect();
    private final Path clipPath = new Path();
    private final Runnable animationFrame = () -> {
        framePosted = false;
        if (canAnimateMotion()) invalidate();
    };
    private ProfileMusicLayout layout;
    private TLRPC.Document document;
    private int surfaceColor, textColor, artworkColor, progressColor;
    private Bitmap artworkBlur;
    private WeakReference<Bitmap> artworkSource;
    private int artworkGeneration, artworkOrientation;
    private Drawable placeholder;
    private float currentHeight, parentExpanded, progress;
    private boolean attached, playing, selectedTrack, touchStarted, framePosted;
    private boolean ignoreBlur;

    public ProfileMusicView(Context context, Theme.ResourcesProvider resourcesProvider) {
        this(context, UserConfig.selectedAccount, resourcesProvider);
    }

    public ProfileMusicView(Context context, int currentAccount, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.currentAccount = currentAccount;
        this.resourcesProvider = resourcesProvider;
        powerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        setWillNotDraw(false);
        setClickable(true);
        setFocusable(true);
        setClipChildren(true);
        titleView = createText(context, 17, true);
        authorView = createText(context, 12, false);
        addView(titleView);
        addView(authorView);
        playButton = new ImageView(context);
        playButton.setScaleType(ImageView.ScaleType.CENTER);
        playButton.setImageDrawable(playPause);
        playPause.setParent(playButton);
        playButton.setFocusable(true);
        playButton.setOnFocusChangeListener((view, focused) -> invalidate());
        addView(playButton);
        cover.setCurrentAccount(currentAccount);
        cover.setRoundRadius(dp(12));
        cover.setCrossfadeDuration(0);
        cover.setAllowStartAnimation(false);
        cover.setAllowStartLottieAnimation(false);
        cover.setDelegate((receiver, set, thumb, memCache) -> updateArtwork());
        edgePaint.setStyle(Paint.Style.STROKE);
        edgePaint.setStrokeWidth(AndroidUtilities.dpf2(1));
        buttonRimPaint.setStyle(Paint.Style.STROKE);
        buttonRimPaint.setStrokeWidth(AndroidUtilities.dpf2(1));
        setColor(null);
        setText(getString(R.string.AudioUnknownArtist), getString(R.string.AudioUnknownTitle));
    }

    private TextView createText(Context context, int size, boolean bold) {
        TextView view = new TextView(context);
        view.setTextSize(TypedValue.COMPLEX_UNIT_SP, size);
        if (bold) view.setTypeface(AndroidUtilities.bold());
        view.setSingleLine(true);
        view.setEllipsize(TextUtils.TruncateAt.END);
        view.setIncludeFontPadding(false);
        view.setTextDirection(View.TEXT_DIRECTION_LOCALE);
        view.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        return view;
    }

    public static int getExtraHeight(Context context) {
        return dp(ProfileMusicLayout.height(context.getResources().getConfiguration().fontScale));
    }

    public void setOnPlayClickListener(OnClickListener listener) {
        playButton.setOnClickListener(listener);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = MeasureSpec.getSize(widthMeasureSpec);
        layout = new ProfileMusicLayout(width / AndroidUtilities.density,
                getResources().getConfiguration().fontScale, LocaleController.isRTL);
        setMeasuredDimension(width, dp(layout.height));
        titleView.measure(MeasureSpec.makeMeasureSpec(dp(layout.textWidth), MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(dp(layout.titleHeight), MeasureSpec.AT_MOST));
        authorView.measure(MeasureSpec.makeMeasureSpec(dp(layout.textWidth), MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(dp(layout.authorHeight), MeasureSpec.EXACTLY));
        playButton.measure(MeasureSpec.makeMeasureSpec(dp(48), MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(dp(48), MeasureSpec.EXACTLY));
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        titleView.setGravity(Gravity.CENTER_VERTICAL | (LocaleController.isRTL ? Gravity.RIGHT : Gravity.LEFT));
        authorView.setGravity(titleView.getGravity());
        float titleHeight = titleView.getMeasuredHeight() / AndroidUtilities.density;
        float authorHeight = authorView.getMeasuredHeight() / AndroidUtilities.density;
        float titleTop = layout.titleTop + (layout.titleHeight + layout.authorHeight - titleHeight - authorHeight) / 2f;
        place(titleView, layout.textLeft, titleTop);
        place(authorView, layout.textLeft, titleTop + titleHeight + 3f);
        place(playButton, layout.buttonLeft, layout.buttonTop);
        rect.set(dp(16), dp(8), getWidth() - dp(16), getHeight() - dp(8));
        cover.setImageCoords(dp(layout.coverLeft), dp(layout.coverTop), dp(layout.coverSize), dp(layout.coverSize));
        updateShaders();
    }

    private void place(View view, float x, float y) {
        int left = dp(x), top = dp(y);
        view.layout(left, top, left + view.getMeasuredWidth(), top + view.getMeasuredHeight());
    }

    public void setColor(MessagesController.PeerColor peerColor) {
        surfaceColor = MD3ListAdapter.getBackgroundColor(resourcesProvider);
        if (artworkBlur != null) surfaceColor = ColorUtils.blendARGB(surfaceColor, artworkColor,
                Theme.isCurrentThemeDark() ? .14f : .05f);
        int backdrop = ColorUtils.compositeColors(
                Theme.getColor(Theme.key_windowBackgroundGray, resourcesProvider),
                Theme.isCurrentThemeDark() ? Color.BLACK : Color.WHITE);
        int contrastBackground = ColorUtils.compositeColors(surfaceColor, backdrop);
        textColor = Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider);
        if (ColorUtils.calculateContrast(textColor, contrastBackground) < 4.5) {
            textColor = ColorUtils.calculateContrast(Color.BLACK, contrastBackground)
                    >= ColorUtils.calculateContrast(Color.WHITE, contrastBackground) ? Color.BLACK : Color.WHITE;
        }
        progressColor = artworkBlur == null ? textColor : ColorUtils.blendARGB(artworkColor, textColor, .55f);
        for (int step = 0; step < 10 && ColorUtils.calculateContrast(progressColor, contrastBackground) < 3; step++) {
            progressColor = ColorUtils.blendARGB(progressColor, textColor, .2f);
        }
        if (ColorUtils.calculateContrast(progressColor, contrastBackground) < 3) progressColor = textColor;
        titleView.setTextColor(textColor);
        authorView.setTextColor(ColorUtils.blendARGB(surfaceColor, textColor, .78f));
        playPause.setColor(textColor);
        playButton.setBackground(Theme.createSimpleSelectorRoundRectDrawable(dp(24), Color.TRANSPARENT,
                Theme.multAlpha(textColor, .1f)));
        if (placeholder != null) placeholder.setColorFilter(textColor, android.graphics.PorterDuff.Mode.SRC_IN);
        updateShaders();
        invalidate();
    }

    private void updateShaders() {
        if (rect.isEmpty()) return;
        boolean dark = Theme.isCurrentThemeDark();
        float start = LocaleController.isRTL ? rect.right : rect.left;
        float end = LocaleController.isRTL ? rect.left : rect.right;
        washPaint.setShader(new LinearGradient(start, 0, end, 0,
                new int[]{surfaceColor, Theme.multAlpha(surfaceColor, .76f), Theme.multAlpha(surfaceColor, .12f)},
                new float[]{0f, .36f, 1f}, Shader.TileMode.CLAMP));
        sheenPaint.setShader(new LinearGradient(rect.left, rect.top, rect.right, rect.bottom,
                new int[]{Theme.multAlpha(Color.WHITE, dark ? .06f : .32f), Color.TRANSPARENT},
                null, Shader.TileMode.CLAMP));
        edgePaint.setShader(new LinearGradient(0, rect.top, 0, rect.bottom,
                new int[]{Theme.multAlpha(Color.WHITE, dark ? .32f : .9f), Theme.multAlpha(textColor, .09f)},
                null, Shader.TileMode.CLAMP));
        buttonRimPaint.setShader(new LinearGradient(0, dp(layout.buttonTop), 0, dp(layout.buttonTop + 48f),
                new int[]{Theme.multAlpha(Color.WHITE, dark ? .4f : .95f), Theme.multAlpha(textColor, .14f)},
                null, Shader.TileMode.CLAMP));
    }

    public void setParentExpanded(float expanded) {
        if (parentExpanded != expanded) {
            parentExpanded = expanded;
            invalidate();
        }
    }

    public void setMusicDocument(TLRPC.Document document) {
        boolean same = this.document != null && document != null && this.document.id == document.id;
        this.document = document;
        setText(getAuthor(document), getTitle(document));
        if (!same) {
            clearArtwork();
            placeholder = getContext().getResources().getDrawable(R.drawable.music_empty).mutate();
            setColor(null);
            TLRPC.PhotoSize thumb = document == null ? null : FileLoader.getClosestPhotoSizeWithSize(document.thumbs, 160);
            String artwork = MessageObject.getArtworkUrl(document, true);
            cover.setImage(TextUtils.isEmpty(artwork) ? null : ImageLocation.getForPath(artwork), "52_52",
                    thumb == null ? null : ImageLocation.getForDocument(thumb, document), "52_52",
                    placeholder, 0, null, document, 1);
        }
        updatePlayback();
    }

    private void clearArtwork() {
        if (artworkBlur != null) artworkBlur.recycle();
        artworkBlur = null;
        artworkSource = null;
        artworkColor = 0;
    }

    private void updateArtwork() {
        if (!attached) return;
        if (document == null || !cover.hasImageLoaded() && cover.getThumb() == null) {
            if (artworkBlur != null) {
                clearArtwork();
                setColor(null);
            }
            return;
        }
        if (cover.getAnimation() != null || cover.getLottieAnimation() != null) return;
        ImageReceiver.BitmapHolder holder = cover.getBitmapSafe();
        if (holder == null) return;
        Bitmap small = null, readable = null;
        try {
            Bitmap source = holder.bitmap;
            if (source == null || source.isRecycled()) return;
            int generation = source.getGenerationId();
            if (artworkSource != null && artworkSource.get() == source
                    && artworkGeneration == generation && artworkOrientation == holder.orientation) return;
            Bitmap input = source;
            if (Build.VERSION.SDK_INT >= 26 && source.getConfig() == Bitmap.Config.HARDWARE) {
                readable = source.copy(Bitmap.Config.ARGB_8888, false);
                if (readable == null) return;
                input = readable;
            }
            small = Bitmap.createBitmap(48, 48, Bitmap.Config.ARGB_8888);
            Canvas light = new Canvas(small);
            float scale = Math.max(48f / input.getWidth(), 48f / input.getHeight());
            float width = input.getWidth() * scale, height = input.getHeight() * scale;
            light.rotate(holder.orientation, 24f, 24f);
            artworkPaint.setAlpha(255);
            light.drawBitmap(input, null, new RectF((48f - width) / 2f, (48f - height) / 2f,
                    (48f + width) / 2f, (48f + height) / 2f), artworkPaint);
            int[] pixels = new int[48 * 48];
            small.getPixels(pixels, 0, 48, 0, 0, 48, 48);
            long red = 0, green = 0, blue = 0, weight = 0;
            for (int color : pixels) {
                int alpha = Color.alpha(color);
                red += Color.red(color) * alpha;
                green += Color.green(color) * alpha;
                blue += Color.blue(color) * alpha;
                weight += alpha;
            }
            if (weight == 0) {
                clearArtwork();
                setColor(null);
                return;
            }
            int color = Color.rgb((int) (red / weight), (int) (green / weight), (int) (blue / weight));
            Utilities.stackBlurBitmap(small, 8);
            clearArtwork();
            artworkBlur = small;
            small = null;
            artworkColor = color;
            artworkSource = new WeakReference<>(source);
            artworkGeneration = generation;
            artworkOrientation = holder.orientation;
            setColor(null);
        } catch (RuntimeException | OutOfMemoryError error) {
            FileLog.e(error);
        } finally {
            if (small != null) small.recycle();
            if (readable != null) readable.recycle();
            holder.release();
        }
    }

    public static CharSequence getTitle(TLRPC.Document document) {
        if (document == null) return null;
        for (TLRPC.DocumentAttribute attribute : document.attributes) {
            if (attribute instanceof TLRPC.TL_documentAttributeAudio && !TextUtils.isEmpty(attribute.title)) return attribute.title;
        }
        return FileLoader.getDocumentFileName(document);
    }

    public static CharSequence getAuthor(TLRPC.Document document) {
        if (document == null) return null;
        for (TLRPC.DocumentAttribute attribute : document.attributes) {
            if (attribute instanceof TLRPC.TL_documentAttributeAudio && !attribute.voice) return attribute.performer;
        }
        return null;
    }

    public void setText(CharSequence author, CharSequence title) {
        if (TextUtils.isEmpty(author)) author = getString(R.string.AudioUnknownArtist);
        if (TextUtils.isEmpty(title)) title = getString(R.string.AudioUnknownTitle);
        titleView.setText(title);
        authorView.setText(author);
        setContentDescription(getString(R.string.AccDescrProfileMusic) + ": " + title + " — " + author);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        attached = true;
        cover.onAttachedToWindow();
        NotificationCenter center = NotificationCenter.getInstance(currentAccount);
        center.addObserver(this, NotificationCenter.messagePlayingDidStart);
        center.addObserver(this, NotificationCenter.messagePlayingDidReset);
        center.addObserver(this, NotificationCenter.messagePlayingPlayStateChanged);
        center.addObserver(this, NotificationCenter.messagePlayingProgressDidChanged);
        updateArtwork();
        setColor(null);
        updatePlayback();
    }

    @Override
    protected void onDetachedFromWindow() {
        attached = false;
        stopAnimation();
        cover.onDetachedFromWindow();
        NotificationCenter center = NotificationCenter.getInstance(currentAccount);
        center.removeObserver(this, NotificationCenter.messagePlayingDidStart);
        center.removeObserver(this, NotificationCenter.messagePlayingDidReset);
        center.removeObserver(this, NotificationCenter.messagePlayingPlayStateChanged);
        center.removeObserver(this, NotificationCenter.messagePlayingProgressDidChanged);
        clearArtwork();
        super.onDetachedFromWindow();
    }

    @Override
    public void didReceivedNotification(int id, int account, Object... args) {
        if (account == currentAccount) updatePlayback();
    }

    private void updatePlayback() {
        boolean wasPlaying = playing, wasSelected = selectedTrack;
        float previousProgress = progress;
        MediaController controller = MediaController.getInstance();
        MessageObject message = controller.getPlayingMessageObject();
        TLRPC.Document current = message == null ? null : message.getDocument();
        selectedTrack = message != null && current != null && document != null
                && ProfileMusicLayout.isCurrentTrack(currentAccount, document.id, message.currentAccount, current.id);
        playing = selectedTrack && !controller.isMessagePaused() && !controller.isDownloadingCurrentMessage();
        progress = selectedTrack ? Math.max(0f, Math.min(1f, message.audioProgress)) : 0f;
        playPause.setPause(playing, false);
        playButton.setContentDescription(getString(playing ? R.string.AccActionPause : R.string.AccActionPlay));
        if (!canAnimateMotion()) stopAnimation();
        if (isShown() && (wasPlaying != playing || wasSelected != selectedTrack || previousProgress != progress)) invalidate();
    }

    private boolean motionEnabled() {
        return SharedConfig.animationsEnabled() && !LiteMode.isPowerSaverApplied()
                && (Build.VERSION.SDK_INT < 26 || ValueAnimator.areAnimatorsEnabled())
                && (powerManager == null || !powerManager.isPowerSaveMode());
    }

    private boolean canAnimateMotion() {
        if (!attached || ignoreBlur || currentHeight < getMeasuredHeight() * .5f
                || !isShown() || !hasWindowFocus() || getWindowVisibility() != VISIBLE || !motionEnabled()) return false;
        float alpha = 1f;
        for (View view = this; view != null; view = view.getParent() instanceof View ? (View) view.getParent() : null) {
            alpha *= view.getAlpha();
            if (alpha <= .05f) return false;
        }
        return getGlobalVisibleRect(visibleRect) && !visibleRect.isEmpty();
    }

    private void stopAnimation() {
        removeCallbacks(animationFrame);
        framePosted = false;
    }

    public void drawingBlur(boolean drawing) {
        if (ignoreBlur != drawing) {
            ignoreBlur = drawing;
            if (drawing) stopAnimation();
            invalidate();
        }
    }

    public void drawingBlur(RenderNode node, ProfileActivity.AvatarImageView avatar, float scale, float dy) {
        drawingBlur(false);
    }

    public void updatePosition(float y, float newHeight) {
        float height = Math.max(0f, newHeight);
        boolean changed = currentHeight != height || getTranslationY() != y;
        currentHeight = height;
        setTranslationY(y);
        if (currentHeight < getMeasuredHeight() * .5f) stopAnimation();
        if (changed) invalidate();
    }

    @Override
    public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if (!focused) stopAnimation();
        else if (playing) invalidate();
    }

    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        if (visibility != VISIBLE) stopAnimation();
        else if (playing) invalidate();
    }

    @Override
    protected void onVisibilityChanged(View changedView, int visibility) {
        super.onVisibilityChanged(changedView, visibility);
        if (!isShown()) stopAnimation();
        else if (playing) invalidate();
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            touchStarted = document != null && currentHeight >= getMeasuredHeight() * .5f
                    && event.getY() <= Math.min(currentHeight, getHeight()) && rect.contains(event.getX(), event.getY());
        }
        if (!touchStarted) return false;
        boolean handled = super.dispatchTouchEvent(event);
        if (event.getActionMasked() == MotionEvent.ACTION_UP || event.getActionMasked() == MotionEvent.ACTION_CANCEL) touchStarted = false;
        return handled;
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        float alpha = Math.min(1f, currentHeight / Math.max(1, getMeasuredHeight()));
        if (alpha <= 0f || document == null) return;
        int save = canvas.saveLayerAlpha(0, 0, getWidth(), Math.min(currentHeight, getHeight()), (int) (255 * alpha));
        canvas.clipRect(0, 0, getWidth(), Math.min(currentHeight, getHeight()));
        super.dispatchDraw(canvas);
        canvas.restoreToCount(save);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (layout == null || currentHeight <= 0f || document == null) return;
        boolean motionAllowed = canAnimateMotion();
        boolean moving = playing && artworkBlur != null && motionAllowed;
        float alpha = Math.min(1f, currentHeight / Math.max(1, getMeasuredHeight()));
        int save = canvas.saveLayerAlpha(0, 0, getWidth(), Math.min(currentHeight, getHeight()), (int) (255 * alpha));
        canvas.clipRect(0, 0, getWidth(), Math.min(currentHeight, getHeight()));
        clipPath.rewind();
        clipPath.addRoundRect(rect, dp(22), dp(22), Path.Direction.CW);
        canvas.clipPath(clipPath);
        paint.setColor(surfaceColor);
        canvas.drawRoundRect(rect, dp(22), dp(22), paint);
        drawArtwork(canvas, moving);
        canvas.drawRoundRect(rect, dp(22), dp(22), washPaint);
        canvas.drawRoundRect(rect, dp(22), dp(22), sheenPaint);
        canvas.drawRoundRect(rect, dp(22), dp(22), edgePaint);
        if (isPressed()) {
            paint.setColor(Theme.multAlpha(textColor, .04f));
            canvas.drawRoundRect(rect, dp(22), dp(22), paint);
        }
        if (isFocused()) {
            paint.setColor(progressColor);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(dp(2));
            canvas.drawRoundRect(rect, dp(22), dp(22), paint);
            paint.setStyle(Paint.Style.FILL);
        }
        drawCover(canvas);
        trackRect.set(dp(layout.buttonLeft + 3f), dp(layout.buttonTop + 3f),
                dp(layout.buttonLeft + 45f), dp(layout.buttonTop + 45f));
        paint.setColor(Theme.multAlpha(Color.WHITE, Theme.isCurrentThemeDark() ? .09f : .48f));
        canvas.drawRoundRect(trackRect, dp(21), dp(21), paint);
        canvas.drawRoundRect(trackRect, dp(21), dp(21), buttonRimPaint);
        if (playButton.isFocused()) {
            paint.setColor(progressColor);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(dp(2));
            canvas.drawRoundRect(trackRect, dp(21), dp(21), paint);
            paint.setStyle(Paint.Style.FILL);
        }
        float start = dp(layout.progressLeft), end = dp(layout.progressRight), y = dp(layout.progressY);
        float activeEnd = start + (end - start) * progress;
        float point = LocaleController.isRTL ? end - (end - start) * progress : activeEnd;
        float halfThickness = AndroidUtilities.dpf2(1);
        paint.setColor(Theme.multAlpha(textColor, .12f));
        canvas.drawRect(start, y - halfThickness, end, y + halfThickness, paint);
        paint.setColor(progressColor);
        float activeLeft = LocaleController.isRTL ? point : start;
        float activeRight = LocaleController.isRTL ? end : point;
        if (activeRight > activeLeft) {
            canvas.drawRect(activeLeft, y - halfThickness, activeRight, y + halfThickness, paint);
        }
        canvas.restoreToCount(save);
        if (moving && !framePosted) {
            framePosted = true;
            postDelayed(animationFrame, 40);
        } else if (!moving) {
            stopAnimation();
        }
    }

    private void drawArtwork(Canvas canvas, boolean moving) {
        if (artworkBlur == null) return;
        float wave = moving ? (float) Math.sin(SystemClock.uptimeMillis() * Math.PI / 2800.0) : 0f;
        artworkRect.set(rect.left - dp(48), rect.top - dp(40), rect.right + dp(48), rect.bottom + dp(40));
        int save = canvas.save();
        canvas.translate(dp(2) * wave, 0);
        artworkPaint.setAlpha((int) (255 * ((Theme.isCurrentThemeDark() ? .42f : .26f)
                + .025f * wave + .01f * parentExpanded)));
        canvas.drawBitmap(artworkBlur, null, artworkRect, artworkPaint);
        canvas.restoreToCount(save);
    }

    private void drawCover(Canvas canvas) {
        trackRect.set(dp(layout.coverLeft), dp(layout.coverTop), dp(layout.coverLeft + layout.coverSize), dp(layout.coverTop + layout.coverSize));
        int save = canvas.save();
        trackRect.offset(0, dp(4));
        paint.setColor(Theme.multAlpha(Color.BLACK, .08f));
        canvas.drawRoundRect(trackRect, dp(13), dp(13), paint);
        trackRect.offset(0, -dp(2));
        paint.setColor(Theme.multAlpha(Color.BLACK, .08f));
        canvas.drawRoundRect(trackRect, dp(12), dp(12), paint);
        trackRect.offset(0, -dp(2));
        paint.setColor(Theme.multAlpha(textColor, .08f));
        canvas.drawRoundRect(trackRect, dp(12), dp(12), paint);
        cover.draw(canvas);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(AndroidUtilities.dpf2(1));
        paint.setColor(Theme.multAlpha(Color.WHITE, Theme.isCurrentThemeDark() ? .16f : .64f));
        canvas.drawRoundRect(trackRect, dp(12), dp(12), paint);
        paint.setStyle(Paint.Style.FILL);
        canvas.restoreToCount(save);
    }
}

package org.telegram.ui.Components;

import com.th3nekit.finegram.core.configs.FinegramPlayerConfig;
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
import com.th3nekit.finegram.core.ui.MusicPlayerSurface;
import com.th3nekit.finegram.core.ui.PlayerButtonDrawable;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.FileLoader;
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

public class ProfileMusicView extends FrameLayout implements NotificationCenter.NotificationCenterDelegate, Theme.Colorable {
    private final FinegramPlayerConfig.Observer playerSettingsObserver = new FinegramPlayerConfig.Observer("FG_ProfileMusic", () -> {
        applyColors();
        requestLayout();
        updatePlayback();
        invalidate();
    });
    private final Theme.ResourcesProvider resourcesProvider;
    private final int currentAccount;
    private final PowerManager powerManager;
    private final TextView titleView, authorView;
    private final ImageView playButton;
    private final ProfilePillView stockView;
    private final PlayerButtonDrawable playBackground = new PlayerButtonDrawable(Color.TRANSPARENT);
    private final PlayPauseDrawable playPause = new PlayPauseDrawable(22);
    private final ImageReceiver cover = new ImageReceiver(this);
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint edgePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint sheenPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint artworkPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final Paint washPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
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
    private int artworkRequest;
    private Drawable placeholder;
    private float currentHeight, parentExpanded, progress;
    private boolean attached, playing, selectedTrack, touchStarted, framePosted;
    private boolean ignoreBlur;
    private int cardStyle = -1;
    private MessagesController.PeerColor peerColor;
    private float coverAngle;
    private long coverFrame;
    private final Path wavePath = new Path();
    private final Paint chevronPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path chevronPath = new Path();
    private org.telegram.ui.Components.blur3.drawable.BlurredBackgroundDrawable capsuleGlass;
    private float capsuleGlassRadius;
    private CharSequence lastTitle, lastAuthor;
    private boolean capsuleText;
    private final Path liquidPath = new Path();
    private final Paint liquidEdgePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private float liquidProgress, liquidAlpha, liquidPhase;
    private long liquidFrame;

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
        stockView = new ProfilePillView(context, resourcesProvider);
        stockView.setClickable(true);
        stockView.setFocusable(true);
        stockView.setOnClickListener(view -> performClick());
        stockView.setButtonColor(Theme.getColor(Theme.key_windowBackgroundWhite, resourcesProvider), false);
        addView(stockView);
        titleView = createText(context, 17, true);
        authorView = createText(context, 13, false);
        addView(titleView);
        addView(authorView);
        playButton = new ImageView(context);
        playButton.setScaleType(ImageView.ScaleType.CENTER);
        playButton.setImageDrawable(playPause);
        playButton.setBackground(playBackground);
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
        liquidEdgePaint.setStyle(Paint.Style.STROKE);
        liquidEdgePaint.setStrokeCap(Paint.Cap.ROUND);
        chevronPaint.setStyle(Paint.Style.STROKE);
        chevronPaint.setStrokeCap(Paint.Cap.ROUND);
        chevronPaint.setStrokeJoin(Paint.Join.ROUND);
        chevronPath.moveTo(0, -AndroidUtilities.dpf2(4.5f));
        chevronPath.lineTo(AndroidUtilities.dpf2(4.2f), 0);
        chevronPath.lineTo(0, AndroidUtilities.dpf2(4.5f));
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
        if (FinegramPlayerConfig.capsuleProfileCard()) return dp(60);
        return dp(FinegramPlayerConfig.stockProfileCard() ? 37f
                : ProfileMusicLayout.height(context.getResources().getConfiguration().fontScale));
    }

    public void setGlassFactory(org.telegram.ui.Components.blur3.BlurredBackgroundDrawableViewFactory factory) {
        stockView.setGlassFactory(factory);
        capsuleGlass = factory != null ? factory.create(this, ProfileButtonsGlass.NO_COLORS) : null;
        capsuleGlassRadius = 0;
        invalidate();
    }

    @Override
    public void updateColors() {
        applyColors();
        stockView.invalidate();
        invalidate();
    }

    public void setButtonColor(int color, boolean hasColorById) {
        stockView.setButtonColor(color, hasColorById);
    }

    public void setTrailingView(ProfilePillView view) {
        stockView.setTrailingView(view);
    }

    public void setOnPlayClickListener(OnClickListener listener) {
        playButton.setOnClickListener(listener);
    }

    @Override
    public void setOnClickListener(OnClickListener listener) {
        super.setOnClickListener(listener);
        if (stockView != null) setClickable(!FinegramPlayerConfig.stockProfileCard() && listener != null);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = MeasureSpec.getSize(widthMeasureSpec);
        if (FinegramPlayerConfig.stockProfileCard()) {
            stockView.measure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY), heightMeasureSpec);
            setMeasuredDimension(width, stockView.getMeasuredHeight());
            return;
        }
        if (FinegramPlayerConfig.capsuleProfileCard()) {
            setMeasuredDimension(width, dp(60));
            titleView.measure(MeasureSpec.makeMeasureSpec(Math.max(0, width - dp(32 + 48 + 30)), MeasureSpec.AT_MOST),
                    MeasureSpec.makeMeasureSpec(dp(24), MeasureSpec.AT_MOST));
            playButton.measure(MeasureSpec.makeMeasureSpec(dp(32), MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec(dp(32), MeasureSpec.EXACTLY));
            return;
        }
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
        if (FinegramPlayerConfig.stockProfileCard()) {
            stockView.layout(0, 0, getWidth(), stockView.getMeasuredHeight());
            return;
        }
        if (FinegramPlayerConfig.capsuleProfileCard()) {
            boolean rtl = LocaleController.isRTL;
            float capsuleWidth = dp(48 + 30) + titleView.getMeasuredWidth();
            float capsuleLeft = (getWidth() - capsuleWidth) / 2f;
            float capsuleTop = (getHeight() - dp(44)) / 2f;
            rect.set(capsuleLeft, capsuleTop, capsuleLeft + capsuleWidth, capsuleTop + dp(44));
            int buttonLeft = (int) (rtl ? rect.right - dp(6 + 32) : rect.left + dp(6));
            int buttonTop = (int) (rect.centerY() - dp(16));
            playButton.layout(buttonLeft, buttonTop, buttonLeft + dp(32), buttonTop + dp(32));
            int textLeft = (int) (rtl ? rect.right - dp(48) - titleView.getMeasuredWidth() : rect.left + dp(48));
            int textTop = (int) (rect.centerY() - titleView.getMeasuredHeight() / 2f);
            titleView.layout(textLeft, textTop, textLeft + titleView.getMeasuredWidth(), textTop + titleView.getMeasuredHeight());
            cover.setImageCoords(buttonLeft, buttonTop, dp(32), dp(32));
            return;
        }
        titleView.setGravity(Gravity.CENTER_VERTICAL | (LocaleController.isRTL ? Gravity.RIGHT : Gravity.LEFT));
        authorView.setGravity(titleView.getGravity());
        float titleHeight = titleView.getMeasuredHeight() / AndroidUtilities.density;
        float authorHeight = authorView.getMeasuredHeight() / AndroidUtilities.density;
        float titleTop = layout.titleTop + (layout.titleHeight + layout.authorHeight - titleHeight - authorHeight) / 2f;
        place(titleView, layout.textLeft, titleTop);
        place(authorView, layout.textLeft, titleTop + titleHeight + 4f);
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
        this.peerColor = peerColor;
        applyColors();
    }

    private void applyColors() {
        boolean wasStock = cardStyle == FinegramPlayerConfig.CARD_STOCK;
        cardStyle = FinegramPlayerConfig.INSTANCE.getProfileCardStyle();
        boolean stock = cardStyle == FinegramPlayerConfig.CARD_STOCK;
        stockView.setVisibility(stock && document != null && getVisibility() == VISIBLE ? VISIBLE : GONE);
        stockView.setColor(peerColor);
        boolean capsule = cardStyle == FinegramPlayerConfig.CARD_CAPSULE;
        titleView.setVisibility(stock ? GONE : VISIBLE);
        authorView.setVisibility(stock || capsule ? GONE : VISIBLE);
        playButton.setVisibility(stock ? GONE : VISIBLE);
        playButton.setScaleType(capsule ? ImageView.ScaleType.FIT_CENTER : ImageView.ScaleType.CENTER);
        int buttonPadding = capsule ? dp(7) : 0;
        playButton.setPadding(buttonPadding, buttonPadding, buttonPadding, buttonPadding);
        titleView.setTextSize(TypedValue.COMPLEX_UNIT_SP, capsule ? 15 : 17);
        if (capsuleText != capsule) {
            capsuleText = capsule;
            applyText();
        }
        setClipChildren(!stock);
        setClickable(!stock);
        setFocusable(!stock);
        setImportantForAccessibility(stock ? IMPORTANT_FOR_ACCESSIBILITY_NO : IMPORTANT_FOR_ACCESSIBILITY_YES);
        if (stock) {
            stopAnimation();
            if (!wasStock) {
                clearArtwork();
                cover.clearImage();
            }
            return;
        }
        if (wasStock && document != null) loadCover();
        if (capsule) {
            cover.setRoundRadius(dp(16));
            surfaceColor = Theme.multAlpha(MD3ListAdapter.getBackgroundColor(resourcesProvider), .72f);
            boolean light = !Theme.isCurrentThemeDark() && parentExpanded < .5f && peerColor == null;
            textColor = light ? Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider) : Color.WHITE;
            progressColor = textColor;
            titleView.setTextColor(textColor);
            playBackground.setColor(Theme.multAlpha(Color.BLACK, .34f));
            playPause.setColor(Color.WHITE);
            applyText();
            invalidate();
            return;
        }
        cover.setRoundRadius(FinegramPlayerConfig.INSTANCE.getProfileRoundCover() ? dp(32) : dp(14));
        surfaceColor = MD3ListAdapter.getBackgroundColor(resourcesProvider);
        if (cardStyle == FinegramPlayerConfig.CARD_VIVID && artworkBlur != null) {
            float[] hsv = new float[3];
            Color.colorToHSV(artworkColor, hsv);
            hsv[1] = Math.max(hsv[1], .5f);
            hsv[2] = Theme.isCurrentThemeDark() ? .42f : .58f;
            surfaceColor = Color.HSVToColor(hsv);
        } else if (cardStyle == FinegramPlayerConfig.CARD_GLASS) {
            surfaceColor = Theme.multAlpha(surfaceColor, Theme.isCurrentThemeDark() ? .58f : .66f);
        } else if (cardStyle == FinegramPlayerConfig.CARD_ARTWORK && artworkBlur != null) {
            surfaceColor = ColorUtils.blendARGB(surfaceColor, artworkColor,
                    Theme.isCurrentThemeDark() ? .14f : .05f);
        }
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
        int authorColor = ColorUtils.blendARGB(contrastBackground, textColor, .78f);
        if (ColorUtils.calculateContrast(authorColor, contrastBackground) < 4.5) authorColor = textColor;
        authorView.setTextColor(authorColor);
        int buttonSurface = ColorUtils.compositeColors(progressColor, contrastBackground);
        playBackground.setColor(buttonSurface);
        int buttonInk = ColorUtils.calculateContrast(Color.BLACK, buttonSurface)
                >= ColorUtils.calculateContrast(Color.WHITE, buttonSurface) ? Color.BLACK : Color.WHITE;
        playPause.setColor(buttonInk);
        if (placeholder != null) placeholder.setColorFilter(textColor, android.graphics.PorterDuff.Mode.SRC_IN);
        updateShaders();
        invalidate();
    }

    private void updateShaders() {
        if (rect.isEmpty()) return;
        boolean dark = Theme.isCurrentThemeDark();
        float start = LocaleController.isRTL ? rect.right : rect.left;
        float end = LocaleController.isRTL ? rect.left : rect.right;
        if (cardStyle == FinegramPlayerConfig.CARD_VIVID) {
            washPaint.setShader(new LinearGradient(start, rect.top, end, rect.bottom,
                    new int[]{Theme.multAlpha(surfaceColor, .9f), Theme.multAlpha(ColorUtils.blendARGB(surfaceColor, Color.BLACK, .35f), .55f)},
                    null, Shader.TileMode.CLAMP));
        } else {
            washPaint.setShader(new LinearGradient(start, 0, end, 0,
                    new int[]{surfaceColor, Theme.multAlpha(surfaceColor, .76f), Theme.multAlpha(surfaceColor, .12f)},
                    new float[]{0f, .36f, 1f}, Shader.TileMode.CLAMP));
        }
        sheenPaint.setShader(new LinearGradient(rect.left, rect.top, rect.right, rect.bottom,
                new int[]{Theme.multAlpha(Color.WHITE, dark ? .06f : .32f), Color.TRANSPARENT},
                null, Shader.TileMode.CLAMP));
        edgePaint.setShader(new LinearGradient(0, rect.top, 0, rect.bottom,
                new int[]{Theme.multAlpha(Color.WHITE, dark ? .32f : .9f), Theme.multAlpha(textColor, .09f)},
                null, Shader.TileMode.CLAMP));
    }

    public void setParentExpanded(float expanded) {
        if (parentExpanded != expanded) {
            boolean recolor = cardStyle == FinegramPlayerConfig.CARD_CAPSULE && (parentExpanded < .5f) != (expanded < .5f);
            parentExpanded = expanded;
            if (recolor) applyColors();
            stockView.setParentExpanded(expanded);
            invalidate();
        }
    }

    public void setMusicDocument(TLRPC.Document document) {
        boolean same = this.document != null && document != null && this.document.id == document.id;
        this.document = document;
        stockView.setMusicDocument(document);
        setText(getAuthor(document), getTitle(document));
        if (!same) {
            clearArtwork();
            placeholder = getContext().getResources().getDrawable(R.drawable.music_empty).mutate();
            applyColors();
            if (!FinegramPlayerConfig.stockProfileCard()) loadCover();
        }
        updatePlayback();
    }

    private void loadCover() {
        TLRPC.PhotoSize thumb = document == null ? null : FileLoader.getClosestPhotoSizeWithSize(document.thumbs, 160);
        String artwork = document == null ? null : MessageObject.getArtworkUrl(document, true);
        cover.setImage(TextUtils.isEmpty(artwork) ? null : ImageLocation.getForPath(artwork), "64_64",
                thumb == null ? null : ImageLocation.getForDocument(thumb, document), "64_64",
                placeholder, 0, null, document, 1);
    }

    private void clearArtwork() {
        artworkRequest++;
        if (artworkBlur != null) artworkBlur.recycle();
        artworkBlur = null;
        artworkSource = null;
        artworkColor = 0;
    }

    private void updateArtwork() {
        if (!attached || FinegramPlayerConfig.stockProfileCard()) return;
        if (document == null || !cover.hasImageLoaded() && cover.getThumb() == null) {
            if (artworkBlur != null) {
                clearArtwork();
                applyColors();
            }
            return;
        }
        if (cover.getAnimation() != null || cover.getLottieAnimation() != null) return;
        ImageReceiver.BitmapHolder holder = cover.getBitmapSafe();
        if (holder == null) return;
        Bitmap source = holder.bitmap;
        if (source == null || source.isRecycled() || artworkSource != null && artworkSource.get() == source
                && artworkGeneration == source.getGenerationId() && artworkOrientation == holder.orientation) {
            holder.release();
            return;
        }
        artworkSource = new WeakReference<>(source);
        artworkGeneration = source.getGenerationId();
        artworkOrientation = holder.orientation;
        int request = ++artworkRequest;
        Utilities.globalQueue.postRunnable(() -> {
            MusicPlayerSurface.Artwork result = MusicPlayerSurface.prepareArtwork(source, holder.orientation);
            AndroidUtilities.runOnUIThread(() -> {
                holder.release();
                if (!attached || request != artworkRequest || FinegramPlayerConfig.stockProfileCard()) {
                    if (result != null) result.bitmap.recycle();
                    return;
                }
                if (result == null) {
                    artworkSource = null;
                    return;
                }
                if (artworkBlur != null) artworkBlur.recycle();
                artworkBlur = result.bitmap;
                artworkColor = result.accent;
                applyColors();
            });
        });
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
        lastTitle = title;
        lastAuthor = author;
        applyText();
        authorView.setText(author);
        setContentDescription(getString(R.string.AccDescrProfileMusic) + ": " + title + " — " + author);
    }

    private void applyText() {
        if (lastTitle == null) return;
        if (!capsuleText) {
            titleView.setTypeface(AndroidUtilities.bold());
            titleView.setText(lastTitle);
            return;
        }
        android.text.SpannableStringBuilder text = new android.text.SpannableStringBuilder(lastTitle);
        text.setSpan(new TypefaceSpan(AndroidUtilities.bold()), 0, text.length(), android.text.Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        int start = text.length();
        text.append(" — ").append(lastAuthor == null ? "" : lastAuthor);
        text.setSpan(new android.text.style.ForegroundColorSpan(Theme.multAlpha(textColor, .7f)), start, text.length(), android.text.Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        titleView.setTypeface(android.graphics.Typeface.DEFAULT);
        titleView.setText(text);
    }

    @Override
    protected void onAttachedToWindow() {
        playerSettingsObserver.start();
        super.onAttachedToWindow();
        attached = true;
        cover.onAttachedToWindow();
        NotificationCenter center = NotificationCenter.getInstance(currentAccount);
        center.addObserver(this, NotificationCenter.messagePlayingDidStart);
        center.addObserver(this, NotificationCenter.messagePlayingDidReset);
        center.addObserver(this, NotificationCenter.messagePlayingPlayStateChanged);
        center.addObserver(this, NotificationCenter.messagePlayingProgressDidChanged);
        updateArtwork();
        applyColors();
        updatePlayback();
    }

    @Override
    protected void onDetachedFromWindow() {
        playerSettingsObserver.stop();
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
        boolean animate = wasPlaying != playing && canAnimateMotion();
        playPause.setPause(playing, animate);
        playBackground.setCorner(playing && cardStyle != FinegramPlayerConfig.CARD_CAPSULE ? 0f : 1f, animate);
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
        if (FinegramPlayerConfig.stockProfileCard() || !attached || ignoreBlur || currentHeight < getMeasuredHeight() * .5f
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
        coverFrame = 0;
    }

    public void drawingBlur(boolean drawing) {
        if (ignoreBlur != drawing) {
            ignoreBlur = drawing;
            if (drawing) stopAnimation();
            invalidate();
        }
    }

    public void drawingBlur(RenderNode node, ProfileActivity.AvatarImageView avatar, float scale, float dy) {
        if (ignoreBlur) {
            ignoreBlur = false;
            invalidate();
        }
    }

    public void updatePosition(float y, float newHeight) {
        float height = Math.max(0f, newHeight);
        boolean changed = currentHeight != height || getTranslationY() != y;
        currentHeight = height;
        setTranslationY(y);
        stockView.updatePosition(0, height);
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
        if (stockView != null) {
            stockView.setVisibility(FinegramPlayerConfig.stockProfileCard() && document != null
                    && getVisibility() == VISIBLE ? VISIBLE : GONE);
        }
        if (!isShown()) stopAnimation();
        else if (playing) invalidate();
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        if (FinegramPlayerConfig.stockProfileCard()) {
            return document != null && currentHeight > 0f && super.dispatchTouchEvent(event);
        }
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
        if (FinegramPlayerConfig.stockProfileCard()) {
            if (document != null) super.dispatchDraw(canvas);
            return;
        }
        float alpha = Math.min(1f, currentHeight / Math.max(1, getMeasuredHeight()));
        if (alpha <= 0f || document == null) return;
        int save = canvas.saveLayerAlpha(0, 0, getWidth(), Math.min(currentHeight, getHeight()), (int) (255 * alpha));
        canvas.clipRect(0, 0, getWidth(), Math.min(currentHeight, getHeight()));
        super.dispatchDraw(canvas);
        canvas.restoreToCount(save);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (FinegramPlayerConfig.capsuleProfileCard()) {
            if (currentHeight > 0f && document != null) drawCapsule(canvas);
            return;
        }
        if (FinegramPlayerConfig.stockProfileCard() || layout == null || currentHeight <= 0f || document == null) return;
        boolean motionAllowed = canAnimateMotion();
        boolean roundCover = FinegramPlayerConfig.INSTANCE.getProfileRoundCover();
        int progressStyle = FinegramPlayerConfig.INSTANCE.getProfileProgress();
        boolean equalizer = FinegramPlayerConfig.INSTANCE.getProfileEqualizer();
        boolean moving = playing && motionAllowed && (artworkBlur != null && (cardStyle == FinegramPlayerConfig.CARD_ARTWORK || cardStyle == FinegramPlayerConfig.CARD_VIVID)
                || roundCover || equalizer || progressStyle == FinegramPlayerConfig.PROGRESS_WAVE);
        float alpha = Math.min(1f, currentHeight / Math.max(1, getMeasuredHeight()));
        int save = canvas.saveLayerAlpha(0, 0, getWidth(), Math.min(currentHeight, getHeight()), (int) (255 * alpha));
        canvas.clipRect(0, 0, getWidth(), Math.min(currentHeight, getHeight()));
        clipPath.rewind();
        clipPath.addRoundRect(rect, dp(24), dp(24), Path.Direction.CW);
        canvas.clipPath(clipPath);
        paint.setColor(surfaceColor);
        canvas.drawRoundRect(rect, dp(24), dp(24), paint);
        if (cardStyle == FinegramPlayerConfig.CARD_ARTWORK || cardStyle == FinegramPlayerConfig.CARD_VIVID) {
            drawArtwork(canvas, moving);
            canvas.drawRoundRect(rect, dp(24), dp(24), washPaint);
        }
        if (cardStyle != FinegramPlayerConfig.CARD_PLAIN) {
            canvas.drawRoundRect(rect, dp(24), dp(24), sheenPaint);
            canvas.drawRoundRect(rect, dp(24), dp(24), edgePaint);
        }
        if (isPressed()) {
            paint.setColor(Theme.multAlpha(textColor, .04f));
            canvas.drawRoundRect(rect, dp(24), dp(24), paint);
        }
        if (isFocused()) {
            paint.setColor(progressColor);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(dp(2));
            canvas.drawRoundRect(rect, dp(24), dp(24), paint);
            paint.setStyle(Paint.Style.FILL);
        }
        drawCover(canvas, roundCover, equalizer, moving);
        trackRect.set(dp(layout.buttonLeft), dp(layout.buttonTop),
                dp(layout.buttonLeft + 48f), dp(layout.buttonTop + 48f));
        if (playButton.isFocused()) {
            paint.setColor(progressColor);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(dp(2));
            canvas.drawRoundRect(trackRect, dp(21), dp(21), paint);
            paint.setStyle(Paint.Style.FILL);
        }
        if (progressStyle != FinegramPlayerConfig.PROGRESS_NONE) {
            float start = dp(layout.progressLeft), end = dp(layout.progressRight), y = dp(layout.progressY);
            float activeEnd = start + (end - start) * progress;
            float point = LocaleController.isRTL ? end - (end - start) * progress : activeEnd;
            float halfThickness = AndroidUtilities.dpf2(1);
            paint.setColor(Theme.multAlpha(textColor, .12f));
            trackRect.set(start, y - halfThickness, end, y + halfThickness);
            canvas.drawRoundRect(trackRect, halfThickness, halfThickness, paint);
            paint.setColor(progressColor);
            float activeLeft = LocaleController.isRTL ? point : start;
            float activeRight = LocaleController.isRTL ? end : point;
            if (activeRight > activeLeft) {
                if (progressStyle == FinegramPlayerConfig.PROGRESS_WAVE && selectedTrack) {
                    float amplitude = playing ? dp(2) : dp(.6f);
                    float length = dp(14);
                    float phase = moving ? (SystemClock.uptimeMillis() % 1400) / 1400f * length : 0f;
                    wavePath.rewind();
                    for (float x = activeLeft; x <= activeRight; x += dp(1.5f)) {
                        float wy = y + amplitude * (float) Math.sin((x - activeLeft + phase) / length * Math.PI * 2);
                        if (x == activeLeft) wavePath.moveTo(x, wy);
                        else wavePath.lineTo(x, wy);
                    }
                    paint.setStyle(Paint.Style.STROKE);
                    paint.setStrokeWidth(dp(2));
                    paint.setStrokeCap(Paint.Cap.ROUND);
                    canvas.drawPath(wavePath, paint);
                    paint.setStyle(Paint.Style.FILL);
                } else {
                    trackRect.set(activeLeft, y - halfThickness, activeRight, y + halfThickness);
                    canvas.drawRoundRect(trackRect, halfThickness, halfThickness, paint);
                }
            }
        }
        canvas.restoreToCount(save);
        if (moving && !framePosted) {
            framePosted = true;
            postDelayed(animationFrame, 40);
        } else if (!moving) {
            stopAnimation();
        }
    }

    private void drawCapsule(Canvas canvas) {
        float alpha = Math.min(1f, currentHeight / Math.max(1, getMeasuredHeight()));
        if (alpha <= 0f || rect.isEmpty()) {
            if (capsuleGlass != null) capsuleGlass.setAlpha(0);
            return;
        }
        float radius = rect.height() / 2f;
        if (capsuleGlass != null) {
            if (capsuleGlassRadius != radius) {
                capsuleGlassRadius = radius;
                capsuleGlass.setRadius(radius);
            }
            capsuleGlass.setBounds((int) rect.left, (int) rect.top, (int) Math.ceil(rect.right), (int) Math.ceil(rect.bottom));
            capsuleGlass.setAlpha((int) (255 * alpha));
            capsuleGlass.draw(canvas);
        } else {
            paint.setColor(Theme.multAlpha(surfaceColor, alpha));
            canvas.drawRoundRect(rect, radius, radius, paint);
        }
        int save = canvas.save();
        clipPath.rewind();
        clipPath.addRoundRect(rect, radius, radius, Path.Direction.CW);
        canvas.clipPath(clipPath);
        boolean needFrame = drawLiquid(canvas, alpha);
        if (isPressed()) {
            paint.setColor(Theme.multAlpha(textColor, .06f * alpha));
            canvas.drawRect(rect, paint);
        }
        canvas.restoreToCount(save);
        cover.setAlpha(alpha);
        cover.draw(canvas);
        cover.setAlpha(1f);
        float chevronX = LocaleController.isRTL ? rect.left + dp(16) : rect.right - dp(16);
        chevronPaint.setColor(Theme.multAlpha(textColor, .6f * alpha));
        chevronPaint.setStrokeWidth(AndroidUtilities.dpf2(1.6f));
        save = canvas.save();
        canvas.translate(chevronX, rect.centerY());
        if (LocaleController.isRTL) canvas.scale(-1f, 1f);
        canvas.drawPath(chevronPath, chevronPaint);
        canvas.restoreToCount(save);
        if (needFrame && !framePosted && canAnimateMotion()) {
            framePosted = true;
            postDelayed(animationFrame, 32);
        }
    }

    private boolean drawLiquid(Canvas canvas, float alpha) {
        boolean show = selectedTrack && FinegramPlayerConfig.INSTANCE.getProfileProgress() != FinegramPlayerConfig.PROGRESS_NONE;
        long now = SystemClock.uptimeMillis();
        float dt = liquidFrame == 0 ? 0f : Math.min(64f, now - liquidFrame);
        liquidFrame = now;
        boolean motion = canAnimateMotion();
        float targetAlpha = show ? 1f : 0f;
        if (!motion) {
            liquidAlpha = targetAlpha;
            liquidProgress = progress;
        } else {
            liquidAlpha = liquidAlpha < targetAlpha ? Math.min(targetAlpha, liquidAlpha + dt / 260f)
                    : Math.max(targetAlpha, liquidAlpha - dt / 260f);
            if (Math.abs(progress - liquidProgress) > .25f) {
                liquidProgress = progress;
            } else {
                liquidProgress += (progress - liquidProgress) * Math.min(1f, dt / 180f);
                if (Math.abs(progress - liquidProgress) < .0005f) liquidProgress = progress;
            }
            if (playing) liquidPhase = (liquidPhase + dt / 1700f) % 1f;
        }
        if (liquidAlpha <= 0f) {
            liquidFrame = show ? liquidFrame : 0;
            return false;
        }
        boolean rtl = LocaleController.isRTL;
        float start = rtl ? rect.right : rect.left;
        float floor = dp(6 + 32 + 4);
        float span = Math.max(0f, rect.width() - floor);
        float edge = floor + span * Math.max(0f, Math.min(1f, liquidProgress));
        float edgeX = rtl ? start - edge : start + edge;
        float amplitude = AndroidUtilities.dpf2(playing && motion ? 2.6f : 1.2f);
        float wave = rect.height();
        liquidPath.rewind();
        liquidPath.moveTo(start, rect.top);
        float step = AndroidUtilities.dpf2(2f);
        for (float y = rect.top; y <= rect.bottom + step; y += step) {
            float yy = Math.min(rect.bottom, y);
            float offset = amplitude * (float) Math.sin(((yy - rect.top) / wave + liquidPhase) * Math.PI * 2);
            liquidPath.lineTo(rtl ? edgeX - offset : edgeX + offset, yy);
        }
        liquidPath.lineTo(start, rect.bottom);
        liquidPath.close();
        paint.setColor(Theme.multAlpha(textColor, .11f * alpha * liquidAlpha));
        canvas.drawPath(liquidPath, paint);
        liquidEdgePaint.setColor(Theme.multAlpha(textColor, .32f * alpha * liquidAlpha));
        liquidEdgePaint.setStrokeWidth(AndroidUtilities.dpf2(1.4f));
        int save = canvas.save();
        float inset = AndroidUtilities.dpf2(1.5f);
        canvas.clipRect(rtl ? rect.left : edgeX - amplitude * 2 - inset, rect.top + inset,
                rtl ? edgeX + amplitude * 2 + inset : rect.right, rect.bottom - inset);
        canvas.drawPath(liquidPath, liquidEdgePaint);
        canvas.restoreToCount(save);
        return liquidAlpha != targetAlpha || liquidProgress != progress || playing && motion;
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

    private void drawCover(Canvas canvas, boolean round, boolean equalizer, boolean moving) {
        trackRect.set(dp(layout.coverLeft), dp(layout.coverTop), dp(layout.coverLeft + layout.coverSize), dp(layout.coverTop + layout.coverSize));
        long now = SystemClock.uptimeMillis();
        if (round && playing && moving) {
            if (coverFrame != 0) coverAngle = (coverAngle + (now - coverFrame) * .045f) % 360f;
            coverFrame = now;
        } else {
            coverFrame = 0;
        }
        if (round) {
            float cx = trackRect.centerX(), cy = trackRect.centerY();
            int save = canvas.save();
            canvas.rotate(coverAngle, cx, cy);
            cover.draw(canvas);
            canvas.restoreToCount(save);
            paint.setColor(surfaceColor);
            canvas.drawCircle(cx, cy, dp(4), paint);
            paint.setColor(Theme.multAlpha(textColor, .35f));
            canvas.drawCircle(cx, cy, dp(1.6f), paint);
        }
        if (round) {
            if (equalizer && selectedTrack) drawEqualizer(canvas, moving ? now : 0);
            return;
        }
        int save = canvas.save();
        trackRect.offset(0, dp(4));
        paint.setColor(Theme.multAlpha(Color.BLACK, .08f));
        canvas.drawRoundRect(trackRect, dp(15), dp(15), paint);
        trackRect.offset(0, -dp(2));
        paint.setColor(Theme.multAlpha(Color.BLACK, .08f));
        canvas.drawRoundRect(trackRect, dp(14), dp(14), paint);
        trackRect.offset(0, -dp(2));
        paint.setColor(Theme.multAlpha(textColor, .08f));
        canvas.drawRoundRect(trackRect, dp(14), dp(14), paint);
        cover.draw(canvas);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(AndroidUtilities.dpf2(1));
        paint.setColor(Theme.multAlpha(Color.WHITE, Theme.isCurrentThemeDark() ? .16f : .64f));
        canvas.drawRoundRect(trackRect, dp(14), dp(14), paint);
        paint.setStyle(Paint.Style.FILL);
        canvas.restoreToCount(save);
        if (FinegramPlayerConfig.INSTANCE.getProfileEqualizer() && selectedTrack) {
            drawEqualizer(canvas, moving ? now : 0);
        }
    }

    private void drawEqualizer(Canvas canvas, long now) {
        float size = dp(layout.coverSize);
        float left = dp(layout.coverLeft), top = dp(layout.coverTop);
        float barWidth = dp(3.2f), gap = dp(2.6f);
        float total = barWidth * 4 + gap * 3;
        float baseX = left + (size - total) / 2f;
        float bottom = top + size / 2f + dp(8);
        paint.setColor(Theme.multAlpha(Color.BLACK, .32f));
        if (FinegramPlayerConfig.INSTANCE.getProfileRoundCover()) {
            canvas.drawCircle(left + size / 2f, top + size / 2f, size / 2f, paint);
        } else {
            artworkRect.set(left, top, left + size, top + size);
            canvas.drawRoundRect(artworkRect, dp(14), dp(14), paint);
        }
        paint.setColor(Color.WHITE);
        for (int i = 0; i < 4; i++) {
            float level = playing && now != 0 ? .35f + .65f * Math.abs((float) Math.sin(now / (190f + i * 37f) + i * 1.3f)) : .25f + (i % 2) * .1f;
            float h = dp(16) * level;
            float x = baseX + i * (barWidth + gap);
            artworkRect.set(x, bottom - h, x + barWidth, bottom);
            canvas.drawRoundRect(artworkRect, barWidth / 2f, barWidth / 2f, paint);
        }
    }
}

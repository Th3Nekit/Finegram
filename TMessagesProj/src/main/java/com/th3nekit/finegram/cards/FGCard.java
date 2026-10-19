/* Licensed under GNU GPL v. 2 or later. */            
                                              
  
                                                                                                                                                                     
                                                                                                                                                                    
                                                         
   

package com.th3nekit.finegram.cards;

import android.animation.TimeInterpolator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.StateListAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.LinearGradient;
import android.graphics.Outline;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;
import android.view.Gravity;
import android.view.View;
import android.view.ViewOutlineProvider;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.core.graphics.ColorUtils;

import androidx.annotation.NonNull;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.AnimatedTextView;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.LoadingDrawable;

public abstract class FGCard extends FrameLayout {
    private org.telegram.ui.ActionBar.BaseFragment hostFragment;

    public void setHostFragment(org.telegram.ui.ActionBar.BaseFragment fragment) {
        hostFragment = fragment;
    }

    protected org.telegram.ui.ActionBar.BaseFragment getHostFragment() {
        if (hostFragment != null && !hostFragment.isFinished && hostFragment.getParentActivity() != null) {
            return hostFragment;
        }
        return com.th3nekit.finegram.core.ui.FGHostScreen.find();
    }

    protected final Theme.ResourcesProvider resourcesProvider;

    private static final int CHIP_HEIGHT_DP = 32;
    private static final int STACK_INSET_DP = 4;
    private static final int CORNER_RADIUS_DP = 12;

    /** Поля внутри пилюли: с ними значение не липнет к её краям. */
    private static final int CHIP_PADDING_DP = 10;
    private static final int ICON_SIZE_DP = 16;
    private static final int ICON_GAP_DP = 6;

    private static final long RESIZE_DURATION_MS = 180;
    private static final float RESIZE_TEXT_SCALE_POP = 0.03f;
    private static final TimeInterpolator RESIZE_INTERPOLATOR = CubicBezierInterpolator.EASE_OUT_QUINT;

    private final LinearLayout content;
    private final Paint backFill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint backStroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF backRect = new RectF();
    private float stackDepth = 1f;
    protected final ImageView iconView;
    protected final AnimatedTextView textView;
    private final CardBackground background;
    private boolean iconVisible = true;

    private int brandTop = 0xff2b2b2b, brandBottom = 0xff202020;
    private int colorMode = FGCardsConfig.COLOR_MODE_CUSTOM;
    private int resolvedForeground;
    private boolean opaqueFlat;
    private boolean inlineFolderStyle;
    private boolean renderingInstantly;
    private int lastIconRes;
    private int maxChipWidth;
    private int appliedTextMaxWidth;
    private CharSequence accessibilityLabel;
    private CharSequence accessibilityValue;

    private boolean loading;
    private long lastUpdateMs;

    private LoadingDrawable loadingDrawable;
    private final RectF loadingRect = new RectF();
    private final Paint staticLoadingPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    private final Runnable autoRefresh = new Runnable() {
        @Override
        public void run() {
            try { onUpdateData(false); } catch (Throwable ignore) {}
            scheduleNext();
        }
    };

    public FGCard(Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;
        setClipChildren(false);
        setClipToPadding(false);
        setMinimumHeight(AndroidUtilities.dp(44));
        setMinimumWidth(AndroidUtilities.dp(44));
        backStroke.setStyle(Paint.Style.STROKE);
        backStroke.setStrokeWidth(AndroidUtilities.dpf2(0.7f));

        background = new CardBackground(0xff2b2b2b, 0xff202020,
                Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider));

        content = new LinearLayout(context);
        content.setOrientation(LinearLayout.HORIZONTAL);
        content.setGravity(Gravity.CENTER_VERTICAL | Gravity.START);
        content.setClipChildren(true);
        content.setClipToPadding(false);
        content.setBackground(background);
        content.setPadding(AndroidUtilities.dp(CHIP_PADDING_DP), 0, AndroidUtilities.dp(CHIP_PADDING_DP), 0);
        final float radius = AndroidUtilities.dp(CORNER_RADIUS_DP);
        content.setOutlineProvider(new ViewOutlineProvider() {
            @Override
            public void getOutline(View view, Outline outline) {
                outline.setRoundRect(0, 0, view.getWidth(), view.getHeight(), radius);
            }
        });
        content.setClipToOutline(true);
        content.addOnLayoutChangeListener((v, l, t, r, b, ol, ot, or, ob) -> {
            if (r - l != or - ol || b - t != ob - ot) v.invalidateOutline();
        });
        int chipGravity = Gravity.TOP | (LocaleController.isRTL ? Gravity.LEFT : Gravity.RIGHT);
        addView(content, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT, CHIP_HEIGHT_DP, chipGravity,
                LocaleController.isRTL ? STACK_INSET_DP : 0, 5,
                LocaleController.isRTL ? 0 : STACK_INSET_DP, 7));

        iconView = new ImageView(context);
        iconView.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
        LinearLayout.LayoutParams iconParams = LayoutHelper.createLinear(ICON_SIZE_DP, ICON_SIZE_DP,
                Gravity.CENTER_VERTICAL);
        iconParams.setMarginEnd(AndroidUtilities.dp(ICON_GAP_DP));
        content.addView(iconView, iconParams);

        textView = new ChipTextView(context, true, true, true);
        textView.adaptWidth = true;
        textView.setTextSize(AndroidUtilities.dp(13));
        textView.setTypeface(AndroidUtilities.bold());
        textView.setIncludeFontPadding(false);
        textView.setTextColor(currentContentColor());
        textView.setGravity(Gravity.CENTER_VERTICAL);
        textView.setAnimationProperties(0f, 0, RESIZE_DURATION_MS, RESIZE_INTERPOLATOR);
        textView.setScaleProperty(RESIZE_TEXT_SCALE_POP);
        textView.setOnWidthUpdatedListener(this::onAnimatedTextWidthUpdated);
        textView.setText("", false, false);
        content.addView(textView, LayoutHelper.createLinear(LayoutHelper.WRAP_CONTENT, LayoutHelper.MATCH_PARENT));

        configurePressAnimation();

        setOnClickListener(v -> onCardClicked());
        setOnLongClickListener(v -> onCardLongClicked());
    }

    @Override
    public void setPressed(boolean pressed) {
        if (loading) pressed = false;
        super.setPressed(pressed);
        if (content != null) {
            if (!FGCardsStrip.shouldAnimate(getContext())) {
                content.setStateListAnimator(null);
                content.setScaleX(1f);
                content.setScaleY(1f);
            } else if (content.getStateListAnimator() == null) {
                configurePressAnimation();
            }
            content.setPressed(pressed);
        }
    }

    private void configurePressAnimation() {
        AnimatorSet press = new AnimatorSet();
        press.playTogether(ObjectAnimator.ofFloat(content, View.SCALE_X, 0.97f),
                ObjectAnimator.ofFloat(content, View.SCALE_Y, 0.97f));
        press.setDuration(100);
        press.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
        AnimatorSet release = new AnimatorSet();
        release.playTogether(ObjectAnimator.ofFloat(content, View.SCALE_X, 1f),
                ObjectAnimator.ofFloat(content, View.SCALE_Y, 1f));
        release.setDuration(160);
        release.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
        StateListAnimator states = new StateListAnimator();
        states.addState(new int[]{android.R.attr.state_pressed}, press);
        states.addState(new int[0], release);
        content.setStateListAnimator(states);
    }

    public void setCardLayerType(int layerType) {
        if (getLayerType() != layerType) {
            setLayerType(layerType, null);
        }
    }

    public void setCardColors(int top, int bottom) {
        brandTop = top;
        brandBottom = bottom;
        applyColorMode();
    }

    public void setCardAccessibilityLabel(CharSequence label) {
        accessibilityLabel = label;
        updateAccessibilityDescription();
    }

    private void updateAccessibilityDescription() {
        if (android.text.TextUtils.isEmpty(accessibilityLabel)) {
            setContentDescription(accessibilityValue);
        } else if (android.text.TextUtils.isEmpty(accessibilityValue)) {
            setContentDescription(accessibilityLabel);
        } else {
            setContentDescription(accessibilityLabel + ": " + accessibilityValue);
        }
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);
        info.setClassName("android.widget.Button");
        info.setClickable(true);
    }

    protected void setIcon(int resId) {
        boolean iconChanged = resId != lastIconRes;
        iconVisible = true;
        lastIconRes = resId;
        iconView.setVisibility(View.VISIBLE);
        applyMaxChipWidth();
        iconView.setImageResource(resId);
        iconView.setColorFilter(currentContentColor());
        if (iconChanged && getVisibility() == VISIBLE && getTranslationX() == 0f && getTranslationY() == 0f) {
            content.requestLayout();
        }
    }

    protected boolean isBranded() {
        return false;
    }

    private boolean isFlat() {
        return colorMode == FGCardsConfig.COLOR_MODE_THEME || !isBranded();
    }

    protected final int currentContentColor() {
        return resolvedForeground != 0 ? resolvedForeground
                : Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider);
    }

    protected void applyColorMode() {
        colorMode = FGCardsConfig.getColorMode();
        boolean flat = isFlat();
        int stroke = Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider);
        int top, bottom;
        if (colorMode == FGCardsConfig.COLOR_MODE_PALETTE) {
            boolean dark = ColorUtils.calculateLuminance(
                    Theme.getColor(Theme.key_windowBackgroundWhite, resourcesProvider)) < 0.5;
            FGCardColors palette = FGCardColors.paletteForCard(FGCardsConfig.getPalette(), getCardId(),
                    FGCardsConfig.getSaturation(), dark, stroke);
            top = palette.top;
            bottom = palette.bottom;
        } else if (flat) {
            int text = Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider);
            int fill = opaqueFlat
                    ? Theme.blendOver(Theme.getColor(Theme.key_windowBackgroundWhite, resourcesProvider), Theme.multAlpha(text, 0.09f))
                    : Theme.multAlpha(text, 0.09f);
            int ovFill = fillColorOverride();
            if (ovFill != 0) fill = ovFill;
            int base = Theme.getColor(Theme.key_windowBackgroundWhite, resourcesProvider);
            int accent = Theme.getColor(Theme.key_dialogButton, resourcesProvider);
            int glass = Theme.blendOver(base, fill);
            top = ColorUtils.blendARGB(glass, accent, 0.08f);
            bottom = glass;
        } else {
            int base = Theme.getColor(Theme.key_windowBackgroundWhite, resourcesProvider);
            int surface = Theme.blendOver(base, Theme.multAlpha(stroke, 0.08f));
            top = ColorUtils.blendARGB(surface, brandTop, 0.18f);
            bottom = ColorUtils.blendARGB(surface, brandBottom, 0.09f);
        }
        content.setElevation(0f);
        int fg = stroke;
        int ov = contentColorOverride();
        if (ov != 0) fg = ov;
        FGCardColors colors = FGCardColors.resolve(top, bottom, fg, FGCardsConfig.getBrightness(),
                colorMode == FGCardsConfig.COLOR_MODE_PALETTE ? FGCardColors.DEFAULT_SATURATION : FGCardsConfig.getSaturation());
        resolvedForeground = fg = colors.foreground;
        background.setColors(colors.top, colors.bottom, fg);
        textView.setTextColor(fg);
        int base = Theme.getColor(Theme.key_windowBackgroundWhite, resourcesProvider);
        int tint = colorMode == FGCardsConfig.COLOR_MODE_PALETTE ? colors.top
                : flat ? Theme.getColor(Theme.key_dialogButton, resourcesProvider) : brandTop;
        backFill.setColor(ColorUtils.blendARGB(base, tint, 0.10f));
        backStroke.setColor(Theme.multAlpha(fg, 0.15f));
        invalidate();
        if (iconVisible) iconView.setColorFilter(fg);
        onColorsApplied(fg);
        updateLoadingColors();
        content.invalidateOutline();
        content.invalidate();
    }

    public void setOpaqueFlat(boolean v) {
        if (opaqueFlat == v) return;
        opaqueFlat = v;
        applyColorMode();
    }
    public void setInlineFolderStyle(boolean inline) {
        if (inlineFolderStyle == inline) return;
        inlineFolderStyle = inline;
        textView.setTextSize(AndroidUtilities.dp(13));
        requestLayout();
    }

    protected int contentColorOverride() {
        return 0;
    }

    protected void onColorsApplied(int foreground) {
    }

    protected int fillColorOverride() {
        return 0;
    }

    protected void setIconVisible(boolean visible) {
        if (iconVisible == visible) return;
        iconVisible = visible;
        iconView.setVisibility(visible ? View.VISIBLE : View.GONE);
        applyMaxChipWidth();
    }

    protected void setText(CharSequence text, boolean animated) {
        accessibilityValue = text;
        updateAccessibilityDescription();
        animated &= !renderingInstantly;
        boolean changed = !android.text.TextUtils.equals(textView.getText(), text);
        if (!changed) {
            return;
        }
        boolean canAnimate = animated
                && FGCardsStrip.shouldAnimate(getContext())
                && isAttachedToWindow()
                && getWindowVisibility() == View.VISIBLE;

        boolean visibleResting = getVisibility() == VISIBLE && isShown()
                && getTranslationX() == 0f && getTranslationY() == 0f
                && getAlpha() > 0.999f
                && Math.abs(getScaleX() - 1f) < 0.001f
                && Math.abs(getScaleY() - 1f) < 0.001f
                && (!(getParent() instanceof FGCardsStrip)
                    || ((FGCardsStrip) getParent()).canAnimateCardResize());

        if (!visibleResting || !canAnimate) {
            textView.setText(text, false, false);
            if (getVisibility() == VISIBLE) {
                content.requestLayout();
                content.invalidateOutline();
            }
            return;
        }

        textView.setText(text, true);
    }

    /** Ширина, под которую в последний раз пересчитывали раскладку. */
    private int lastLaidOutTextWidth = -1;

    /** Держим максимальную ширину старого и нового значения до конца перехода. */
    private void onAnimatedTextWidthUpdated() {
        if (!isAttachedToWindow() || getWindowVisibility() != View.VISIBLE
                || getVisibility() != VISIBLE || !isShown()) {
            return;
        }
        if (getParent() instanceof FGCardsStrip
                && ((FGCardsStrip) getParent()).isLayoutSuppressed()) {
            return;
        }
        final int width = (int) Math.ceil(textView.getDrawable().getWidth());
        if (width == lastLaidOutTextWidth) {
            invalidate();
            return;
        }
        lastLaidOutTextWidth = width;
        textView.requestLayout();
        if (getVisibility() == VISIBLE) {
            content.requestLayout();
            content.invalidateOutline();
        }
        invalidate();
    }

    void finishResizeAnimation() {
        if (textView.isAnimating()) {
            textView.cancelAnimation();
        }
        lastLaidOutTextWidth = -1;
        if (accessibilityValue != null) {
            textView.setText(accessibilityValue, false, false);
        }
        content.requestLayout();
        content.invalidateOutline();
    }

    void updateLayoutDirection() {
        android.widget.FrameLayout.LayoutParams lp =
                (android.widget.FrameLayout.LayoutParams) content.getLayoutParams();
        int gravity = Gravity.TOP | (LocaleController.isRTL ? Gravity.LEFT : Gravity.RIGHT);
        if (lp != null) {
            lp.gravity = gravity;
            lp.leftMargin = AndroidUtilities.dp(LocaleController.isRTL ? STACK_INSET_DP : 0);
            lp.rightMargin = AndroidUtilities.dp(LocaleController.isRTL ? 0 : STACK_INSET_DP);
            content.setLayoutParams(lp);
        }
    }

    void updateDataInstantly() {
        boolean previous = renderingInstantly;
        renderingInstantly = true;
        try {
            onUpdateData(false);
        } finally {
            renderingInstantly = previous;
        }
    }

    public void setMaxChipWidth(int maxTextWidth) {
        maxChipWidth = Math.max(0, maxTextWidth);
        applyMaxChipWidth();
    }

    private void applyMaxChipWidth() {
        int chrome = content.getPaddingLeft() + content.getPaddingRight() + AndroidUtilities.dp(STACK_INSET_DP);
        if (iconVisible) chrome += AndroidUtilities.dp(ICON_SIZE_DP + ICON_GAP_DP); 
        int textMax = maxChipWidth > 0 ? Math.max(1, maxChipWidth - chrome) : 0;
        if (appliedTextMaxWidth == textMax) return;
        appliedTextMaxWidth = textMax;
        textView.setMaxWidth(textMax);

        // Затухание к краю искажает последние цифры — «$77,240.00» читается как размытое
        // пятно вместо суммы. Пилюля и так растёт по своему тексту, а на узком экране
        // лучше честно обрезать хвост, чем показывать его нечитаемым.
        textView.setEllipsizeByGradient(false);
    }

    protected void setTextColor(int color) {
        textView.setTextColor(currentContentColor());
    }

    public abstract int getCardId();

    public abstract long getRefreshInterval();

    public abstract void onUpdateData(boolean force);

    public abstract void onCardClicked();

    public abstract boolean onCardLongClicked();

    public abstract void updateColors();

    public void onCardSelected() {
        onUpdateData(false);
    }

    public void onCardUnselected() {
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        updateColors();
        applyColorMode();
        if (isRefreshDue()) {
            onUpdateData(false);
        }
        scheduleNext();
    }

    @Override
    protected void onDetachedFromWindow() {
        AndroidUtilities.cancelRunOnUIThread(stopLoadingByTimeout);
        finishResizeAnimation();
        super.onDetachedFromWindow();
        AndroidUtilities.cancelRunOnUIThread(autoRefresh);
    }

    private void scheduleNext() {
        AndroidUtilities.cancelRunOnUIThread(autoRefresh);
        long iv = getRefreshInterval();
        if (iv > 0) {
            AndroidUtilities.runOnUIThread(autoRefresh, iv);
        }
    }

    protected void markDataUpdated() {
        lastUpdateMs = System.currentTimeMillis();
    }

    protected boolean isRefreshDue() {
        long iv = getRefreshInterval();
        return iv <= 0 || System.currentTimeMillis() - lastUpdateMs >= iv;
    }

    /**
     * Сколько ждать данных, показывая блик.
     *
     * Дальше гасим: если за это время ответа нет, его, скорее всего, не будет вовсе —
     * сеть недоступна или источник молчит. Вечный блик выглядит как «сейчас-сейчас», а на
     * деле просто не даёт погаснуть потоку отрисовки и греет телефон.
     */
    private static final long LOADING_TIMEOUT_MS = 12_000L;

    private final Runnable stopLoadingByTimeout = this::stopLoading;

    public void startLoading() {
        AndroidUtilities.cancelRunOnUIThread(stopLoadingByTimeout);
        AndroidUtilities.runOnUIThread(stopLoadingByTimeout, LOADING_TIMEOUT_MS);
        loading = true;
        if (loadingDrawable == null) {
            loadingDrawable = new LoadingDrawable(resourcesProvider);
            loadingDrawable.setCallback(this);
            loadingDrawable.setGradientScale(2.0f);
            loadingDrawable.setRadiiDp(CORNER_RADIUS_DP);
            updateLoadingColors();
        }
        loadingDrawable.reset();
        loadingDrawable.resetDisappear();
        loadingDrawable.setAlpha(255);
        invalidate();
    }

    public void stopLoading() {
        AndroidUtilities.cancelRunOnUIThread(stopLoadingByTimeout);
        loading = false;
        if (loadingDrawable != null) {
            loadingDrawable.disappear();
            invalidate();
        }
    }

    public boolean isLoading() {
        return loading;
    }

    protected void updateLoadingColors() {
        if (loadingDrawable != null) {
            int color = currentContentColor();
            loadingDrawable.setColors(
                    Theme.multAlpha(color, 0.1f),
                    Theme.multAlpha(color, 0.3f));
        }
    }

    @Override
    protected void dispatchDraw(@NonNull Canvas canvas) {
        drawStackLayers(canvas);
        super.dispatchDraw(canvas);
        LoadingDrawable ld = loadingDrawable;
        if (ld != null && (loading || ld.isDisappearing()) && !ld.isDisappeared()) {
            // Блик просит следующий кадр сам за себя, и пока он жив, поток отрисовки не
            // засыпает. Для невидимой карточки это чистая трата: полоса могла уехать за
            // край или погаснуть, а телефон продолжал бы греться ради того, чего не видно.
            if (!isShown() || getAlpha() <= 0.01f) {
                return;
            }
            loadingRect.set(content.getLeft(), content.getTop(), content.getRight(), content.getBottom());
            if (!FGCardsStrip.shouldAnimate(getContext())) {
                if (loading) {
                    staticLoadingPaint.setColor(Theme.multAlpha(currentContentColor(), 0.1f));
                    canvas.drawRoundRect(loadingRect, AndroidUtilities.dp(CORNER_RADIUS_DP),
                            AndroidUtilities.dp(CORNER_RADIUS_DP), staticLoadingPaint);
                }
                return;
            }
            ld.setBounds(loadingRect);
            ld.draw(canvas);
            invalidate();
        }
    }

    void setStackDepth(float depth) {
        depth = Math.max(0f, Math.min(1f, depth));
        if (stackDepth == depth) return;
        stackDepth = depth;
        invalidate();
    }

    private void drawStackLayers(Canvas canvas) {
        if (content.getWidth() <= 0 || stackDepth <= 0f) return;
        float direction = LocaleController.isRTL ? -1f : 1f;
        float radius = AndroidUtilities.dp(CORNER_RADIUS_DP);
        for (int layer = 2; layer >= 1; layer--) {
            float x = direction * AndroidUtilities.dp(2 * layer);
            float y = AndroidUtilities.dp(layer);
            backRect.set(content.getLeft() + x, content.getTop() + y,
                    content.getRight() + x, content.getBottom() + y - AndroidUtilities.dp(2));
            int alpha = Math.round(255 * stackDepth * (layer == 1 ? 0.72f : 0.42f));
            backFill.setAlpha(alpha);
            backStroke.setAlpha(Math.round(alpha * 0.22f));
            canvas.drawRoundRect(backRect, radius, radius, backFill);
            canvas.drawRoundRect(backRect, radius, radius, backStroke);
        }
    }

    @Override
    protected boolean verifyDrawable(@NonNull Drawable who) {
        return who == loadingDrawable || super.verifyDrawable(who);
    }

    private static final class ChipTextView extends AnimatedTextView {
        ChipTextView(android.content.Context c, boolean splitByWords, boolean preserveIndex, boolean startFromEnd) {
            super(c, splitByWords, preserveIndex, startFromEnd);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            if (adaptWidth && View.MeasureSpec.getMode(widthMeasureSpec) == View.MeasureSpec.AT_MOST) {
                int want = getPaddingLeft() + (int) Math.ceil(getDrawable().getWidth()) + getPaddingRight();
                int avail = View.MeasureSpec.getSize(widthMeasureSpec);
                setMeasuredDimension(Math.min(want, avail), getMeasuredHeight());
            }
        }
    }

    private static final class CardBackground extends Drawable {
        private final Paint fillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint strokePaint = new Paint(Paint.ANTI_ALIAS_FLAG);

        CardBackground(int top, int bottom, int stroke) {
            strokePaint.setStyle(Paint.Style.STROKE);
            strokePaint.setStrokeWidth(AndroidUtilities.dp(1));
            setColors(top, bottom, stroke);
        }

        void setColors(int top, int bottom, int stroke) {
            float h = AndroidUtilities.dp(CHIP_HEIGHT_DP);
            fillPaint.setShader(new LinearGradient(0, 0, 0, h,
                    new int[]{top, bottom}, new float[]{0f, 1f}, Shader.TileMode.CLAMP));
            strokePaint.setShader(new LinearGradient(0, 0, 0, h,
                    new int[]{Theme.multAlpha(stroke, 0.20f), 0x00000000, Theme.multAlpha(stroke, 0.08f)}, new float[]{0f, 0.5f, 1f},
                    Shader.TileMode.CLAMP));
            invalidateSelf();
        }

        @Override
        public void draw(Canvas canvas) {
            Rect bounds = getBounds();
            float r = AndroidUtilities.dp(CORNER_RADIUS_DP);
            RectF rf = AndroidUtilities.rectTmp;
            rf.set(bounds);
            canvas.drawRoundRect(rf, r, r, fillPaint);
            float sw = AndroidUtilities.dp(1);
            strokePaint.setStrokeWidth(sw);
            float half = sw / 2f;
            rf.inset(half, half);
            canvas.drawRoundRect(rf, r, r, strokePaint);
        }

        @Override
        public void setAlpha(int alpha) {
            fillPaint.setAlpha(alpha);
            strokePaint.setAlpha(alpha);
        }

        @Override
        public void setColorFilter(ColorFilter colorFilter) {
            fillPaint.setColorFilter(colorFilter);
            strokePaint.setColorFilter(colorFilter);
        }

        @Override
        public int getOpacity() {
            return android.graphics.PixelFormat.TRANSLUCENT;
        }
    }
}

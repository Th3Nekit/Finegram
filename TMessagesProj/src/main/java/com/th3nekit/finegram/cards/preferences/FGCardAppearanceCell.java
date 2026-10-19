/* Licensed under GNU GPL v. 2 or later. */

package com.th3nekit.finegram.cards.preferences;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.text.Layout;
import android.view.Gravity;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.core.graphics.ColorUtils;

import com.th3nekit.finegram.cards.FGCard;
import com.th3nekit.finegram.cards.FGCardColors;
import com.th3nekit.finegram.cards.FGCardsConfig;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.SeekBarView;

public final class FGCardAppearanceCell extends LinearLayout implements NotificationCenter.NotificationCenterDelegate {
    private static final int[] PALETTE_NAMES = {
            R.string.FG_Cards_PaletteViolet, R.string.FG_Cards_PaletteBlue,
            R.string.FG_Cards_PaletteTeal, R.string.FG_Cards_PaletteGreen,
            R.string.FG_Cards_PaletteLime, R.string.FG_Cards_PaletteOrangeCocoa,
            R.string.FG_Cards_PaletteCoral, R.string.FG_Cards_PaletteRose
    };

    private final Theme.ResourcesProvider resourcesProvider;
    private final TextView[] modes = new TextView[2];
    private final PaletteChoice[] choices = new PaletteChoice[FGCardColors.presetCount()];
    private final TextView brightnessValue;
    private final TextView brightnessLabel;
    private final TextView saturationValue;
    private final TextView saturationLabel;
    private final TextView paletteLabel;
    private final TextView previewLabel;
    private final TextView reset;
    private final LinearLayout previewHeader;
    private final SeekBarView brightness;
    private final SeekBarView saturation;
    private final FrameLayout preview;
    private final PreviewCard ratePreview;
    private final PreviewCard weatherPreview;

    public FGCardAppearanceCell(Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;
        setOrientation(VERTICAL);
        setPadding(AndroidUtilities.dp(16), AndroidUtilities.dp(4), AndroidUtilities.dp(16), AndroidUtilities.dp(12));

        previewHeader = new LinearLayout(context);
        previewHeader.setGravity(Gravity.CENTER_VERTICAL);
        previewHeader.setMinimumHeight(AndroidUtilities.dp(44));
        previewLabel = label(R.string.FG_Cards_ColorPreview, 13);
        LinearLayout.LayoutParams previewLabelParams = LayoutHelper.createLinear(0, -2, 1f);
        previewLabelParams.setMarginEnd(AndroidUtilities.dp(8));
        previewHeader.addView(previewLabel, previewLabelParams);
        reset = label(R.string.Reset, 14);
        reset.setGravity(Gravity.CENTER);
        reset.setMinimumWidth(AndroidUtilities.dp(88));
        reset.setMinimumHeight(AndroidUtilities.dp(44));
        reset.setPadding(AndroidUtilities.dp(8), AndroidUtilities.dp(8), AndroidUtilities.dp(8), AndroidUtilities.dp(8));
        reset.setOnClickListener(v -> {
            FGCardsConfig.resetColors();
            refresh();
        });
        previewHeader.addView(reset, LayoutHelper.createLinear(-2, -2, Gravity.END | Gravity.CENTER_VERTICAL));
        addView(previewHeader, LayoutHelper.createLinear(-1, -2));

        preview = new FrameLayout(context);
        LinearLayout samples = new LinearLayout(context);
        samples.setGravity(Gravity.CENTER);
        ratePreview = new PreviewCard(context, resourcesProvider, 4, R.drawable.pill_usd, "$83.73", 0xff115a39, 0xff0e4a30);
        weatherPreview = new PreviewCard(context, resourcesProvider, 1, R.drawable.pill_weather, "24°", 0xff377896, 0xff2d586f);
        LinearLayout.LayoutParams sampleParams = LayoutHelper.createLinear(-2, 44, Gravity.CENTER_VERTICAL);
        sampleParams.setMarginEnd(AndroidUtilities.dp(12));
        samples.addView(ratePreview, sampleParams);
        samples.addView(weatherPreview, LayoutHelper.createLinear(-2, 44));
        preview.addView(samples, LayoutHelper.createFrame(-1, -1, Gravity.CENTER));
        addView(preview, LayoutHelper.createLinear(-1, 56, 0, 0, 0, 12));

        LinearLayout modeRow = new LinearLayout(context);
        for (int i = 0; i < modes.length; i++) {
            final int mode = i;
            TextView option = modes[i] = label(i == 0 ? R.string.FG_Cards_OriginalColors : R.string.FG_Cards_ThemeColors, 14);
            option.setGravity(Gravity.CENTER);
            option.setMinimumHeight(AndroidUtilities.dp(44));
            option.setPadding(AndroidUtilities.dp(6), AndroidUtilities.dp(8), AndroidUtilities.dp(6), AndroidUtilities.dp(8));
            option.setOnClickListener(v -> {
                FGCardsConfig.setColorMode(mode);
                refresh();
            });
            LinearLayout.LayoutParams modeParams = LayoutHelper.createLinear(0, -1, 1f);
            modeParams.setMarginEnd(AndroidUtilities.dp(i == 0 ? 8 : 0));
            modeRow.addView(option, modeParams);
        }
        addView(modeRow, LayoutHelper.createLinear(-1, -2));
        paletteLabel = label(R.string.FG_Cards_MD3Palette, 13);
        paletteLabel.setMinimumHeight(AndroidUtilities.dp(36));
        addView(paletteLabel, LayoutHelper.createLinear(-1, -2));

        for (int row = 0; row < 4; row++) {
            LinearLayout paletteRow = new LinearLayout(context);
            for (int column = 0; column < 2; column++) {
                int index = row * 2 + column;
                PaletteChoice option = choices[index] = new PaletteChoice(context, index);
                LinearLayout.LayoutParams paletteParams = LayoutHelper.createLinear(0, -1, 1f);
                paletteParams.setMarginEnd(AndroidUtilities.dp(column == 0 ? 6 : 0));
                paletteRow.addView(option, paletteParams);
            }
            addView(paletteRow, LayoutHelper.createLinear(-1, -2));
        }

        LinearLayout brightnessHeader = new LinearLayout(context);
        brightnessHeader.setGravity(Gravity.CENTER_VERTICAL);
        brightnessHeader.setMinimumHeight(AndroidUtilities.dp(40));
        brightnessLabel = label(R.string.FG_Cards_Brightness, 15);
        brightnessHeader.addView(brightnessLabel, LayoutHelper.createLinear(0, -2, 1f));
        brightnessValue = label(0, 14);
        brightnessValue.setGravity(Gravity.CENTER_VERTICAL | Gravity.END);
        LinearLayout.LayoutParams brightnessValueParams = LayoutHelper.createLinear(-2, -2);
        brightnessValueParams.setMarginStart(AndroidUtilities.dp(8));
        brightnessHeader.addView(brightnessValue, brightnessValueParams);
        addView(brightnessHeader, LayoutHelper.createLinear(-1, -2));

        brightness = new SeekBarView(context, resourcesProvider);
        brightness.setReportChanges(true);
        brightness.setContentDescription(LocaleController.getString(R.string.FG_Cards_Brightness));
        brightness.setDelegate(new SeekBarView.SeekBarViewDelegate() {
            @Override
            public void onSeekBarDrag(boolean stop, float progress) {
                FGCardsConfig.setBrightness(FGCardColors.MIN_BRIGHTNESS + Math.round(progress *
                        (FGCardColors.MAX_BRIGHTNESS - FGCardColors.MIN_BRIGHTNESS)));
                refreshPreview();
            }

            @Override
            public CharSequence getContentDescription() {
                return LocaleController.getString(R.string.FG_Cards_Brightness) + ", " + FGCardsConfig.getBrightness() + "%";
            }

            @Override
            public void onSeekBarPressed(boolean pressed) {
                if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(pressed);
            }
        });
        addView(brightness, LayoutHelper.createLinear(-1, 44));

        LinearLayout saturationHeader = new LinearLayout(context);
        saturationHeader.setGravity(Gravity.CENTER_VERTICAL);
        saturationHeader.setMinimumHeight(AndroidUtilities.dp(40));
        saturationLabel = label(R.string.FG_Cards_Saturation, 15);
        saturationHeader.addView(saturationLabel, LayoutHelper.createLinear(0, -2, 1f));
        saturationValue = label(0, 14);
        saturationValue.setGravity(Gravity.CENTER_VERTICAL | Gravity.END);
        LinearLayout.LayoutParams saturationValueParams = LayoutHelper.createLinear(-2, -2);
        saturationValueParams.setMarginStart(AndroidUtilities.dp(8));
        saturationHeader.addView(saturationValue, saturationValueParams);
        addView(saturationHeader, LayoutHelper.createLinear(-1, -2));

        saturation = new SeekBarView(context, resourcesProvider);
        saturation.setReportChanges(true);
        saturation.setContentDescription(LocaleController.getString(R.string.FG_Cards_Saturation));
        saturation.setDelegate(new SeekBarView.SeekBarViewDelegate() {
            @Override
            public void onSeekBarDrag(boolean stop, float progress) {
                FGCardsConfig.setSaturation(FGCardColors.MIN_SATURATION + Math.round(progress *
                        (FGCardColors.MAX_SATURATION - FGCardColors.MIN_SATURATION)));
                refreshPreview();
            }

            @Override
            public CharSequence getContentDescription() {
                return LocaleController.getString(R.string.FG_Cards_Saturation) + ", " + FGCardsConfig.getSaturation() + "%";
            }

            @Override
            public void onSeekBarPressed(boolean pressed) {
                if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(pressed);
            }
        });
        addView(saturation, LayoutHelper.createLinear(-1, 44));
        refresh();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int availableWidth = MeasureSpec.getSize(widthMeasureSpec) - getPaddingLeft() - getPaddingRight()
                - previewHeader.getPaddingLeft() - previewHeader.getPaddingRight();
        float titleWidth = Layout.getDesiredWidth(previewLabel.getText(), previewLabel.getPaint())
                + previewLabel.getCompoundPaddingLeft() + previewLabel.getCompoundPaddingRight();
        float resetWidth = Math.max(reset.getMinimumWidth(), Layout.getDesiredWidth(reset.getText(), reset.getPaint())
                + reset.getCompoundPaddingLeft() + reset.getCompoundPaddingRight());
        boolean stacked = MeasureSpec.getMode(widthMeasureSpec) != MeasureSpec.UNSPECIFIED
                && Math.ceil(titleWidth) + Math.ceil(resetWidth) + AndroidUtilities.dp(8) > availableWidth;
        int orientation = stacked ? VERTICAL : HORIZONTAL;
        if (previewHeader.getOrientation() != orientation) {
            previewHeader.setOrientation(orientation);
            LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) previewLabel.getLayoutParams();
            params.width = stacked ? LayoutParams.MATCH_PARENT : 0;
            params.weight = stacked ? 0 : 1;
            params.setMarginEnd(stacked ? 0 : AndroidUtilities.dp(8));
            params.bottomMargin = stacked ? AndroidUtilities.dp(4) : 0;
            previewLabel.setLayoutParams(params);
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    private TextView label(int string, int size) {
        TextView text = new TextView(getContext());
        if (string != 0) text.setText(LocaleController.getString(string));
        text.setTextSize(size);
        text.setGravity(Gravity.CENTER_VERTICAL | Gravity.START);
        text.setFontFeatureSettings("tnum");
        return text;
    }

    private void refreshPreview() {
        brightnessValue.setText(FGCardsConfig.getBrightness() + "%");
        saturationValue.setText(FGCardsConfig.getSaturation() + "%");
        for (PaletteChoice choice : choices) {
            if (choice != null) choice.invalidateSwatch();
        }
        ratePreview.updateColors();
        weatherPreview.updateColors();
    }

    public void refresh() {
        int text = Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider);
        int secondary = Theme.getColor(Theme.key_windowBackgroundWhiteGrayText, resourcesProvider);
        int accent = Theme.getColor(Theme.key_dialogButton, resourcesProvider);
        int base = Theme.getColor(Theme.key_windowBackgroundWhite, resourcesProvider);
        previewLabel.setTextColor(secondary);
        paletteLabel.setTextColor(secondary);
        brightnessLabel.setTextColor(text);
        brightnessValue.setTextColor(accent);
        saturationLabel.setTextColor(text);
        saturationValue.setTextColor(accent);
        reset.setTextColor(accent);
        reset.setBackground(Theme.createSimpleSelectorRoundRectDrawable(AndroidUtilities.dp(12), 0, Theme.multAlpha(accent, .12f)));
        preview.setBackground(Theme.createRoundRectDrawable(AndroidUtilities.dp(16), ColorUtils.blendARGB(base, text, .04f)));
        for (int i = 0; i < modes.length; i++) {
            boolean selected = FGCardsConfig.getColorMode() == i;
            modes[i].setSelected(selected);
            modes[i].setTextColor(selected ? accent : text);
            modes[i].setBackground(Theme.createSimpleSelectorRoundRectDrawable(AndroidUtilities.dp(12),
                    Theme.multAlpha(selected ? accent : text, selected ? .14f : .05f), Theme.multAlpha(accent, .20f)));
        }
        for (PaletteChoice choice : choices) choice.refresh();
        brightness.setColors(Theme.multAlpha(accent, .18f), accent);
        brightness.setProgress((FGCardsConfig.getBrightness() - FGCardColors.MIN_BRIGHTNESS)
                / (float) (FGCardColors.MAX_BRIGHTNESS - FGCardColors.MIN_BRIGHTNESS));
        saturation.setColors(Theme.multAlpha(accent, .18f), accent);
        saturation.setProgress((FGCardsConfig.getSaturation() - FGCardColors.MIN_SATURATION)
                / (float) (FGCardColors.MAX_SATURATION - FGCardColors.MIN_SATURATION));
        refreshPreview();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        NotificationCenter.getGlobalInstance().addObserver(this, NotificationCenter.didSetNewTheme);
        refresh();
    }

    @Override
    protected void onDetachedFromWindow() {
        NotificationCenter.getGlobalInstance().removeObserver(this, NotificationCenter.didSetNewTheme);
        if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(false);
        super.onDetachedFromWindow();
    }

    @Override
    public void didReceivedNotification(int id, int account, Object... args) {
        refresh();
    }

    private final class PaletteChoice extends LinearLayout {
        private final TextView title;
        private final int index;
        private final View swatch;

        PaletteChoice(Context context, int index) {
            super(context);
            this.index = index;
            setOrientation(VERTICAL);
            setGravity(Gravity.CENTER);
            setMinimumHeight(AndroidUtilities.dp(72));
            swatch = new View(context) {
                private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
                private final Path selection = new Path();

                @Override
                protected void onDraw(Canvas canvas) {
                    float cy = getHeight() / 2f;
                    float diameter = AndroidUtilities.dp(9);
                    float gap = AndroidUtilities.dp(3);
                    float groupWidth = diameter * 6 + gap * 5;
                    float start = (getWidth() - groupWidth) / 2f;
                    for (int cardId = 1; cardId <= 6; cardId++) {
                        paint.setStyle(Paint.Style.FILL);
                        paint.setColor(FGCardColors.saturationColor(index, cardId, FGCardsConfig.getSaturation(),
                                ColorUtils.calculateLuminance(Theme.getColor(Theme.key_windowBackgroundWhite, resourcesProvider)) < .5));
                        canvas.drawCircle(start + (cardId - 1) * (diameter + gap) + diameter / 2f, cy, diameter / 2f, paint);
                    }
                    if (PaletteChoice.this.isSelected()) {
                        float inset = AndroidUtilities.dp(2);
                        selection.reset();
                        selection.addRoundRect(inset, inset, getWidth() - inset, getHeight() - inset,
                                AndroidUtilities.dp(12), AndroidUtilities.dp(12), Path.Direction.CW);
                        paint.setColor(Theme.getColor(Theme.key_dialogButton, resourcesProvider));
                        paint.setStyle(Paint.Style.STROKE);
                        paint.setStrokeWidth(AndroidUtilities.dp(1.5f));
                        canvas.drawPath(selection, paint);
                    }
                }
            };
            swatch.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
            addView(swatch, LayoutHelper.createLinear(-1, 28));
            title = label(PALETTE_NAMES[index], 12);
            title.setGravity(Gravity.CENTER);
            title.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
            title.setSingleLine(false);
            title.setTextSize(11);
            title.setIncludeFontPadding(false);
            title.setMinimumHeight(AndroidUtilities.dp(36));
            addView(title, LayoutHelper.createLinear(-1, -2, 2, 2, 2, 2));
            setContentDescription(LocaleController.getString(PALETTE_NAMES[index]));
            setOnClickListener(v -> {
                FGCardsConfig.setPalette(index);
                FGCardAppearanceCell.this.refresh();
            });
        }

        void refresh() {
            int accent = Theme.getColor(Theme.key_dialogButton, resourcesProvider);
            boolean selected = FGCardsConfig.getColorMode() == FGCardsConfig.COLOR_MODE_PALETTE && FGCardsConfig.getPalette() == index;
            setSelected(selected);
            title.setTextColor(selected ? accent : Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider));
            setBackground(Theme.createSimpleSelectorRoundRectDrawable(AndroidUtilities.dp(12),
                    selected ? Theme.multAlpha(accent, .10f) : 0, Theme.multAlpha(accent, .16f)));
            swatch.invalidate();
        }

        void invalidateSwatch() {
            swatch.invalidate();
        }

        @Override
        public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
            super.onInitializeAccessibilityNodeInfo(info);
            info.setClassName("android.widget.RadioButton");
            info.setCheckable(true);
            info.setChecked(isSelected());
        }
    }

    private static final class PreviewCard extends FGCard {
        private final int cardId;

        PreviewCard(Context context, Theme.ResourcesProvider provider, int cardId, int icon, String value, int top, int bottom) {
            super(context, provider);
            this.cardId = cardId;
            setIcon(icon);
            setText(value, false);
            setCardColors(top, bottom);
            setClickable(false);
            setLongClickable(false);
            setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        }

        @Override public int getCardId() { return cardId; }
        @Override public long getRefreshInterval() { return 0; }
        @Override public void onUpdateData(boolean force) { }
        @Override public void onCardClicked() { }
        @Override public boolean onCardLongClicked() { return false; }
        @Override protected boolean isBranded() { return true; }
        @Override public void updateColors() { applyColorMode(); }
    }
}

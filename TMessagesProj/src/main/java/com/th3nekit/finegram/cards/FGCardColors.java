/* Licensed under GNU GPL v. 2 or later. */

package com.th3nekit.finegram.cards;

import androidx.core.graphics.ColorUtils;

import com.google.android.material.color.utilities.Hct;
import com.google.android.material.color.utilities.SchemeTonalSpot;
import com.google.android.material.color.utilities.TonalPalette;

public final class FGCardColors {
    public static final int MIN_BRIGHTNESS = 50;
    public static final int MAX_BRIGHTNESS = 150;
    public static final int DEFAULT_BRIGHTNESS = 100;
    public static final int MIN_SATURATION = 0;
    public static final int MAX_SATURATION = 200;
    public static final int DEFAULT_SATURATION = 100;

    private static final int[] SEEDS = {
            0xff6750a4, 0xff386693, 0xff006a6a, 0xff386a20,
            0xff879000, 0xffd05f00, 0xffb84b3f, 0xff984061
    };
    private static final SchemeTonalSpot[] SCHEMES = new SchemeTonalSpot[SEEDS.length];

    static {
        for (int i = 0; i < SEEDS.length; i++) {
            SCHEMES[i] = new SchemeTonalSpot(Hct.fromInt(SEEDS[i]), false, 0);
        }
    }

    public final int top;
    public final int bottom;
    public final int foreground;

    private FGCardColors(int top, int bottom, int foreground) {
        this.top = top;
        this.bottom = bottom;
        this.foreground = foreground;
    }

    public static int presetCount() {
        return SEEDS.length;
    }

    public static int clampPreset(int preset) {
        return preset >= 0 && preset < SEEDS.length ? preset : 0;
    }

    public static int clampBrightness(int brightness) {
        return Math.max(MIN_BRIGHTNESS, Math.min(MAX_BRIGHTNESS, brightness));
    }

    public static int clampSaturation(int saturation) {
        return Math.max(MIN_SATURATION, Math.min(MAX_SATURATION, saturation));
    }

    public static int seed(int preset) {
        return SEEDS[clampPreset(preset)];
    }

    private static TonalPalette cardPalette(int preset, int cardId, int saturation) {
        SchemeTonalSpot scheme = SCHEMES[clampPreset(preset)];
        int role = cardRole(cardId);
        TonalPalette palette = role == 0 ? scheme.primaryPalette
                : role == 1 ? scheme.secondaryPalette : scheme.tertiaryPalette;
        int value = clampSaturation(saturation);
        return value == DEFAULT_SATURATION ? palette
                : TonalPalette.fromHueAndChroma(palette.getHue(), palette.getChroma() * value / 100.0);
    }

    private static int cardRole(int cardId) {
        return cardId >= 1 && cardId <= 6 ? (cardId - 1) % 3 : 0;
    }

    public static int saturationColor(int preset, int cardId, int saturation, boolean dark) {
        int tone = dark ? (cardId <= 3 ? 68 : 56) - 2 * cardRole(cardId)
                : (cardId <= 3 ? 48 : 60) + 2 * cardRole(cardId);
        return cardPalette(preset, cardId, saturation).tone(tone);
    }

    public static FGCardColors paletteForCard(int preset, int cardId, int saturation,
                                               boolean dark, int preferredText) {
        TonalPalette palette = cardPalette(preset, cardId, saturation);
        // Separate tones also distinguish roles when sRGB clips their chroma or saturation is zero.
        int tone = dark ? (cardId <= 3 ? 30 : 24) + 2 * cardRole(cardId)
                : (cardId <= 3 ? 92 : 86) - 2 * cardRole(cardId);
        return new FGCardColors(palette.tone(tone), palette.tone(tone - 4), preferredText);
    }

    public static FGCardColors palette(int preset, int brightness, boolean dark, int preferredText) {
        TonalPalette palette = TonalPalette.fromInt(seed(preset));
        return resolve(palette.tone(dark ? 30 : 92), palette.tone(dark ? 26 : 88),
                preferredText, brightness);
    }

    public static FGCardColors resolve(int top, int bottom, int preferredText, int brightness) {
        top = changeTone(top | 0xff000000, brightness);
        bottom = changeTone(bottom | 0xff000000, brightness);
        preferredText |= 0xff000000;
        if (contrast(preferredText, top, bottom) >= 4.5) {
            return new FGCardColors(top, bottom, preferredText);
        }
        int foreground = contrast(0xff000000, top, bottom) >= contrast(0xffffffff, top, bottom)
                ? 0xff000000 : 0xffffffff;
        int target = foreground == 0xff000000 ? 0xffffffff : 0xff000000;
        // A gradient must remain readable at both ends, including the middle tones.
        while (contrast(foreground, top, bottom) < 4.5) {
            top = ColorUtils.blendARGB(top, target, 0.025f);
            bottom = ColorUtils.blendARGB(bottom, target, 0.025f);
        }
        return new FGCardColors(top, bottom, foreground);
    }

    public static FGCardColors resolve(int top, int bottom, int preferredText, int brightness, int saturation) {
        return resolve(changeSaturation(top, saturation), changeSaturation(bottom, saturation), preferredText, brightness);
    }

    private static int changeSaturation(int color, int saturation) {
        int value = clampSaturation(saturation);
        if (value == DEFAULT_SATURATION) return color;
        Hct hct = Hct.fromInt(color);
        return Hct.from(hct.getHue(), hct.getChroma() * value / 100.0, hct.getTone()).toInt();
    }

    private static int changeTone(int color, int brightness) {
        int value = clampBrightness(brightness);
        if (value == DEFAULT_BRIGHTNESS) return color;
        Hct hct = Hct.fromInt(color);
        return Hct.from(hct.getHue(), hct.getChroma(),
                Math.max(0, Math.min(100, hct.getTone() + (value - DEFAULT_BRIGHTNESS) * 0.3))).toInt();
    }

    private static double contrast(int foreground, int top, int bottom) {
        double min = Double.MAX_VALUE;
        for (int i = 0; i <= 8; i++) {
            min = Math.min(min, ColorUtils.calculateContrast(foreground,
                    ColorUtils.blendARGB(top, bottom, i / 8f)));
        }
        return min;
    }
}

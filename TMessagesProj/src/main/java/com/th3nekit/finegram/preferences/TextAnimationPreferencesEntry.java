/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.preferences;

import static org.telegram.messenger.LocaleController.getString;

import android.view.View;

import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;

import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class TextAnimationPreferencesEntry extends BaseCGPreferencesEntry {

    private final int appearRow = 1;
    private final int cursorRow = 2;
    private final int particlesRow = 3;

    private final int appearDurationRow = 4;
    private final int appearSlideRow = 5;
    private final int appearBlurRow = 6;
    private final int cursorSpeedRow = 7;
    private final int particleCountRow = 8;
    private final int particleSpeedRow = 9;
    private final int particleSizeRow = 10;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.FG_TextAnim);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        items.add(UItem.asTopViewStatic(getString(R.string.FG_TextAnim_Desc), R.drawable.formatting_bold));

        items.add(UItem.asHeader(getString(R.string.FG_TextAnim_Appear)));
        items.add(SettingsHelper.asSwitchCG(appearRow, getString(R.string.FG_ComposerAppear),
                        getString(R.string.FG_ComposerAppear_Desc))
                .setChecked(FinegramAppearanceConfig.INSTANCE.getComposerAppear()));
        if (FinegramAppearanceConfig.INSTANCE.getComposerAppear()) {
            items.add(UItem.asIntSlideView(0, 80, FinegramAppearanceConfig.INSTANCE.getComposerAppearDuration(), 800,
                    (where, value) -> where != 0
                            ? String.valueOf(value)
                            : LocaleController.formatString(R.string.FG_TextAnim_Duration, value),
                    FinegramAppearanceConfig.INSTANCE::setComposerAppearDuration).setId(appearDurationRow));
            items.add(UItem.asIntSlideView(0, 0, FinegramAppearanceConfig.INSTANCE.getComposerAppearSlide(), 40,
                    (where, value) -> {
                        if (where != 0) {
                            return String.valueOf(value);
                        }
                        return value == 0
                                ? getString(R.string.FG_TextAnim_Slide_Off)
                                : LocaleController.formatString(R.string.FG_TextAnim_Slide, value);
                    },
                    FinegramAppearanceConfig.INSTANCE::setComposerAppearSlide).setId(appearSlideRow));
            items.add(UItem.asIntSlideView(0, 0, FinegramAppearanceConfig.INSTANCE.getComposerAppearBlur(), 30,
                    (where, value) -> {
                        if (where != 0) {
                            return String.valueOf(value);
                        }
                        return value == 0
                                ? getString(R.string.FG_TextAnim_Blur_Off)
                                : LocaleController.formatString(R.string.FG_TextAnim_Blur, value);
                    },
                    FinegramAppearanceConfig.INSTANCE::setComposerAppearBlur).setId(appearBlurRow));
        }
        items.add(UItem.asShadow(getString(R.string.FG_TextAnim_Appear_Hint)));

        items.add(UItem.asHeader(getString(R.string.FG_TextAnim_Cursor)));
        items.add(SettingsHelper.asSwitchCG(cursorRow, getString(R.string.FG_ComposerCursorGlide),
                        getString(R.string.FG_ComposerCursorGlide_Desc))
                .setChecked(FinegramAppearanceConfig.INSTANCE.getComposerCursorGlide()));
        if (FinegramAppearanceConfig.INSTANCE.getComposerCursorGlide()) {
            items.add(UItem.asIntSlideView(0, 5, FinegramAppearanceConfig.INSTANCE.getComposerCursorSpeed(), 60,
                    (where, value) -> where != 0
                            ? String.valueOf(value)
                            : LocaleController.formatString(R.string.FG_TextAnim_Speed, value),
                    FinegramAppearanceConfig.INSTANCE::setComposerCursorSpeed).setId(cursorSpeedRow));
        }
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.FG_TextAnim_Erase)));
        items.add(SettingsHelper.asSwitchCG(particlesRow, getString(R.string.FG_ComposerEffects),
                        getString(R.string.FG_ComposerEffects_Desc))
                .setChecked(FinegramAppearanceConfig.INSTANCE.getComposerEffects()));
        if (FinegramAppearanceConfig.INSTANCE.getComposerEffects()) {
            items.add(UItem.asIntSlideView(0, 1, FinegramAppearanceConfig.INSTANCE.getComposerParticleCount(), 24,
                    (where, value) -> where != 0
                            ? String.valueOf(value)
                            : LocaleController.formatString(R.string.FG_TextAnim_Particles, value),
                    FinegramAppearanceConfig.INSTANCE::setComposerParticleCount).setId(particleCountRow));
            items.add(UItem.asIntSlideView(0, 20, FinegramAppearanceConfig.INSTANCE.getComposerParticleSpeed(), 300,
                    (where, value) -> where != 0
                            ? value + "%"
                            : LocaleController.formatString(R.string.FG_TextAnim_ParticleSpeed, value),
                    FinegramAppearanceConfig.INSTANCE::setComposerParticleSpeed).setId(particleSpeedRow));
            items.add(UItem.asIntSlideView(0, 20, FinegramAppearanceConfig.INSTANCE.getComposerParticleSize(), 300,
                    (where, value) -> where != 0
                            ? value + "%"
                            : LocaleController.formatString(R.string.FG_TextAnim_ParticleSize, value),
                    FinegramAppearanceConfig.INSTANCE::setComposerParticleSize).setId(particleSizeRow));
        }
        items.add(UItem.asShadow(getString(R.string.FG_TextAnim_Credit)));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == appearRow) {
            FinegramAppearanceConfig.INSTANCE.setComposerAppear(!FinegramAppearanceConfig.INSTANCE.getComposerAppear());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getComposerAppear());
            updateRows(true);
        } else if (item.id == cursorRow) {
            FinegramAppearanceConfig.INSTANCE.setComposerCursorGlide(!FinegramAppearanceConfig.INSTANCE.getComposerCursorGlide());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getComposerCursorGlide());
            updateRows(true);
        } else if (item.id == particlesRow) {
            FinegramAppearanceConfig.INSTANCE.setComposerEffects(!FinegramAppearanceConfig.INSTANCE.getComposerEffects());
            SettingsHelper.updateCheckState(view, FinegramAppearanceConfig.INSTANCE.getComposerEffects());
            updateRows(true);
        }
    }
}

/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.preferences;

import static org.telegram.messenger.LocaleController.getString;

import android.view.View;

import org.telegram.messenger.R;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;
import java.util.Arrays;

import com.th3nekit.finegram.core.configs.FinegramPlayerConfig;
import com.th3nekit.finegram.helpers.ui.PopupHelper;
import com.th3nekit.finegram.preferences.cells.PlayerPreviewCell;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

public class MusicPlayerPreferencesEntry extends BaseCGPreferencesEntry {

    private final int fullscreenRow = 1;
    private final int coverRow = 2;
    private final int breathRow = 3;
    private final int backgroundRow = 4;
    private final int accentRow = 5;
    private final int waveRow = 6;
    private final int miniGlassRow = 7;
    private final int miniTintRow = 8;
    private final int cardRow = 9;
    private final int roundCoverRow = 10;
    private final int progressRow = 11;
    private final int equalizerRow = 12;
    private final int miniCoverRow = 13;
    private final int miniNextRow = 14;

    private PlayerPreviewCell preview;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.FG_Player_Title);
    }

    private static String[] coverNames() {
        return new String[]{getString(R.string.FG_Player_Cover_Flat), getString(R.string.FG_Player_Cover_Tilt), getString(R.string.FG_Player_Cover_Vinyl)};
    }

    private static String[] backgroundNames() {
        return new String[]{getString(R.string.FG_Player_Background_Artwork), getString(R.string.FG_Player_Background_Live), getString(R.string.FG_Player_Background_Theme)};
    }

    private static String[] accentNames() {
        return new String[]{getString(R.string.FG_Player_Accent_Artwork), getString(R.string.FG_Player_Accent_Theme)};
    }

    private static String[] cardNames() {
        return new String[]{getString(R.string.FG_Player_Card_Artwork), getString(R.string.FG_Player_Card_Vivid), getString(R.string.FG_Player_Card_Glass), getString(R.string.FG_Player_Card_Plain), getString(R.string.FG_Player_Card_Stock), getString(R.string.FG_Player_Card_Capsule)};
    }

    private static String[] progressNames() {
        return new String[]{getString(R.string.FG_Player_Progress_Line), getString(R.string.FG_Player_Progress_Wave), getString(R.string.FG_Player_Progress_None)};
    }

    private static String pick(String[] names, int index) {
        return names[Math.max(0, Math.min(names.length - 1, index))];
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        if (preview == null) {
            preview = new PlayerPreviewCell(getContext(), getResourceProvider());
        }
        items.add(UItem.asCustom(preview));
        items.add(UItem.asShadow(null));

        FinegramPlayerConfig config = FinegramPlayerConfig.INSTANCE;
        items.add(UItem.asHeader(getString(R.string.FG_Player_Screen)));
        items.add(SettingsHelper.asSwitchCG(fullscreenRow, getString(R.string.FG_Player_Fullscreen),
                getString(R.string.FG_Player_Fullscreen_Desc)).setChecked(config.getFullscreenPlayer()));
        if (config.getFullscreenPlayer()) {
            items.add(UItem.asButton(coverRow, getString(R.string.FG_Player_Cover), pick(coverNames(), config.getCoverStyle())));
            items.add(UItem.asButton(backgroundRow, getString(R.string.FG_Player_Background), pick(backgroundNames(), config.getBackground())));
            items.add(UItem.asButton(accentRow, getString(R.string.FG_Player_Accent), pick(accentNames(), config.getAccent())));
            items.add(SettingsHelper.asSwitchCG(breathRow, getString(R.string.FG_Player_CoverBreath),
                    getString(R.string.FG_Player_CoverBreath_Desc)).setChecked(config.getCoverBreath()));
        }
        items.add(SettingsHelper.asSwitchCG(waveRow, getString(R.string.FG_Player_Wave),
                getString(R.string.FG_Player_Wave_Desc)).setChecked(config.getWavyProgress()));
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.FG_Player_Mini)));
        items.add(SettingsHelper.asSwitchCG(miniCoverRow, getString(R.string.FG_Player_MiniCover),
                getString(R.string.FG_Player_MiniCover_Desc)).setChecked(config.getMiniPlayerShowCover()));
        items.add(SettingsHelper.asSwitchCG(miniNextRow, getString(R.string.FG_Player_MiniNext),
                getString(R.string.FG_Player_MiniNext_Desc)).setChecked(config.getMiniPlayerNextTrack()));
        items.add(SettingsHelper.asSwitchCG(miniGlassRow, getString(R.string.FG_Player_MiniGlass),
                getString(R.string.FG_Player_MiniGlass_Desc)).setChecked(config.getMiniPlayerGlass()));
        items.add(SettingsHelper.asSwitchCG(miniTintRow, getString(R.string.FG_Player_MiniTint),
                getString(R.string.FG_Player_MiniTint_Desc)).setChecked(config.getMiniPlayerArtworkTint()));
        items.add(UItem.asShadow(null));

        items.add(UItem.asHeader(getString(R.string.FG_Player_Profile)));
        items.add(UItem.asButton(cardRow, getString(R.string.FG_Player_ProfileCard), pick(cardNames(), config.getProfileCardStyle())));
        if (!FinegramPlayerConfig.stockProfileCard()) {
            items.add(UItem.asButton(progressRow, getString(R.string.FG_Player_ProfileProgress), pick(progressNames(), config.getProfileProgress())));
            if (!FinegramPlayerConfig.capsuleProfileCard()) {
                items.add(SettingsHelper.asSwitchCG(roundCoverRow, getString(R.string.FG_Player_ProfileRound),
                        getString(R.string.FG_Player_ProfileRound_Desc)).setChecked(config.getProfileRoundCover()));
                items.add(SettingsHelper.asSwitchCG(equalizerRow, getString(R.string.FG_Player_Equalizer),
                        getString(R.string.FG_Player_Equalizer_Desc)).setChecked(config.getProfileEqualizer()));
            }
        }
        items.add(UItem.asShadow(getString(R.string.FG_Player_Profile_Hint)));
    }

    private void choose(View view, String title, String[] names, int current, java.util.function.IntConsumer apply) {
        PopupHelper.show(new ArrayList<>(Arrays.asList(names)), title, current, getContext(), index -> {
            apply.accept(index);
            SettingsHelper.updateButtonValue(view, pick(names, index));
            if (preview != null) preview.refresh();
        });
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id < fullscreenRow || item.id > miniNextRow) return;
        FinegramPlayerConfig config = FinegramPlayerConfig.INSTANCE;
        if (preview != null) preview.showMode(item.id >= cardRow && item.id <= equalizerRow ? PlayerPreviewCell.PROFILE
                : item.id == miniGlassRow || item.id == miniTintRow || item.id == miniCoverRow || item.id == miniNextRow
                ? PlayerPreviewCell.MINI : PlayerPreviewCell.FULL);
        if (item.id == fullscreenRow) {
            config.setFullscreenPlayer(!config.getFullscreenPlayer());
            SettingsHelper.updateCheckState(view, config.getFullscreenPlayer());
            updateRows(true);
        } else if (item.id == coverRow) {
            choose(view, getString(R.string.FG_Player_Cover), coverNames(), config.getCoverStyle(), config::setCoverStyle);
        } else if (item.id == backgroundRow) {
            choose(view, getString(R.string.FG_Player_Background), backgroundNames(), config.getBackground(), config::setBackground);
        } else if (item.id == accentRow) {
            choose(view, getString(R.string.FG_Player_Accent), accentNames(), config.getAccent(), config::setAccent);
        } else if (item.id == cardRow) {
            choose(view, getString(R.string.FG_Player_ProfileCard), cardNames(), config.getProfileCardStyle(), index -> {
                config.setProfileCardStyle(index);
                updateRows(true);
            });
        } else if (item.id == progressRow) {
            choose(view, getString(R.string.FG_Player_ProfileProgress), progressNames(), config.getProfileProgress(), config::setProfileProgress);
        } else if (item.id == breathRow) {
            config.setCoverBreath(!config.getCoverBreath());
            SettingsHelper.updateCheckState(view, config.getCoverBreath());
        } else if (item.id == waveRow) {
            config.setWavyProgress(!config.getWavyProgress());
            SettingsHelper.updateCheckState(view, config.getWavyProgress());
        } else if (item.id == miniCoverRow) {
            config.setMiniPlayerShowCover(!config.getMiniPlayerShowCover());
            SettingsHelper.updateCheckState(view, config.getMiniPlayerShowCover());
        } else if (item.id == miniNextRow) {
            config.setMiniPlayerNextTrack(!config.getMiniPlayerNextTrack());
            SettingsHelper.updateCheckState(view, config.getMiniPlayerNextTrack());
        } else if (item.id == miniGlassRow) {
            config.setMiniPlayerGlass(!config.getMiniPlayerGlass());
            SettingsHelper.updateCheckState(view, config.getMiniPlayerGlass());
        } else if (item.id == miniTintRow) {
            config.setMiniPlayerArtworkTint(!config.getMiniPlayerArtworkTint());
            SettingsHelper.updateCheckState(view, config.getMiniPlayerArtworkTint());
        } else if (item.id == roundCoverRow) {
            config.setProfileRoundCover(!config.getProfileRoundCover());
            SettingsHelper.updateCheckState(view, config.getProfileRoundCover());
        } else if (item.id == equalizerRow) {
            config.setProfileEqualizer(!config.getProfileEqualizer());
            SettingsHelper.updateCheckState(view, config.getProfileEqualizer());
        }
        if (preview != null) preview.refresh();
    }
}

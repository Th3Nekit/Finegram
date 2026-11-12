package com.th3nekit.finegram.appearance;

import android.graphics.Color;

import androidx.core.graphics.ColorUtils;

import com.th3nekit.finegram.core.configs.FinegramChatsConfig;

import org.telegram.ui.ActionBar.Theme;

public final class FGPeerGlass {
    public interface ColorProvider {
        int getPeerGlassColor();
    }

    private FGPeerGlass() {
    }

    public static int tint(int color, Theme.ResourcesProvider resourcesProvider) {
        if (!FinegramChatsConfig.INSTANCE.getGlassPeerTint() || !(resourcesProvider instanceof ColorProvider provider)) return color;
        int accent = provider.getPeerGlassColor();
        if (Color.alpha(accent) == 0) return color;
        int blended = ColorUtils.blendARGB(color, accent, 0.18f);
        return ColorUtils.setAlphaComponent(blended, Color.alpha(color));
    }
}

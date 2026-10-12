package org.telegram.ui;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;

import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.R;

public class LauncherIconController {
    public static void tryFixLauncherIconIfNeeded() {
        for (LauncherIcon icon : LauncherIcon.values()) {
            if (isEnabled(icon)) {

                if (icon.replacement != null) {
                    setIcon(icon.replacement);
                }
                return;
            }
        }

        setIcon(LauncherIcon.DEFAULT_ICON);
    }

    public static boolean isEnabled(LauncherIcon icon) {
        Context ctx = ApplicationLoader.applicationContext;
        int i = ctx.getPackageManager().getComponentEnabledSetting(icon.getComponentName(ctx));
        return i == PackageManager.COMPONENT_ENABLED_STATE_ENABLED || i == PackageManager.COMPONENT_ENABLED_STATE_DEFAULT && icon == LauncherIcon.DEFAULT_ICON;
    }

    public static void setIcon(LauncherIcon icon) {
        Context ctx = ApplicationLoader.applicationContext;
        PackageManager pm = ctx.getPackageManager();
        for (LauncherIcon i : LauncherIcon.values()) {
            pm.setComponentEnabledSetting(i.getComponentName(ctx), i == icon ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED :
                    PackageManager.COMPONENT_ENABLED_STATE_DISABLED, PackageManager.DONT_KILL_APP);
        }
    }

    public enum LauncherIcon {
        DEFAULT_ICON("FG_Icon_Default", R.drawable.icon_background_laguna, R.drawable.fg_plane_laguna, R.string.Default),
        DARK("FG_Icon_Dark", R.drawable.icon_background_dark, R.drawable.fg_plane_dark, R.string.AP_ChangeIcon_Dark),
        DARK_ALT("FG_Icon_Dark_Bra", DARK),
        WHITE("FG_Icon_White", R.drawable.icon_background_white, R.drawable.fg_plane_white, R.string.AP_ChangeIcon_White),
        WHITE_ALT("FG_Icon_White_Alt", WHITE),
        LAGUNA("FG_Icon_Laguna", DEFAULT_ICON),
        AQUA("FG_Icon_Aqua", R.drawable.icon_background_aqua, R.drawable.fg_plane_aqua, R.string.AppIconAqua),
        GREEN("FG_Icon_Green", R.drawable.icon_background_green, R.drawable.fg_plane_green, R.string.AP_ChangeIcon_Green),
        LAVANDA("FG_Icon_Lavanda", R.drawable.icon_background_lavanda, R.drawable.fg_plane_lavanda, R.string.AP_ChangeIcon_Lavanda),
        VIOLET_SUNSET("FG_Icon_Violet_Sunset", R.drawable.icon_background_violet_sunset, R.drawable.fg_plane_violet_sunset, R.string.AP_ChangeIcon_Violet_Sunset),
        VIOLET_SUNSET_ALT("FG_Icon_Violet_Sunset_Bra", VIOLET_SUNSET),
        SUNSET("FG_Icon_Sunset", R.drawable.icon_background_sunset, R.drawable.fg_plane_sunset, R.string.AP_ChangeIcon_Sunset),
        SUNRISE("FG_Icon_Sunrise", R.drawable.icon_background_sunrise, R.drawable.fg_plane_sunrise, R.string.AP_ChangeIcon_Sunrise),
        TURBO_ALT("FG_Icon_Turbo", R.drawable.icon_5_background_sa, R.drawable.fg_plane_turbo, R.string.AppIconTurbo),
        NOX_ALT("FG_Icon_Night", R.mipmap.icon_2_background_sa, R.drawable.fg_plane_nox, R.string.AppIconNox),
        NIGHT_SKY("FG_Icon_NightSky", R.mipmap.icon_background_night_sky, R.drawable.fg_plane_night_sky, R.string.FG_AppIcon_NightSky),
        AURORA("FG_Icon_Aurora", R.mipmap.icon_background_aurora, R.drawable.fg_plane_aurora, R.string.FG_AppIcon_Aurora),
        KRAFT("FG_Icon_Kraft", R.mipmap.icon_background_kraft, R.drawable.fg_plane_kraft, R.string.FG_AppIcon_Kraft),
        PIXELS("FG_Icon_Pixels", R.mipmap.icon_background_pixels, R.drawable.fg_plane_pixels, R.string.FG_AppIcon_Pixels),
        HALFTONE("FG_Icon_Halftone", R.mipmap.icon_background_halftone, R.drawable.fg_plane_halftone, R.string.FG_AppIcon_Halftone),
        DEPTH("FG_Icon_Depth", R.mipmap.icon_background_depth, R.drawable.fg_plane_depth, R.string.FG_AppIcon_Depth),
        TOPOGRAPHY("FG_Icon_Topography", R.mipmap.icon_background_topography, R.drawable.fg_plane_topography, R.string.FG_AppIcon_Topography),
        MOUNTAINS("FG_Icon_Mountains", R.mipmap.icon_background_mountains, R.drawable.fg_plane_mountains, R.string.FG_AppIcon_Mountains),
        NEBULA("FG_Icon_Nebula", R.mipmap.icon_background_nebula, R.drawable.fg_plane_nebula, R.string.FG_AppIcon_Nebula),
        ULTRAVIOLET("FG_Icon_Ultraviolet", R.drawable.icon_background_ultraviolet, R.drawable.fg_plane_ultraviolet, R.string.FG_AppIcon_Ultraviolet),
        BAUHAUS("FG_Icon_Bauhaus", R.mipmap.icon_background_bauhaus, R.drawable.fg_plane_bauhaus, R.string.FG_AppIcon_Bauhaus),
        MEMPHIS("FG_Icon_Memphis", R.mipmap.icon_background_memphis, R.drawable.fg_plane_memphis, R.string.FG_AppIcon_Memphis),
        LOW_POLY("FG_Icon_LowPoly", R.mipmap.icon_background_low_poly, R.drawable.fg_plane_low_poly, R.string.FG_AppIcon_LowPoly),
        PAPER_LAYERS("FG_Icon_PaperLayers", R.mipmap.icon_background_paper_layers, R.drawable.fg_plane_paper_layers, R.string.FG_AppIcon_PaperLayers),
        MALACHITE("FG_Icon_Malachite", R.mipmap.icon_background_malachite, R.drawable.fg_plane_malachite, R.string.FG_AppIcon_Malachite),
        DARK_NY("FG_Icon_Dark_NY", R.drawable.icon_background_dark_ny, R.drawable.fg_plane_dark_ny, R.string.AP_ChangeIcon_NewYear),

        PREMIUM("PremiumIcon", R.drawable.icon_3_background_sa, R.drawable.fg_plane_premium, R.string.AppIconPremium, true),
        TURBO("TurboIcon", TURBO_ALT),
        NOX("NoxIcon", NOX_ALT);

        public final String key;
        public final int background;
        public final int foreground;
        public final int title;
        public final boolean premium;

        public final LauncherIcon replacement;

        private ComponentName componentName;

        public ComponentName getComponentName(Context ctx) {
            if (componentName == null) {
                componentName = new ComponentName(ctx.getPackageName(), "com.th3nekit.finegram." + key);
            }
            return componentName;
        }

        LauncherIcon(String key, int background, int foreground, int title) {
            this(key, background, foreground, title, false);
        }

        LauncherIcon(String key, int background, int foreground, int title, boolean premium) {
            this.key = key;
            this.background = background;
            this.foreground = foreground;
            this.title = title;
            this.premium = premium;
            this.replacement = null;
        }

        LauncherIcon(String key, LauncherIcon replacement) {
            this.key = key;
            this.background = replacement.background;
            this.foreground = replacement.foreground;
            this.title = replacement.title;
            this.premium = replacement.premium;
            this.replacement = replacement;
        }
    }
}

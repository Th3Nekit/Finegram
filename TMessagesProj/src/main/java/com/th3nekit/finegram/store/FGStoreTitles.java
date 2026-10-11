/*
 * Finegram plugin store.
 * Adapted from Kangel Plugins Manager (KangelPlugins).
 * Upstream: https://git.kangel.xyz/KangelPlugins/PluginManager
 * Licensed under GNU GPL v3; see LICENSE.PluginManager and NOTICE.
 */

package com.th3nekit.finegram.store;

import static org.telegram.messenger.LocaleController.getString;

import org.telegram.messenger.R;

public final class FGStoreTitles {

    private FGStoreTitles() {
    }

    public static String of(FGStore.Category category) {
        switch (category) {
            case CUSTOMIZATION: return getString(R.string.FG_Store_Cat_Customization);
            case MESSAGES: return getString(R.string.FG_Store_Cat_Messages);
            case UTILITIES: return getString(R.string.FG_Store_Cat_Utilities);
            case INFORMATIONAL: return getString(R.string.FG_Store_Cat_Informational);
            case FUN: return getString(R.string.FG_Store_Cat_Fun);
            case LIBRARY: return getString(R.string.FG_Store_Cat_Library);
            case ICONS: return getString(R.string.FG_Store_Icons);
            default: return getString(R.string.FG_Store_Cat_Other);
        }
    }
}

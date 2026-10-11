/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins.api;

public interface FinegramDexPlugin {

    String getId();

    String getName();

    String getVersion();

    String getAuthor();

    String getDescription();

    void onLoad(PluginHost host);

    void onUnload();
}

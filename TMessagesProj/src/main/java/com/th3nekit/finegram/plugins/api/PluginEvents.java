/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins.api;

public interface PluginEvents {

    default boolean onSendRequest(int account, String requestName, Object request) {
        return false;
    }

    default void onRequestResponse(int account, String requestName, Object response, Object error) {
    }

    default void onUpdate(int account, String updateName, Object update) {
    }

    default boolean onSendMessage(int account, Object params) {
        return false;
    }
}

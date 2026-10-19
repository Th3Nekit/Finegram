/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins.api;

/**
 * События клиента, на которые может подписаться плагин.
 *
 * Имена запросов и обновлений — короткие имена классов TL, например
 * {@code "TL_messages_readHistory"} или {@code "TL_updateUserTyping"}.
 */
public interface PluginEvents {

    /**
     * Клиент собирается отправить запрос. Вернуть true — запрос не уйдёт.
     * Сам объект запроса можно менять на месте.
     */
    default boolean onSendRequest(int account, String requestName, Object request) {
        return false;
    }

    /** Пришёл ответ на запрос; [error] не null, если сервер ответил ошибкой. */
    default void onRequestResponse(int account, String requestName, Object response, Object error) {
    }

    /** Пришло обновление от сервера. */
    default void onUpdate(int account, String updateName, Object update) {
    }

    /** Клиент собирается отправить сообщение. Вернуть true — сообщение не уйдёт. */
    default boolean onSendMessage(int account, Object params) {
        return false;
    }
}

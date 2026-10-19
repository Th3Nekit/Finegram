/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins.api;

/**
 * Обработчик перехвата метода клиента.
 *
 * До вызова можно поменять аргументы или подменить результат — тогда оригинальный метод
 * не выполнится. После вызова доступен настоящий результат.
 */
public interface PluginMethodHook {

    /** Вызывается перед методом. Изменения в [call] влияют на сам вызов. */
    default void before(PluginMethodCall call) {
    }

    /** Вызывается после метода. */
    default void after(PluginMethodCall call) {
    }
}

/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins.api;

/** Один перехваченный вызов: кто вызвал, с чем и что вернёт. */
public interface PluginMethodCall {

    /** Объект, у которого вызвали метод; null у статических методов. */
    Object thisObject();

    /** Аргументы вызова. Их можно менять до вызова оригинала. */
    Object[] args();

    /** Значение, которое вернёт метод. */
    Object result();

    /**
     * Подменяет результат. Если вызвать это до метода, оригинал не выполнится —
     * так плагин отменяет действие клиента.
     */
    void setResult(Object value);
}

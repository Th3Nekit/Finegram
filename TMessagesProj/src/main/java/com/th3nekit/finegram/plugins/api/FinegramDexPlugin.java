/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins.api;

/**
 * Плагин, собранный в dex.
 *
 * Такой плагин пишется на Java или Kotlin и компилируется в обычный dex-файл: он работает
 * с теми же скоростями, что и сам клиент, и ему доступны все классы Telegram напрямую.
 * Питоновские плагины удобнее для быстрых правок, dex — для того, что должно работать
 * в горячих местах.
 *
 * От плагина нужен один класс, реализующий этот интерфейс. Клиент найдёт его сам:
 * ни регистрировать, ни называть особым образом ничего не нужно.
 */
public interface FinegramDexPlugin {

    /** Постоянный опознаватель плагина, например {@code "my-plugin"}. По нему хранятся настройки. */
    String getId();

    /** Название, которое увидит пользователь. */
    String getName();

    /** Версия в свободной форме, например {@code "1.0.0"}. */
    String getVersion();

    /** Автор: имя или ссылка вида {@code "@nickname"}. */
    String getAuthor();

    /** Одно-два предложения о том, что плагин делает. */
    String getDescription();

    /**
     * Плагин включили. Всё, что он делает с клиентом, идёт через [host].
     * Исключение отсюда не уронит клиент, но плагин останется выключенным.
     */
    void onLoad(PluginHost host);

    /** Плагин выключают. Перехваты снимаются сами, здесь нужно отпустить своё. */
    void onUnload();
}

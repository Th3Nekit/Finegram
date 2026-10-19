/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins.api;

import android.content.Context;

import java.lang.reflect.Member;

/**
 * Всё, что dex-плагин может попросить у клиента.
 *
 * Объект выдаётся плагину при включении и живёт, пока плагин включён. Перехваты и
 * подписки, поставленные через него, снимаются сами при выключении плагина — вручную
 * прибирать за собой не нужно.
 */
public interface PluginHost {

    /** Опознаватель плагина, которому выдан этот доступ. */
    String id();

    /** Контекст приложения: ресурсы, файлы, системные службы. */
    Context context();

    /** Пишет строку в журнал клиента с пометкой плагина. */
    void log(String message);

    /** Показывает всплывающее сообщение: [kind] — {@code "info"}, {@code "success"} или {@code "error"}. */
    void showBulletin(String text, String kind);

    /** Выполняет код в потоке интерфейса. */
    void runOnUiThread(Runnable action);

    /** Выполняет код в фоновой очереди клиента. */
    void runInBackground(Runnable action);

    // --- настройки плагина ---

    boolean getBoolean(String key, boolean fallback);

    void setBoolean(String key, boolean value);

    int getInt(String key, int fallback);

    void setInt(String key, int value);

    String getString(String key, String fallback);

    void setString(String key, String value);

    // --- перехват методов клиента ---

    /**
     * Перехватывает метод клиента. Возвращает пропуск, которым перехват снимают,
     * или null, если перехват не встал.
     */
    Object hook(Member method, PluginMethodHook hook);

    /** Снимает один перехват по пропуску, полученному от {@link #hook}. */
    void unhook(Object token);

    /** Ищет класс клиента по полному имени; null, если такого нет. */
    Class<?> findClass(String className);

    /** Читает закрытое поле объекта или класса. */
    Object getField(Object target, String fieldName);

    /** Записывает закрытое поле объекта или класса. */
    void setField(Object target, String fieldName, Object value);

    // --- события клиента ---

    /** Подписывает плагин на запросы, ответы, обновления и отправку сообщений. */
    void subscribe(PluginEvents events);

    /** Отписывает плагин от событий. */
    void unsubscribe();
}

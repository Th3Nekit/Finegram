/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.utils.ui;

/**
 * Кисть для текста чата.
 *
 * Обычная {@link android.text.TextPaint}, но своего типа — и это здесь главное. Плагины
 * собраны под клиент, где поля вроде {@code Theme.chat_timePaint} объявлены именно этим
 * классом, а виртуальная машина ищет поле по имени вместе с типом. Объяви мы его как
 * {@code android.text.TextPaint}, и плагин падал бы с {@code NoSuchFieldError} ещё до
 * первой отрисовки, хотя значение подошло бы.
 *
 * Поведения своего нет и не нужно: всё, что от класса требуется, — существовать под этим
 * именем и быть кистью.
 */
public class TextPaint extends android.text.TextPaint {

    public TextPaint() {
        super();
    }

    public TextPaint(int flags) {
        super(flags);
    }

    public TextPaint(android.graphics.Paint paint) {
        super(paint);
    }
}

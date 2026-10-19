/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.privacy;

import android.text.TextUtils;

import org.telegram.messenger.LocaleController;
import org.telegram.messenger.MessageObject;
import org.telegram.messenger.R;

/**
 * Пометка «удалено» на сохранённом сообщении.
 *
 * Без неё сохранение выглядит не функцией, а поломкой: собеседник удалил, а сообщение
 * висит, и непонятно почему. Хуже того, можно решить, что у него не получилось, и ответить
 * на то, что он хотел забрать назад.
 *
 * Приписываем слово к времени — ровно так же, как клиент помечает изменённые и
 * заблокированные сообщения. Своей отрисовки не заводим нарочно: разметка времени уже
 * умеет считать ширину и переносить, а новая рисовалка означала бы свои ошибки в чужом
 * месте.
 */
public final class FGDeletedMark {

    private FGDeletedMark() {
    }

    /** Время с пометкой, если сообщение удалено; иначе то же самое время. */
    public static CharSequence decorate(MessageObject message, CharSequence time) {
        try {
            if (message == null || message.messageOwner == null) return time;
            if (!FGDeletedStore.isDeleted(message.currentAccount, message.getDialogId(), message.getId())) return time;
            final String mark = LocaleController.getString(R.string.FG_Saved_Mark);
            if (TextUtils.isEmpty(time)) return mark;
            return mark + ", " + time;
        } catch (Throwable t) {
            return time;
        }
    }
}

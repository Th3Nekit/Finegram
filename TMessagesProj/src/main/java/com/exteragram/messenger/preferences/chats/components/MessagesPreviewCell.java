/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.preferences.chats.components;

import android.content.Context;

import org.telegram.ui.ActionBar.INavigationLayout;
import org.telegram.ui.Cells.ThemePreviewMessagesCell;

/**
 * Пара сообщений-образцов для экрана настроек: плагины показывают на них, как будут
 * выглядеть пузыри, аватарки или поле ввода после их правок.
 */
public class MessagesPreviewCell extends ThemePreviewMessagesCell {

    public MessagesPreviewCell(Context context, INavigationLayout layout, int type) {
        super(context, layout, type);
    }

    /** Перерисовать образцы: плагин зовёт это после смены своей настройки. */
    public void refreshMessages() {
        for (int i = 0; i < getChildCount(); i++) {
            getChildAt(i).invalidate();
            getChildAt(i).requestLayout();
        }
        invalidate();
    }
}

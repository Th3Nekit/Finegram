/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.utils.ui;

import static org.telegram.messenger.AndroidUtilities.dp;

import org.telegram.ui.ActionBar.ActionBar;
import org.telegram.ui.Components.chat.layouts.ChatActivityFadeView;

/**
 * Шапка чата: затемнение, под которым сообщения уходят за шапку.
 *
 * Чат пересчитывает высоту затемнения при каждом изменении шапки (закреп, теги, вкладки
 * тем) и отдаёт её сюда. Плагины, которые рисуют затемнение по-своему, перехватывают этот
 * вызов и получают уже посчитанные значения.
 */
public final class ChatHeaderUiHelper {

    /** Высота самой полосы затемнения, в пикселях. */
    public static int getChatTopFadeHeight() {
        return dp(48);
    }

    /** Докуда от верха экрана тянется зона под шапкой. */
    public static int getChatTopFadeZone(int headerBottom) {
        return headerBottom;
    }

    /**
     * Выставить затемнение под шапкой.
     *
     * @param zone   нижний край шапки со всем, что под ней, — сообщения ниже видны целиком
     * @param height высота плавного перехода от шапки к сообщениям
     */
    public static void setupChatTopFade(ChatActivityFadeView fadeView, ActionBar actionBar, int zone, int height) {
        if (fadeView == null) {
            return;
        }
        fadeView.setFadeZoneTop(getChatTopFadeZone(zone));
    }

    private ChatHeaderUiHelper() {
    }
}

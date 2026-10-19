/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.utils.text;

import android.text.SpannableStringBuilder;

import org.telegram.messenger.MessageObject;

/**
 * Разметка текста для плагинов на dex.
 *
 * Плагины зовут эти два метода, когда показывают свой текст: один делает кликабельными
 * имена и ссылки, другой — только имена. Клиент умеет и то и другое, здесь лишь вход под
 * тем именем, которое плагины ищут.
 */
public final class LocaleUtils {

    private LocaleUtils() {
    }

    /** Ссылки, команды и имена пользователей становятся кликабельными. */
    public static CharSequence fullyFormatText(CharSequence text) {
        if (text == null) {
            return null;
        }
        final SpannableStringBuilder builder = new SpannableStringBuilder(text);
        MessageObject.addLinks(false, builder, true, false);
        return builder;
    }

    /** То же, но без команд бота: кликабельными становятся имена и метки. */
    public static CharSequence formatWithUsernames(CharSequence text) {
        if (text == null) {
            return null;
        }
        final SpannableStringBuilder builder = new SpannableStringBuilder(text);
        MessageObject.addUrlsByPattern(false, builder, false, 0, 0, false);
        return builder;
    }
}

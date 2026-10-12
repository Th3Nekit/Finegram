/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.utils.text;

import android.text.SpannableStringBuilder;

import org.telegram.messenger.MessageObject;

public final class LocaleUtils {

    private LocaleUtils() {
    }

    public static CharSequence fullyFormatText(CharSequence text) {
        if (text == null) {
            return null;
        }
        final SpannableStringBuilder builder = new SpannableStringBuilder(text);
        MessageObject.addLinks(false, builder, true, false);
        return builder;
    }

    public static CharSequence formatWithUsernames(CharSequence text) {
        if (text == null) {
            return null;
        }
        final SpannableStringBuilder builder = new SpannableStringBuilder(text);
        MessageObject.addUrlsByPattern(false, builder, false, 0, 0, false);
        return builder;
    }
}

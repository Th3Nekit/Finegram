/*
 * Finegram plugin store.
 * Licensed under GNU GPL v3; see LICENSE.PluginManager and NOTICE.
 */

package com.th3nekit.finegram.store;

import android.text.SpannableString;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.widget.TextView;

import org.telegram.ui.Components.URLSpanNoUnderline;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

final class FGStoreAuthorLinks {

    private static final Pattern USERNAME = Pattern.compile(
            "(?<![A-Za-z0-9_@])@([A-Za-z][A-Za-z0-9_]{0,31})(?![A-Za-z0-9_])");

    private FGStoreAuthorLinks() {
    }

    static CharSequence linkify(CharSequence text, int authorStart, int authorEnd) {
        if (text == null || authorStart < 0 || authorEnd <= authorStart || authorEnd > text.length()) {
            return text;
        }
        final Matcher matcher = USERNAME.matcher(text.subSequence(authorStart, authorEnd));
        final SpannableString linked = new SpannableString(text);
        while (matcher.find()) {
            linked.setSpan(new URLSpanNoUnderline("@" + matcher.group(1), true),
                    authorStart + matcher.start(), authorStart + matcher.end(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
        return linked;
    }

    static void setText(TextView view, CharSequence text, int linkColor) {
        final boolean hasLinks = text instanceof Spanned && ((Spanned) text)
                .getSpans(0, text.length(), URLSpanNoUnderline.class).length > 0;
        view.setText(text);
        view.setLinkTextColor(linkColor);
        view.setMovementMethod(hasLinks ? LinkMovementMethod.getInstance() : null);
        view.setFocusableInTouchMode(false);
        view.setFocusable(false);
        view.setClickable(hasLinks);
        view.setLongClickable(false);
    }
}

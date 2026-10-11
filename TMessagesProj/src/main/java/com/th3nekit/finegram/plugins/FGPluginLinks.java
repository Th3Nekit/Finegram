package com.th3nekit.finegram.plugins;

import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.URLSpan;
import android.text.util.Linkify;
import android.widget.TextView;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.ui.Components.URLSpanNoUnderline;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class FGPluginLinks {
    private static final Pattern USERNAME = Pattern.compile("(?<![\\p{L}\\p{N}_@])@[a-zA-Z0-9_]{1,32}(?![a-zA-Z0-9_])");

    private FGPluginLinks() {
    }

    public static void setText(TextView view, CharSequence text, int linkColor) {
        SpannableStringBuilder linked = new SpannableStringBuilder(text == null ? "" : text);
        AndroidUtilities.addLinksSafe(linked, Linkify.WEB_URLS, false, false);
        Matcher matcher = USERNAME.matcher(linked);
        while (matcher.find()) {
            if (linked.getSpans(matcher.start(), matcher.end(), URLSpan.class).length == 0) {
                linked.setSpan(new URLSpanNoUnderline(matcher.group()), matcher.start(), matcher.end(), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
        }
        view.setText(linked);
        view.setLinkTextColor(linkColor);
        view.setLinksClickable(true);
        view.setMovementMethod(new AndroidUtilities.LinkMovementMethodMy());
    }
}

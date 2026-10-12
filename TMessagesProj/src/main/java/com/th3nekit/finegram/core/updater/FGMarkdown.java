/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.updater;

import android.graphics.Typeface;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.BulletSpan;
import android.text.style.ForegroundColorSpan;
import android.text.style.LeadingMarginSpan;
import android.text.style.RelativeSizeSpan;
import android.text.style.StrikethroughSpan;
import android.text.style.StyleSpan;
import android.text.style.TypefaceSpan;
import android.text.style.URLSpan;

import org.telegram.messenger.AndroidUtilities;

import java.util.ArrayList;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class FGMarkdown {

    private static final Pattern LINK = Pattern.compile("\\[([^\\]]+)]\\(([^)\\s]+)\\)");

    private static final String[] MARKS = {"***", "**", "__", "~~", "`", "*", "_"};

    private FGMarkdown() {
    }

    public static CharSequence parse(CharSequence source, int accentColor) {
        if (TextUtils.isEmpty(source)) return "";
        final SpannableStringBuilder out = new SpannableStringBuilder();
        final String[] lines = source.toString().replace("\r\n", "\n").split("\n", -1);
        boolean previousBlank = true;
        for (String raw : lines) {
            final String line = raw.trim();
            if (line.isEmpty()) {
                previousBlank = true;
                continue;
            }
            if (out.length() > 0) {
                out.append('\n');

                if (previousBlank && !isBullet(line)) out.append('\n');
            }
            previousBlank = false;

            if (line.startsWith("###") || line.startsWith("##") || line.startsWith("#")) {
                appendHeader(out, line, accentColor);
            } else if (isBullet(line)) {
                appendBullet(out, line, accentColor);
            } else {
                appendInline(out, line, accentColor);
            }
        }
        return out;
    }

    private static boolean isBullet(String line) {
        return line.startsWith("- ") || line.startsWith("* ") || line.startsWith("— ") || line.startsWith("• ");
    }

    private static void appendHeader(SpannableStringBuilder out, String line, int accentColor) {
        int level = 0;
        while (level < line.length() && line.charAt(level) == '#') level++;
        final int start = out.length();
        appendInline(out, line.substring(level).trim(), accentColor);
        out.setSpan(new StyleSpan(Typeface.BOLD), start, out.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        out.setSpan(new RelativeSizeSpan(level <= 1 ? 1.25f : 1.1f), start, out.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
    }

    private static void appendBullet(SpannableStringBuilder out, String line, int accentColor) {
        final int start = out.length();
        appendInline(out, line.substring(line.indexOf(' ') + 1).trim(), accentColor);
        final int gap = AndroidUtilities.dp(14);
        out.setSpan(new BulletSpanCompat(gap, accentColor), start, out.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        out.setSpan(new LeadingMarginSpan.Standard(0, gap), start, out.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
    }

    private static void appendInline(SpannableStringBuilder out, String line, int accentColor) {
        final int base = out.length();
        final StringBuilder plain = new StringBuilder();
        final ArrayList<int[]> marks = new ArrayList<>();

        int i = 0;
        while (i < line.length()) {
            final int mark = markAt(line, i);
            if (mark >= 0) {
                final String label = MARKS[mark];
                final int close = line.indexOf(label, i + label.length());
                if (close > i) {
                    final int from = plain.length();
                    plain.append(line, i + label.length(), close);
                    marks.add(new int[]{from, plain.length(), mark});
                    i = close + label.length();
                    continue;
                }
            }
            plain.append(line.charAt(i));
            i++;
        }

        out.append(plain);
        for (int[] mark : marks) {
            applyMark(out, base + mark[0], base + mark[1], mark[2]);
        }
        applyLinks(out, base, accentColor);
    }

    private static int markAt(String line, int index) {
        for (int i = 0; i < MARKS.length; i++) {
            if (line.startsWith(MARKS[i], index)) {

                if (MARKS[i].length() == 1 && index + 1 < line.length() && line.charAt(index + 1) == ' ') continue;
                return i;
            }
        }
        return -1;
    }

    private static void applyMark(Spannable text, int start, int end, int mark) {
        if (start >= end) return;
        switch (MARKS[mark]) {
            case "***":
                text.setSpan(new StyleSpan(Typeface.BOLD_ITALIC), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
                break;
            case "**":
                text.setSpan(new StyleSpan(Typeface.BOLD), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
                break;
            case "__":
            case "_":
            case "*":
                text.setSpan(new StyleSpan(Typeface.ITALIC), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
                break;
            case "~~":
                text.setSpan(new StrikethroughSpan(), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
                break;
            case "`":
                text.setSpan(new TypefaceSpan("monospace"), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
                break;
        }
    }

    private static void applyLinks(SpannableStringBuilder out, int from, int accentColor) {
        final Matcher matcher = LINK.matcher(out.subSequence(from, out.length()).toString());
        int shift = 0;
        while (matcher.find()) {
            final int start = from + matcher.start() - shift;
            final int end = from + matcher.end() - shift;
            final String label = matcher.group(1);
            final String url = matcher.group(2);
            out.replace(start, end, label);
            out.setSpan(new URLSpan(url), start, start + label.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            out.setSpan(new ForegroundColorSpan(accentColor), start, start + label.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            shift += (end - start) - label.length();
        }
    }

    private static class BulletSpanCompat extends BulletSpan {

        private final int gap;
        private final int color;

        BulletSpanCompat(int gap, int color) {
            super(gap);
            this.gap = gap;
            this.color = color;
        }

        @Override
        public void drawLeadingMargin(android.graphics.Canvas canvas, android.graphics.Paint paint,
                                      int x, int dir, int top, int baseline, int bottom,
                                      CharSequence text, int start, int end, boolean first,
                                      android.text.Layout layout) {
            if (!first || !(text instanceof Spanned) || ((Spanned) text).getSpanStart(this) != start) return;
            final int oldColor = paint.getColor();
            final android.graphics.Paint.Style oldStyle = paint.getStyle();
            paint.setColor(color);
            paint.setStyle(android.graphics.Paint.Style.FILL);
            canvas.drawCircle(x + dir * AndroidUtilities.dp(3), (top + bottom) / 2f, AndroidUtilities.dp(2.2f), paint);
            paint.setColor(oldColor);
            paint.setStyle(oldStyle);
        }

        @Override
        public int getLeadingMargin(boolean first) {
            return gap;
        }
    }
}

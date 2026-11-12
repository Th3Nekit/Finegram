/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.privacy;

import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;

import com.th3nekit.finegram.core.configs.FinegramPrivacyConfig;

import org.telegram.messenger.LocaleController;
import org.telegram.messenger.MessageObject;
import org.telegram.messenger.R;

public final class FGDeletedMark {

    private FGDeletedMark() {
    }

    public static String normalizeText(String text) {
        if (text == null) return "";
        String value = text.replace('\n', ' ').replace('\r', ' ').replace('\t', ' ').trim();
        int count = value.codePointCount(0, value.length());
        return count > 12 ? value.substring(0, value.offsetByCodePoints(0, 12)) : value;
    }

    public static CharSequence decorate(MessageObject message, CharSequence time) {
        try {
            if (message == null || message.messageOwner == null) return time;
            if (!FGDeletedStore.isDeleted(message.currentAccount, message.getDialogId(), message.getId())) return time;
            final boolean custom = FinegramPrivacyConfig.INSTANCE.getCustomDeletedMark();
            String mark = custom ? normalizeText(FinegramPrivacyConfig.INSTANCE.getDeletedMarkText()) : "";
            if (TextUtils.isEmpty(mark)) mark = LocaleController.getString(R.string.FG_Saved_Mark);
            SpannableStringBuilder result = new SpannableStringBuilder(mark);
            final int color = custom ? FinegramPrivacyConfig.INSTANCE.getDeletedMarkColor() : 0;
            if (color != 0) result.setSpan(new ForegroundColorSpan(color), 0, result.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            if (!TextUtils.isEmpty(time)) result.append(", ").append(time);
            return result;
        } catch (Throwable t) {
            return time;
        }
    }
}

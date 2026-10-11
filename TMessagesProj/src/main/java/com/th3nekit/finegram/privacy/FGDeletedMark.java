/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.privacy;

import android.text.TextUtils;

import org.telegram.messenger.LocaleController;
import org.telegram.messenger.MessageObject;
import org.telegram.messenger.R;

public final class FGDeletedMark {

    private FGDeletedMark() {
    }

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

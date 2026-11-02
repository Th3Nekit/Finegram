/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.chats.ui;

import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;

import java.util.ArrayList;

import com.th3nekit.finegram.core.configs.FinegramMessagesConfig;

public final class MessageMenuHold {

    public static final int OPTION_SELECT = 2064;

    private MessageMenuHold() {
    }

    public static boolean isEnabled() {
        return FinegramMessagesConfig.INSTANCE.getBlurMessageMenuBackground()
                && FinegramMessagesConfig.INSTANCE.getMessageMenuOnHold();
    }

    public static void addSelectItem(boolean canSelect, ArrayList<CharSequence> items, ArrayList<Integer> options, ArrayList<Integer> icons) {
        if (!canSelect || !isEnabled()) {
            return;
        }
        items.add(LocaleController.getString(R.string.Select));
        options.add(OPTION_SELECT);
        icons.add(R.drawable.msg_select);
    }
}

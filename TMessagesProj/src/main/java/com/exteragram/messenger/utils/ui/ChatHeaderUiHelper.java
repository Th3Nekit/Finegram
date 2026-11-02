/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.utils.ui;

import static org.telegram.messenger.AndroidUtilities.dp;

import org.telegram.ui.ActionBar.ActionBar;
import org.telegram.ui.Components.chat.layouts.ChatActivityFadeView;

public final class ChatHeaderUiHelper {

    public static int getChatTopFadeHeight() {
        return dp(48);
    }

    public static int getChatTopFadeZone(int headerBottom) {
        return headerBottom;
    }

    public static void setupChatTopFade(ChatActivityFadeView fadeView, ActionBar actionBar, int zone, int height) {
        if (fadeView == null) {
            return;
        }
        fadeView.setFadeZoneTop(getChatTopFadeZone(zone));
    }

    private ChatHeaderUiHelper() {
    }
}

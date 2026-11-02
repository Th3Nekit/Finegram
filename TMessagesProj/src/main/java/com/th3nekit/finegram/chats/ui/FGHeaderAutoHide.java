package com.th3nekit.finegram.chats.ui;

import static org.telegram.messenger.AndroidUtilities.dp;

import com.th3nekit.finegram.core.configs.FinegramChatsConfig;

import org.telegram.ui.ActionBar.ActionBar;

public class FGHeaderAutoHide {

    private static final int COLLAPSE_DISTANCE_DP = 64;
    private static final int EXPAND_DISTANCE_DP = 28;

    private final ActionBar actionBar;
    private int towardOlder;
    private int towardNewer;

    public FGHeaderAutoHide(ActionBar actionBar) {
        this.actionBar = actionBar;
    }

    public void onScrolled(int dy, boolean byUser, boolean atBottom) {
        if (!FinegramChatsConfig.INSTANCE.getChatHeaderAutoHide() || atBottom) {
            reset();
            return;
        }
        if (!byUser || dy == 0) {
            return;
        }
        if (dy < 0) {
            towardNewer = 0;
            towardOlder -= dy;
            if (towardOlder >= dp(COLLAPSE_DISTANCE_DP)) {
                actionBar.setHeaderCollapsed(true, true);
            }
        } else {
            towardOlder = 0;
            towardNewer += dy;
            if (towardNewer >= dp(EXPAND_DISTANCE_DP)) {
                actionBar.setHeaderCollapsed(false, true);
            }
        }
    }

    public void reset() {
        towardOlder = 0;
        towardNewer = 0;
        actionBar.setHeaderCollapsed(false, true);
    }
}

/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.chats.ui;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.view.View;
import android.widget.LinearLayout;

import com.th3nekit.finegram.core.configs.FinegramMessagesConfig;

import org.telegram.messenger.BaseController;
import org.telegram.messenger.MessageObject;
import org.telegram.messenger.UserConfig;
import org.telegram.ui.ActionBar.ActionBarPopupWindow;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.ChatActivity;
import org.telegram.ui.Components.ChatScrimPopupContainerLayout;
import org.telegram.ui.Components.ReactionsContainerLayout;

public class MessageMenuHelper extends BaseController {

    private static final MessageMenuHelper[] Instance = new MessageMenuHelper[UserConfig.MAX_ACCOUNT_COUNT];

    public MessageMenuHelper(int num) {
        super(num);
    }

    public static MessageMenuHelper getInstance(int num) {
        MessageMenuHelper localInstance = Instance[num];
        if (localInstance == null) {
            synchronized (MessageMenuHelper.class) {
                localInstance = Instance[num];
                if (localInstance == null) {
                    Instance[num] = localInstance = new MessageMenuHelper(num);
                }
            }
        }
        return localInstance;
    }

    public void checkBlur(Activity activity, boolean enable, boolean hideStatusBar, float windowBlurRadius) {
        checkBlur(activity, enable, hideStatusBar, windowBlurRadius, 0);
    }

    public void checkBlur(Activity activity, boolean enable, boolean hideStatusBar, float windowBlurRadius, float windowDimAlpha) {
        if (activity == null || Build.VERSION.SDK_INT < Build.VERSION_CODES.S) return;

        WindowBlurHelper blurHelper = new WindowBlurHelper();
        blurHelper.setWindowBlur(activity, enable, hideStatusBar, windowBlurRadius, windowDimAlpha);
    }

    public static class MaxHeightLinearLayout extends LinearLayout {

        private int maxHeight;
        private int maxWidth;

        public MaxHeightLinearLayout(Context context) {
            super(context);
        }

        public void setMaxHeight(int maxHeight) {
            if (this.maxHeight == maxHeight) return;
            this.maxHeight = maxHeight;
            requestLayout();
        }

        public void setMaxWidth(int maxWidth) {
            if (this.maxWidth == maxWidth) return;
            this.maxWidth = maxWidth;
            requestLayout();
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            if (FinegramMessagesConfig.INSTANCE.getMsgMenuFixedHeight()) {
                widthMeasureSpec = limit(widthMeasureSpec, maxWidth);
                heightMeasureSpec = limit(heightMeasureSpec, maxHeight);
            }
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }

        private static int limit(int spec, int max) {
            if (max <= 0) return spec;
            final int mode = MeasureSpec.getMode(spec);
            final int size = MeasureSpec.getSize(spec);
            if (mode != MeasureSpec.UNSPECIFIED && size <= max) return spec;
            return MeasureSpec.makeMeasureSpec(max, mode == MeasureSpec.EXACTLY ? MeasureSpec.EXACTLY : MeasureSpec.AT_MOST);
        }
    }

    public static int getMessageMenuBackgroundColor(Theme.ResourcesProvider resourcesProvider) {
        return Theme.getColor(Theme.key_actionBarDefaultSubmenuBackground, resourcesProvider);
    }

    public static int getMessageMenuGapColor(Theme.ResourcesProvider resourcesProvider) {
        return Theme.getColor(Theme.key_windowBackgroundGrayShadow, resourcesProvider);
    }

    public boolean showDivider() {
        return true;
    }

    public boolean showCustomDivider(boolean verifyDonates) {
        return false;
    }

    public boolean allowUnifiedScroll(boolean verifyDonates) {
        return false;
    }

    public boolean keepSubmenuSize() {
        return FinegramMessagesConfig.INSTANCE.getMsgMenuUnifiedScroll();
    }

    public boolean allowToOccupyStatusBar() {
        return false;
    }

    public boolean allowNewMessageMenu() {
        return false;
    }

    public boolean allowNewMessageMenu(MessageObject messageObject) {
        return false;
    }

    public boolean allowNewMessageMenu(boolean verifyDonates) {
        return false;
    }

}
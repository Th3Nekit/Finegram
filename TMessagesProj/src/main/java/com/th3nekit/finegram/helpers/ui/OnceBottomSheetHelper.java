/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.helpers.ui;

import android.content.Context;

import org.telegram.ui.ActionBar.BottomSheet;

public class OnceBottomSheetHelper extends BottomSheet {

    private static boolean shown = false;

    public OnceBottomSheetHelper(Context context, boolean needFocus) {
        super(context, needFocus);
    }

    @Override
    public void show() {
        if (shown) {
            return;
        }
        shown = true;
        super.show();
    }

}

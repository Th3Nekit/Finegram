/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.utils.ui;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.content.Context;
import android.widget.LinearLayout;

import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Cells.RadioColorCell;

import java.util.ArrayList;

public class PopupUtils {

    public interface OnItemClickListener {
        void onClick(int i);
    }

    public static void showDialog(CharSequence[] items, String title, int selected, Context context, OnItemClickListener listener) {
        showDialog(items, null, title, selected, context, listener, null, true);
    }

    public static void showDialog(CharSequence[] items, int[] icons, String title, int selected, Context context, OnItemClickListener listener) {
        showDialog(items, icons, title, selected, context, listener, null, true);
    }

    public static void showDialogWithoutRadio(ArrayList<? extends CharSequence> items, String title, Context context, OnItemClickListener listener) {
        showDialog(items.toArray(new CharSequence[0]), null, title, -1, context, listener, null, false);
    }

    public static void showDialog(CharSequence[] items, int[] icons, String title, int selected, Context context,
                                  OnItemClickListener listener, Theme.ResourcesProvider resourcesProvider, boolean withRadio) {
        AlertDialog.Builder builder = new AlertDialog.Builder(context, resourcesProvider);
        builder.setTitle(title);
        if (withRadio) {
            LinearLayout layout = new LinearLayout(context);
            layout.setOrientation(LinearLayout.VERTICAL);
            builder.setView(layout);
            for (int i = 0; i < items.length; i++) {
                RadioColorCell cell = new RadioColorCell(context);
                cell.setPadding(dp(4), 0, dp(4), 0);
                cell.setTag(i);
                cell.setCheckColor(Theme.getColor(Theme.key_radioBackground, resourcesProvider), Theme.getColor(Theme.key_dialogRadioBackgroundChecked, resourcesProvider));
                cell.setTextAndValue(items[i], selected == i);
                cell.setBackground(Theme.createSelectorDrawable(Theme.getColor(Theme.key_listSelector, resourcesProvider), 2));
                layout.addView(cell);
                cell.setOnClickListener(v -> {
                    builder.getDismissRunnable().run();
                    listener.onClick((Integer) v.getTag());
                });
            }
        } else if (icons != null) {
            builder.setItems(items, icons, (dialog, which) -> {
                builder.getDismissRunnable().run();
                listener.onClick(which);
            });
        } else {
            builder.setItems(items, (dialog, which) -> {
                builder.getDismissRunnable().run();
                listener.onClick(which);
            });
        }
        builder.setNegativeButton(LocaleController.getString(R.string.Cancel), null);
        builder.show();
    }
}

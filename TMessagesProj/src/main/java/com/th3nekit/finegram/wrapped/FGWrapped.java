package com.th3nekit.finegram.wrapped;

import android.app.Activity;
import android.app.Dialog;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;

import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.AlertDialog;

import java.text.SimpleDateFormat;
import java.util.Calendar;

public final class FGWrapped {

    private FGWrapped() {
    }

    public static void open(Activity activity, int account) {
        if (activity == null) {
            return;
        }
        int[] previous = FGWrappedStats.previousMonth();
        Calendar calendar = Calendar.getInstance();
        calendar.clear();
        calendar.set(previous[0], previous[1], 1);
        SimpleDateFormat format = new SimpleDateFormat("LLLL", LocaleController.getInstance().getCurrentLocale());
        String previousName = capitalize(format.format(calendar.getTime()));
        String currentName = capitalize(format.format(Calendar.getInstance().getTime()));
        int year = Calendar.getInstance().get(Calendar.YEAR);
        CharSequence[] items = {
                previousName,
                LocaleController.formatString(R.string.FG_Wrapped_ThisMonth, currentName),
                LocaleController.formatString(R.string.FG_Wrapped_YearItem, year)
        };
        int[] periods = {FGWrappedStats.PERIOD_PREVIOUS_MONTH, FGWrappedStats.PERIOD_CURRENT_MONTH, FGWrappedStats.PERIOD_YEAR};
        AlertDialog.Builder builder = new AlertDialog.Builder(activity);
        builder.setTitle(LocaleController.getString(R.string.FG_Wrapped_Pick));
        builder.setItems(items, (dialog, which) -> show(activity, account, periods[which]));
        builder.show();
    }

    private static String capitalize(String value) {
        if (value == null || value.isEmpty()) {
            return "";
        }
        return value.substring(0, 1).toUpperCase(LocaleController.getInstance().getCurrentLocale()) + value.substring(1);
    }

    private static void show(Activity activity, int account, int period) {
        Dialog dialog = new Dialog(activity, android.R.style.Theme_Black_NoTitleBar_Fullscreen);
        FrameLayout root = new FrameLayout(activity);
        root.setBackgroundColor(Color.BLACK);
        FGWrappedView view = new FGWrappedView(activity, account, period, dialog::dismiss);
        root.addView(view, new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        dialog.setContentView(root);
        Window window = dialog.getWindow();
        if (window != null) {
            window.setBackgroundDrawable(new ColorDrawable(Color.BLACK));
            window.setLayout(WindowManager.LayoutParams.MATCH_PARENT, WindowManager.LayoutParams.MATCH_PARENT);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                WindowManager.LayoutParams params = window.getAttributes();
                params.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
                window.setAttributes(params);
            }
        }
        dialog.show();
    }
}

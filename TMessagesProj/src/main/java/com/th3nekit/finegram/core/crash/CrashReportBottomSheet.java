/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.crash;

import static org.telegram.messenger.LocaleController.getString;

import android.app.Activity;
import android.util.TypedValue;
import android.view.Gravity;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.StickerImageView;
import org.telegram.ui.Stories.recorder.ButtonWithCounterView;

import com.th3nekit.finegram.helpers.ui.OnceBottomSheetHelper;

public class CrashReportBottomSheet extends OnceBottomSheetHelper {

    public CrashReportBottomSheet(BaseFragment fragment) {
        super(fragment.getParentActivity(), false);

        Activity activity = fragment.getParentActivity();

        FrameLayout frameLayout = new FrameLayout(activity);
        LinearLayout linearLayout = new LinearLayout(activity);
        linearLayout.setOrientation(LinearLayout.VERTICAL);
        frameLayout.addView(linearLayout);

        StickerImageView imageView = new StickerImageView(activity, currentAccount);
        imageView.setStickerPackName("Finegram");
        imageView.setStickerNum(30);
        imageView.getImageReceiver().setAutoRepeat(1);
        linearLayout.addView(imageView, LayoutHelper.createLinear(200, 200, Gravity.CENTER_HORIZONTAL, 0, 16, 0, 0));

        TextView title = new TextView(activity);
        title.setGravity(Gravity.CENTER_HORIZONTAL);
        title.setTextColor(Theme.getColor(Theme.key_dialogTextBlack));
        title.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 20);
        title.setTypeface(AndroidUtilities.bold());
        title.setText(getString(R.string.FG_AppCrashed));
        linearLayout.addView(title, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 21, 20, 21, 0));

        TextView description = new TextView(activity);
        description.setGravity(Gravity.CENTER_HORIZONTAL);
        description.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 15);
        description.setTextColor(Theme.getColor(Theme.key_dialogTextGray3));
        description.setText(getString(R.string.FG_AppCrashedDesc));
        linearLayout.addView(description, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 21, 15, 21, 16));

        final boolean safeMode = com.th3nekit.finegram.plugins.FGSafeMode.isActive();
        if (safeMode) {
            TextView safeModeText = new TextView(activity);
            safeModeText.setGravity(Gravity.CENTER_HORIZONTAL);
            safeModeText.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 14);
            safeModeText.setTextColor(Theme.getColor(Theme.key_dialogTextBlack));
            safeModeText.setText(getString(R.string.FG_SafeMode_Crash));
            safeModeText.setPadding(AndroidUtilities.dp(14), AndroidUtilities.dp(12), AndroidUtilities.dp(14), AndroidUtilities.dp(12));
            safeModeText.setBackground(Theme.createRoundRectDrawable(AndroidUtilities.dp(14),
                    androidx.core.graphics.ColorUtils.setAlphaComponent(Theme.getColor(Theme.key_featuredStickers_addButton), 26)));
            linearLayout.addView(safeModeText, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 16, 0, 16, 16));
        }

        ButtonWithCounterView sendLogsButton = new ButtonWithCounterView(getContext(), resourcesProvider);
        sendLogsButton.setRound();
        sendLogsButton.setText(getString(R.string.DebugSendLogs));
        sendLogsButton.setOnClickListener(view -> {
            CrashLogs.sendCrashLogs(activity, this);
        });
        linearLayout.addView(sendLogsButton, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 48, 0, 16, 8, 16, safeMode ? 8 : 16));

        if (safeMode) {
            ButtonWithCounterView pluginsButton = new ButtonWithCounterView(getContext(), false, true, resourcesProvider);
            pluginsButton.setRoundRadius(24);
            pluginsButton.text.setTypeface(AndroidUtilities.bold());
            pluginsButton.setText(getString(R.string.FG_SafeMode_TurnOff));
            pluginsButton.setOnClickListener(view -> {
                dismiss();
                CrashLogs.deleteCrashLogs();
                com.th3nekit.finegram.plugins.FGSafeMode.turnOff();
                org.telegram.ui.Components.BulletinFactory.global()
                        .createSimpleBulletin(R.raw.contact_check, getString(R.string.FG_SafeMode_Started)).show();
            });
            linearLayout.addView(pluginsButton, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 48, 0, 16, 0, 16, 8));
        }

        ButtonWithCounterView cancelButton = new ButtonWithCounterView(getContext(), false, true, resourcesProvider);
        cancelButton.setRoundRadius(24);
        cancelButton.text.setTypeface(AndroidUtilities.bold());
        cancelButton.setText(getString(R.string.Cancel));
        cancelButton.setOnClickListener(view -> {
            dismiss();
            CrashLogs.deleteCrashLogs();
        });
        linearLayout.addView(cancelButton, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 48, 0, 16, 0, 16, 16));

        ScrollView scrollView = new ScrollView(activity);
        scrollView.addView(frameLayout);
        setCustomView(scrollView);

    }

    public static void checkBottomSheet(BaseFragment fragment) {
        try {
            CrashReportBottomSheet dialog = new CrashReportBottomSheet(fragment);
            dialog.setCancelable(false);
            dialog.show();
        } catch (Exception ignored) {
        }
    }

}

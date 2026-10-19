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
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.core.graphics.ColorUtils;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.RLottieImageView;
import org.telegram.ui.Stories.recorder.ButtonWithCounterView;

import com.th3nekit.finegram.helpers.ui.OnceBottomSheetHelper;

public class CrashReportBottomSheet extends OnceBottomSheetHelper {

    public CrashReportBottomSheet(BaseFragment fragment) {
        super(fragment.getParentActivity(), false);

        Activity activity = fragment.getParentActivity();
        resourcesProvider = fragment.getResourceProvider();
        int surface = Theme.getColor(Theme.key_dialogBackground, resourcesProvider);
        int textColor = Theme.getColor(Theme.key_dialogTextBlack, resourcesProvider);
        int mutedColor = Theme.getColor(Theme.key_dialogTextGray3, resourcesProvider);
        int accent = Theme.getColor(Theme.key_featuredStickers_addButton, resourcesProvider);

        setBackgroundColor(surface);
        fixNavigationBar(surface);
        setApplyTopPadding(false);
        setApplyBottomPadding(false);

        GradientDrawable sheetBackground = new GradientDrawable(GradientDrawable.Orientation.TL_BR,
                new int[]{ColorUtils.blendARGB(surface, accent, 0.06f), surface});
        float radius = AndroidUtilities.dp(28);
        sheetBackground.setCornerRadii(new float[]{radius, radius, radius, radius, 0, 0, 0, 0});
        shadowDrawable = new InsetDrawable(sheetBackground, backgroundPaddingLeft, backgroundPaddingTop,
                backgroundPaddingLeft, 0);

        FrameLayout frameLayout = new FrameLayout(activity);
        LinearLayout linearLayout = new LinearLayout(activity);
        linearLayout.setOrientation(LinearLayout.VERTICAL);
        linearLayout.setPadding(AndroidUtilities.dp(24), AndroidUtilities.dp(16),
                AndroidUtilities.dp(24), AndroidUtilities.dp(20));

        RLottieImageView imageView = new RLottieImageView(activity);
        imageView.setAutoRepeat(true);
        imageView.setAnimation(R.raw.utyan_empty2, 200, 200);
        imageView.playAnimation();
        imageView.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        linearLayout.addView(imageView, LayoutHelper.createLinear(112, 112, Gravity.CENTER_HORIZONTAL, 0, 0, 0, 12));

        TextView title = new TextView(activity);
        title.setGravity(Gravity.START);
        title.setIncludeFontPadding(false);
        title.setTextColor(textColor);
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 23);
        title.setTypeface(AndroidUtilities.bold());
        title.setText(getString(R.string.FG_AppCrashed));
        linearLayout.addView(title, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        TextView description = new TextView(activity);
        description.setGravity(Gravity.START);
        description.setIncludeFontPadding(false);
        description.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        description.setLineSpacing(AndroidUtilities.dp(3), 1);
        description.setTextColor(mutedColor);
        description.setText(getString(R.string.FG_AppCrashedDesc));
        linearLayout.addView(description, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                0, 10, 0, 0));

        final boolean safeMode = com.th3nekit.finegram.plugins.FGSafeMode.isActive();
        if (safeMode) {
            LinearLayout safeModeCard = new LinearLayout(activity);
            safeModeCard.setOrientation(LinearLayout.HORIZONTAL);
            safeModeCard.setGravity(Gravity.TOP);
            safeModeCard.setPadding(AndroidUtilities.dp(14), AndroidUtilities.dp(14),
                    AndroidUtilities.dp(14), AndroidUtilities.dp(14));
            GradientDrawable stateBackground = new GradientDrawable();
            stateBackground.setColor(ColorUtils.blendARGB(surface, accent, 0.06f));
            stateBackground.setCornerRadius(AndroidUtilities.dp(16));
            stateBackground.setStroke(AndroidUtilities.dp(1), ColorUtils.setAlphaComponent(textColor, 28));
            safeModeCard.setBackground(stateBackground);

            ImageView shield = new ImageView(activity);
            shield.setImageResource(R.drawable.outline_shield_check);
            shield.setColorFilter(accent, PorterDuff.Mode.SRC_IN);
            shield.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            LinearLayout.LayoutParams shieldParams = LayoutHelper.createLinear(22, 22);
            shieldParams.setMarginEnd(AndroidUtilities.dp(12));
            safeModeCard.addView(shield, shieldParams);

            LinearLayout safeModeCopy = new LinearLayout(activity);
            safeModeCopy.setOrientation(LinearLayout.VERTICAL);
            safeModeCard.addView(safeModeCopy, LayoutHelper.createLinear(0, LayoutHelper.WRAP_CONTENT, 1.0f));

            TextView safeModeTitle = new TextView(activity);
            safeModeTitle.setGravity(Gravity.START);
            safeModeTitle.setIncludeFontPadding(false);
            safeModeTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
            safeModeTitle.setTextColor(textColor);
            safeModeTitle.setTypeface(AndroidUtilities.bold());
            safeModeTitle.setText(getString(R.string.FG_SafeMode_Title));
            safeModeCopy.addView(safeModeTitle, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

            TextView safeModeText = new TextView(activity);
            safeModeText.setGravity(Gravity.START);
            safeModeText.setIncludeFontPadding(false);
            safeModeText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
            safeModeText.setLineSpacing(AndroidUtilities.dp(2), 1);
            safeModeText.setTextColor(mutedColor);
            safeModeText.setText(getString(R.string.FG_SafeMode_Crash));
            safeModeCopy.addView(safeModeText, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                    0, 5, 0, 0));
            linearLayout.addView(safeModeCard, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                    0, 18, 0, 0));
        }

        ButtonWithCounterView sendLogsButton = new ButtonWithCounterView(getContext(), resourcesProvider);
        sendLogsButton.setRoundRadius(16);
        sendLogsButton.setMinimumHeight(AndroidUtilities.dp(48));
        sendLogsButton.setText(getString(R.string.DebugSendLogs));
        sendLogsButton.setOnClickListener(view -> {
            CrashLogs.sendCrashLogs(activity, this);
        });
        linearLayout.addView(sendLogsButton, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 52,
                0, 22, 0, 8));

        if (safeMode) {
            ButtonWithCounterView pluginsButton = new ButtonWithCounterView(getContext(), resourcesProvider);
            pluginsButton.setRoundRadius(16);
            pluginsButton.setMinimumHeight(AndroidUtilities.dp(48));
            pluginsButton.setColor(ColorUtils.blendARGB(surface, accent, 0.12f));
            pluginsButton.setTextColor(textColor);
            pluginsButton.text.setTypeface(AndroidUtilities.bold());
            pluginsButton.setText(getString(R.string.FG_SafeMode_TurnOff));
            pluginsButton.setOnClickListener(view -> {
                dismiss();
                CrashLogs.deleteCrashLogs();
                com.th3nekit.finegram.plugins.FGSafeMode.turnOff();
                org.telegram.ui.Components.BulletinFactory.global()
                        .createSimpleBulletin(R.raw.contact_check, getString(R.string.FG_SafeMode_Started)).show();
            });
            linearLayout.addView(pluginsButton, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 52,
                    0, 0, 0, 8));
        }

        ButtonWithCounterView cancelButton = new ButtonWithCounterView(getContext(), false, resourcesProvider);
        cancelButton.setRoundRadius(16);
        cancelButton.setMinimumHeight(AndroidUtilities.dp(48));
        cancelButton.setTextColor(mutedColor);
        cancelButton.setText(getString(R.string.Cancel));
        cancelButton.setOnClickListener(view -> {
            dismiss();
            CrashLogs.deleteCrashLogs();
        });
        linearLayout.addView(cancelButton, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 48));

        ScrollView scrollView = new ScrollView(activity);
        scrollView.setVerticalScrollBarEnabled(false);
        scrollView.setClipToPadding(false);
        scrollView.addView(linearLayout, new ScrollView.LayoutParams(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
        frameLayout.addView(scrollView, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        int rimColor = ColorUtils.blendARGB(accent, textColor, 0.35f);
        int rimEnd = ColorUtils.blendARGB(accent,
                Theme.getColor(Theme.key_dialogButton, resourcesProvider), 0.5f);
        View rim = new View(activity);
        GradientDrawable rimBackground = new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT,
                new int[]{ColorUtils.setAlphaComponent(rimColor, 0), rimColor,
                        ColorUtils.blendARGB(rimColor, mutedColor, 0.3f), rimEnd,
                        ColorUtils.setAlphaComponent(rimEnd, 0)});
        rimBackground.setCornerRadius(AndroidUtilities.dp(2));
        rim.setBackground(rimBackground);
        rim.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        frameLayout.addView(rim, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, 2, Gravity.TOP, 40, 1, 40, 0));
        setCustomView(frameLayout);

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

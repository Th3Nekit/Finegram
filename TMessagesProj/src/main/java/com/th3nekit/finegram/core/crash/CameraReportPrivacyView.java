package com.th3nekit.finegram.core.crash;

import static org.telegram.messenger.AndroidUtilities.dp;
import static org.telegram.messenger.LocaleController.getString;

import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.drawable.GradientDrawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.core.graphics.ColorUtils;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.LayoutHelper;

final class CameraReportPrivacyView extends LinearLayout {

    CameraReportPrivacyView(Context context, int surface, int textColor, int mutedColor, int accent) {
        super(context);
        setOrientation(HORIZONTAL);
        setGravity(Gravity.TOP);
        setPadding(dp(14), dp(14), dp(16), dp(15));

        GradientDrawable background = new GradientDrawable();
        background.setColor(ColorUtils.blendARGB(surface, accent, 0.08f));
        background.setCornerRadius(dp(16));
        background.setStroke(dp(1), ColorUtils.setAlphaComponent(accent, 48));
        setBackground(background);

        ImageView shield = new ImageView(context);
        shield.setImageResource(R.drawable.outline_shield_check);
        shield.setColorFilter(accent, PorterDuff.Mode.SRC_IN);
        shield.setPadding(dp(8), dp(8), dp(8), dp(8));
        shield.setBackground(Theme.createCircleDrawable(dp(38), ColorUtils.setAlphaComponent(accent, 36)));
        shield.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        LinearLayout.LayoutParams shieldParams = LayoutHelper.createLinear(38, 38);
        shieldParams.setMarginEnd(dp(12));
        addView(shield, shieldParams);

        LinearLayout copy = new LinearLayout(context);
        copy.setOrientation(VERTICAL);
        addView(copy, LayoutHelper.createLinear(0, LayoutHelper.WRAP_CONTENT, 1.0f));

        TextView title = new TextView(context);
        title.setGravity(Gravity.START);
        title.setIncludeFontPadding(false);
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        title.setTextColor(textColor);
        title.setTypeface(AndroidUtilities.bold());
        title.setText(getString(R.string.FG_Diagnostics_PrivacyTitle));
        copy.addView(title, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 2, 0, 0));

        TextView text = new TextView(context);
        text.setGravity(Gravity.START);
        text.setIncludeFontPadding(false);
        text.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        text.setLineSpacing(dp(2), 1);
        text.setTextColor(mutedColor);
        text.setText(getString(R.string.FG_Diagnostics_Privacy));
        copy.addView(text, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 6, 0, 0));
    }
}

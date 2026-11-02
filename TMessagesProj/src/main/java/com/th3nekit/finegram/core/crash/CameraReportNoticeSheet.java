package com.th3nekit.finegram.core.crash;

import static org.telegram.messenger.LocaleController.getString;

import android.app.Activity;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.core.graphics.ColorUtils;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.ActionBar.BottomSheet;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.RLottieImageView;
import org.telegram.ui.Stories.recorder.ButtonWithCounterView;

public class CameraReportNoticeSheet extends BottomSheet {

    public CameraReportNoticeSheet(BaseFragment fragment) {
        super(fragment.getParentActivity(), false, fragment.getResourceProvider());

        Activity context = fragment.getParentActivity();
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

        LinearLayout linearLayout = new LinearLayout(context);
        linearLayout.setOrientation(LinearLayout.VERTICAL);
        linearLayout.setPadding(AndroidUtilities.dp(24), AndroidUtilities.dp(24),
                AndroidUtilities.dp(24), AndroidUtilities.dp(20));

        RLottieImageView imageView = new RLottieImageView(context);
        imageView.setScaleType(ImageView.ScaleType.CENTER);
        imageView.setAnimation(R.raw.permission_request_camera, 56, 56);
        imageView.setBackground(Theme.createCircleDrawable(AndroidUtilities.dp(88), accent));
        imageView.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        imageView.playAnimation();
        linearLayout.addView(imageView, LayoutHelper.createLinear(88, 88, Gravity.CENTER_HORIZONTAL, 0, 0, 0, 20));

        TextView title = new TextView(context);
        title.setGravity(Gravity.START);
        title.setIncludeFontPadding(false);
        title.setTextColor(textColor);
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 21);
        title.setTypeface(AndroidUtilities.bold());
        title.setText(getString(R.string.FG_Diagnostics_NoticeTitle));
        linearLayout.addView(title, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        TextView description = new TextView(context);
        description.setGravity(Gravity.START);
        description.setIncludeFontPadding(false);
        description.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        description.setLineSpacing(AndroidUtilities.dp(3), 1);
        description.setTextColor(mutedColor);
        description.setText(getString(R.string.FG_Diagnostics_Notice));
        linearLayout.addView(description, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                0, 10, 0, 0));

        linearLayout.addView(new CameraReportPrivacyView(context, surface, textColor, mutedColor, accent),
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 18, 0, 0));

        ButtonWithCounterView okButton = new ButtonWithCounterView(context, resourcesProvider);
        okButton.setRoundRadius(16);
        okButton.setMinimumHeight(AndroidUtilities.dp(48));
        okButton.setText(getString(R.string.FG_Diagnostics_Understood));
        okButton.setOnClickListener(view -> dismiss());
        linearLayout.addView(okButton, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 52,
                0, 22, 0, 8));

        ButtonWithCounterView disableButton = new ButtonWithCounterView(context, false, resourcesProvider);
        disableButton.setRoundRadius(16);
        disableButton.setMinimumHeight(AndroidUtilities.dp(48));
        disableButton.setTextColor(mutedColor);
        disableButton.setText(getString(R.string.FG_Diagnostics_Disable));
        disableButton.setOnClickListener(view -> {
            CameraLogUpload.setEnabled(false);
            dismiss();
            BulletinFactory.of(fragment).createSimpleBulletin(R.raw.contact_check,
                    getString(R.string.FG_Diagnostics_Disabled)).show();
        });
        linearLayout.addView(disableButton, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 48));

        ScrollView scrollView = new ScrollView(context);
        scrollView.setVerticalScrollBarEnabled(false);
        scrollView.setClipToPadding(false);
        scrollView.addView(linearLayout, new ScrollView.LayoutParams(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
        setCustomView(scrollView);
    }
}

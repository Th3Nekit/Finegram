package com.th3nekit.finegram.camera.roundfx;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.view.HapticFeedbackConstants;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.core.graphics.ColorUtils;

import com.th3nekit.finegram.core.configs.FinegramCameraConfig;

import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.ItemOptions;
import org.telegram.ui.Stories.recorder.FlashViews;

public class FGRoundFxButton extends FlashViews.ImageViewInvertable {

    private final ViewGroup host;
    private final Theme.ResourcesProvider resourcesProvider;
    private final Paint circlePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private float invert;
    private float active;
    private boolean activeTarget;
    private long animationStart;
    private float animationFrom;

    public FGRoundFxButton(Context context, ViewGroup host, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.host = host;
        this.resourcesProvider = resourcesProvider;
        setScaleType(ImageView.ScaleType.CENTER);
        setContentDescription(LocaleController.getString(R.string.FG_RoundFx));
        setOnClickListener(v -> showOptions());
        activeTarget = FGRoundFx.shared().isEnabled();
        active = activeTarget ? 1f : 0f;
        updateIcon();
    }

    @Override
    public void setInvert(float invert) {
        this.invert = invert;
        super.setInvert(invert);
        invalidate();
    }

    public void sync() {
        boolean enabled = FGRoundFx.shared().isEnabled();
        if (enabled != activeTarget) {
            animationFrom = active;
            activeTarget = enabled;
            animationStart = System.currentTimeMillis();
            updateIcon();
            invalidate();
        }
    }

    private void updateIcon() {
        setImageResource(activeTarget ? R.drawable.magic_stick_filled_solar : R.drawable.magic_stick_solar);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (animationStart != 0) {
            float t = Math.min(1f, (System.currentTimeMillis() - animationStart) / 220f);
            float eased = CubicBezierInterpolator.EASE_OUT_QUINT.getInterpolation(t);
            active = animationFrom + ((activeTarget ? 1f : 0f) - animationFrom) * eased;
            if (t >= 1f) {
                animationStart = 0;
            } else {
                invalidate();
            }
        }
        if (active > 0.001f) {
            int base = ColorUtils.blendARGB(Color.WHITE, Color.BLACK, invert);
            circlePaint.setColor(ColorUtils.setAlphaComponent(base, (int) (52 * active)));
            float radius = dp(17) * (0.7f + 0.3f * active);
            canvas.drawCircle(getWidth() / 2f, getHeight() / 2f, radius, circlePaint);
        }
        super.onDraw(canvas);
    }

    private void showOptions() {
        FGRoundFx fx = FGRoundFx.shared();
        try {
            performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP, HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING);
        } catch (Exception ignored) {
        }
        ItemOptions options = ItemOptions.makeOptions(host, resourcesProvider, this);
        options.addChecked(fx.isBlurEnabled(), R.drawable.msg_blur_radial, LocaleController.getString(R.string.FG_RoundBlur), () -> {
            fx.setBlurEnabled(!fx.isBlurEnabled());
            sync();
        });
        options.addChecked(fx.isLightEnabled(), R.drawable.menu_lightbulb, LocaleController.getString(R.string.FG_RoundLight), () -> {
            fx.setLightEnabled(!fx.isLightEnabled());
            sync();
        });
        if (fx.isBlurEnabled()) {
            int strength = FinegramCameraConfig.INSTANCE.getRoundBlurStrength();
            options.addGap();
            options.addChecked(strength == 0, LocaleController.getString(R.string.FG_RoundBlurLow), () -> fx.setStrength(0));
            options.addChecked(strength == 1, LocaleController.getString(R.string.FG_RoundBlurMedium), () -> fx.setStrength(1));
            options.addChecked(strength == 2, LocaleController.getString(R.string.FG_RoundBlurHigh), () -> fx.setStrength(2));
        }
        options.setDrawScrim(false);
        options.setDimAlpha(0);
        options.show();
    }
}

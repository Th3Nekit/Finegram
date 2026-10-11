/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.preferences.cells;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.RadialGradient;
import android.graphics.Shader;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.core.graphics.ColorUtils;

import com.th3nekit.finegram.plugins.FGPluginAvatar;
import com.th3nekit.finegram.plugins.FGPluginIcon;
import com.th3nekit.finegram.plugins.FGPluginLinks;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.BackupImageView;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.ScaleStateListAnimator;

import java.util.ArrayList;
import java.util.List;

public class FGPluginHeaderCell extends LinearLayout implements Theme.Colorable {
    private boolean stateAlert;
    private boolean pluginEnabled;

    public interface Delegate {
        void onToggle();

        void onShare();

        void onLog();

        void onUpdate();
    }

    public static final class Action {
        final int icon;
        final CharSequence text;
        final Runnable run;
        final boolean accent;

        public Action(int icon, CharSequence text, boolean accent, Runnable run) {
            this.icon = icon;
            this.text = text;
            this.accent = accent;
            this.run = run;
        }
    }

    private static final int BADGE_SIZE_DP = 92;

    private final Theme.ResourcesProvider resourcesProvider;
    private final FrameLayout badge;
    private final View placeholderView;
    private final BackupImageView badgeView;
    private final TextView nameView;
    private final TextView metaView;
    private final TextView stateView;
    private final TextView aboutView;
    private final LinearLayout actionsView;
    private final Paint glowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    private int glowColor;
    private int glowWidth;
    private boolean introPlayed;

    public FGPluginHeaderCell(Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;
        setOrientation(VERTICAL);
        setWillNotDraw(false);
        setPadding(dp(20), dp(20), dp(20), dp(16));

        badge = new FrameLayout(context);
        placeholderView = new View(context);
        badge.addView(placeholderView, LayoutHelper.createFrame(BADGE_SIZE_DP, BADGE_SIZE_DP, Gravity.CENTER));
        badgeView = new BackupImageView(context);
        badgeView.setVisibility(GONE);
        badge.addView(badgeView, LayoutHelper.createFrame(BADGE_SIZE_DP, BADGE_SIZE_DP, Gravity.CENTER));
        addView(badge, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, BADGE_SIZE_DP));

        nameView = new TextView(context);
        nameView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 22);
        nameView.setTypeface(AndroidUtilities.bold());
        nameView.setLetterSpacing(-0.01f);
        nameView.setGravity(Gravity.CENTER);
        nameView.setMaxLines(2);
        nameView.setEllipsize(TextUtils.TruncateAt.END);
        addView(nameView, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 14, 0, 0));

        final LinearLayout metaRow = new LinearLayout(context);
        metaRow.setOrientation(HORIZONTAL);
        metaRow.setGravity(Gravity.CENTER);
        addView(metaRow, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 6, 0, 0));

        metaView = new TextView(context);
        metaView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 14);
        metaView.setLines(1);
        metaView.setEllipsize(TextUtils.TruncateAt.END);
        metaRow.addView(metaView, LayoutHelper.createLinear(LayoutHelper.WRAP_CONTENT, LayoutHelper.WRAP_CONTENT, 0f, Gravity.CENTER_VERTICAL));

        stateView = new TextView(context);
        stateView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 12);
        stateView.setTypeface(AndroidUtilities.bold());
        stateView.setGravity(Gravity.CENTER);
        stateView.setLines(1);
        stateView.setPadding(dp(8), 0, dp(8), 0);
        metaRow.addView(stateView, LayoutHelper.createLinear(LayoutHelper.WRAP_CONTENT, 20, 0f, Gravity.CENTER_VERTICAL, 8, 0, 0, 0));

        aboutView = new TextView(context);
        aboutView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 15);
        aboutView.setGravity(Gravity.CENTER);
        aboutView.setLineSpacing(dp(2), 1f);
        addView(aboutView, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 4, 12, 4, 0));

        actionsView = new LinearLayout(context);
        actionsView.setOrientation(HORIZONTAL);
        addView(actionsView, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 62, 0, 18, 0, 0));
    }

    public void set(String id, String name, String icon, CharSequence about, CharSequence meta,
                    CharSequence state, boolean stateAlert, boolean enabled, List<Action> actions) {
        this.stateAlert = stateAlert;
        pluginEnabled = enabled;
        final int text = Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider);
        final int gray = Theme.getColor(Theme.key_windowBackgroundWhiteGrayText, resourcesProvider);

        placeholderView.setBackground(new FGPluginAvatar(getContext(), id, icon, 0.3f));
        placeholderView.setVisibility(VISIBLE);
        badgeView.setVisibility(GONE);
        FGPluginIcon.load(badgeView, icon, BADGE_SIZE_DP, () -> placeholderView.setVisibility(GONE));
        badge.setAlpha(enabled ? 1f : 0.6f);

        glowColor = FGPluginIcon.accentFor(id);
        glowWidth = 0;

        nameView.setText(name);
        nameView.setTextColor(text);
        FGPluginLinks.setText(metaView, meta, Theme.getColor(Theme.key_windowBackgroundWhiteLinkText, resourcesProvider));
        metaView.setTextColor(gray);
        metaView.setVisibility(TextUtils.isEmpty(meta) ? GONE : VISIBLE);

        final int stateColor = stateAlert
                ? Theme.getColor(Theme.key_text_RedBold, resourcesProvider)
                : enabled ? Theme.getColor(Theme.key_windowBackgroundWhiteBlueText, resourcesProvider) : gray;
        stateView.setText(state);
        stateView.setTextColor(stateColor);
        stateView.setBackground(Theme.createRoundRectDrawable(dp(10), ColorUtils.setAlphaComponent(stateColor, 32)));

        FGPluginLinks.setText(aboutView, about, Theme.getColor(Theme.key_windowBackgroundWhiteLinkText, resourcesProvider));
        aboutView.setTextColor(gray);
        aboutView.setVisibility(TextUtils.isEmpty(about) ? GONE : VISIBLE);

        fillActions(actions);
        invalidate();
    }

    private void fillActions(List<Action> actions) {
        actionsView.removeAllViews();
        final int accent = Theme.getColor(Theme.key_featuredStickers_addButton, resourcesProvider);
        final int text = Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider);
        final List<Action> list = actions == null ? new ArrayList<>() : actions;
        for (int i = 0; i < list.size(); i++) {
            final Action action = list.get(i);
            final LinearLayout tile = new LinearLayout(getContext());
            tile.setTag(action.accent);
            tile.setOrientation(VERTICAL);
            tile.setGravity(Gravity.CENTER);
            final int background = action.accent ? accent : ColorUtils.setAlphaComponent(accent, 26);
            final int foreground = action.accent ? Theme.getColor(Theme.key_featuredStickers_buttonText, resourcesProvider) : accent;
            tile.setBackground(Theme.createSimpleSelectorRoundRectDrawable(dp(16), background,
                    ColorUtils.setAlphaComponent(action.accent ? Color.WHITE : accent, 40)));

            final ImageView icon = new ImageView(getContext());
            icon.setImageResource(action.icon);
            icon.setColorFilter(new PorterDuffColorFilter(foreground, PorterDuff.Mode.SRC_IN));
            tile.addView(icon, LayoutHelper.createLinear(24, 24));

            final TextView label = new TextView(getContext());
            label.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 12);
            label.setTypeface(AndroidUtilities.bold());
            label.setTextColor(action.accent ? foreground : text);
            label.setLines(1);
            label.setEllipsize(TextUtils.TruncateAt.END);
            label.setGravity(Gravity.CENTER);
            label.setText(action.text);
            tile.addView(label, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 4, 5, 4, 0));

            tile.setOnClickListener(v -> {
                if (action.run != null) action.run.run();
            });
            ScaleStateListAnimator.apply(tile, 0.04f, 1.2f);
            actionsView.addView(tile, LayoutHelper.createLinear(0, 62, 1f, i == 0 ? 0 : 8, 0, 0, 0));
        }
        actionsView.setVisibility(list.isEmpty() ? GONE : VISIBLE);
    }

    @Override
    public void updateColors() {
        final int text = Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider);
        final int gray = Theme.getColor(Theme.key_windowBackgroundWhiteGrayText, resourcesProvider);
        final int link = Theme.getColor(Theme.key_windowBackgroundWhiteLinkText, resourcesProvider);
        nameView.setTextColor(text);
        metaView.setTextColor(gray);
        aboutView.setTextColor(gray);
        metaView.setLinkTextColor(link);
        aboutView.setLinkTextColor(link);
        final int stateColor = stateAlert ? Theme.getColor(Theme.key_text_RedBold, resourcesProvider)
                : pluginEnabled ? Theme.getColor(Theme.key_windowBackgroundWhiteBlueText, resourcesProvider) : gray;
        stateView.setTextColor(stateColor);
        stateView.setBackground(Theme.createRoundRectDrawable(dp(10), ColorUtils.setAlphaComponent(stateColor, 32)));
        final int accent = Theme.getColor(Theme.key_featuredStickers_addButton, resourcesProvider);
        for (int i = 0; i < actionsView.getChildCount(); i++) {
            final LinearLayout tile = (LinearLayout) actionsView.getChildAt(i);
            final boolean filled = Boolean.TRUE.equals(tile.getTag());
            final int foreground = filled ? Theme.getColor(Theme.key_featuredStickers_buttonText, resourcesProvider) : accent;
            tile.setBackground(Theme.createSimpleSelectorRoundRectDrawable(dp(16),
                    filled ? accent : ColorUtils.setAlphaComponent(accent, 26),
                    ColorUtils.setAlphaComponent(filled ? Color.WHITE : accent, 40)));
            ((ImageView) tile.getChildAt(0)).setColorFilter(new PorterDuffColorFilter(foreground, PorterDuff.Mode.SRC_IN));
            ((TextView) tile.getChildAt(1)).setTextColor(filled ? foreground : text);
        }
        invalidate();
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        final int available = MeasureSpec.getSize(widthSpec) - getPaddingLeft() - getPaddingRight();
        if (available > 0) {
            int taken = 0;
            if (stateView.getVisibility() != GONE) {
                stateView.measure(
                        MeasureSpec.makeMeasureSpec(available, MeasureSpec.AT_MOST),
                        MeasureSpec.makeMeasureSpec(dp(20), MeasureSpec.EXACTLY));
                taken = stateView.getMeasuredWidth() + dp(8);
            }
            metaView.setMaxWidth(Math.max(dp(56), available - taken));
        }
        super.onMeasure(widthSpec, heightSpec);
    }

    @Override
    protected void onDraw(Canvas canvas) {

        if (glowWidth != getMeasuredWidth() && getMeasuredWidth() > 0) {
            glowWidth = getMeasuredWidth();
            final float cx = getMeasuredWidth() / 2f;
            final float cy = getPaddingTop() + dp(BADGE_SIZE_DP) / 2f;
            glowPaint.setShader(new RadialGradient(cx, cy, dp(150),
                    new int[]{ColorUtils.setAlphaComponent(glowColor, 70), ColorUtils.setAlphaComponent(glowColor, 22), Color.TRANSPARENT},
                    new float[]{0f, 0.45f, 1f}, Shader.TileMode.CLAMP));
        }
        if (glowPaint.getShader() != null) {
            canvas.drawRect(0, 0, getMeasuredWidth(), getPaddingTop() + dp(BADGE_SIZE_DP + 110), glowPaint);
        }
        super.onDraw(canvas);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (introPlayed) {
            return;
        }
        introPlayed = true;
        playIntro(badge, 0, 0.9f);
        playIntro(nameView, 60, 1f);
        playIntro((View) metaView.getParent(), 80, 1f);
        playIntro(aboutView, 100, 1f);
        playIntro(actionsView, 130, 1f);
    }

    private void playIntro(View view, long delay, float fromScale) {
        if (view == null) {
            return;
        }
        final float target = view.getAlpha() == 0f ? 1f : view.getAlpha();
        view.setAlpha(0f);
        view.setScaleX(fromScale);
        view.setScaleY(fromScale);
        view.setTranslationY(fromScale == 1f ? dp(8) : 0);
        view.animate().alpha(view == badge ? target : 1f).scaleX(1f).scaleY(1f).translationY(0)
                .setStartDelay(delay)
                .setDuration(300)
                .setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT)
                .start();
    }
}

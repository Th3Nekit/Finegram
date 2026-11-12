/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.preferences.cells;

import static org.telegram.messenger.AndroidUtilities.dp;
import static org.telegram.messenger.AndroidUtilities.dpf2;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.RadialGradient;
import android.graphics.Shader;
import android.graphics.SweepGradient;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.core.graphics.ColorUtils;

import com.th3nekit.finegram.plugins.FGPluginAvatar;
import com.th3nekit.finegram.plugins.FGPluginIcon;
import com.th3nekit.finegram.plugins.FGPluginLinks;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LiteMode;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.SharedConfig;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.BackupImageView;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.ScaleStateListAnimator;

import java.util.ArrayList;
import java.util.List;

public class FGPluginHeaderCell extends LinearLayout implements Theme.Colorable {

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

    private static final int BADGE_SIZE_DP = 88;
    private static final int RING_SIZE_DP = 108;

    private final Theme.ResourcesProvider resourcesProvider;
    private final FrameLayout badge;
    private final View placeholderView;
    private final BackupImageView badgeView;
    private final TextView nameView;
    private final ChipRow chipsView;
    private final TextView aboutView;
    private final LinearLayout primaryView;
    private final LinearLayout secondaryView;
    private final Paint glowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint ringPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Matrix ringMatrix = new Matrix();

    private String pluginId;
    private String version;
    private CharSequence author;
    private CharSequence state;
    private boolean stateAlert;
    private boolean pluginEnabled;
    private int accent;
    private int glowWidth;
    private int ringState = -1;
    private boolean introPlayed;
    private boolean actionsEnabled = true;
    private List<Action> lastActions = new ArrayList<>();

    public FGPluginHeaderCell(Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;
        setOrientation(VERTICAL);
        setLayoutDirection(LocaleController.isRTL ? LAYOUT_DIRECTION_RTL : LAYOUT_DIRECTION_LTR);
        setWillNotDraw(false);
        setPadding(dp(20), dp(18), dp(20), dp(18));
        ringPaint.setStyle(Paint.Style.STROKE);
        ringPaint.setStrokeCap(Paint.Cap.ROUND);

        badge = new FrameLayout(context);
        placeholderView = new View(context);
        badge.addView(placeholderView, LayoutHelper.createFrame(BADGE_SIZE_DP, BADGE_SIZE_DP, Gravity.CENTER));
        badgeView = new BackupImageView(context);
        badgeView.setVisibility(GONE);
        badge.addView(badgeView, LayoutHelper.createFrame(BADGE_SIZE_DP, BADGE_SIZE_DP, Gravity.CENTER));
        addView(badge, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, RING_SIZE_DP));

        nameView = new TextView(context);
        nameView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 24);
        nameView.setTypeface(AndroidUtilities.bold());
        nameView.setLetterSpacing(-0.015f);
        nameView.setGravity(Gravity.CENTER);
        nameView.setMaxLines(2);
        nameView.setEllipsize(TextUtils.TruncateAt.END);
        addView(nameView, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 12, 0, 0));

        chipsView = new ChipRow(context);
        addView(chipsView, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 10, 0, 0));

        aboutView = new TextView(context);
        aboutView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        aboutView.setGravity(Gravity.CENTER);
        aboutView.setLineSpacing(dp(3), 1f);
        addView(aboutView, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 6, 14, 6, 0));

        primaryView = new LinearLayout(context);
        primaryView.setOrientation(VERTICAL);
        addView(primaryView, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 18, 0, 0));

        secondaryView = new LinearLayout(context);
        secondaryView.setOrientation(HORIZONTAL);
        secondaryView.setBaselineAligned(false);
        addView(secondaryView, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 10, 0, 0));
    }

    public void set(String id, String name, String icon, CharSequence about, String version, CharSequence author,
                    CharSequence state, boolean stateAlert, boolean enabled, List<Action> actions) {
        pluginId = id;
        this.version = version;
        this.author = author;
        this.state = state;
        this.stateAlert = stateAlert;
        pluginEnabled = enabled;
        accent = readableAccent(FGPluginIcon.accentFor(id));

        placeholderView.setBackground(new FGPluginAvatar(getContext(), id, icon, 0.3f));
        placeholderView.setVisibility(VISIBLE);
        badgeView.setVisibility(GONE);
        FGPluginIcon.load(badgeView, icon, BADGE_SIZE_DP, () -> placeholderView.setVisibility(GONE));
        applyBadgeState();

        nameView.setText(name);
        FGPluginLinks.setText(aboutView, about, Theme.getColor(Theme.key_windowBackgroundWhiteLinkText, resourcesProvider));
        aboutView.setVisibility(TextUtils.isEmpty(about) ? GONE : VISIBLE);
        lastActions = actions == null ? new ArrayList<>() : actions;
        glowWidth = 0;
        ringState = -1;
        updateColors();
    }

    private int readableAccent(int color) {
        int background = Theme.getColor(Theme.key_windowBackgroundWhite, resourcesProvider);
        int contrast = ColorUtils.calculateLuminance(background) > .5 ? Color.BLACK : Color.WHITE;
        color = ColorUtils.setAlphaComponent(color, 255);
        for (int i = 0; i < 12 && ColorUtils.calculateContrast(color, ColorUtils.setAlphaComponent(background, 255)) < 4.5; i++) {
            color = ColorUtils.blendARGB(color, contrast, .18f);
        }
        return color;
    }

    private int readableAccent(int color, int background) {
        background = ColorUtils.setAlphaComponent(background, 255);
        int contrast = ColorUtils.calculateContrast(Color.BLACK, background) >= ColorUtils.calculateContrast(Color.WHITE, background)
                ? Color.BLACK : Color.WHITE;
        color = ColorUtils.setAlphaComponent(color, 255);
        for (int i = 0; i < 12 && ColorUtils.calculateContrast(color, ColorUtils.setAlphaComponent(background, 255)) < 4.5; i++) {
            color = ColorUtils.blendARGB(color, contrast, .18f);
        }
        return ColorUtils.calculateContrast(color, background) >= 4.5 ? color : contrast;
    }

    private void applyBadgeState() {
        float alpha = pluginEnabled ? 1f : .78f;
        badge.setAlpha(alpha);
        if (pluginEnabled) {
            badgeView.getImageReceiver().setColorFilter(null);
        } else {
            ColorMatrix matrix = new ColorMatrix();
            matrix.setSaturation(.15f);
            badgeView.getImageReceiver().setColorFilter(new ColorMatrixColorFilter(matrix));
        }
        badgeView.invalidate();
    }

    private void fillChips() {
        final int text = Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider);
        final int gray = Theme.getColor(Theme.key_windowBackgroundWhiteGrayText, resourcesProvider);
        final int link = Theme.getColor(Theme.key_windowBackgroundWhiteLinkText, resourcesProvider);
        chipsView.removeAllViews();
        if (!TextUtils.isEmpty(state)) {
            int stateColor = readableAccent(stateAlert ? Theme.getColor(Theme.key_text_RedBold, resourcesProvider)
                    : pluginEnabled ? 0xFF2FB35C : gray);
            chipsView.addView(chip(state, stateColor, Theme.multAlpha(stateColor, .14f), stateColor, true));
        }
        if (!TextUtils.isEmpty(version)) {
            chipsView.addView(chip(version.startsWith("v") || !Character.isDigit(version.charAt(0)) ? version : "v" + version,
                    gray, Theme.multAlpha(text, .07f), 0, false));
        }
        if (!TextUtils.isEmpty(author)) {
            int background = ColorUtils.compositeColors(Theme.multAlpha(text, .07f),
                    ColorUtils.setAlphaComponent(Theme.getColor(Theme.key_windowBackgroundWhite, resourcesProvider), 255));
            TextView authorChip = chip(null, readableAccent(gray, background), background, 0, false);
            FGPluginLinks.setText(authorChip, author, readableAccent(link, background));
            authorChip.setMaxLines(2);
            authorChip.setSingleLine(false);
            chipsView.addView(authorChip);
        }
        chipsView.setVisibility(chipsView.getChildCount() == 0 ? GONE : VISIBLE);
    }

    private TextView chip(CharSequence value, int textColor, int background, int dotColor, boolean bold) {
        TextView view = new TextView(getContext());
        view.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        if (bold) view.setTypeface(AndroidUtilities.bold());
        view.setMaxLines(1);
        view.setEllipsize(TextUtils.TruncateAt.END);
        view.setGravity(Gravity.CENTER);
        view.setTextColor(textColor);
        view.setMinHeight(dp(28));
        view.setPadding(dp(dotColor != 0 ? 22 : 12), dp(4), dp(12), dp(4));
        if (value != null) view.setText(value);
        view.setBackground(new ChipBackground(background, dotColor));
        return view;
    }

    private void fillActions() {
        primaryView.removeAllViews();
        secondaryView.removeAllViews();
        final int text = Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider);
        final int onAccent = ColorUtils.calculateContrast(Color.BLACK, accent) >= ColorUtils.calculateContrast(Color.WHITE, accent) ? Color.BLACK : Color.WHITE;
        int secondaryCount = 0;
        for (Action action : lastActions) {
            if (!action.accent) secondaryCount++;
        }
        for (Action action : lastActions) {
            if (action.accent) {
                LinearLayout button = new LinearLayout(getContext());
                button.setOrientation(HORIZONTAL);
                button.setGravity(Gravity.CENTER);
                button.setMinimumHeight(dp(52));
                button.setPadding(dp(16), dp(12), dp(16), dp(12));
                button.setBackground(Theme.createSimpleSelectorRoundRectDrawable(dp(26), accent,
                        Theme.multAlpha(onAccent, .18f)));
                ImageView icon = new ImageView(getContext());
                icon.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
                icon.setImageResource(action.icon);
                icon.setColorFilter(new PorterDuffColorFilter(onAccent, PorterDuff.Mode.SRC_IN));
                button.addView(icon, LayoutHelper.createLinear(22, 22, Gravity.CENTER_VERTICAL, 0, 0, 10, 0));
                TextView label = new TextView(getContext());
                label.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
                label.setTypeface(AndroidUtilities.bold());
                label.setTextColor(onAccent);
                label.setMaxLines(3);
                label.setEllipsize(TextUtils.TruncateAt.END);
                label.setText(action.text);
                button.addView(label, LayoutHelper.createLinear(0, LayoutHelper.WRAP_CONTENT, 1f, Gravity.CENTER_VERTICAL));
                label.setGravity(Gravity.CENTER);
                button.setContentDescription(action.text);
                button.setEnabled(actionsEnabled);
                button.setAlpha(actionsEnabled ? 1f : .5f);
                button.setOnClickListener(v -> {
                    if (action.run != null) action.run.run();
                });
                ScaleStateListAnimator.apply(button, .03f, 1.2f);
                primaryView.addView(button, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                        0, primaryView.getChildCount() == 0 ? 0 : 8, 0, 0));
            } else {
                LinearLayout tile = new LinearLayout(getContext());
                tile.setOrientation(VERTICAL);
                tile.setGravity(Gravity.CENTER);
                tile.setMinimumHeight(dp(66));
                tile.setPadding(dp(4), dp(10), dp(4), dp(10));
                tile.setBackground(Theme.createSimpleSelectorRoundRectDrawable(dp(18), Theme.multAlpha(text, .06f),
                        Theme.multAlpha(text, .12f)));
                ImageView icon = new ImageView(getContext());
                icon.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
                icon.setImageResource(action.icon);
                icon.setColorFilter(new PorterDuffColorFilter(text, PorterDuff.Mode.SRC_IN));
                tile.addView(icon, LayoutHelper.createLinear(24, 24));
                TextView label = new TextView(getContext());
                label.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
                label.setTypeface(AndroidUtilities.bold());
                label.setTextColor(text);
                label.setMaxLines(3);
                label.setEllipsize(TextUtils.TruncateAt.END);
                label.setGravity(Gravity.CENTER);
                label.setText(action.text);
                tile.addView(label, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 6, 0, 0));
                tile.setContentDescription(action.text);
                tile.setEnabled(actionsEnabled);
                tile.setAlpha(actionsEnabled ? 1f : .5f);
                tile.setOnClickListener(v -> {
                    if (action.run != null) action.run.run();
                });
                ScaleStateListAnimator.apply(tile, .05f, 1.2f);
                int gap = secondaryView.getChildCount() == 0 ? 0 : 8;
                secondaryView.addView(tile, LayoutHelper.createLinear(0, LayoutHelper.WRAP_CONTENT, 1f,
                        LocaleController.isRTL ? 0 : gap, 0, LocaleController.isRTL ? gap : 0, 0));
            }
        }
        primaryView.setVisibility(primaryView.getChildCount() == 0 ? GONE : VISIBLE);
        secondaryView.setVisibility(secondaryCount == 0 ? GONE : VISIBLE);
        ((LinearLayout.LayoutParams) secondaryView.getLayoutParams()).topMargin = primaryView.getChildCount() == 0 ? dp(18) : dp(10);
    }

    public void setActionsEnabled(boolean enabled) {
        actionsEnabled = enabled;
        for (LinearLayout group : new LinearLayout[]{primaryView, secondaryView}) {
            for (int i = 0; i < group.getChildCount(); i++) {
                group.getChildAt(i).setEnabled(enabled);
                group.getChildAt(i).setAlpha(enabled ? 1f : .5f);
            }
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = MeasureSpec.getSize(widthMeasureSpec) - getPaddingLeft() - getPaddingRight();
        boolean stacked = width < dp(300) || getResources().getConfiguration().fontScale > 1.25f;
        secondaryView.setOrientation(stacked ? VERTICAL : HORIZONTAL);
        for (int i = 0; i < secondaryView.getChildCount(); i++) {
            LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) secondaryView.getChildAt(i).getLayoutParams();
            params.width = stacked ? LayoutHelper.MATCH_PARENT : 0;
            params.weight = stacked ? 0f : 1f;
            params.leftMargin = !stacked && !LocaleController.isRTL && i > 0 ? dp(8) : 0;
            params.rightMargin = !stacked && LocaleController.isRTL && i > 0 ? dp(8) : 0;
            params.topMargin = stacked && i > 0 ? dp(8) : 0;
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public void updateColors() {
        final int text = Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider);
        final int gray = Theme.getColor(Theme.key_windowBackgroundWhiteGrayText, resourcesProvider);
        final int link = Theme.getColor(Theme.key_windowBackgroundWhiteLinkText, resourcesProvider);
        if (pluginId != null) accent = readableAccent(FGPluginIcon.accentFor(pluginId));
        nameView.setTextColor(text);
        aboutView.setTextColor(gray);
        aboutView.setLinkTextColor(link);
        fillChips();
        fillActions();
        glowWidth = 0;
        ringState = -1;
        invalidate();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (introPlayed) {
            invalidate();
            return;
        }
        introPlayed = true;
        if (!SharedConfig.animationsEnabled() || LiteMode.isPowerSaverApplied()
                || android.os.Build.VERSION.SDK_INT >= 26 && !android.animation.ValueAnimator.areAnimatorsEnabled()) return;
        playIntro(badge, 0, .96f);
        playIntro(nameView, 60, 1f);
        playIntro(chipsView, 90, 1f);
        playIntro(aboutView, 120, 1f);
        playIntro(primaryView, 150, 1f);
        playIntro(secondaryView, 180, 1f);
    }

    @Override
    protected void onDetachedFromWindow() {
        for (View view : new View[]{badge, nameView, chipsView, aboutView, primaryView, secondaryView}) {
            view.animate().cancel();
            view.setAlpha(view == badge ? pluginEnabled ? 1f : .78f : 1f);
            view.setScaleX(1f);
            view.setScaleY(1f);
            view.setTranslationY(0f);
        }
        super.onDetachedFromWindow();
    }

    private void playIntro(View view, long delay, float fromScale) {
        if (view == null) return;
        final float target = view == badge ? badge.getAlpha() : 1f;
        view.setAlpha(0f);
        view.setScaleX(fromScale);
        view.setScaleY(fromScale);
        view.setTranslationY(fromScale == 1f ? dp(10) : 0);
        view.animate().alpha(target).scaleX(1f).scaleY(1f).translationY(0)
                .setStartDelay(delay)
                .setDuration(220)
                .setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT)
                .start();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        final int width = getMeasuredWidth();
        if (width <= 0) return;
        final float cx = width / 2f;
        final float cy = getPaddingTop() + dp(RING_SIZE_DP) / 2f;
        if (glowWidth != width) {
            glowWidth = width;
            int glow = pluginEnabled ? accent : Theme.getColor(Theme.key_windowBackgroundWhiteGrayText, resourcesProvider);
            glowPaint.setShader(new RadialGradient(cx, cy, dp(190),
                    new int[]{Theme.multAlpha(glow, .32f), Theme.multAlpha(glow, .1f), Color.TRANSPARENT},
                    new float[]{0f, .5f, 1f}, Shader.TileMode.CLAMP));
        }
        canvas.drawRect(0, 0, width, getPaddingTop() + dp(RING_SIZE_DP + 140), glowPaint);

        int state = stateAlert ? 2 : pluginEnabled ? 1 : 0;
        if (ringState != state) {
            ringState = state;
            int color = state == 2 ? Theme.getColor(Theme.key_text_RedBold, resourcesProvider)
                    : state == 1 ? accent : Theme.multAlpha(Theme.getColor(Theme.key_windowBackgroundWhiteGrayText, resourcesProvider), .5f);
            if (state == 1) {
                ringPaint.setShader(new SweepGradient(cx, cy, new int[]{color, Theme.multAlpha(color, .15f), color}, null));
            } else {
                ringPaint.setShader(null);
                ringPaint.setColor(color);
            }
        }
        float radius = dp(RING_SIZE_DP) / 2f - dp(4);
        ringPaint.setStrokeWidth(dpf2(3f));
        if (ringPaint.getShader() != null) {
            ringMatrix.setRotate(-90f, cx, cy);
            ringPaint.getShader().setLocalMatrix(ringMatrix);
        }
        canvas.drawCircle(cx, cy, radius, ringPaint);
        super.onDraw(canvas);

    }

    private static final class ChipBackground extends android.graphics.drawable.Drawable {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final android.graphics.RectF rect = new android.graphics.RectF();
        private final int color;
        private final int dot;

        ChipBackground(int color, int dot) {
            this.color = color;
            this.dot = dot;
        }

        @Override
        public void draw(Canvas canvas) {
            rect.set(getBounds());
            paint.setColor(color);
            canvas.drawRoundRect(rect, rect.height() / 2f, rect.height() / 2f, paint);
            if (dot != 0) {
                paint.setColor(dot);
                canvas.drawCircle(rect.left + dp(12), rect.centerY(), dp(3.5f), paint);
            }
        }

        @Override
        public void setAlpha(int alpha) {
        }

        @Override
        public void setColorFilter(android.graphics.ColorFilter colorFilter) {
        }

        @Override
        public int getOpacity() {
            return android.graphics.PixelFormat.TRANSLUCENT;
        }
    }

    private static final class ChipRow extends ViewGroup {
        ChipRow(Context context) {
            super(context);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int width = MeasureSpec.getSize(widthMeasureSpec);
            int gap = dp(6);
            int lineWidth = 0, height = 0, lineHeight = 0;
            for (int i = 0; i < getChildCount(); i++) {
                View child = getChildAt(i);
                child.measure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.AT_MOST), MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
                int cw = child.getMeasuredWidth();
                if (lineWidth > 0 && lineWidth + gap + cw > width) {
                    height += lineHeight + gap;
                    lineWidth = 0;
                    lineHeight = 0;
                }
                lineWidth += (lineWidth > 0 ? gap : 0) + cw;
                lineHeight = Math.max(lineHeight, child.getMeasuredHeight());
            }
            height += lineHeight;
            setMeasuredDimension(width, height);
        }

        @Override
        protected void onLayout(boolean changed, int l, int t, int r, int b) {
            int width = r - l;
            int gap = dp(6);
            int start = 0;
            int top = 0;
            while (start < getChildCount()) {
                int lineWidth = 0, lineHeight = 0, end = start;
                while (end < getChildCount()) {
                    int cw = getChildAt(end).getMeasuredWidth();
                    if (end > start && lineWidth + gap + cw > width) break;
                    lineWidth += (end > start ? gap : 0) + cw;
                    lineHeight = Math.max(lineHeight, getChildAt(end).getMeasuredHeight());
                    end++;
                }
                int x = (width - lineWidth) / 2;
                for (int i = start; i < end; i++) {
                    View child = getChildAt(LocaleController.isRTL ? end - 1 - (i - start) : i);
                    int y = top + (lineHeight - child.getMeasuredHeight()) / 2;
                    child.layout(x, y, x + child.getMeasuredWidth(), y + child.getMeasuredHeight());
                    x += child.getMeasuredWidth() + gap;
                }
                top += lineHeight + gap;
                start = end;
            }
        }
    }
}

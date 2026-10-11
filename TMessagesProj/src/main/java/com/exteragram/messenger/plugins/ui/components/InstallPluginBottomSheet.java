/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.ui.components;

import android.content.Context;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.RectF;
import android.graphics.Shader;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.core.graphics.ColorUtils;

import com.th3nekit.finegram.plugins.FGPluginAbilities;
import com.th3nekit.finegram.plugins.FGPluginIcon;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.BottomSheet;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.BackupImageView;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.LayoutHelper;

import java.util.List;

public class InstallPluginBottomSheet extends BottomSheet {

    private static final int BADGE_SIZE_DP = 56;
    private static final int ABILITY_ICON_DP = 20;
    private static final int ABILITY_CHIP_DP = 32;
    private static final int ABILITY_ROW_DP = 40;

    private static final float GLOW_LENGTH = 0.78f;
    private static final float GLOW_CORNER = 0.15f;

    private static final int ABILITY_ROWS_VISIBLE = 4;

    private static final int DESCRIPTION_MAX_LINES = 6;

    public static class InstallParams {
        public String filePath;
        public String id;
        public String name;
        public String version;
        public String author;
        public String description;
        public String icon;

        public InstallParams(String filePath) {
            this.filePath = filePath;
        }

        public String getFilePath() {
            return filePath;
        }
    }

    public static final class PluginInstallParams {
        private String filePath;
        private boolean trusted;

        public PluginInstallParams(String filePath, boolean trusted) {
            this.filePath = filePath;
            this.trusted = trusted;
        }

        public static PluginInstallParams of(org.telegram.messenger.MessageObject message) {
            if (message == null) {
                return null;
            }
            java.io.File file = null;
            if (message.messageOwner != null && !TextUtils.isEmpty(message.messageOwner.attachPath)) {
                file = new java.io.File(message.messageOwner.attachPath);
            }
            if (file == null || !file.exists()) {
                file = org.telegram.messenger.FileLoader.getInstance(message.currentAccount)
                        .getPathToMessage(message.messageOwner);
            }
            return file == null ? null : new PluginInstallParams(file.getAbsolutePath(), false);
        }

        public String getFilePath() {
            return filePath;
        }

        public void setFilePath(String filePath) {
            this.filePath = filePath;
        }

        public boolean getTrusted() {
            return trusted;
        }

        public void setTrusted(boolean trusted) {
            this.trusted = trusted;
        }
    }

    public interface Delegate {
        void onInstall(InstallParams params);
    }

    public View customView;
    public InstallParams installParams;

    private final Delegate delegate;

    public InstallPluginBottomSheet(Context context, Theme.ResourcesProvider resourcesProvider,
                                    InstallParams params, Delegate delegate) {
        super(context, false, resourcesProvider);
        this.installParams = params;
        this.delegate = delegate;

        fixNavigationBar();
        setApplyBottomPadding(false);

        LinearLayout layout = new LinearLayout(context);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(AndroidUtilities.dp(22), AndroidUtilities.dp(18),
                AndroidUtilities.dp(22), AndroidUtilities.dp(12));

        layout.addView(createHeader(context, params),
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, BADGE_SIZE_DP));

        if (params != null && params.description != null && !params.description.isEmpty()) {
            TextView description = new TextView(context);
            description.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 15);
            description.setTextColor(getThemedColor(Theme.key_dialogTextBlack));
            description.setMaxLines(DESCRIPTION_MAX_LINES);
            description.setEllipsize(TextUtils.TruncateAt.END);
            com.th3nekit.finegram.plugins.FGPluginLinks.setText(description, params.description, getThemedColor(Theme.key_dialogTextLink));
            layout.addView(description, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT,
                    LayoutHelper.WRAP_CONTENT, 0, 16, 0, 0));
        }

        layout.addView(createAbilities(context, params),
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                        0, 16, 0, 0));

        layout.addView(createInstallButton(context),
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 48, 0, 18, 0, 0));

        customView = layout;
        setCustomView(layout);
    }

    private View createHeader(Context context, InstallParams params) {
        final int accent = FGPluginIcon.accentFor(params == null ? null : params.id);

        FrameLayout header = new FrameLayout(context) {
            private final Paint glowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
            private final RectF glow = new RectF();

            @Override
            protected void onSizeChanged(int w, int h, int oldw, int oldh) {
                super.onSizeChanged(w, h, oldw, oldh);
                if (w == 0 || h == 0) {
                    return;
                }
                glow.set(0, 0, w, h);

                glowPaint.setShader(new LinearGradient(0, 0, w * GLOW_LENGTH, 0,
                        new int[]{
                                ColorUtils.setAlphaComponent(accent, 72),
                                ColorUtils.setAlphaComponent(accent, 44),
                                ColorUtils.setAlphaComponent(accent, 16),
                                Color.TRANSPARENT
                        },
                        new float[]{0f, 0.35f, 0.7f, 1f},
                        Shader.TileMode.CLAMP));
            }

            @Override
            protected void onDraw(android.graphics.Canvas canvas) {
                if (glowPaint.getShader() != null) {
                    final float radius = glow.height() * GLOW_CORNER;
                    canvas.drawRoundRect(glow, radius, radius, glowPaint);
                }
                super.onDraw(canvas);
            }
        };
        header.setWillNotDraw(false);

        String name = params == null || params.name == null || params.name.isEmpty()
                ? LocaleController.getString(R.string.FG_Plugins_Install) : params.name;

        TextView letter = new TextView(context);
        letter.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 24);
        letter.setTypeface(AndroidUtilities.bold());
        letter.setTextColor(0xFFFFFFFF);
        letter.setGravity(Gravity.CENTER);
        letter.setText("");
        letter.setBackground(new com.th3nekit.finegram.plugins.FGPluginAvatar(context,
                params == null ? null : params.id, params == null ? null : params.icon, 0.3f));
        header.addView(letter, LayoutHelper.createFrame(BADGE_SIZE_DP, BADGE_SIZE_DP,
                Gravity.LEFT | Gravity.CENTER_VERTICAL));

        BackupImageView badge = new BackupImageView(context);
        badge.setVisibility(View.GONE);
        header.addView(badge, LayoutHelper.createFrame(BADGE_SIZE_DP, BADGE_SIZE_DP,
                Gravity.LEFT | Gravity.CENTER_VERTICAL));
        FGPluginIcon.load(badge, params == null ? null : params.icon, BADGE_SIZE_DP,
                () -> letter.setVisibility(View.GONE));

        LinearLayout texts = new LinearLayout(context);
        texts.setOrientation(LinearLayout.VERTICAL);
        texts.setGravity(Gravity.CENTER_VERTICAL);
        header.addView(texts, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT,
                LayoutHelper.MATCH_PARENT, Gravity.LEFT | Gravity.CENTER_VERTICAL,
                BADGE_SIZE_DP + 14, 0, 0, 0));

        intro(letter, 0, 0.92f);
        intro(badge, 0, 0.92f);
        intro(texts, 50, 1f);

        TextView title = new TextView(context);
        title.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 19);
        title.setTypeface(AndroidUtilities.bold());
        title.setTextColor(getThemedColor(Theme.key_dialogTextBlack));
        title.setMaxLines(1);
        title.setEllipsize(TextUtils.TruncateAt.END);
        title.setText(name);
        texts.addView(title, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT,
                LayoutHelper.WRAP_CONTENT));

        String subtitle = subtitleOf(params);
        if (!subtitle.isEmpty()) {
            TextView author = new TextView(context);
            author.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 13);
            author.setTextColor(getThemedColor(Theme.key_dialogTextGray2));
            author.setMaxLines(1);
            author.setEllipsize(TextUtils.TruncateAt.END);
            com.th3nekit.finegram.plugins.FGPluginLinks.setText(author, subtitle, getThemedColor(Theme.key_dialogTextLink));
            texts.addView(author, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT,
                    LayoutHelper.WRAP_CONTENT, 0, 2, 0, 0));
        }

        return header;
    }

    private View createAbilities(Context context, InstallParams params) {
        LinearLayout box = new LinearLayout(context);
        box.setOrientation(LinearLayout.VERTICAL);

        TextView header = new TextView(context);
        header.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 13);
        header.setTypeface(AndroidUtilities.bold());
        header.setTextColor(getThemedColor(Theme.key_dialogTextGray2));
        header.setText(LocaleController.getString(R.string.FG_Plugin_Abilities));
        box.addView(header, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT,
                LayoutHelper.WRAP_CONTENT, 0, 0, 0, 6));

        List<FGPluginAbilities.Ability> abilities =
                FGPluginAbilities.of(params == null ? null : params.filePath);
        if (abilities.isEmpty()) {
            TextView nothing = new TextView(context);
            nothing.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 15);
            nothing.setTextColor(getThemedColor(Theme.key_dialogTextBlack));
            nothing.setText(LocaleController.getString(R.string.FG_Plugin_NoAbilities));
            box.addView(nothing, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT,
                    LayoutHelper.WRAP_CONTENT));
            return box;
        }

        final int accent = FGPluginIcon.accentFor(params == null ? null : params.id);
        LinearLayout rows = new LinearLayout(context);
        rows.setOrientation(LinearLayout.VERTICAL);
        for (int i = 0; i < abilities.size(); i++) {
            rows.addView(createAbilityRow(context, abilities.get(i), i, accent),
                    LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, ABILITY_ROW_DP));
        }

        if (abilities.size() <= ABILITY_ROWS_VISIBLE) {
            box.addView(rows, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT,
                    LayoutHelper.WRAP_CONTENT));
            return box;
        }

        ScrollView scroll = new ScrollView(context);
        scroll.setVerticalScrollBarEnabled(false);
        scroll.setOverScrollMode(View.OVER_SCROLL_NEVER);
        scroll.addView(rows, new ScrollView.LayoutParams(
                LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
        box.addView(scroll, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT,
                ABILITY_ROW_DP * ABILITY_ROWS_VISIBLE));
        return box;
    }

    private View createAbilityRow(Context context, FGPluginAbilities.Ability ability, int index,
                                  int accent) {
        FrameLayout row = new FrameLayout(context);

        View chip = new View(context);
        chip.setBackground(Theme.createCircleDrawable(AndroidUtilities.dp(ABILITY_CHIP_DP),
                ColorUtils.setAlphaComponent(accent, 36)));
        row.addView(chip, LayoutHelper.createFrame(ABILITY_CHIP_DP, ABILITY_CHIP_DP,
                Gravity.LEFT | Gravity.CENTER_VERTICAL));

        ImageView icon = new ImageView(context);
        icon.setScaleType(ImageView.ScaleType.FIT_CENTER);
        icon.setImageResource(ability.icon);
        icon.setColorFilter(accent, PorterDuff.Mode.SRC_IN);
        row.addView(icon, LayoutHelper.createFrame(ABILITY_ICON_DP, ABILITY_ICON_DP,
                Gravity.LEFT | Gravity.CENTER_VERTICAL,
                (ABILITY_CHIP_DP - ABILITY_ICON_DP) / 2f, 0, 0, 0));

        TextView text = new TextView(context);
        text.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 15);
        text.setTextColor(getThemedColor(Theme.key_dialogTextBlack));
        text.setMaxLines(1);
        text.setEllipsize(TextUtils.TruncateAt.END);
        text.setText(LocaleController.getString(ability.title));
        row.addView(text, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT,
                LayoutHelper.WRAP_CONTENT, Gravity.LEFT | Gravity.CENTER_VERTICAL,
                ABILITY_CHIP_DP + 14, 0, 0, 0));

        row.setAlpha(0f);
        row.setTranslationX(AndroidUtilities.dp(10));
        row.animate().alpha(1f).translationX(0)
                .setStartDelay(90L + index * 45L)
                .setDuration(260)
                .setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT)
                .start();
        return row;
    }

    private View createInstallButton(Context context) {
        TextView install = new TextView(context) {
            @Override
            public boolean onTouchEvent(MotionEvent event) {

                int action = event.getActionMasked();
                if (action == MotionEvent.ACTION_DOWN) {
                    animate().scaleX(0.97f).scaleY(0.97f).setDuration(160)
                            .setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT).start();
                } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                    animate().scaleX(1f).scaleY(1f).setDuration(160)
                            .setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT).start();
                }
                return super.onTouchEvent(event);
            }
        };
        install.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 15);
        install.setTypeface(AndroidUtilities.bold());
        install.setGravity(Gravity.CENTER);
        install.setTextColor(getThemedColor(Theme.key_featuredStickers_buttonText));
        install.setBackground(Theme.createSimpleSelectorRoundRectDrawable(AndroidUtilities.dp(8),
                getThemedColor(Theme.key_featuredStickers_addButton),
                getThemedColor(Theme.key_featuredStickers_addButtonPressed)));
        install.setText(LocaleController.getString(R.string.FG_Plugins_InstallConfirm));
        install.setOnClickListener(v -> {
            dismiss();
            if (this.delegate != null) {
                this.delegate.onInstall(installParams);
            }
        });
        return install;
    }

    private void intro(View view, long delay, float fromScale) {
        view.setAlpha(0f);
        view.setScaleX(fromScale);
        view.setScaleY(fromScale);
        view.setTranslationX(fromScale == 1f ? AndroidUtilities.dp(8) : 0);
        view.animate().alpha(1f).scaleX(1f).scaleY(1f).translationX(0)
                .setStartDelay(delay)
                .setDuration(260)
                .setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT)
                .start();
    }

    private static String subtitleOf(InstallParams params) {
        if (params == null) {
            return "";
        }
        StringBuilder out = new StringBuilder();
        if (params.author != null && !params.author.isEmpty()) {
            out.append(params.author);
        }
        if (params.version != null && !params.version.isEmpty()) {
            if (out.length() > 0) {
                out.append(" · ");
            }
            out.append(params.version);
        }
        return out.toString();
    }
}

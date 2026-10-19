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
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.core.graphics.ColorUtils;

import com.th3nekit.finegram.plugins.FGPluginAvatar;
import com.th3nekit.finegram.plugins.FGPluginIcon;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.BackupImageView;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.ScaleStateListAnimator;

import java.util.ArrayList;
import java.util.List;

/**
 * Шапка экрана плагинов.
 *
 * Карточка отвечает на три вопроса, с которыми сюда приходят: сколько всего стоит, всё ли
 * работает и где взять ещё. Большое число — сколько плагинов, строка под ним — сколько
 * включено и сколько ждут обновления. Справа стопкой лежат значки самих плагинов: по ним
 * экран узнаётся с первого взгляда, а не по заголовку. Ниже три действия, ради которых
 * раньше приходилось листать список кнопок.
 *
 * Появляется один раз за жизнь экрана: карточка подъезжает, значки ложатся друг за другом.
 * Список перерисовывается часто, и повтор этого движения на каждое переключение раздражал бы.
 */
public class FGPluginsHeroCell extends FrameLayout implements Theme.Colorable {

    public interface Delegate {
        void onStore();

        void onInstall();

        void onUpdates();
    }

    public static final class AvatarSpec {
        final String id;
        final String icon;

        public AvatarSpec(String id, String icon) {
            this.id = id;
            this.icon = icon;
        }
    }

    /**
     * Действия — плитки со значком над подписью, а не строки «значок + текст». Три штуки
     * в ряд на узком экране с крупным системным шрифтом оставляют подписи ~70dp, и строчный
     * вариант обрезал даже «Магазин».
     */
    private static final int TILE_DP = 64;
    private static final int MAX_AVATARS = 4;
    private static final int AVATAR_DP = 44;
    private static final int AVATAR_STEP_DP = 30;
    private static final float[] AVATAR_TILT = {-5f, 3f, -2f, 6f};

    private final Theme.ResourcesProvider resourcesProvider;
    private final Paint cardPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint glowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF cardRect = new RectF();

    private final LinearLayout content;
    private final FrameLayout top;
    private final LinearLayout texts;
    private final TextView countView;
    private final TextView wordView;
    private final TextView statsView;
    private final FrameLayout stackView;
    private final TextView storeChip;
    private final TextView installChip;
    private final TextView updatesChip;

    private Delegate delegate;
    private boolean introPlayed;
    private int glowWidth;

    public FGPluginsHeroCell(Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;
        setLayoutDirection(LocaleController.isRTL ? LAYOUT_DIRECTION_RTL : LAYOUT_DIRECTION_LTR);
        setWillNotDraw(false);

        content = new LinearLayout(context);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(dp(20), dp(18), dp(20), dp(18));
        addView(content, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        top = new FrameLayout(context);
        content.addView(top, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        texts = new LinearLayout(context);
        texts.setOrientation(LinearLayout.VERTICAL);
        top.addView(texts, LayoutHelper.createFrameRelatively(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                Gravity.START | Gravity.TOP, 0, 0, AVATAR_DP + AVATAR_STEP_DP * (MAX_AVATARS - 1) + 8, 0));

        countView = new TextView(context);
        countView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 36);
        countView.setTypeface(AndroidUtilities.bold());
        countView.setLetterSpacing(-0.02f);
        countView.setIncludeFontPadding(false);
        countView.setGravity(LayoutHelper.getAbsoluteGravityStart());
        texts.addView(countView, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        wordView = new TextView(context);
        wordView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        wordView.setTypeface(AndroidUtilities.bold());
        wordView.setSingleLine(false);
        wordView.setGravity(LayoutHelper.getAbsoluteGravityStart());
        texts.addView(wordView, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 2, 0, 0));

        statsView = new TextView(context);
        statsView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        statsView.setSingleLine(false);
        statsView.setGravity(LayoutHelper.getAbsoluteGravityStart());
        texts.addView(statsView, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 4, 0, 0));

        stackView = new FrameLayout(context);
        stackView.setClipChildren(false);
        stackView.setPadding(dp(2), 0, dp(2), 0);
        top.addView(stackView, LayoutHelper.createFrameRelatively(AVATAR_DP + AVATAR_STEP_DP * (MAX_AVATARS - 1) + 4, AVATAR_DP + 12,
                Gravity.END | Gravity.CENTER_VERTICAL));

        final LinearLayout chips = new LinearLayout(context);
        chips.setOrientation(LinearLayout.HORIZONTAL);
        chips.setBaselineAligned(false);
        chips.setMinimumHeight(dp(TILE_DP));
        content.addView(chips, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 18, 0, 0));

        storeChip = chip(context);
        installChip = chip(context);
        updatesChip = chip(context);
        chips.addView(storeChip, LayoutHelper.createLinear(0, LayoutHelper.MATCH_PARENT, 1f));
        chips.addView(installChip, LayoutHelper.createLinear(0, LayoutHelper.MATCH_PARENT, 1f,
                LocaleController.isRTL ? 0 : 8, 0, LocaleController.isRTL ? 8 : 0, 0));
        chips.addView(updatesChip, LayoutHelper.createLinear(0, LayoutHelper.MATCH_PARENT, 1f,
                LocaleController.isRTL ? 0 : 8, 0, LocaleController.isRTL ? 8 : 0, 0));
        storeChip.setOnClickListener(v -> {
            if (delegate != null) delegate.onStore();
        });
        installChip.setOnClickListener(v -> {
            if (delegate != null) delegate.onInstall();
        });
        updatesChip.setOnClickListener(v -> {
            if (delegate != null) delegate.onUpdates();
        });
    }

    private TextView chip(Context context) {
        final TextView view = new TextView(context);
        view.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        view.setTypeface(AndroidUtilities.bold());
        view.setGravity(Gravity.CENTER);
        view.setSingleLine(false);
        view.setMinWidth(dp(48));
        view.setMinHeight(dp(TILE_DP));
        view.setPadding(dp(4), dp(10), dp(4), dp(8));
        view.setCompoundDrawablePadding(dp(5));
        ScaleStateListAnimator.apply(view, 0.04f, 1.2f);
        return view;
    }

    public void setDelegate(Delegate delegate) {
        this.delegate = delegate;
    }

    private int accent() {
        return Theme.getColor(Theme.key_featuredStickers_addButton, resourcesProvider);
    }

    /**
     * @param word слово при числе: «плагин», «плагина», «плагинов»
     * @param stats строка под ним: что включено и что ждёт обновления
     */
    public void set(int total, CharSequence word, CharSequence stats,
                    CharSequence store, int storeIcon, CharSequence install, int installIcon,
                    CharSequence updates, int updatesIcon, boolean updatesHighlighted,
                    List<AvatarSpec> avatars) {
        final int accent = accent();
        final int text = Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider);
        countView.setText(String.valueOf(total));
        countView.setTextColor(text);
        wordView.setText(word);
        wordView.setTextColor(text);
        statsView.setText(stats);
        statsView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteGrayText, resourcesProvider));

        styleChip(storeChip, store, storeIcon, true, accent);
        styleChip(installChip, install, installIcon, false, accent);
        styleChip(updatesChip, updates, updatesIcon, updatesHighlighted, accent);

        fillStack(avatars);
        glowWidth = 0;
        invalidate();
    }

    private void styleChip(TextView chip, CharSequence text, int icon, boolean filled, int accent) {
        chip.setTag(filled);
        final int foreground = filled
                ? Theme.getColor(Theme.key_featuredStickers_buttonText, resourcesProvider)
                : accent;
        chip.setText(text);
        chip.setTextColor(foreground);
        chip.setBackground(Theme.createSimpleSelectorRoundRectDrawable(dp(18),
                filled ? accent : ColorUtils.setAlphaComponent(accent, 30),
                ColorUtils.setAlphaComponent(filled ? Color.WHITE : accent, 40)));
        final Drawable drawable = icon == 0 ? null : getContext().getResources().getDrawable(icon, null).mutate();
        if (drawable != null) {
            drawable.setColorFilter(new PorterDuffColorFilter(foreground, PorterDuff.Mode.SRC_IN));
            drawable.setBounds(0, 0, dp(24), dp(24));
        }
        chip.setCompoundDrawables(null, drawable, null, null);
    }

    @Override
    public void updateColors() {
        final int text = Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider);
        countView.setTextColor(text);
        wordView.setTextColor(text);
        statsView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteGrayText, resourcesProvider));
        final int accent = accent();
        for (TextView chip : new TextView[]{storeChip, installChip, updatesChip}) {
            final boolean filled = Boolean.TRUE.equals(chip.getTag());
            final int foreground = filled ? Theme.getColor(Theme.key_featuredStickers_buttonText, resourcesProvider) : accent;
            chip.setTextColor(foreground);
            chip.setBackground(Theme.createSimpleSelectorRoundRectDrawable(dp(18),
                    filled ? accent : ColorUtils.setAlphaComponent(accent, 30),
                    ColorUtils.setAlphaComponent(filled ? Color.WHITE : accent, 40)));
            for (Drawable drawable : chip.getCompoundDrawables()) {
                if (drawable != null) drawable.setColorFilter(new PorterDuffColorFilter(foreground, PorterDuff.Mode.SRC_IN));
            }
        }
        for (int i = 0; i < stackView.getChildCount(); i++) {
            stackView.getChildAt(i).setBackground(Theme.createRoundRectDrawable(dp(15), sectionColor()));
        }
        invalidate();
    }

    /** Значки плагинов стопкой: каждый чуть повёрнут, как карточки, брошенные на стол. */
    private void fillStack(List<AvatarSpec> avatars) {
        stackView.removeAllViews();
        final int count = Math.min(MAX_AVATARS, avatars == null ? 0 : avatars.size());
        stackView.setVisibility(count == 0 ? GONE : VISIBLE);
        for (int i = count - 1; i >= 0; i--) {
            final AvatarSpec spec = avatars.get(i);
            final FrameLayout holder = new FrameLayout(getContext());
            holder.setPadding(dp(2), dp(2), dp(2), dp(2));
            holder.setBackground(Theme.createRoundRectDrawable(dp(15), sectionColor()));

            final View placeholder = new View(getContext());
            placeholder.setBackground(new FGPluginAvatar(getContext(), spec.id, spec.icon, 14f / 44f));
            holder.addView(placeholder, LayoutHelper.createFrame(AVATAR_DP - 4, AVATAR_DP - 4));

            final BackupImageView icon = new BackupImageView(getContext());
            icon.setVisibility(GONE);
            holder.addView(icon, LayoutHelper.createFrame(AVATAR_DP - 4, AVATAR_DP - 4));
            FGPluginIcon.load(icon, spec.icon, AVATAR_DP, () -> placeholder.setVisibility(GONE));

            holder.setRotation(AVATAR_TILT[i % AVATAR_TILT.length]);
            final int right = (count - 1 - i) * AVATAR_STEP_DP;
            stackView.addView(holder, LayoutHelper.createFrameRelatively(AVATAR_DP, AVATAR_DP,
                    Gravity.END | Gravity.CENTER_VERTICAL, 0, 0, right, 0));
        }
    }

    private int sectionColor() {
        return ColorUtils.blendARGB(Theme.getColor(Theme.key_windowBackgroundWhite, resourcesProvider),
                accent(), 0.05f);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        final int available = Math.max(0, MeasureSpec.getSize(widthMeasureSpec)
                - content.getPaddingLeft() - content.getPaddingRight());
        final int count = stackView.getChildCount();
        final int minimumStackWidth = dp(AVATAR_DP) + stackView.getPaddingLeft() + stackView.getPaddingRight();
        final int desiredStackWidth = count == 0 ? 0 : minimumStackWidth + dp(AVATAR_STEP_DP) * (count - 1);
        final int countWidth = (int) Math.ceil(countView.getPaint().measureText(countView.getText().toString()));
        final int stackWidth = count == 0 ? 0 : Math.min(desiredStackWidth,
                Math.max(minimumStackWidth, available - Math.max(dp(100), countWidth) - dp(8)));
        final int reserved = count == 0 ? 0 : stackWidth + dp(8);
        final LayoutParams textParams = (LayoutParams) texts.getLayoutParams();
        textParams.leftMargin = LocaleController.isRTL ? reserved : 0;
        textParams.rightMargin = LocaleController.isRTL ? 0 : reserved;
        final LayoutParams stackParams = (LayoutParams) stackView.getLayoutParams();
        stackParams.width = stackWidth;
        final int step = count < 2 ? 0 : Math.max(0,
                Math.min(dp(AVATAR_STEP_DP), (stackWidth - minimumStackWidth) / (count - 1)));
        for (int i = 0; i < count; i++) {
            final LayoutParams params = (LayoutParams) stackView.getChildAt(i).getLayoutParams();
            params.leftMargin = LocaleController.isRTL ? i * step : 0;
            params.rightMargin = LocaleController.isRTL ? 0 : i * step;
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * Фон карточки рисует сам список: шапка у него — отдельная секция со скруглёнными краями.
     * Здесь только свечение поверх, обрезанное по тому же скруглению.
     */
    @Override
    protected void onDraw(Canvas canvas) {
        cardRect.set(0, 0, getMeasuredWidth(), getMeasuredHeight());
        final float radius = dp(16);
        cardPaint.setColor(ColorUtils.setAlphaComponent(accent(), 14));
        canvas.drawRoundRect(cardRect, radius, radius, cardPaint);

        // Свечение в углу, где лежат значки: оно собирает взгляд к ним и не даёт карточке
        // выглядеть плоской заливкой.
        if (glowWidth != getMeasuredWidth()) {
            glowWidth = getMeasuredWidth();
            glowPaint.setShader(new RadialGradient(LocaleController.isRTL ? cardRect.left + dp(40) : cardRect.right - dp(40), cardRect.top + dp(36),
                    Math.max(1, cardRect.width() * 0.62f),
                    new int[]{ColorUtils.setAlphaComponent(accent(), 58), ColorUtils.setAlphaComponent(accent(), 18), Color.TRANSPARENT},
                    new float[]{0f, 0.5f, 1f}, Shader.TileMode.CLAMP));
        }
        canvas.drawRoundRect(cardRect, radius, radius, glowPaint);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (introPlayed) {
            return;
        }
        introPlayed = true;
        setAlpha(0f);
        setTranslationY(dp(12));
        animate().alpha(1f).translationY(0).setDuration(300)
                .setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT).start();
        final ArrayList<View> children = new ArrayList<>();
        for (int i = stackView.getChildCount() - 1; i >= 0; i--) {
            children.add(stackView.getChildAt(i));
        }
        for (int i = 0; i < children.size(); i++) {
            final View child = children.get(i);
            final float rotation = child.getRotation();
            child.setAlpha(0f);
            child.setScaleX(0.88f);
            child.setScaleY(0.88f);
            child.setRotation(rotation - 10f);
            child.animate().alpha(1f).scaleX(1f).scaleY(1f).rotation(rotation)
                    .setStartDelay(120 + 50L * i).setDuration(320)
                    .setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT).start();
        }
    }
}

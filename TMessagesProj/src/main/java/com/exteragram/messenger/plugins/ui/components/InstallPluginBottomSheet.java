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
import android.graphics.drawable.GradientDrawable;
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
import com.th3nekit.finegram.plugins.FGPluginInstallReview;
import com.th3nekit.finegram.plugins.FGPluginSecurity;
import com.th3nekit.finegram.plugins.FGPluginsController;

import org.json.JSONObject;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.BottomSheet;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.BackupImageView;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.LayoutHelper;

import java.util.List;
import java.io.File;

/**
 * Спрашивает, ставить ли плагин, и показывает, что именно ставится.
 *
 * Кроме имени и описания здесь видно, к чему плагин обращается в своём коде: он работает
 * внутри клиента и видит то же, что видит клиент, а согласие даётся один раз и надолго.
 * Это не запрет и не проверка на злой умысел — код можно написать хитро, — а честный
 * рассказ о том, что в нём видно.
 *
 * Плагины, которые проверяют устанавливаемое на безопасность, встраиваются сюда: они
 * перехватывают создание этой шторки и дописывают в неё свой разбор. Поэтому и имя класса,
 * и порядок полей, и [customView] — те, что они ожидают.
 */
public class InstallPluginBottomSheet extends BottomSheet {

    private static final int BADGE_SIZE_DP = 56;
    private static final int ABILITY_ICON_DP = 20;
    private static final int ABILITY_CHIP_DP = 32;
    private static final int ABILITY_ROW_DP = 40;

    /**
     * Свечение за значком: докуда тянется и насколько скруглены углы.
     *
     * Гаснет до правого края — иначе цвет упирается в границу и у свечения видно конец,
     * а его быть не должно: оно рассеивается. Углы скруглены слегка, только чтобы не
     * торчали, — полная скругление превращало полосу в таблетку.
     */
    private static final float GLOW_LENGTH = 0.78f;
    private static final float GLOW_CORNER = 0.15f;

    /** Сколько строк перечня видно сразу: остальные листаются, кнопка остаётся на экране. */
    private static final int ABILITY_ROWS_VISIBLE = 4;

    /** Описание автор пишет какое хочет: длинное обрезаем, окно не должно занимать экран. */
    private static final int DESCRIPTION_MAX_LINES = 6;

    /** Что ставим: путь к файлу и то, что удалось прочитать из его шапки. */
    public static class InstallParams {
        public String filePath;
        public String id;
        public String name;
        public String version;
        public String author;
        public String description;
        public String icon;
        public final String expectedFingerprint;
        public final boolean securityRiskAccepted;

        public InstallParams(String filePath) {
            this(null, filePath, null, false);
        }

        public InstallParams(InstallParams source, String filePath, String expectedFingerprint,
                             boolean securityRiskAccepted) {
            this.filePath = filePath;
            this.expectedFingerprint = expectedFingerprint;
            this.securityRiskAccepted = securityRiskAccepted;
            if (source != null) {
                id = source.id;
                name = source.name;
                version = source.version;
                author = source.author;
                description = source.description;
                icon = source.icon;
            }
        }

        public String getFilePath() {
            return filePath;
        }
    }

    /**
     * Что установить, в том виде, в каком это передают плагины-менеджеры: путь к файлу и
     * признак «файл проверен». Окно у нас одно и проверку делает само, признак только
     * запоминаем.
     */
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

    /** Что сделать, если человек согласился. */
    public interface Delegate {
        void onInstall(InstallParams params);
    }

    public View customView;
    public InstallParams installParams;

    private final Delegate delegate;
    private LinearLayout securityCard;
    private TextView installButton;
    private ScrollView contentScroll;
    private JSONObject securityReport;
    private String reviewedPath;
    private volatile int reviewGeneration;
    private volatile boolean reviewClosed;
    private boolean checking;
    private final FGPluginInstallReview reviews = new FGPluginInstallReview();
    private FGPluginInstallReview.Snapshot review;

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

        LinearLayout content = new LinearLayout(context);
        content.setOrientation(LinearLayout.VERTICAL);
        content.addView(createHeader(context, params),
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, BADGE_SIZE_DP));

        securityCard = new LinearLayout(context);
        securityCard.setOrientation(LinearLayout.VERTICAL);
        content.addView(securityCard, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT,
                LayoutHelper.WRAP_CONTENT, 0, 16, 0, 0));

        if (params != null && params.description != null && !params.description.isEmpty()) {
            TextView description = new TextView(context);
            description.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 15);
            description.setTextColor(getThemedColor(Theme.key_dialogTextBlack));
            description.setMaxLines(DESCRIPTION_MAX_LINES);
            description.setEllipsize(TextUtils.TruncateAt.END);
            com.th3nekit.finegram.plugins.FGPluginLinks.setText(description, params.description, getThemedColor(Theme.key_dialogTextLink));
            content.addView(description, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT,
                    LayoutHelper.WRAP_CONTENT, 0, 16, 0, 0));
        }

        content.addView(createAbilities(context, params),
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                        0, 16, 0, 0));

        contentScroll = new ScrollView(context) {
            @Override
            protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                int maxHeight = Math.max(AndroidUtilities.dp(120),
                        (int) (AndroidUtilities.displaySize.y * 0.78f) - AndroidUtilities.dp(104));
                int height = MeasureSpec.getSize(heightMeasureSpec);
                if (MeasureSpec.getMode(heightMeasureSpec) != MeasureSpec.UNSPECIFIED) {
                    maxHeight = Math.min(maxHeight, height);
                }
                super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(maxHeight, MeasureSpec.AT_MOST));
            }
        };
        contentScroll.setFillViewport(false);
        contentScroll.setVerticalScrollBarEnabled(false);
        contentScroll.addView(content, new ScrollView.LayoutParams(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
        layout.addView(contentScroll, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        installButton = createInstallButton(context);
        layout.addView(installButton,
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 48, 0, 18, 0, 0));

        customView = layout;
        setCustomView(layout);
        checkPlugin();
    }

    /** Шапка: значок плагина, имя и строка с автором и версией. */
    private View createHeader(Context context, InstallParams params) {
        final int accent = FGPluginIcon.accentFor(params == null ? null : params.id);

        // Свечение в цвете плагина: у каждого плагина цвет свой, и окно установки перестаёт
        // быть безликим — по нему видно, кого ставишь, ещё до того, как прочитал имя.
        //
        // Это не пятно за значком, а полоса от него вправо, которая по дороге сходит на
        // нет. Конца у неё быть не должно — ни ровной черты, ни закруглённого торца:
        // свечение рассеивается, а не заканчивается, поэтому цвет гаснет раньше, чем
        // полоса доходит до края, а углы скруглены лишь настолько, чтобы не торчали.
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
                // Цвет сходит на нет раньше правого края: дальше рисовать уже нечего, и
                // края у свечения не видно — оно просто рассеивается.
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

        // Значок подрастает с 0.92, тексты подъезжают следом: взгляд идёт от плагина к его
        // имени, а не хватает всё разом.
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

    /**
     * Перечень того, к чему плагин обращается.
     *
     * Список кладём в свою прокрутку: у плагина, который делает много, строк набирается
     * столько, что кнопка установки уехала бы за нижний край экрана.
     */
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

    /**
     * Строка перечня.
     *
     * Строки въезжают одна за другой с небольшим сдвигом: так видно, что это перечень, а
     * не одна плотная плашка. Сдвиг короткий — читать список человек начинает сразу.
     */
    private View createAbilityRow(Context context, FGPluginAbilities.Ability ability, int index,
                                  int accent) {
        FrameLayout row = new FrameLayout(context);

        // Значок в кружке цвета плагина: перечень читается списком возможностей, а не
        // россыпью серых картинок, и держится заодно с шапкой.
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

    private TextView createInstallButton(Context context) {
        TextView install = new TextView(context) {
            @Override
            public boolean onTouchEvent(MotionEvent event) {
                if (!isEnabled()) return false;
                // Нажатие должно быть заметно сразу: кнопка чуть проседает под пальцем и
                // возвращается, когда его убрали.
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
        install.setEnabled(false);
        install.setAlpha(0.55f);
        install.setOnClickListener(v -> installReviewed());
        return install;
    }

    private void checkPlugin() {
        if (reviewClosed || isDismissed()) return;
        final int generation = ++reviewGeneration;
        final String path = installParams == null ? null : installParams.filePath;
        final Context context = getContext().getApplicationContext();
        closeReview();
        checking = true;
        securityReport = null;
        reviewedPath = path;
        updateReview(false);
        FGPluginsController.INSTANCE.getQueue().postRunnable(() -> {
            FGPluginInstallReview.Snapshot result = reviews.inspect(context, path,
                    () -> reviewClosed || generation != reviewGeneration);
            AndroidUtilities.runOnUIThread(() -> {
                if (reviewClosed || isDismissed() || generation != reviewGeneration) {
                    result.close();
                    return;
                }
                if (!TextUtils.equals(path, installParams == null ? null : installParams.filePath)) {
                    result.close();
                    checkPlugin();
                    return;
                }
                review = result;
                securityReport = result.report;
                checking = false;
                updateReview(false);
            });
        });
    }

    private void installReviewed() {
        if (reviewClosed || isDismissed() || checking || securityReport == null) return;
        if (review == null || review.file == null || !securityReport.optBoolean("reviewable")
                || securityReport.optString("fingerprint").isEmpty()) {
            checkPlugin();
            return;
        }
        if (installParams == null || !TextUtils.equals(reviewedPath, installParams.filePath)) {
            checkPlugin();
            return;
        }
        final JSONObject reviewed = securityReport;
        final String fingerprint = reviewed.optString("fingerprint");
        final boolean acceptsRisk = FGPluginSecurity.hasFindings(reviewed);
        final String path = reviewedPath;
        final Context context = getContext().getApplicationContext();
        final int generation = ++reviewGeneration;
        checking = true;
        installButton.setEnabled(false);
        installButton.setAlpha(0.55f);
        installButton.setText(LocaleController.getString(R.string.FG_Plugins_Security_Checking));
        FGPluginsController.INSTANCE.getQueue().postRunnable(() -> {
            FGPluginInstallReview.Snapshot current = reviews.inspect(context, path,
                    () -> reviewClosed || generation != reviewGeneration);
            final boolean allowed = current.file != null
                    && FGPluginSecurity.canInstall(reviewed, current.report, acceptsRisk);
            AndroidUtilities.runOnUIThread(() -> {
                if (reviewClosed || isDismissed() || generation != reviewGeneration || !isShowing()) {
                    current.close();
                    return;
                }
                checking = false;
                if (installParams == null || !TextUtils.equals(path, installParams.filePath)) {
                    current.close();
                    checkPlugin();
                    return;
                }
                closeReview();
                if (!allowed) {
                    review = current;
                    securityReport = current.report;
                    updateReview(true);
                    contentScroll.smoothScrollTo(0, 0);
                    return;
                }
                if (delegate == null) {
                    current.close();
                    dismiss();
                    return;
                }
                File file = current.takeFile();
                if (file == null) {
                    current.close();
                    checkPlugin();
                    return;
                }
                InstallParams params = new InstallParams(installParams, file.getAbsolutePath(),
                        fingerprint, acceptsRisk);
                cancelReview();
                try {
                    delegate.onInstall(params);
                } finally {
                    dismiss();
                }
            });
        });
    }

    private void closeReview() {
        if (review != null) {
            review.close();
            review = null;
        }
    }

    private void cancelReview() {
        reviewClosed = true;
        ++reviewGeneration;
        closeReview();
        reviews.close();
    }

    private void updateReview(boolean changed) {
        Context context = getContext();
        securityCard.removeAllViews();
        boolean loading = securityReport == null;
        boolean readable = !loading && review != null && review.file != null
                && securityReport.optBoolean("reviewable") && !securityReport.optString("fingerprint").isEmpty();
        boolean unchecked = !loading && ("error".equals(securityReport.optString("status"))
                || "skipped".equals(securityReport.optString("status")));
        boolean risk = !loading && FGPluginSecurity.hasFindings(securityReport);
        boolean warning = !loading && (!readable || risk || changed);
        int background = getThemedColor(Theme.key_dialogBackground);
        boolean dark = ColorUtils.calculateLuminance(background) < 0.4;
        int red = dark ? 0xFFFF969B : 0xFFB91C30;
        securityCard.setPadding(warning ? AndroidUtilities.dp(14) : 0, warning ? AndroidUtilities.dp(14) : 0,
                warning ? AndroidUtilities.dp(14) : 0, warning ? AndroidUtilities.dp(14) : 0);
        if (warning) {
            GradientDrawable card = new GradientDrawable();
            card.setCornerRadius(AndroidUtilities.dp(16));
            card.setColor(ColorUtils.blendARGB(background, red, dark ? 0.10f : 0.06f));
            card.setStroke(AndroidUtilities.dp(1), ColorUtils.blendARGB(background, red, 0.38f));
            securityCard.setBackground(card);
            LinearLayout heading = new LinearLayout(context);
            heading.setGravity(Gravity.CENTER_VERTICAL);
            ImageView icon = new ImageView(context);
            icon.setImageResource(R.drawable.warning_sign);
            icon.setColorFilter(red, PorterDuff.Mode.SRC_IN);
            heading.addView(icon, LayoutHelper.createLinear(24, 24));
            TextView title = securityText(context, LocaleController.getString(!readable || unchecked
                    ? R.string.FG_Plugins_Security_UncheckedTitle : risk
                    ? R.string.FG_Plugins_Security_RiskTitle : R.string.FG_Plugins_Security_ChangedTitle), 16, red);
            title.setTypeface(AndroidUtilities.bold());
            LinearLayout.LayoutParams titleParams = LayoutHelper.createLinear(0, LayoutHelper.WRAP_CONTENT, 1f);
            titleParams.setMarginStart(AndroidUtilities.dp(10));
            heading.addView(title, titleParams);
            securityCard.addView(heading, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
            if (changed) addSecurityText(LocaleController.getString(R.string.FG_Plugins_Security_Changed), red, 10);
            if (readable) {
                if (unchecked) addSecurityText(LocaleController.getString(R.string.FG_Plugins_CheckUnavailable),
                        getThemedColor(Theme.key_dialogTextBlack), 10);
                String summary = FGPluginSecurity.findingsSummary(securityReport);
                if (!summary.isEmpty()) addSecurityText(summary, getThemedColor(Theme.key_dialogTextBlack), 10);
                addSecurityText(LocaleController.getString(R.string.FG_Plugins_Security_Warning),
                        getThemedColor(Theme.key_dialogTextBlack), 10);
            } else {
                addSecurityText(LocaleController.getString(R.string.FG_Plugins_Security_Unreadable),
                        getThemedColor(Theme.key_dialogTextBlack), 10);
            }
        } else {
            securityCard.setBackground(null);
            addSecurityText(LocaleController.getString(loading ? R.string.FG_Plugins_Security_Checking
                    : R.string.FG_Plugins_Security_ScanDone), getThemedColor(Theme.key_dialogTextGray2), 0);
        }
        installButton.setEnabled(!checking);
        installButton.setAlpha(checking ? 0.55f : 1f);
        installButton.setText(LocaleController.getString(loading ? R.string.FG_Plugins_Security_Checking
                : !readable ? R.string.FG_Plugins_Security_Retry
                : risk ? R.string.FG_Plugins_Security_InstallRisk : R.string.FG_Plugins_InstallConfirm));
        installButton.setBackground(Theme.createSimpleSelectorRoundRectDrawable(AndroidUtilities.dp(12),
                warning ? (dark ? 0xFFBD3549 : 0xFFB91C30) : getThemedColor(Theme.key_featuredStickers_addButton),
                warning ? 0xFF922036 : getThemedColor(Theme.key_featuredStickers_addButtonPressed)));
        installButton.setTextColor(warning ? Color.WHITE : getThemedColor(Theme.key_featuredStickers_buttonText));
    }

    private TextView securityText(Context context, String text, int size, int color) {
        TextView view = new TextView(context);
        view.setTextSize(TypedValue.COMPLEX_UNIT_DIP, size);
        view.setTextColor(color);
        view.setText(text);
        view.setGravity((LocaleController.isRTL ? Gravity.RIGHT : Gravity.LEFT) | Gravity.CENTER_VERTICAL);
        view.setTextDirection(View.TEXT_DIRECTION_LOCALE);
        view.setLineSpacing(AndroidUtilities.dp(2), 1f);
        return view;
    }

    private void addSecurityText(String text, int color, int top) {
        securityCard.addView(securityText(getContext(), text, 14, color),
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, top, 0, 0));
    }

    @Override
    public void dismiss() {
        cancelReview();
        super.dismiss();
    }

    @Override
    public void cancel() {
        cancelReview();
        super.cancel();
    }

    @Override
    public void dismissInternal() {
        cancelReview();
        super.dismissInternal();
    }

    @Override
    public void dismissWithButtonClick(int item) {
        cancelReview();
        super.dismissWithButtonClick(item);
    }

    /** Появление части шапки: короткое, потому что смотреть тут будут на текст. */
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

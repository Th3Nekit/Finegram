/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.outbound;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.format.Formatter;
import android.text.style.RelativeSizeSpan;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.core.graphics.ColorUtils;

import com.th3nekit.finegram.core.ui.ProxySettingsSheet25;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.ProxyListActivity;

import java.text.DateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public final class FGSubscriptionOrbitCell extends LinearLayout implements ProxySettingsSheet25.Themed {
    private final LinearLayout summary;
    private final LinearLayout identity;
    private final LinearLayout orbit;
    private final FrameLayout ringFrame;
    private final RingView ring;
    private final TextView label;
    private final TextView name;
    private final TextView count;
    private final TextView directions;
    private final TextView used;
    private final TextView traffic;
    private final TextView expiry;
    private final TextView state;
    private final LinearLayout actions;
    private final LinearLayout controls;
    private final ActionButton serversButton;
    private final TextView serversText;
    private final ChevronView chevron;
    private final ProxySettingsSheet25.Glyph serverGlyph;
    private final ActionButton refreshButton;
    private final ImageView refreshIcon;
    private final ProxySettingsSheet25.Glyph refreshGlyph;
    private final ProgressBar progress;
    private final ActionButton editButton;
    private final ProxySettingsSheet25.Glyph editGlyph;
    private final LinearLayout serverArea;
    private final View divider;
    private final LinearLayout serverRows;
    private final ActionButton moreButton;
    private final TextView moreText;
    private boolean expanded;
    private boolean busy;
    private boolean expired;
    private boolean failed;

    public FGSubscriptionOrbitCell(Context context) {
        super(context);
        setOrientation(VERTICAL);
        setLayoutDirection(LocaleController.isRTL ? LAYOUT_DIRECTION_RTL : LAYOUT_DIRECTION_LTR);
        setPadding(dp(18), dp(18), dp(18), dp(16));
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);

        summary = new LinearLayout(context);
        summary.setGravity(Gravity.CENTER_VERTICAL);
        identity = new LinearLayout(context);
        identity.setOrientation(VERTICAL);
        label = text(context, 14, false);
        identity.addView(label, wrap());
        name = text(context, 22, true);
        name.setBreakStrategy(android.text.Layout.BREAK_STRATEGY_HIGH_QUALITY);
        name.setHyphenationFrequency(android.text.Layout.HYPHENATION_FREQUENCY_NORMAL);
        identity.addView(name, spaced(4));
        count = text(context, 13, false);
        count.setPadding(dp(10), dp(6), dp(10), dp(6));
        LayoutParams countParams = new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        countParams.topMargin = dp(12);
        countParams.gravity = Gravity.START;
        identity.addView(count, countParams);
        directions = text(context, 13, false);
        identity.addView(directions, spaced(8));
        summary.addView(identity, new LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));

        orbit = new LinearLayout(context);
        orbit.setOrientation(VERTICAL);
        orbit.setGravity(Gravity.CENTER_HORIZONTAL);
        orbit.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES);
        ringFrame = new FrameLayout(context);
        ring = new RingView(context);
        ringFrame.addView(ring, new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        used = text(context, 22, true);
        used.setGravity(Gravity.CENTER);
        used.setTextDirection(TEXT_DIRECTION_LOCALE);
        used.setPadding(dp(10), dp(8), dp(10), dp(8));
        used.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        FrameLayout.LayoutParams usedParams = new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, Gravity.CENTER);
        usedParams.leftMargin = dp(8);
        usedParams.rightMargin = dp(8);
        ringFrame.addView(used, usedParams);
        orbit.addView(ringFrame, new LayoutParams(dp(100), dp(100)));
        traffic = text(context, 13, false);
        traffic.setGravity(Gravity.CENTER);
        traffic.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        orbit.addView(traffic, spaced(6));
        LayoutParams orbitParams = new LayoutParams(dp(124), LayoutParams.WRAP_CONTENT);
        orbitParams.setMarginStart(dp(12));
        summary.addView(orbit, orbitParams);
        addView(summary, wrap());

        expiry = text(context, 13, false);
        addView(expiry, spaced(14));
        actions = new LinearLayout(context);
        actions.setGravity(Gravity.CENTER_VERTICAL);
        serversButton = new ActionButton(context, true);
        LinearLayout serversContent = new LinearLayout(context);
        serversContent.setGravity(Gravity.CENTER_VERTICAL);
        serversContent.setPadding(dp(12), dp(10), dp(12), dp(10));
        ImageView serverIcon = new ImageView(context);
        serverGlyph = new ProxySettingsSheet25.Glyph(ProxySettingsSheet25.Glyph.PROXY);
        serverIcon.setImageDrawable(serverGlyph);
        serverIcon.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        LayoutParams serverIconParams = new LayoutParams(dp(20), dp(20));
        serverIconParams.setMarginEnd(dp(8));
        serversContent.addView(serverIcon, serverIconParams);
        serversText = text(context, 14, true);
        serversText.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        serversContent.addView(serversText, new LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
        chevron = new ChevronView(context);
        LayoutParams chevronParams = new LayoutParams(dp(20), dp(20));
        chevronParams.setMarginStart(dp(8));
        serversContent.addView(chevron, chevronParams);
        serversButton.addView(serversContent, new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, Gravity.CENTER));
        actions.addView(serversButton, new LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));

        controls = new LinearLayout(context);
        controls.setGravity(Gravity.CENTER_VERTICAL | Gravity.END);
        refreshButton = new ActionButton(context, false);
        refreshGlyph = new ProxySettingsSheet25.Glyph(ProxySettingsSheet25.Glyph.ROTATE);
        refreshIcon = icon(context, refreshGlyph);
        refreshButton.addView(refreshIcon, centeredIcon());
        progress = new ProgressBar(context, null, android.R.attr.progressBarStyleSmall);
        progress.setIndeterminate(true);
        progress.setVisibility(GONE);
        progress.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        refreshButton.addView(progress, centeredIcon());
        controls.addView(refreshButton, new LayoutParams(dp(48), dp(48)));
        editButton = new ActionButton(context, false);
        editGlyph = new ProxySettingsSheet25.Glyph(ProxySettingsSheet25.Glyph.EDIT);
        editButton.addView(icon(context, editGlyph), centeredIcon());
        LayoutParams editParams = new LayoutParams(dp(48), dp(48));
        editParams.setMarginStart(dp(8));
        controls.addView(editButton, editParams);
        LayoutParams controlsParams = new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        controlsParams.setMarginStart(dp(8));
        actions.addView(controls, controlsParams);
        addView(actions, spaced(16));
        state = text(context, 13, false);
        state.setVisibility(GONE);
        state.setAccessibilityLiveRegion(ACCESSIBILITY_LIVE_REGION_POLITE);
        addView(state, spaced(12));

        serverArea = new LinearLayout(context);
        serverArea.setOrientation(VERTICAL);
        divider = new View(context);
        divider.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        serverArea.addView(divider, new LayoutParams(LayoutParams.MATCH_PARENT, 1));
        serverRows = new LinearLayout(context);
        serverRows.setOrientation(VERTICAL);
        serverArea.addView(serverRows, spaced(8));
        moreButton = new ActionButton(context, false);
        moreText = text(context, 14, true);
        moreText.setGravity(Gravity.CENTER);
        moreText.setPadding(dp(12), dp(12), dp(12), dp(12));
        moreText.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        moreButton.addView(moreText, new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, Gravity.CENTER));
        moreButton.setVisibility(GONE);
        serverArea.addView(moreButton, spaced(8));
        serverArea.setVisibility(GONE);
        addView(serverArea, spaced(14));
        bind(null, false, false, null, null, null);
    }

    public void bind(FGSubscription source, boolean expanded, boolean busy, Runnable refresh, Runnable edit, Runnable expand) {
        this.expanded = expanded;
        this.busy = busy;
        failed = source != null && source.lastError != null;
        label.setText(FGSubscriptionUi.text("FG_Subscription_Label"));
        String title = source == null || source.name == null ? "" : source.name.trim();
        if (title.isEmpty()) {
            title = FGSubscriptionUi.text("FG_Subscription_DefaultName");
        }
        name.setText(title);
        count.setText(format("FG_Subscription_ServerCount", source == null ? 0 : source.serverCount));
        serversText.setText(FGSubscriptionUi.text("FG_Subscription_Servers"));
        serversButton.setContentDescription(serversText.getText() + ": " + title);
        refreshButton.setContentDescription(FGSubscriptionUi.text(busy ? "FG_Subscription_Updating" : "FG_Subscription_Refresh") + ": " + title);
        editButton.setContentDescription(FGSubscriptionUi.text("FG_Subscription_Edit") + ": " + title);
        moreText.setText(FGSubscriptionUi.text("FG_Subscription_MoreServers"));
        moreButton.setContentDescription(moreText.getText());
        serversButton.setOnClickListener(v -> {
            if (expand != null) expand.run();
        });
        serversButton.setEnabled(source != null && expand != null);
        refreshButton.setOnClickListener(v -> {
            if (refresh != null) refresh.run();
        });
        refreshButton.setEnabled(source != null && refresh != null && !busy);
        editButton.setOnClickListener(v -> {
            if (edit != null) edit.run();
        });
        editButton.setEnabled(source != null && edit != null && !busy);
        refreshIcon.setVisibility(busy ? GONE : VISIBLE);
        progress.setVisibility(busy ? VISIBLE : GONE);
        int skipped = source == null ? 0 : source.skipped;
        state.setText(busy ? FGSubscriptionUi.text("FG_Subscription_Updating") : failed ? FGSubscriptionUi.text("FG_Subscription_SavedServers")
                : skipped > 0 ? FGSubscriptionUi.format("FG_Subscription_Skipped", skipped) : "");
        state.setVisibility(busy || failed || skipped > 0 ? VISIBLE : GONE);
        chevron.invalidate();
        serverArea.setVisibility(expanded ? VISIBLE : GONE);
        bindTraffic(source == null ? null : source.metadata);
        bindExpiry(source == null ? null : source.metadata);
        updateColors();
    }

    public void setServerViews(List<View> rows, boolean more, Runnable showMore) {
        serverRows.removeAllViews();
        if (rows != null) {
            for (View row : rows) {
                if (row == null) continue;
                ViewParent parent = row.getParent();
                if (parent instanceof ViewGroup) {
                    ((ViewGroup) parent).removeView(row);
                }
                serverRows.addView(row, wrap());
            }
        }
        moreButton.setVisibility(more ? VISIBLE : GONE);
        moreButton.setEnabled(more && showMore != null);
        moreButton.setOnClickListener(v -> {
            if (showMore != null) showMore.run();
        });
        serverArea.setVisibility(expanded ? VISIBLE : GONE);
        updateColors();
    }

    private void bindTraffic(FGSubscriptionMetadata metadata) {
        long upload = metadata == null ? -1 : metadata.upload;
        long download = metadata == null ? -1 : metadata.download;
        long total = metadata == null ? -1 : metadata.total;
        boolean known = upload >= 0 && download >= 0 && upload <= Long.MAX_VALUE - download;
        long bytes = known ? upload + download : -1;
        String unknown = FGSubscriptionUi.text("FG_Subscription_TrafficUnknown");
        String amount = known ? Formatter.formatShortFileSize(getContext(), bytes) : "—";
        used.setText(ringText(amount));
        String caption;
        if (total > 0) {
            caption = format("FG_Subscription_OfTotal", amount, Formatter.formatShortFileSize(getContext(), total));
        } else if (total == 0) {
            caption = FGSubscriptionUi.text("FG_Subscription_Unlimited");
        } else {
            caption = unknown;
        }
        if (!known && total >= 0) caption = unknown + "\n" + caption;
        traffic.setText(caption);
        orbit.setContentDescription(known ? amount + ". " + caption : caption);
        directions.setText("↑ " + (upload >= 0 ? Formatter.formatShortFileSize(getContext(), upload) : "—")
                + "\n↓ " + (download >= 0 ? Formatter.formatShortFileSize(getContext(), download) : "—"));
        directions.setContentDescription(directions.getText() + (upload < 0 || download < 0 ? ". " + unknown : ""));
        directions.setVisibility(upload >= 0 || download >= 0 ? VISIBLE : GONE);
        ring.fraction = known && total > 0 ? (float) Math.min(1d, (double) bytes / total) : -1f;
        ring.invalidate();
    }

    private CharSequence ringText(String value) {
        int split = -1;
        for (int i = value.length() - 1; i > 0; i--) {
            if (Character.isWhitespace(value.charAt(i)) || Character.isSpaceChar(value.charAt(i))) {
                split = i;
                break;
            }
        }
        if (split < 0) return value;
        SpannableString text = new SpannableString(value.substring(0, split) + "\n" + value.substring(split + 1));
        text.setSpan(new RelativeSizeSpan(13f / 22f), split + 1, text.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        return text;
    }

    private void bindExpiry(FGSubscriptionMetadata metadata) {
        long seconds = metadata == null ? -1 : metadata.expire;
        expired = false;
        if (seconds == 0) {
            expiry.setText(FGSubscriptionUi.text("FG_Subscription_NoExpiry"));
        } else if (seconds < 0 || seconds > Long.MAX_VALUE / 1000L) {
            expiry.setText(FGSubscriptionUi.text("FG_Subscription_ExpiryUnknown"));
        } else {
            Locale locale = LocaleController.getInstance().getCurrentLocale();
            String date = DateFormat.getDateInstance(DateFormat.MEDIUM, locale == null ? Locale.getDefault() : locale).format(new Date(seconds * 1000L));
            expired = seconds <= System.currentTimeMillis() / 1000L;
            String value = format("FG_Subscription_Expires", date);
            expiry.setText(expired ? FGSubscriptionUi.text("FG_Subscription_Expired") + "\n" + value : value);
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int available = Math.max(0, MeasureSpec.getSize(widthMeasureSpec) - getPaddingLeft() - getPaddingRight());
        float scale = getResources().getConfiguration().fontScale;
        int ringSize = Math.min(available, dp(100 * Math.max(1f, scale)));
        int orbitWidth = Math.max(dp(124), ringSize);
        boolean stacked = available < orbitWidth + dp(136) || scale >= 1.3f;
        summary.setOrientation(stacked ? VERTICAL : HORIZONTAL);
        LayoutParams identityParams = (LayoutParams) identity.getLayoutParams();
        identityParams.width = stacked ? LayoutParams.MATCH_PARENT : 0;
        identityParams.weight = stacked ? 0 : 1;
        LayoutParams orbitParams = (LayoutParams) orbit.getLayoutParams();
        orbitParams.width = stacked ? LayoutParams.MATCH_PARENT : orbitWidth;
        orbitParams.setMarginStart(stacked ? 0 : dp(12));
        orbitParams.topMargin = stacked ? dp(16) : 0;
        LayoutParams ringParams = (LayoutParams) ringFrame.getLayoutParams();
        ringParams.width = ringParams.height = ringSize;
        float actionWidth = serversText.getPaint().measureText(serversText.getText().toString()) + dp(192);
        boolean stackActions = actionWidth > available || scale >= 1.3f;
        actions.setOrientation(stackActions ? VERTICAL : HORIZONTAL);
        LayoutParams serversParams = (LayoutParams) serversButton.getLayoutParams();
        serversParams.width = stackActions ? LayoutParams.MATCH_PARENT : 0;
        serversParams.weight = stackActions ? 0 : 1;
        LayoutParams controlsParams = (LayoutParams) controls.getLayoutParams();
        controlsParams.width = stackActions ? LayoutParams.MATCH_PARENT : LayoutParams.WRAP_CONTENT;
        controlsParams.setMarginStart(stackActions ? 0 : dp(8));
        controlsParams.topMargin = stackActions ? dp(8) : 0;
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public void updateColors() {
        int surface = ColorUtils.setAlphaComponent(Theme.getColor(Theme.key_windowBackgroundWhite), 255);
        int accent = ColorUtils.setAlphaComponent(Theme.getColor(Theme.key_windowBackgroundWhiteBlueText6), 255);
        boolean dark = Theme.isCurrentThemeDark();
        int[] tones = {
                blendOpaque(surface, accent, dark ? 0.16f : 0.09f),
                blendOpaque(surface, 0xffa69ee5, dark ? 0.09f : 0.04f),
                blendOpaque(surface, 0xff83bcc4, dark ? 0.08f : 0.04f)
        };
        int baseForeground = readable(Theme.getColor(Theme.key_windowBackgroundWhiteBlackText), new int[]{surface, tones[0], tones[1], tones[2]}, 4.5);
        int buttonSurface = blendOpaque(tones[1], baseForeground, 0.04f);
        int countSurface = blendOpaque(tones[0], baseForeground, 0.04f);
        int pressed = blendOpaque(buttonSurface, baseForeground, 0.12f);
        int[] backgrounds = {surface, tones[0], tones[1], tones[2], buttonSurface, countSurface, pressed};
        int foreground = readable(baseForeground, backgrounds, 4.5);
        int secondary = readable(Theme.getColor(Theme.key_windowBackgroundWhiteGrayText2), backgrounds, 4.5);
        accent = readable(accent, backgrounds, 3);
        int error = readable(Theme.getColor(Theme.key_text_RedRegular), backgrounds, 4.5);
        GradientDrawable background = new GradientDrawable(LocaleController.isRTL ? GradientDrawable.Orientation.TR_BL : GradientDrawable.Orientation.TL_BR, tones);
        background.setCornerRadius(dp(28));
        background.setStroke(1, blendOpaque(surface, foreground, dark ? 0.2f : 0.16f));
        setBackground(background);
        label.setTextColor(secondary);
        name.setTextColor(foreground);
        count.setTextColor(secondary);
        count.setBackground(Theme.createRoundRectDrawable(dp(12), countSurface));
        directions.setTextColor(secondary);
        used.setTextColor(foreground);
        traffic.setTextColor(secondary);
        expiry.setTextColor(expired ? error : secondary);
        state.setTextColor(busy ? readable(accent, backgrounds, 4.5) : failed ? error : secondary);
        serversText.setTextColor(foreground);
        moreText.setTextColor(readable(accent, backgrounds, 4.5));
        serverGlyph.setColor(foreground);
        refreshGlyph.setColor(foreground);
        editGlyph.setColor(foreground);
        chevron.paint.setColor(foreground);
        chevron.invalidate();
        progress.setIndeterminateTintList(ColorStateList.valueOf(accent));
        for (ActionButton button : new ActionButton[]{serversButton, refreshButton, editButton, moreButton}) {
            button.setBackground(Theme.createSimpleSelectorRoundRectDrawable(dp(15), buttonSurface, pressed));
            button.setAlpha(button.isEnabled() || button == refreshButton && busy ? 1f : 0.5f);
        }
        divider.setBackgroundColor(blendOpaque(tones[1], foreground, 0.15f));
        ring.track.setColor(blendOpaque(tones[1], foreground, 0.15f));
        ring.arc.setColor(accent);
        ring.fill.setColor(surface);
        ring.invalidate();
        for (int i = 0; i < serverRows.getChildCount(); i++) {
            View row = serverRows.getChildAt(i);
            if (row instanceof ProxySettingsSheet25.Themed) {
                ((ProxySettingsSheet25.Themed) row).updateColors();
            } else if (row instanceof ProxyListActivity.TextDetailProxyCell) {
                ((ProxyListActivity.TextDetailProxyCell) row).updateStatus();
                row.invalidate();
            } else if (row instanceof TextView && row.isClickable()) {
                ((TextView) row).setTextColor(readable(accent, backgrounds, 4.5));
                row.setBackground(Theme.createSimpleSelectorRoundRectDrawable(dp(15), buttonSurface, pressed));
            }
        }
    }

    private static int blendOpaque(int first, int second, float ratio) {
        return ColorUtils.setAlphaComponent(ColorUtils.blendARGB(first, second, ratio), 255);
    }

    private static int readable(int color, int[] backgrounds, double minimum) {
        color = ColorUtils.setAlphaComponent(color, 255);
        double white = Double.MAX_VALUE;
        double black = Double.MAX_VALUE;
        for (int background : backgrounds) {
            white = Math.min(white, ColorUtils.calculateContrast(Color.WHITE, background));
            black = Math.min(black, ColorUtils.calculateContrast(Color.BLACK, background));
        }
        int target = white > black ? Color.WHITE : Color.BLACK;
        for (int step = 0; step <= 20; step++) {
            int candidate = blendOpaque(color, target, step / 20f);
            boolean valid = true;
            for (int background : backgrounds) {
                if (ColorUtils.calculateContrast(candidate, background) < minimum) valid = false;
            }
            if (valid) return candidate;
        }
        return target;
    }

    private static String format(String key, Object... values) {
        return FGSubscriptionUi.format(key, values);
    }

    private static TextView text(Context context, float size, boolean medium) {
        TextView view = new TextView(context);
        view.setTextSize(TypedValue.COMPLEX_UNIT_SP, size);
        view.setTypeface(Typeface.create(medium ? "sans-serif-medium" : "sans-serif", Typeface.NORMAL));
        view.setIncludeFontPadding(false);
        view.setGravity(Gravity.START | Gravity.CENTER_VERTICAL);
        view.setTextDirection(TEXT_DIRECTION_FIRST_STRONG);
        return view;
    }

    private static ImageView icon(Context context, ProxySettingsSheet25.Glyph glyph) {
        ImageView view = new ImageView(context);
        view.setImageDrawable(glyph);
        view.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        return view;
    }

    private static FrameLayout.LayoutParams centeredIcon() {
        return new FrameLayout.LayoutParams(dp(24), dp(24), Gravity.CENTER);
    }

    private static LayoutParams wrap() {
        return new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
    }

    private static LayoutParams spaced(float top) {
        LayoutParams params = wrap();
        params.topMargin = dp(top);
        return params;
    }

    private static int dp(float value) {
        return AndroidUtilities.dp(value);
    }

    private final class ActionButton extends FrameLayout {
        private final boolean disclosure;

        ActionButton(Context context, boolean disclosure) {
            super(context);
            this.disclosure = disclosure;
            setMinimumHeight(dp(48));
            setFocusable(true);
            setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES);
        }

        @Override
        public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
            super.onInitializeAccessibilityNodeInfo(info);
            info.setClassName("android.widget.Button");
            if (disclosure) {
                info.setCheckable(true);
                info.setChecked(expanded);
                info.addAction(expanded ? AccessibilityNodeInfo.AccessibilityAction.ACTION_COLLAPSE : AccessibilityNodeInfo.AccessibilityAction.ACTION_EXPAND);
            }
        }

        @Override
        public boolean performAccessibilityAction(int action, Bundle arguments) {
            if (disclosure && isEnabled() && (action == AccessibilityNodeInfo.ACTION_EXPAND && !expanded
                    || action == AccessibilityNodeInfo.ACTION_COLLAPSE && expanded)) {
                return performClick();
            }
            return super.performAccessibilityAction(action, arguments);
        }
    }

    private static final class RingView extends View {
        private final Paint track = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint arc = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF bounds = new RectF();
        private float fraction = -1;

        RingView(Context context) {
            super(context);
            setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
            track.setStyle(Paint.Style.STROKE);
            track.setStrokeWidth(dp(4));
            arc.setStyle(Paint.Style.STROKE);
            arc.setStrokeWidth(dp(4));
            arc.setStrokeCap(Paint.Cap.ROUND);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            float radius = Math.min(getWidth(), getHeight()) / 2f - dp(4);
            if (radius <= 0) return;
            float cx = getWidth() / 2f;
            float cy = getHeight() / 2f;
            canvas.drawCircle(cx, cy, radius - dp(4), fill);
            canvas.drawCircle(cx, cy, radius, track);
            if (fraction > 0) {
                bounds.set(cx - radius, cy - radius, cx + radius, cy + radius);
                canvas.drawArc(bounds, -90, 360 * fraction, false, arc);
            }
        }
    }

    private final class ChevronView extends View {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Path path = new Path();

        ChevronView(Context context) {
            super(context);
            setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(dp(1.7f));
            paint.setStrokeCap(Paint.Cap.ROUND);
            paint.setStrokeJoin(Paint.Join.ROUND);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            float cx = getWidth() / 2f;
            float cy = getHeight() / 2f;
            float sign = expanded ? -1 : 1;
            path.reset();
            path.moveTo(cx - dp(5), cy - sign * dp(2.5f));
            path.lineTo(cx, cy + sign * dp(2.5f));
            path.lineTo(cx + dp(5), cy - sign * dp(2.5f));
            canvas.drawPath(path, paint);
        }
    }
}

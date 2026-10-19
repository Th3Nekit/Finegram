package com.th3nekit.finegram.core.ui;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.core.graphics.ColorUtils;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.Switch;

public final class ProxySettingsSheet25 {
    private ProxySettingsSheet25() {
    }

    public interface Themed {
        void updateColors();
    }

    private static TextView text(Context context, float size, boolean medium) {
        TextView view = new TextView(context);
        view.setTextSize(TypedValue.COMPLEX_UNIT_SP, size);
        view.setIncludeFontPadding(false);
        view.setGravity(Gravity.START | Gravity.CENTER_VERTICAL);
        view.setTextDirection(View.TEXT_DIRECTION_FIRST_STRONG);
        if (medium) {
            view.setTypeface(AndroidUtilities.bold());
        }
        return view;
    }

    private static void direction(View view) {
        view.setLayoutDirection(LocaleController.isRTL ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR);
    }

    private static int tint(int background, int foreground, float amount) {
        return ColorUtils.blendARGB(background, foreground, amount);
    }

    public static final class RouteHeader extends LinearLayout implements Themed {
        private final FrameLayout[] symbols = new FrameLayout[3];
        private final Glyph[] glyphs = new Glyph[3];
        private final TextView[] labels = new TextView[3];
        private final Glyph[] arrows = new Glyph[2];
        private final TextView name;
        private final TextView status;
        private boolean proxyEnabled;
        private boolean connected;
        private int statusColorKey;

        public RouteHeader(Context context) {
            super(context);
            direction(this);
            setOrientation(VERTICAL);
            setGravity(Gravity.CENTER_HORIZONTAL);
            setPadding(AndroidUtilities.dp(20), AndroidUtilities.dp(20), AndroidUtilities.dp(20), AndroidUtilities.dp(28));
            LinearLayout route = new LinearLayout(context);
            direction(route);
            route.setGravity(Gravity.TOP);
            int[] strings = {R.string.FG_ProxyPhone, R.string.FG_ProxyNode, R.string.FG_ProxyTelegram};
            int[] types = {Glyph.PHONE, Glyph.PROXY, Glyph.TELEGRAM};
            for (int i = 0; i < 3; i++) {
                LinearLayout node = new LinearLayout(context);
                node.setOrientation(VERTICAL);
                node.setGravity(Gravity.CENTER_HORIZONTAL);
                symbols[i] = new FrameLayout(context);
                ImageView icon = new ImageView(context);
                glyphs[i] = new Glyph(types[i]);
                icon.setImageDrawable(glyphs[i]);
                symbols[i].addView(icon, LayoutHelper.createFrame(24, 24, Gravity.CENTER));
                node.addView(symbols[i], new LinearLayout.LayoutParams(AndroidUtilities.dp(54), AndroidUtilities.dp(54)));
                labels[i] = text(context, 13, false);
                labels[i].setGravity(Gravity.CENTER);
                labels[i].setText(LocaleController.getString(strings[i]));
                LinearLayout.LayoutParams labelParams = new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
                labelParams.topMargin = AndroidUtilities.dp(9);
                node.addView(labels[i], labelParams);
                route.addView(node, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
                if (i < 2) {
                    ImageView arrow = new ImageView(context);
                    arrows[i] = new Glyph(Glyph.ARROW);
                    arrow.setImageDrawable(arrows[i]);
                    LinearLayout.LayoutParams arrowParams = new LinearLayout.LayoutParams(AndroidUtilities.dp(20), AndroidUtilities.dp(20));
                    arrowParams.topMargin = AndroidUtilities.dp(17);
                    route.addView(arrow, arrowParams);
                }
            }
            addView(route, new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
            name = text(context, 26, true);
            name.setGravity(Gravity.CENTER);
            name.setLetterSpacing(-0.025f);
            name.setBreakStrategy(android.text.Layout.BREAK_STRATEGY_HIGH_QUALITY);
            LinearLayout.LayoutParams nameParams = new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
            nameParams.topMargin = AndroidUtilities.dp(22);
            addView(name, nameParams);
            status = text(context, 14, false);
            status.setGravity(Gravity.CENTER);
            LinearLayout.LayoutParams statusParams = new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
            statusParams.topMargin = AndroidUtilities.dp(8);
            addView(status, statusParams);
            setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES);
            route.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
            name.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
            status.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        }

        public void bind(CharSequence title, CharSequence state, int colorKey, boolean enabled, boolean telegramConnected) {
            name.setText(title);
            status.setText(state);
            statusColorKey = colorKey;
            proxyEnabled = enabled;
            connected = telegramConnected;
            setContentDescription(labels[0].getText() + " → " + labels[1].getText() + " → " + labels[2].getText() + ". " + title + ". " + state);
            updateColors();
        }

        @Override
        public void updateColors() {
            int surface = Theme.getColor(Theme.key_windowBackgroundWhite);
            int accent = Theme.getColor(Theme.key_windowBackgroundWhiteBlueText6);
            int secondary = Theme.getColor(Theme.key_windowBackgroundWhiteGrayText2);
            name.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlackText));
            status.setTextColor(Theme.getColor(statusColorKey));
            for (int i = 0; i < 3; i++) {
                boolean active = i == 1 && proxyEnabled || i == 2 && connected;
                symbols[i].setBackground(Theme.createRoundRectDrawable(AndroidUtilities.dp(20), active ? tint(surface, accent, 0.12f) : surface));
                glyphs[i].setColor(active ? accent : secondary);
                labels[i].setTextColor(secondary);
            }
            for (Glyph arrow : arrows) {
                arrow.setColor(secondary);
                arrow.setAlpha(proxyEnabled ? 180 : 100);
            }
        }
    }

    public static final class PanelHeader extends LinearLayout implements Themed {
        private final View handle;
        private final TextView heading;
        private final TextView ping;

        public PanelHeader(Context context) {
            super(context);
            direction(this);
            setOrientation(VERTICAL);
            setPadding(AndroidUtilities.dp(24), AndroidUtilities.dp(12), AndroidUtilities.dp(24), AndroidUtilities.dp(16));
            handle = new View(context);
            LinearLayout.LayoutParams handleParams = new LinearLayout.LayoutParams(AndroidUtilities.dp(32), AndroidUtilities.dp(4));
            handleParams.gravity = Gravity.CENTER_HORIZONTAL;
            handleParams.bottomMargin = AndroidUtilities.dp(20);
            addView(handle, handleParams);
            LinearLayout row = new LinearLayout(context);
            direction(row);
            row.setGravity(Gravity.CENTER_VERTICAL);
            heading = text(context, 17, true);
            heading.setText(LocaleController.getString(R.string.FG_ProxyConnection));
            row.addView(heading, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
            ping = text(context, 22, true);
            LinearLayout.LayoutParams pingParams = new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
            pingParams.setMarginStart(AndroidUtilities.dp(12));
            row.addView(ping, pingParams);
            addView(row, new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
            handle.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
            updateColors();
        }

        public void bind(CharSequence latency) {
            ping.setText(latency);
            ping.setVisibility(latency == null || latency.length() == 0 ? GONE : VISIBLE);
            updateColors();
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            LinearLayout row = (LinearLayout) heading.getParent();
            int available = MeasureSpec.getSize(widthMeasureSpec) - getPaddingLeft() - getPaddingRight();
            float needed = heading.getPaint().measureText(heading.getText().toString()) + ping.getPaint().measureText(ping.getText().toString()) + AndroidUtilities.dp(12);
            boolean stacked = ping.getVisibility() == VISIBLE && needed > available;
            row.setOrientation(stacked ? VERTICAL : HORIZONTAL);
            LinearLayout.LayoutParams headingParams = (LinearLayout.LayoutParams) heading.getLayoutParams();
            headingParams.width = stacked ? LayoutParams.MATCH_PARENT : 0;
            headingParams.weight = stacked ? 0 : 1;
            LinearLayout.LayoutParams pingParams = (LinearLayout.LayoutParams) ping.getLayoutParams();
            pingParams.setMarginStart(stacked ? 0 : AndroidUtilities.dp(12));
            pingParams.topMargin = stacked ? AndroidUtilities.dp(6) : 0;
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }

        @Override
        public void updateColors() {
            handle.setBackground(Theme.createRoundRectDrawable(AndroidUtilities.dp(2), Theme.getColor(Theme.key_divider)));
            heading.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlackText));
            ping.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlueText6));
        }
    }

    public static final class TogglePairCell extends LinearLayout implements Themed {
        private final ToggleCell proxy;
        private final ToggleCell bypass;

        public TogglePairCell(Context context) {
            super(context);
            direction(this);
            setOrientation(HORIZONTAL);
            setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
            proxy = new ToggleCell(context, true, true);
            bypass = new ToggleCell(context, true, true);
            LinearLayout.LayoutParams first = new LinearLayout.LayoutParams(0, LayoutParams.MATCH_PARENT, 1);
            first.setMarginEnd(AndroidUtilities.dp(12));
            addView(proxy, first);
            addView(bypass, new LinearLayout.LayoutParams(0, LayoutParams.MATCH_PARENT, 1));
        }

        public void bind(boolean proxyEnabled, boolean bypassEnabled, Runnable toggleProxy, Runnable toggleBypass) {
            proxy.bind(LocaleController.getString(R.string.UseProxySettings), proxyEnabled);
            bypass.bind(LocaleController.getString(R.string.FG_Bypass_Enable), bypassEnabled);
            proxy.setOnClickListener(v -> toggleProxy.run());
            bypass.setOnClickListener(v -> toggleBypass.run());
        }

        @Override
        public void updateColors() {
            proxy.updateColors();
            bypass.updateColors();
        }
    }

    public static final class ToggleCell extends LinearLayout implements Themed {
        private final TextView title;
        private final Glyph glyph;
        private final Switch toggle;
        private final boolean primary;

        public ToggleCell(Context context, boolean primary) {
            this(context, primary, false);
        }

        private ToggleCell(Context context, boolean primary, boolean compact) {
            super(context);
            direction(this);
            this.primary = primary;
            setGravity(Gravity.CENTER_VERTICAL);
            setOrientation(compact ? VERTICAL : HORIZONTAL);
            setMinimumHeight(AndroidUtilities.dp(compact ? 80 : primary ? 60 : 64));
            setPadding(AndroidUtilities.dp(compact ? 14 : primary ? 16 : 24), AndroidUtilities.dp(10), AndroidUtilities.dp(compact ? 14 : primary ? 12 : 16), AndroidUtilities.dp(10));
            ImageView icon = new ImageView(context);
            glyph = new Glyph(primary ? Glyph.POWER : Glyph.ROTATE);
            icon.setImageDrawable(glyph);
            LinearLayout.LayoutParams iconParams = new LinearLayout.LayoutParams(AndroidUtilities.dp(compact ? 20 : 24), AndroidUtilities.dp(compact ? 20 : 24));
            title = text(context, compact ? 14 : 16, primary);
            toggle = new Switch(context);
            toggle.setClickable(false);
            toggle.setFocusable(false);
            if (compact) {
                LinearLayout controls = new LinearLayout(context);
                controls.setGravity(Gravity.CENTER_VERTICAL);
                controls.addView(icon, iconParams);
                controls.addView(new View(context), new LinearLayout.LayoutParams(0, 1, 1));
                controls.addView(toggle, new LinearLayout.LayoutParams(AndroidUtilities.dp(36), AndroidUtilities.dp(28)));
                addView(controls, new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
                LinearLayout.LayoutParams titleParams = new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
                titleParams.topMargin = AndroidUtilities.dp(6);
                addView(title, titleParams);
                setFocusable(true);
            } else {
                iconParams.setMarginEnd(AndroidUtilities.dp(14));
                addView(icon, iconParams);
                addView(title, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
                LinearLayout.LayoutParams toggleParams = new LinearLayout.LayoutParams(AndroidUtilities.dp(48), AndroidUtilities.dp(48));
                toggleParams.setMarginStart(AndroidUtilities.dp(8));
                addView(toggle, toggleParams);
            }
            icon.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
            title.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
            toggle.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
            setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES);
            updateColors();
        }

        public void bind(CharSequence text, boolean checked) {
            title.setText(text);
            setChecked(checked, false);
        }

        public void setChecked(boolean checked, boolean animated) {
            toggle.setChecked(checked, animated);
            updateColors();
        }

        @Override
        public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
            super.onInitializeAccessibilityNodeInfo(info);
            info.setClassName("android.widget.Switch");
            info.setText(title.getText());
            info.setCheckable(true);
            info.setChecked(toggle.isChecked());
        }

        @Override
        public void updateColors() {
            int accent = Theme.getColor(Theme.key_windowBackgroundWhiteBlueButton);
            boolean active = primary && toggle.isChecked();
            int foreground = Theme.getColor(active ? Theme.key_featuredStickers_buttonText : Theme.key_windowBackgroundWhiteBlackText);
            title.setTextColor(foreground);
            glyph.setColor(primary ? foreground : Theme.getColor(Theme.key_windowBackgroundWhiteGrayText2));
            if (primary) {
                int background = active ? accent : tint(Theme.getColor(Theme.key_windowBackgroundWhite), foreground, .07f);
                setBackground(Theme.createSimpleSelectorRoundRectDrawable(AndroidUtilities.dp(20), background, tint(background, foreground, .12f)));
                glyph.setColor(active ? foreground : Theme.getColor(Theme.key_windowBackgroundWhiteGrayText2));
                if (active) {
                    toggle.setColors(Theme.key_windowBackgroundWhiteGrayText3, Theme.key_windowBackgroundWhite, Theme.key_windowBackgroundWhite, Theme.key_windowBackgroundWhiteBlueButton);
                } else {
                    toggle.setColors(Theme.key_switchTrack, Theme.key_switchTrackChecked, Theme.key_windowBackgroundWhite, Theme.key_windowBackgroundWhite);
                }
            } else {
                toggle.setColors(Theme.key_switchTrack, Theme.key_switchTrackChecked, Theme.key_windowBackgroundWhite, Theme.key_windowBackgroundWhite);
            }
            toggle.invalidate();
        }
    }

    public static final class ActionCell extends LinearLayout implements Themed {
        private final TextView title;
        private final ImageView icon;
        private Glyph glyph;
        private boolean destructive;

        public ActionCell(Context context) {
            super(context);
            direction(this);
            setGravity(Gravity.CENTER_VERTICAL);
            setMinimumHeight(AndroidUtilities.dp(56));
            setPadding(AndroidUtilities.dp(16), AndroidUtilities.dp(14), AndroidUtilities.dp(16), AndroidUtilities.dp(14));
            icon = new ImageView(context);
            LinearLayout.LayoutParams iconParams = new LinearLayout.LayoutParams(AndroidUtilities.dp(24), AndroidUtilities.dp(24));
            iconParams.setMarginEnd(AndroidUtilities.dp(14));
            addView(icon, iconParams);
            title = text(context, 16, false);
            addView(title, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
            icon.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
            title.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
            setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES);
        }

        public void bind(CharSequence text, int type, boolean destructive) {
            title.setText(text);
            this.destructive = destructive;
            glyph = new Glyph(type);
            icon.setImageDrawable(glyph);
            updateColors();
        }

        @Override
        public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
            super.onInitializeAccessibilityNodeInfo(info);
            info.setClassName("android.widget.Button");
            info.setText(title.getText());
        }

        @Override
        public void updateColors() {
            int surface = Theme.getColor(Theme.key_windowBackgroundWhite);
            int foreground = Theme.getColor(destructive ? Theme.key_text_RedRegular : Theme.key_windowBackgroundWhiteBlueText6);
            title.setTextColor(foreground);
            if (glyph != null) {
                glyph.setColor(foreground);
            }
            setBackground(Theme.createRoundRectDrawable(AndroidUtilities.dp(16), tint(surface, foreground, destructive ? 0.06f : 0.07f)));
        }
    }

    public static final class SectionHeader extends TextView implements Themed {
        public SectionHeader(Context context) {
            super(context);
            direction(this);
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
            setTypeface(AndroidUtilities.bold());
            setIncludeFontPadding(false);
            setGravity(Gravity.START);
            setPadding(AndroidUtilities.dp(24), AndroidUtilities.dp(24), AndroidUtilities.dp(24), AndroidUtilities.dp(10));
            updateColors();
        }

        @Override
        public void updateColors() {
            setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteGrayText2));
        }
    }

    public static final class Glyph extends Drawable {
        public static final int PHONE = 0, PROXY = 1, TELEGRAM = 2, ARROW = 3, POWER = 4, ROTATE = 5, ADD = 6, IMPORT = 7, EDIT = 8, DELETE = 9;
        private final int type;
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Path path = new Path();
        private final RectF rect = new RectF();

        public Glyph(int type) {
            this.type = type;
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(1.7f);
            paint.setStrokeCap(Paint.Cap.ROUND);
            paint.setStrokeJoin(Paint.Join.ROUND);
        }

        public void setColor(int color) {
            paint.setColor(color);
            invalidateSelf();
        }

        private void line(Canvas canvas, float x1, float y1, float x2, float y2) {
            canvas.drawLine(x1, y1, x2, y2, paint);
        }

        @Override
        public void draw(Canvas canvas) {
            canvas.save();
            canvas.translate(getBounds().left, getBounds().top);
            canvas.scale(getBounds().width() / 24f, getBounds().height() / 24f);
            path.reset();
            switch (type) {
                case PHONE:
                    rect.set(6.5f, 2.5f, 17.5f, 21.5f);
                    canvas.drawRoundRect(rect, 2.7f, 2.7f, paint);
                    line(canvas, 10, 5, 14, 5);
                    line(canvas, 11, 18.5f, 13, 18.5f);
                    break;
                case PROXY:
                    rect.set(3.5f, 4, 20.5f, 11);
                    canvas.drawRoundRect(rect, 2.5f, 2.5f, paint);
                    rect.set(3.5f, 13, 20.5f, 20);
                    canvas.drawRoundRect(rect, 2.5f, 2.5f, paint);
                    line(canvas, 7, 7.5f, 7.3f, 7.5f);
                    line(canvas, 7, 16.5f, 7.3f, 16.5f);
                    line(canvas, 13, 7.5f, 17, 7.5f);
                    line(canvas, 13, 16.5f, 17, 16.5f);
                    break;
                case TELEGRAM:
                    path.moveTo(3, 10.5f);
                    path.lineTo(21, 3.5f);
                    path.lineTo(17.5f, 20.5f);
                    path.lineTo(10.5f, 15);
                    path.lineTo(8, 19);
                    path.lineTo(7.5f, 12.5f);
                    path.close();
                    canvas.drawPath(path, paint);
                    line(canvas, 7.5f, 12.5f, 16, 8);
                    line(canvas, 10.5f, 15, 16, 8);
                    break;
                case ARROW:
                    if (LocaleController.isRTL) {
                        canvas.translate(24, 0);
                        canvas.scale(-1, 1);
                    }
                    line(canvas, 4, 12, 20, 12);
                    line(canvas, 16, 8, 20, 12);
                    line(canvas, 16, 16, 20, 12);
                    break;
                case POWER:
                    rect.set(4, 4, 20, 20);
                    canvas.drawArc(rect, -45, 270, false, paint);
                    line(canvas, 12, 2.5f, 12, 11);
                    break;
                case ROTATE:
                    rect.set(4, 4, 20, 20);
                    canvas.drawArc(rect, 35, 270, false, paint);
                    line(canvas, 20, 4, 20, 9);
                    line(canvas, 15, 9, 20, 9);
                    break;
                case ADD:
                    line(canvas, 12, 4, 12, 20);
                    line(canvas, 4, 12, 20, 12);
                    break;
                case IMPORT:
                    line(canvas, 12, 3, 12, 15);
                    line(canvas, 7.5f, 10.5f, 12, 15);
                    line(canvas, 16.5f, 10.5f, 12, 15);
                    path.moveTo(4, 15);
                    path.lineTo(4, 20);
                    path.lineTo(20, 20);
                    path.lineTo(20, 15);
                    canvas.drawPath(path, paint);
                    break;
                case EDIT:
                    path.moveTo(4, 20);
                    path.lineTo(5, 15);
                    path.lineTo(16.5f, 3.5f);
                    path.lineTo(20.5f, 7.5f);
                    path.lineTo(9, 19);
                    path.close();
                    canvas.drawPath(path, paint);
                    line(canvas, 14, 6, 18, 10);
                    break;
                case DELETE:
                    line(canvas, 4, 6, 20, 6);
                    line(canvas, 9, 3, 15, 3);
                    path.moveTo(6, 6);
                    path.lineTo(7, 21);
                    path.lineTo(17, 21);
                    path.lineTo(18, 6);
                    canvas.drawPath(path, paint);
                    line(canvas, 10, 10, 10, 17);
                    line(canvas, 14, 10, 14, 17);
                    break;
            }
            canvas.restore();
        }

        @Override
        public int getIntrinsicWidth() {
            return AndroidUtilities.dp(24);
        }

        @Override
        public int getIntrinsicHeight() {
            return AndroidUtilities.dp(24);
        }

        @Override
        public void setAlpha(int alpha) {
            paint.setAlpha(alpha);
            invalidateSelf();
        }

        @Override
        public void setColorFilter(ColorFilter colorFilter) {
            paint.setColorFilter(colorFilter);
            invalidateSelf();
        }

        @Override
        public int getOpacity() {
            return PixelFormat.TRANSLUCENT;
        }
    }
}

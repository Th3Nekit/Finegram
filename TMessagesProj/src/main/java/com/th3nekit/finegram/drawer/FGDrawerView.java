/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.drawer;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.Shader;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.core.graphics.ColorUtils;

import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig;
import com.th3nekit.finegram.plugins.FGPluginsMenu;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.MediaDataController;
import org.telegram.messenger.MessagesStorage;
import org.telegram.messenger.R;
import org.telegram.messenger.UserConfig;
import org.telegram.messenger.UserObject;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.AvatarDrawable;
import org.telegram.ui.Components.BackupImageView;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.LayoutHelper;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

public class FGDrawerView extends LinearLayout {

    private static final int AVATAR_SIZE_DP = 62;
    private static final int SIDE_PADDING_DP = 22;
    private static final int ROW_HEIGHT_DP = 50;
    private static final int ACCOUNT_ROW_HEIGHT_DP = 50;
    private static final int ICON_SIZE_DP = 24;
    private static final int ACCOUNT_AVATAR_DP = 32;

    private static final int TEXT_LEFT_DP = SIDE_PADDING_DP + ICON_SIZE_DP + 18;

    private static final long ROW_STAGGER = 22;
    private static final long ROW_APPEAR = 260;
    private static final int MAX_STAGGERED_ROWS = 12;

    public interface Listener {
        void onItemClick(int item);

        void onProfileClick();

        void onThemeToggle(View anchor);

        void onAccountClick(int account);

        void onAddAccount();

        void onBotClick(TLRPC.TL_attachMenuBot bot);
    }

    private final Theme.ResourcesProvider resourcesProvider;

    private final HeaderView header;
    private final BackupImageView avatarView;
    private final TextView nameView;
    private final TextView phoneView;
    private final ImageView themeView;
    private final ImageView chevronView;
    private final LinearLayout accountsLayout;
    private final ScrollView scroll;
    private final LinearLayout itemsLayout;

    private Listener listener;
    private boolean accountsExpanded;

    public FGDrawerView(Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;
        setOrientation(VERTICAL);

        header = new HeaderView(context);
        header.setOrientation(VERTICAL);
        header.setOnClickListener(v -> {
            if (listener != null) {
                listener.onProfileClick();
            }
        });

        FrameLayout topRow = new FrameLayout(context);
        avatarView = new BackupImageView(context);
        avatarView.setRoundRadius(AndroidUtilities.dp(AVATAR_SIZE_DP / 2f));
        topRow.addView(avatarView, LayoutHelper.createFrame(AVATAR_SIZE_DP, AVATAR_SIZE_DP,
                (LocaleController.isRTL ? Gravity.RIGHT : Gravity.LEFT) | Gravity.TOP));

        themeView = new ImageView(context);
        themeView.setScaleType(ImageView.ScaleType.CENTER);
        themeView.setOnClickListener(v -> {
            if (listener != null) {
                listener.onThemeToggle(v);
            }
        });

        topRow.addView(themeView, LayoutHelper.createFrame(40, 40,
                (LocaleController.isRTL ? Gravity.LEFT : Gravity.RIGHT) | Gravity.TOP));
        header.addView(topRow, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        FrameLayout nameRow = new FrameLayout(context);
        nameView = new TextView(context);
        nameView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 16);
        nameView.setTypeface(AndroidUtilities.bold());
        nameView.setMaxLines(1);
        nameView.setEllipsize(TextUtils.TruncateAt.END);
        nameView.setGravity(LocaleController.isRTL ? Gravity.RIGHT : Gravity.LEFT);
        nameRow.addView(nameView, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                Gravity.TOP, LocaleController.isRTL ? 48 : 0, 0, LocaleController.isRTL ? 0 : 48, 0));

        phoneView = new TextView(context);
        phoneView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 13);
        phoneView.setMaxLines(1);
        phoneView.setEllipsize(TextUtils.TruncateAt.END);
        phoneView.setGravity(LocaleController.isRTL ? Gravity.RIGHT : Gravity.LEFT);
        nameRow.addView(phoneView, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                Gravity.TOP, LocaleController.isRTL ? 48 : 0, 24, LocaleController.isRTL ? 0 : 48, 0));

        chevronView = new ImageView(context);
        chevronView.setScaleType(ImageView.ScaleType.CENTER);
        chevronView.setImageResource(R.drawable.arrow_more);
        chevronView.setContentDescription(LocaleController.getString(R.string.AccountSwitch));
        chevronView.setOnClickListener(v -> setAccountsExpanded(!accountsExpanded, true));
        nameRow.addView(chevronView, LayoutHelper.createFrame(40, 40,
                (LocaleController.isRTL ? Gravity.LEFT : Gravity.RIGHT) | Gravity.CENTER_VERTICAL));
        header.addView(nameRow, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 14, 0, 0));

        addView(header, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        itemsLayout = new LinearLayout(context);
        itemsLayout.setOrientation(VERTICAL);

        accountsLayout = new LinearLayout(context);
        accountsLayout.setOrientation(VERTICAL);
        accountsLayout.setVisibility(GONE);

        LinearLayout scrollContent = new LinearLayout(context);
        scrollContent.setOrientation(VERTICAL);
        scrollContent.addView(accountsLayout, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
        scrollContent.addView(itemsLayout, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        scroll = new ScrollView(context);
        scroll.setVerticalScrollBarEnabled(false);
        scroll.setOverScrollMode(View.OVER_SCROLL_NEVER);
        scroll.setClipToPadding(false);
        scroll.addView(scrollContent, new ScrollView.LayoutParams(
                LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
        addView(scroll, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 0, 1f));

        updateColors();
    }

    private int panelColor() {
        final int base = Theme.getColor(Theme.key_windowBackgroundWhite, resourcesProvider);
        final float lift = Theme.isCurrentThemeDark() ? 0.055f : 0.02f;
        return ColorUtils.blendARGB(base, Theme.isCurrentThemeDark() ? Color.WHITE : Color.BLACK, lift);
    }

    public void setListener(Listener listener) {
        this.listener = listener;
    }

    public void updateColors() {
        setBackgroundColor(panelColor());
        nameView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider));
        phoneView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteGrayText, resourcesProvider));
        final int iconColor = Theme.getColor(Theme.key_windowBackgroundWhiteGrayIcon, resourcesProvider);
        chevronView.setColorFilter(new PorterDuffColorFilter(iconColor, PorterDuff.Mode.SRC_IN));
        themeView.setColorFilter(new PorterDuffColorFilter(iconColor, PorterDuff.Mode.SRC_IN));
        final int selector = Theme.getColor(Theme.key_listSelector, resourcesProvider);
        chevronView.setBackground(Theme.createSelectorDrawable(selector, Theme.RIPPLE_MASK_CIRCLE_20DP));
        themeView.setBackground(Theme.createSelectorDrawable(selector, Theme.RIPPLE_MASK_CIRCLE_20DP));
        themeView.setImageResource(Theme.isCurrentThemeDark() ? R.drawable.menu_day_mode_24 : R.drawable.menu_night_mode_24);
        themeView.setContentDescription(LocaleController.getString(Theme.isCurrentThemeDark()
                ? R.string.SwitchThemeToDay : R.string.SwitchThemeToNight));
        header.updateColors();
        for (LinearLayout layout : new LinearLayout[]{itemsLayout, accountsLayout}) {
            for (int i = 0; i < layout.getChildCount(); i++) {
                final View child = layout.getChildAt(i);
                if (child instanceof RowView) {
                    ((RowView) child).updateColors();
                } else if (child instanceof DividerView) {
                    child.invalidate();
                }
            }
        }
    }

    public void refresh() {
        final boolean withPhone = FinegramAppearanceConfig.INSTANCE.getSideDrawerPhone();
        final int account = UserConfig.selectedAccount;
        final TLRPC.User user = UserConfig.getInstance(account).getCurrentUser();
        if (user != null) {
            final AvatarDrawable avatarDrawable = new AvatarDrawable(user);
            avatarView.setForUserOrChat(user, avatarDrawable);
            nameView.setText(UserObject.getUserName(user));
            if (withPhone && user.phone != null && !user.phone.isEmpty()) {
                phoneView.setText(PhoneFormatter.format(user.phone));
            } else {
                final String username = UserObject.getPublicUsername(user);
                phoneView.setText(username == null ? "" : "@" + username);
            }
        }
        phoneView.setVisibility(phoneView.length() > 0 ? VISIBLE : GONE);
        header.applyPadding();
        updateColors();

        buildAccounts();
        setAccountsExpanded(accountsExpanded, false);

        itemsLayout.removeAllViews();
        int row = 0;
        final List<Integer> items = FGDrawerItems.visible();
        for (int i = 0; i < items.size(); i++) {
            final int item = items.get(i);
            if (i > 0 && FGDrawerItems.startsGroup(item)) {
                addDivider();
            }
            final RowView view = addRow(FGDrawerItems.iconOf(item),
                    LocaleController.getString(FGDrawerItems.titleOf(item)), null);
            view.setOnClickListener(v -> {
                if (listener != null) {
                    listener.onItemClick(item);
                }
            });
            appear(view, row++);
        }

        final List<TLRPC.TL_attachMenuBot> bots = sideMenuBots(account);
        if (!bots.isEmpty()) {
            addDivider();
            for (TLRPC.TL_attachMenuBot bot : bots) {
                final RowView view = addRow(R.drawable.msg_bot, bot.short_name, null);
                view.setOnClickListener(v -> {
                    if (listener != null) {
                        listener.onBotClick(bot);
                    }
                });
                appear(view, row++);
            }
        }

        final HashMap<String, Object> pluginContext = new HashMap<>();
        pluginContext.put("fragment", "DrawerMenu");
        pluginContext.put("account", account);
        final List<FGPluginsMenu.Row> pluginRows = new java.util.ArrayList<>(
                FGPluginsMenu.rows(FGPluginsMenu.DRAWER, getContext(), pluginContext));

        pluginRows.addAll(FGPluginsMenu.rows(FGPluginsMenu.MAIN, getContext(), pluginContext));
        if (!pluginRows.isEmpty()) {
            addDivider();
            for (FGPluginsMenu.Row pluginRow : pluginRows) {
                final RowView view = addRow(pluginRow.icon, pluginRow.text, null);
                view.setOnClickListener(v -> {
                    if (listener != null) {
                        listener.onItemClick(-1);
                    }
                    pluginRow.action.run();
                });
                appear(view, row++);
            }
        }

        scroll.setPadding(0, AndroidUtilities.dp(6), 0,
                AndroidUtilities.navigationBarHeight + AndroidUtilities.dp(10));
        scroll.scrollTo(0, 0);
        appear(header, 0);
    }

    private static List<TLRPC.TL_attachMenuBot> sideMenuBots(int account) {
        final ArrayList<TLRPC.TL_attachMenuBot> out = new ArrayList<>();
        final TLRPC.TL_attachMenuBots bots = MediaDataController.getInstance(account).getAttachMenuBots();
        if (bots != null && bots.bots != null) {
            for (TLRPC.TL_attachMenuBot bot : bots.bots) {
                if (bot.show_in_side_menu) {
                    out.add(bot);
                }
            }
        }
        return out;
    }

    private void buildAccounts() {
        accountsLayout.removeAllViews();
        final ArrayList<Integer> accounts = new ArrayList<>();
        for (int a = 0; a < UserConfig.MAX_ACCOUNT_COUNT; a++) {
            if (a != UserConfig.selectedAccount && UserConfig.getInstance(a).isClientActivated()) {
                accounts.add(a);
            }
        }
        accounts.sort((a1, a2) -> Long.compare(UserConfig.getInstance(a1).loginTime, UserConfig.getInstance(a2).loginTime));
        for (int acc : accounts) {
            final TLRPC.User user = UserConfig.getInstance(acc).getCurrentUser();
            if (user == null) {
                continue;
            }
            final RowView view = new RowView(getContext(), resourcesProvider);
            view.bindAccount(user, MessagesStorage.getInstance(acc).getMainUnreadCount());
            view.setOnClickListener(v -> {
                if (listener != null) {
                    listener.onAccountClick(acc);
                }
            });
            accountsLayout.addView(view, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, ACCOUNT_ROW_HEIGHT_DP));
        }
        if (UserConfig.getActivatedAccountsCount() < UserConfig.MAX_ACCOUNT_COUNT) {
            final RowView add = new RowView(getContext(), resourcesProvider);
            add.bind(R.drawable.msg_add, LocaleController.getString(R.string.AddAccount), null);
            add.setOnClickListener(v -> {
                if (listener != null) {
                    listener.onAddAccount();
                }
            });
            accountsLayout.addView(add, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, ROW_HEIGHT_DP));
        }
        accountsLayout.addView(new DividerView(getContext()), LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 13));
    }

    private void setAccountsExpanded(boolean expanded, boolean animated) {
        accountsExpanded = expanded;
        accountsLayout.setVisibility(expanded ? VISIBLE : GONE);
        final float rotation = expanded ? 180f : 0f;
        if (animated) {
            chevronView.animate().rotation(rotation).setDuration(220)
                    .setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT).start();
            if (expanded) {
                for (int i = 0; i < accountsLayout.getChildCount(); i++) {
                    appear(accountsLayout.getChildAt(i), i);
                }
                scroll.smoothScrollTo(0, 0);
            }
        } else {
            chevronView.animate().cancel();
            chevronView.setRotation(rotation);
        }
    }

    private RowView addRow(int icon, CharSequence title, TLRPC.User user) {
        final RowView view = new RowView(getContext(), resourcesProvider);
        view.bind(icon, title, user);
        itemsLayout.addView(view, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, ROW_HEIGHT_DP));
        return view;
    }

    private void addDivider() {
        itemsLayout.addView(new DividerView(getContext()), LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 13));
    }

    private void appear(View view, int index) {
        view.animate().cancel();
        view.setAlpha(0f);
        view.setTranslationX(AndroidUtilities.dp(LocaleController.isRTL ? 12 : -12));
        view.animate().alpha(1f).translationX(0)
                .setStartDelay(ROW_STAGGER * Math.min(index, MAX_STAGGERED_ROWS))
                .setDuration(ROW_APPEAR)
                .setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT)
                .start();
    }

    private class HeaderView extends LinearLayout {

        private final Paint glowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint dividerPaint = new Paint();
        private final Paint surfacePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint outlinePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF surfaceBounds = new RectF();

        HeaderView(Context context) {
            super(context);
            setWillNotDraw(false);
            outlinePaint.setStyle(Paint.Style.STROKE);
            outlinePaint.setStrokeWidth(AndroidUtilities.dp(0.75f));
            applyPadding();
        }

        void applyPadding() {
            setPadding(AndroidUtilities.dp(SIDE_PADDING_DP),
                    AndroidUtilities.statusBarHeight + AndroidUtilities.dp(18),
                    AndroidUtilities.dp(SIDE_PADDING_DP),
                    AndroidUtilities.dp(14));
        }

        void updateColors() {
            dividerPaint.setColor(ColorUtils.setAlphaComponent(
                    Theme.isCurrentThemeDark() ? Color.WHITE : Color.BLACK, 28));
            setBackground(Theme.createSelectorDrawable(
                    Theme.getColor(Theme.key_listSelector, resourcesProvider), Theme.RIPPLE_MASK_ALL));
            updateGlow(getMeasuredWidth(), getMeasuredHeight());
            invalidate();
        }

        private void updateGlow(int w, int h) {
            if (w == 0 || h == 0) {
                glowPaint.setShader(null);
                return;
            }
            final int accent = Theme.getColor(Theme.key_windowBackgroundWhiteBlueText, resourcesProvider);
            final int background = Theme.getColor(Theme.key_windowBackgroundWhite, resourcesProvider);
            surfacePaint.setColor(ColorUtils.blendARGB(background, accent,
                    Theme.isCurrentThemeDark() ? 0.10f : 0.06f));
            outlinePaint.setColor(ColorUtils.setAlphaComponent(accent,
                    Theme.isCurrentThemeDark() ? 40 : 26));
            surfaceBounds.set(AndroidUtilities.dp(8), AndroidUtilities.statusBarHeight + AndroidUtilities.dp(6),
                    w - AndroidUtilities.dp(8), h - AndroidUtilities.dp(4));
            glowPaint.setShader(new LinearGradient(0, surfaceBounds.top, w, surfaceBounds.bottom,
                    ColorUtils.setAlphaComponent(accent, 28), Color.TRANSPARENT,
                    Shader.TileMode.CLAMP));
        }

        @Override
        protected void onSizeChanged(int w, int h, int oldw, int oldh) {
            super.onSizeChanged(w, h, oldw, oldh);
            updateGlow(w, h);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            if (glowPaint.getShader() != null) {
                final float radius = AndroidUtilities.dp(24);
                canvas.drawRoundRect(surfaceBounds, radius, radius, surfacePaint);
                canvas.drawRoundRect(surfaceBounds, radius, radius, glowPaint);
                canvas.drawRoundRect(surfaceBounds, radius, radius, outlinePaint);
            }
        }
    }

    private static class DividerView extends View {

        private final Paint paint = new Paint();

        DividerView(Context context) {
            super(context);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            paint.setColor(ColorUtils.setAlphaComponent(
                    Theme.isCurrentThemeDark() ? Color.WHITE : Color.BLACK, 24));
            final float y = getMeasuredHeight() / 2f;
            canvas.drawRect(AndroidUtilities.dp(SIDE_PADDING_DP), y,
                    getMeasuredWidth() - AndroidUtilities.dp(SIDE_PADDING_DP), y + Math.max(1, AndroidUtilities.dp(0.66f)), paint);
        }
    }

    private static class RowView extends FrameLayout {

        private final ImageView icon;
        private final BackupImageView avatar;
        private final TextView title;
        private final TextView badge;
        private final Theme.ResourcesProvider resourcesProvider;

        RowView(Context context, Theme.ResourcesProvider resourcesProvider) {
            super(context);
            this.resourcesProvider = resourcesProvider;
            final boolean rtl = LocaleController.isRTL;

            icon = new ImageView(context);
            icon.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
            addView(icon, LayoutHelper.createFrame(ICON_SIZE_DP, ICON_SIZE_DP,
                    (rtl ? Gravity.RIGHT : Gravity.LEFT) | Gravity.CENTER_VERTICAL,
                    rtl ? 0 : SIDE_PADDING_DP, 0, rtl ? SIDE_PADDING_DP : 0, 0));

            avatar = new BackupImageView(context);
            avatar.setRoundRadius(AndroidUtilities.dp(ACCOUNT_AVATAR_DP / 2f));
            avatar.setVisibility(GONE);
            final int avatarInset = SIDE_PADDING_DP - (ACCOUNT_AVATAR_DP - ICON_SIZE_DP) / 2;
            addView(avatar, LayoutHelper.createFrame(ACCOUNT_AVATAR_DP, ACCOUNT_AVATAR_DP,
                    (rtl ? Gravity.RIGHT : Gravity.LEFT) | Gravity.CENTER_VERTICAL,
                    rtl ? 0 : avatarInset, 0, rtl ? avatarInset : 0, 0));

            badge = new TextView(context);
            badge.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 12);
            badge.setTypeface(AndroidUtilities.bold());
            badge.setGravity(Gravity.CENTER);
            badge.setPadding(AndroidUtilities.dp(7), 0, AndroidUtilities.dp(7), 0);
            badge.setMinWidth(AndroidUtilities.dp(22));
            badge.setVisibility(GONE);
            addView(badge, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT, 22,
                    (rtl ? Gravity.LEFT : Gravity.RIGHT) | Gravity.CENTER_VERTICAL,
                    rtl ? SIDE_PADDING_DP : 0, 0, rtl ? 0 : SIDE_PADDING_DP, 0));

            title = new TextView(context);
            title.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 15);
            title.setTypeface(AndroidUtilities.bold());
            title.setMaxLines(1);
            title.setEllipsize(TextUtils.TruncateAt.END);
            title.setGravity((rtl ? Gravity.RIGHT : Gravity.LEFT) | Gravity.CENTER_VERTICAL);
            addView(title, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT,
                    LayoutHelper.MATCH_PARENT, Gravity.LEFT | Gravity.CENTER_VERTICAL,
                    rtl ? SIDE_PADDING_DP + 34 : TEXT_LEFT_DP, 0,
                    rtl ? TEXT_LEFT_DP : SIDE_PADDING_DP + 34, 0));

            updateColors();
        }

        void bind(int iconRes, CharSequence text, TLRPC.User user) {
            icon.setVisibility(VISIBLE);
            avatar.setVisibility(GONE);
            icon.setImageResource(iconRes);
            title.setText(text);
            badge.setVisibility(GONE);
        }

        void bindAccount(TLRPC.User user, int unread) {
            icon.setVisibility(GONE);
            avatar.setVisibility(VISIBLE);
            avatar.setForUserOrChat(user, new AvatarDrawable(user));
            title.setText(UserObject.getUserName(user));
            if (unread > 0) {
                badge.setText(unread > 999 ? "999+" : String.valueOf(unread));
                badge.setVisibility(VISIBLE);
            } else {
                badge.setVisibility(GONE);
            }
        }

        void updateColors() {
            title.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider));
            icon.setColorFilter(new PorterDuffColorFilter(
                    Theme.getColor(Theme.key_windowBackgroundWhiteGrayIcon, resourcesProvider), PorterDuff.Mode.SRC_IN));
            badge.setTextColor(Theme.getColor(Theme.key_chats_unreadCounterText, resourcesProvider));
            badge.setBackground(Theme.createRoundRectDrawable(AndroidUtilities.dp(11),
                    Theme.getColor(Theme.key_chats_unreadCounter, resourcesProvider)));
            setBackground(Theme.createSelectorDrawable(
                    Theme.getColor(Theme.key_listSelector, resourcesProvider), Theme.RIPPLE_MASK_ALL));
        }
    }

    private static final class PhoneFormatter {
        static String format(String phone) {
            try {
                return org.telegram.PhoneFormat.PhoneFormat.getInstance().format("+" + phone);
            } catch (Throwable ignore) {
                return "+" + phone;
            }
        }
    }
}

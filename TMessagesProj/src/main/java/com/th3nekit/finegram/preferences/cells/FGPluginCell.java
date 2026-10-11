/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.preferences.cells;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.core.graphics.ColorUtils;

import com.th3nekit.finegram.plugins.FGPluginAvatar;
import com.th3nekit.finegram.plugins.FGPluginIcon;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.BackupImageView;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.RecyclerListView;
import org.telegram.ui.Components.Switch;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import org.telegram.ui.Components.UniversalRecyclerView;

public class FGPluginCell extends FrameLayout implements Theme.Colorable {
    private boolean badgeAlert;

    public static final int HEIGHT_DP = 72;
    private static final int SIDE_DP = 0;
    private static final int INNER_DP = 14;
    private static final int ICON_DP = 46;
    private static final int ICON_RADIUS_DP = 14;
    private static final int CORNER_DP = 20;
    private static final int TEXT_LEFT_DP = SIDE_DP + INNER_DP + ICON_DP + 14;
    private static final int SWITCH_RIGHT_DP = SIDE_DP + INNER_DP;

    private static final float DISABLED_ALPHA = 0.5f;

    private final Theme.ResourcesProvider resourcesProvider;
    private final View placeholderView;
    private final BackupImageView iconView;
    private final TextView nameView;
    private final TextView badgeView;
    private final TextView statusView;
    private final Switch checkBox;

    private final Paint cardPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint dividerPaint = new Paint();
    private final Path cardPath = new Path();
    private final RectF cardRect = new RectF();
    private final float[] radii = new float[8];

    private boolean first;
    private boolean last;
    private int cardColor;
    private Runnable onToggle;

    public FGPluginCell(@NonNull Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;
        setWillNotDraw(false);
        setClickable(false);
        setFocusable(false);

        placeholderView = new View(context);
        addView(placeholderView, LayoutHelper.createFrame(ICON_DP, ICON_DP,
                Gravity.LEFT | Gravity.CENTER_VERTICAL, SIDE_DP + INNER_DP, 0, 0, 0));

        iconView = new BackupImageView(context);
        iconView.setVisibility(GONE);
        addView(iconView, LayoutHelper.createFrame(ICON_DP, ICON_DP,
                Gravity.LEFT | Gravity.CENTER_VERTICAL, SIDE_DP + INNER_DP, 0, 0, 0));

        nameView = new FGScrollingText(context);
        nameView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 16);
        nameView.setTypeface(AndroidUtilities.bold());
        nameView.setLines(1);
        addView(nameView, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT, LayoutHelper.WRAP_CONTENT,
                Gravity.LEFT | Gravity.TOP, TEXT_LEFT_DP, 14, 0, 0));

        badgeView = new TextView(context);
        badgeView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 11);
        badgeView.setTypeface(AndroidUtilities.bold());
        badgeView.setGravity(Gravity.CENTER);
        badgeView.setLines(1);
        badgeView.setPadding(dp(7), 0, dp(7), 0);
        badgeView.setVisibility(GONE);
        addView(badgeView, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT, 18,
                Gravity.LEFT | Gravity.TOP, TEXT_LEFT_DP, 16, 0, 0));

        statusView = new TextView(context);
        statusView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 13);
        statusView.setLines(1);
        statusView.setEllipsize(TextUtils.TruncateAt.END);
        addView(statusView, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                Gravity.LEFT | Gravity.TOP, TEXT_LEFT_DP, 39, SWITCH_RIGHT_DP + 37 + 12, 0));

        checkBox = new Switch(context, resourcesProvider);
        checkBox.setColors(Theme.key_switchTrack, Theme.key_switchTrackChecked,
                Theme.key_switchTrackBlueThumb, Theme.key_switchTrackBlueThumbChecked);
        addView(checkBox, LayoutHelper.createFrame(37, 20, Gravity.RIGHT | Gravity.CENTER_VERTICAL,
                0, 0, SWITCH_RIGHT_DP, 0));

        cardColor = Theme.getColor(Theme.key_windowBackgroundWhite, resourcesProvider);
    }

    @Override
    public void setBackgroundColor(int color) {
        cardColor = color;
    }

    private static int liftForDark(int color) {
        final boolean dark = ColorUtils.calculateLuminance(color) < 0.05;
        return dark ? ColorUtils.blendARGB(color, Color.WHITE, 0.06f) : color;
    }

    public boolean isOnSwitch(float x) {
        return LocaleController.isRTL ? x <= dp(SWITCH_RIGHT_DP + 37 + 12) : x >= getMeasuredWidth() - dp(SWITCH_RIGHT_DP + 37 + 12);
    }

    public void toggle() {
        if (onToggle != null) {
            onToggle.run();
        }
    }

    public void setOnToggle(Runnable action) {
        this.onToggle = action;
    }

    public void setChecked(boolean checked) {
        checkBox.setChecked(checked, true);
        setEnabledLook(checked, true);
    }

    public boolean isChecked() {
        return checkBox.isChecked();
    }

    public void set(Factory.Data data, boolean first, boolean last) {
        this.first = first;
        this.last = last;
        badgeAlert = data.badgeAlert;
        nameView.setText(data.name);
        nameView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider));
        statusView.setText(data.status);
        statusView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteGrayText, resourcesProvider));

        if (TextUtils.isEmpty(data.badge)) {
            badgeView.setVisibility(GONE);
        } else {
            final int badgeColor = data.badgeAlert
                    ? Theme.getColor(Theme.key_text_RedBold, resourcesProvider)
                    : Theme.getColor(Theme.key_windowBackgroundWhiteBlueText, resourcesProvider);
            badgeView.setText(data.badge);
            badgeView.setTextColor(badgeColor);
            badgeView.setBackground(Theme.createRoundRectDrawable(dp(9), ColorUtils.setAlphaComponent(badgeColor, 34)));
            badgeView.setVisibility(VISIBLE);
        }

        placeholderView.setBackground(new FGPluginAvatar(getContext(), data.id, data.icon,
                ICON_RADIUS_DP / (float) ICON_DP));
        placeholderView.setVisibility(VISIBLE);
        iconView.setVisibility(GONE);
        iconView.setImageDrawable(null);
        FGPluginIcon.load(iconView, data.icon, ICON_DP, () -> placeholderView.setVisibility(GONE));

        checkBox.setChecked(data.enabled, false);
        setEnabledLook(data.enabled, false);
        dividerPaint.setColor(Theme.getColor(Theme.key_divider, resourcesProvider));
        requestLayout();
        invalidate();
    }

    @Override
    public void updateColors() {
        nameView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider));
        statusView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteGrayText, resourcesProvider));
        final int badgeColor = Theme.getColor(badgeAlert ? Theme.key_text_RedBold
                : Theme.key_windowBackgroundWhiteBlueText, resourcesProvider);
        badgeView.setTextColor(badgeColor);
        badgeView.setBackground(Theme.createRoundRectDrawable(dp(9), ColorUtils.setAlphaComponent(badgeColor, 34)));
        dividerPaint.setColor(Theme.getColor(Theme.key_divider, resourcesProvider));
        invalidate();
    }

    private void setEnabledLook(boolean enabled, boolean animated) {
        final float alpha = enabled ? 1f : DISABLED_ALPHA;
        final ColorMatrix matrix = new ColorMatrix();
        matrix.setSaturation(enabled ? 1f : 0f);
        iconView.getImageReceiver().setColorFilter(enabled ? null : new ColorMatrixColorFilter(matrix));
        for (View view : new View[]{placeholderView, iconView}) {
            view.animate().cancel();
            if (animated) {
                view.animate().alpha(alpha).setDuration(200)
                        .setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT).start();
            } else {
                view.setAlpha(alpha);
            }
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        final int width = MeasureSpec.getSize(widthMeasureSpec);

        final int badgeSpace = badgeView.getVisibility() == VISIBLE ? dp(90) : 0;
        final LayoutParams nameParams = (LayoutParams) nameView.getLayoutParams();
        nameParams.width = Math.max(dp(40), width - dp(TEXT_LEFT_DP + SWITCH_RIGHT_DP + 37 + 12) - badgeSpace);
        super.onMeasure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(dp(HEIGHT_DP), MeasureSpec.EXACTLY));
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        if (badgeView.getVisibility() == VISIBLE) {

            final int nameRight = nameView.getLeft() + Math.min(nameView.getMeasuredWidth(),
                    (int) Math.ceil(nameView.getPaint().measureText(String.valueOf(nameView.getText()))));
            final int badgeLeft = nameRight + dp(8);
            final int badgeTop = nameView.getTop() + (nameView.getMeasuredHeight() - badgeView.getMeasuredHeight()) / 2;
            badgeView.layout(badgeLeft, badgeTop, badgeLeft + badgeView.getMeasuredWidth(),
                    badgeTop + badgeView.getMeasuredHeight());
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (!last) {
            final float y = getMeasuredHeight() - Math.max(1, dp(0.5f));
            canvas.drawRect(dp(TEXT_LEFT_DP), y, getMeasuredWidth() - dp(SIDE_DP), getMeasuredHeight(), dividerPaint);
        }
    }

    @Override
    public void setPressed(boolean pressed) {
        super.setPressed(pressed);
        animate().scaleX(pressed ? 0.98f : 1f).scaleY(pressed ? 0.98f : 1f)
                .setDuration(pressed ? 100 : 160)
                .setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT).start();
    }

    public static class Factory extends UItem.UItemFactory<FGPluginCell> {
        static { setup(new Factory()); }

        @Override
        public FGPluginCell createView(Context context, RecyclerListView listView, int currentAccount,
                                       int classGuid, Theme.ResourcesProvider resourcesProvider) {
            return new FGPluginCell(context, resourcesProvider);
        }

        @Override
        public void bindView(View view, UItem item, boolean divider, UniversalAdapter adapter, UniversalRecyclerView listView) {
            final Data data = (Data) item.object;
            final FGPluginCell cell = (FGPluginCell) view;
            cell.set(data, data.first, data.last);
            cell.setOnToggle(data.onToggle);
        }

        public static class Data {
            public final String id;
            public final String name;
            public final CharSequence status;
            public final String icon;
            public final boolean enabled;
            public final Runnable onToggle;
            public String badge;
            public boolean badgeAlert;
            public boolean first;
            public boolean last;

            public Data(String id, String name, CharSequence status, String icon, boolean enabled, Runnable onToggle) {
                this.id = id;
                this.name = name;
                this.status = status;
                this.icon = icon;
                this.enabled = enabled;
                this.onToggle = onToggle;
            }
        }

        public static UItem of(int id, Data data) {
            UItem item = UItem.ofFactory(Factory.class);
            item.id = id;
            item.object = data;
            return item;
        }
    }
}

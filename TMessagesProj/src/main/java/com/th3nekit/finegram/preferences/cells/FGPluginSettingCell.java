/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.preferences.cells;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.telegram.messenger.LocaleController;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.RecyclerListView;
import org.telegram.ui.Components.Switch;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import org.telegram.ui.Components.UniversalRecyclerView;

public class FGPluginSettingCell extends LinearLayout implements Theme.Colorable {
    private final Theme.ResourcesProvider resourcesProvider;
    private final TextView title, value, description;
    private final ImageView icon;
    private final Switch toggle;
    private boolean checkable, checked, red, accent;
    private UItem boundItem;

    public FGPluginSettingCell(Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;
        setOrientation(HORIZONTAL);
        setGravity(Gravity.CENTER_VERTICAL);
        setLayoutDirection(LocaleController.isRTL ? LAYOUT_DIRECTION_RTL : LAYOUT_DIRECTION_LTR);
        setMinimumHeight(dp(56));
        setPaddingRelative(dp(20), dp(14), dp(22), dp(14));
        icon = new ImageView(context);
        icon.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
        icon.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        LinearLayout.LayoutParams iconParams = new LinearLayout.LayoutParams(dp(24), dp(24));
        iconParams.setMarginEnd(dp(16));
        addView(icon, iconParams);
        LinearLayout texts = new LinearLayout(context);
        texts.setOrientation(VERTICAL);
        title = text(context, 16);
        value = text(context, 14);
        description = text(context, 13);
        texts.addView(title, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
        texts.addView(value, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 5, 0, 0));
        texts.addView(description, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 5, 0, 0));
        addView(texts, LayoutHelper.createLinear(0, LayoutHelper.WRAP_CONTENT, 1f));
        toggle = new Switch(context, resourcesProvider);
        toggle.setColors(Theme.key_switchTrack, Theme.key_switchTrackChecked, Theme.key_windowBackgroundWhite, Theme.key_windowBackgroundWhite);
        toggle.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        toggle.setClickable(false);
        toggle.setFocusable(false);
        LinearLayout.LayoutParams switchParams = new LinearLayout.LayoutParams(dp(39), dp(40));
        switchParams.setMarginStart(dp(16));
        addView(toggle, switchParams);
        updateColors();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(MeasureSpec.makeMeasureSpec(MeasureSpec.getSize(widthMeasureSpec), MeasureSpec.EXACTLY), heightMeasureSpec);
    }

    private TextView text(Context context, int size) {
        TextView view = new TextView(context);
        view.setTextSize(TypedValue.COMPLEX_UNIT_SP, size);
        view.setGravity(Gravity.START);
        view.setTextDirection(TEXT_DIRECTION_LOCALE);
        view.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        return view;
    }

    private void bind(UItem item) {
        boundItem = item;
        title.setText(item.text);
        value.setText(item.textValue);
        value.setVisibility(TextUtils.isEmpty(item.textValue) ? GONE : VISIBLE);
        description.setText(item.subtext);
        description.setVisibility(TextUtils.isEmpty(item.subtext) ? GONE : VISIBLE);
        icon.setVisibility(item.iconResId == 0 ? GONE : VISIBLE);
        if (item.iconResId != 0) icon.setImageResource(item.iconResId);
        checkable = item.flags == 1;
        checked = item.checked;
        toggle.setVisibility(checkable ? VISIBLE : GONE);
        toggle.setChecked(checked, false);
        red = item.red;
        accent = item.accent;
        setEnabled(item.enabled);
        setAlpha(item.enabled ? 1f : .5f);
        setContentDescription(TextUtils.concat(item.text == null ? "" : item.text,
                TextUtils.isEmpty(item.textValue) ? "" : ", " + item.textValue,
                TextUtils.isEmpty(item.subtext) ? "" : ", " + item.subtext));
        updateColors();
    }

    public void setChecked(boolean checked) {
        this.checked = checked;
        if (boundItem != null) boundItem.setChecked(checked);
        toggle.setChecked(checked, false);
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);
        if (checkable) {
            info.setClassName("android.widget.Switch");
            info.setCheckable(true);
            info.setChecked(checked);
        }
    }

    @Override
    public void updateColors() {
        int text = Theme.getColor(red ? Theme.key_text_RedBold : accent ? Theme.key_windowBackgroundWhiteBlueText : Theme.key_windowBackgroundWhiteBlackText, resourcesProvider);
        title.setTextColor(text);
        value.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteValueText, resourcesProvider));
        description.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteGrayText, resourcesProvider));
        icon.setColorFilter(new PorterDuffColorFilter(text, PorterDuff.Mode.SRC_IN));
        toggle.invalidate();
    }

    public static class Factory extends UItem.UItemFactory<FGPluginSettingCell> {
        static { setup(new Factory()); }

        @Override
        public boolean isClickable() {
            return true;
        }

        @Override
        public FGPluginSettingCell createView(Context context, RecyclerListView listView, int currentAccount,
                                              int classGuid, Theme.ResourcesProvider resourcesProvider) {
            return new FGPluginSettingCell(context, resourcesProvider);
        }

        @Override
        public void bindView(View view, UItem item, boolean divider, UniversalAdapter adapter, UniversalRecyclerView listView) {
            ((FGPluginSettingCell) view).bind(item);
        }

        public static UItem of(int id, CharSequence title, CharSequence value, CharSequence description,
                               int icon, Boolean checked) {
            UItem item = UItem.ofFactory(Factory.class);
            item.id = id;
            item.text = title;
            item.textValue = value;
            item.subtext = description;
            item.iconResId = icon;
            item.flags = checked == null ? 0 : 1;
            item.checked = Boolean.TRUE.equals(checked);
            return item;
        }
    }
}

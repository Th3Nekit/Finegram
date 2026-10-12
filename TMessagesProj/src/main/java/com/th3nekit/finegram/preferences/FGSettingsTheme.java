package com.th3nekit.finegram.preferences;

import android.view.View;
import android.view.ViewGroup;

import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.ActionBar.ThemeDescription;
import org.telegram.ui.Cells.CheckBoxCell;
import org.telegram.ui.Cells.DialogRadioCell;
import org.telegram.ui.Cells.HeaderCell;
import org.telegram.ui.Cells.NotificationsCheckCell;
import org.telegram.ui.Cells.RadioButtonCell;
import org.telegram.ui.Cells.TextCell;
import org.telegram.ui.Cells.TextCheckCell;
import org.telegram.ui.Cells.TextCheckCell2;
import org.telegram.ui.Cells.TextDetailSettingsCell;
import org.telegram.ui.Cells.TextInfoPrivacyCell;
import org.telegram.ui.Cells.TextSettingsCell;
import org.telegram.ui.Components.SimpleThemeDescription;
import org.telegram.ui.Components.UniversalRecyclerView;

import java.util.ArrayList;

public final class FGSettingsTheme {
    private FGSettingsTheme() {}

    public static ArrayList<ThemeDescription> descriptions(UniversalRecyclerView list,
                                                          ThemeDescription.ThemeDescriptionDelegate delegate) {
        ArrayList<ThemeDescription> result = SimpleThemeDescription.createThemeDescriptions(delegate,
                Theme.key_windowBackgroundWhite, Theme.key_windowBackgroundGray,
                Theme.key_windowBackgroundWhiteBlackText, Theme.key_windowBackgroundWhiteGrayText,
                Theme.key_windowBackgroundWhiteGrayText2, Theme.key_windowBackgroundWhiteGrayText4,
                Theme.key_windowBackgroundWhiteValueText, Theme.key_windowBackgroundWhiteGrayIcon,
                Theme.key_windowBackgroundWhiteBlueHeader, Theme.key_windowBackgroundWhiteBlueText,
                Theme.key_windowBackgroundWhiteBlueText4, Theme.key_windowBackgroundWhiteLinkText,
                Theme.key_text_RedBold, Theme.key_text_RedRegular, Theme.key_divider,
                Theme.key_listSelector, Theme.key_featuredStickers_addButton,
                Theme.key_featuredStickers_buttonText, Theme.key_radioBackground,
                Theme.key_radioBackgroundChecked, Theme.key_switchTrack, Theme.key_switchTrackChecked,
                Theme.key_switchTrackBlue, Theme.key_switchTrackBlueChecked,
                Theme.key_switchTrackBlueThumb, Theme.key_switchTrackBlueThumbChecked,
                Theme.key_windowBackgroundCheckText, Theme.key_windowBackgroundChecked,
                Theme.key_windowBackgroundUnchecked);
        result.add(new ThemeDescription(list, ThemeDescription.FLAG_SELECTOR, null, null, null, null, Theme.key_listSelector));
        result.add(new ThemeDescription(list, ThemeDescription.FLAG_LISTGLOWCOLOR, null, null, null, null, Theme.key_windowBackgroundWhite));
        field(result, list, HeaderCell.class, "textView", Theme.key_windowBackgroundWhiteBlueHeader, ThemeDescription.FLAG_CHECKTAG);
        field(result, list, HeaderCell.class, "animatedTextView", Theme.key_windowBackgroundWhiteBlueHeader, ThemeDescription.FLAG_CHECKTAG);
        field(result, list, HeaderCell.class, "textView", Theme.key_windowBackgroundWhiteBlackText, ThemeDescription.FLAG_CHECKTAG);
        field(result, list, HeaderCell.class, "animatedTextView", Theme.key_windowBackgroundWhiteBlackText, ThemeDescription.FLAG_CHECKTAG);
        field(result, list, HeaderCell.class, "textView2", Theme.key_windowBackgroundWhiteGrayText2, 0);
        for (Class<?> type : new Class<?>[]{TextCheckCell.class, TextCheckCell2.class,
                TextSettingsCell.class, TextDetailSettingsCell.class, DialogRadioCell.class, RadioButtonCell.class, NotificationsCheckCell.class}) {
            field(result, list, type, "textView", Theme.key_windowBackgroundWhiteBlackText,
                    type == TextCheckCell.class ? ThemeDescription.FLAG_CHECKTAG : 0);
            field(result, list, type, "valueTextView", type == TextSettingsCell.class
                    ? Theme.key_windowBackgroundWhiteValueText : Theme.key_windowBackgroundWhiteGrayText2, 0);
        }
        field(result, list, TextCheckCell.class, "textView", Theme.key_windowBackgroundCheckText, ThemeDescription.FLAG_CHECKTAG);
        field(result, list, TextSettingsCell.class, "imageView", Theme.key_windowBackgroundWhiteGrayIcon, ThemeDescription.FLAG_IMAGECOLOR);
        field(result, list, TextCheckCell2.class, "imageView", Theme.key_windowBackgroundWhiteGrayIcon, ThemeDescription.FLAG_IMAGECOLOR);
        field(result, list, TextCheckCell2.class, "animatedTextView", Theme.key_windowBackgroundWhiteBlackText, 0);
        field(result, list, TextCheckCell2.class, "collapsedArrow", Theme.key_windowBackgroundWhiteBlackText, ThemeDescription.FLAG_USEBACKGROUNDDRAWABLE);
        field(result, list, NotificationsCheckCell.class, "imageView", Theme.key_windowBackgroundWhiteGrayIcon, ThemeDescription.FLAG_IMAGECOLOR);
        for (int key : new int[]{Theme.key_windowBackgroundWhiteGrayText4, Theme.key_text_RedRegular, Theme.key_text_RedBold}) {
            field(result, list, TextInfoPrivacyCell.class, "textView", key, ThemeDescription.FLAG_CHECKTAG);
        }
        field(result, list, TextInfoPrivacyCell.class, "textView", Theme.key_windowBackgroundWhiteLinkText, ThemeDescription.FLAG_LINKCOLOR);
        field(result, list, DialogRadioCell.class, "radioButton", Theme.key_radioBackground, ThemeDescription.FLAG_CHECKBOX);
        field(result, list, DialogRadioCell.class, "radioButton", Theme.key_radioBackgroundChecked, ThemeDescription.FLAG_CHECKBOXCHECK);
        field(result, list, RadioButtonCell.class, "radioButton", Theme.key_radioBackground, ThemeDescription.FLAG_CHECKBOX);
        field(result, list, RadioButtonCell.class, "radioButton", Theme.key_radioBackgroundChecked, ThemeDescription.FLAG_CHECKBOXCHECK);
        return result;
    }

    private static void field(ArrayList<ThemeDescription> result, UniversalRecyclerView list,
                              Class<?> type, String field, int key, int flags) {
        result.add(new ThemeDescription(list, flags, new Class[]{type}, new String[]{field}, null, null, null, key));
    }

    public static void refresh(UniversalRecyclerView list, Theme.ResourcesProvider provider) {
        if (list == null) return;
        for (ThemeDescription description : descriptions(list, null)) {
            description.setColor(Theme.getColor(description.getCurrentKey(), provider), false, false);
        }
        refreshRows(list);
    }

    public static void refreshRows(UniversalRecyclerView list) {
        if (list == null) return;
        for (int i = 0; i < list.getChildCount(); i++) {
            refreshView(list.getChildAt(i));
        }
        list.invalidate();
    }

    private static void refreshView(View view) {
        if (view instanceof Theme.Colorable) {
            ((Theme.Colorable) view).updateColors();
        } else if (view instanceof TextCell) {
            ((TextCell) view).updateColors();
        } else if (view instanceof CheckBoxCell) {
            ((CheckBoxCell) view).updateTextColor();
        }
        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++) refreshView(group.getChildAt(i));
        }
        view.invalidate();
    }
}

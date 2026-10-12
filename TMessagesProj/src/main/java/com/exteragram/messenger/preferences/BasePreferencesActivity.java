/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.preferences;

import android.view.HapticFeedbackConstants;
import android.view.View;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.ui.Cells.TextCell;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.ItemOptions;
import org.telegram.ui.Components.ShareAlert;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;
import java.util.Collection;

import com.exteragram.messenger.utils.ui.PopupUtils;
import com.th3nekit.finegram.preferences.BaseCGPreferencesEntry;

public class BasePreferencesActivity extends BaseCGPreferencesEntry {

    @Override
    public String getTitle() {
        return "";
    }

    @Override
    public void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
    }

    @Override
    public void onClick(UItem item, View view, int position, float x, float y) {
    }

    @Override
    public boolean onLongClick(UItem item, View view, int position, float x, float y) {
        return false;
    }

    public boolean hasHeaderCell() {
        return false;
    }

    public boolean needHideTitle() {
        return false;
    }

    public void initializeOptionStrings() {
    }

    public void scrollToItem(int itemId) {
        if (listView == null || listView.adapter == null || layoutManager == null) {
            return;
        }
        int position = listView.findPositionByItemId(itemId);
        if (position < 0 || position >= listView.adapter.getItemCount()) {
            return;
        }
        layoutManager.scrollToPositionWithOffset(position, AndroidUtilities.dp(60));
        listView.highlightRow(() -> listView.findPositionByItemId(itemId));
    }

    @Override
    public void showRestartBulletin() {
        super.showRestartBulletin();
    }

    public void showListDialog(UItem item, CharSequence[] items, String title, int selected, PopupUtils.OnItemClickListener listener) {
        showListDialog(item, items, null, title, selected, listener);
    }

    public void showListDialog(UItem item, CharSequence[] items, int[] icons, String title, int selected, PopupUtils.OnItemClickListener listener) {
        showListDialog(item, items, icons, title, selected, listener, icons == null, true);
    }

    public void showListDialog(UItem item, CharSequence[] items, int[] icons, String title, int selected,
                               PopupUtils.OnItemClickListener listener, boolean withRadio, boolean skipSame) {
        if (getParentActivity() == null) {
            return;
        }
        PopupUtils.showDialog(items, icons, title, selected, getContext(), which -> {
            if (skipSame && which == selected) {
                return;
            }
            listener.onClick(which);
            if (listView == null) {
                return;
            }
            View view = listView.findViewByItemId(item.id);
            if (view instanceof TextCell) {
                ((TextCell) view).setValue(items[which], true);
            }
            listView.adapter.update(true);
        }, getResourceProvider(), withRadio);
    }

    public void showCopyLinkOptions(View view, String link) {
        view.performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP, HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING);
        ItemOptions.makeOptions(this, view)
                .add(R.drawable.msg_copy, LocaleController.getString(R.string.CopyLink), () -> {
                    if (AndroidUtilities.addToClipboard(link)) {
                        BulletinFactory.of(this).createCopyBulletin(LocaleController.getString(R.string.LinkCopied)).show();
                    }
                })
                .add(R.drawable.msg_share, LocaleController.getString(R.string.ShareLink), () ->
                        showDialog(new ShareAlert(getContext(), null, link, false, link, false, getResourceProvider())))
                .setScrimViewBackground(listView.getClipBackground(view))
                .show();
    }

    public int[] unBox(Collection<Integer> values) {
        return values.stream().mapToInt(Integer::intValue).toArray();
    }
}

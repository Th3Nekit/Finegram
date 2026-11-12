/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.preferences.anonymity;

import static org.telegram.messenger.LocaleController.getString;

import android.view.View;
import android.view.Gravity;
import android.widget.FrameLayout;
import android.text.InputFilter;
import android.util.TypedValue;
import org.telegram.messenger.AndroidUtilities;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.EditTextBoldCursor;
import org.telegram.ui.Components.LayoutHelper;
import com.th3nekit.finegram.helpers.ui.PopupHelper;
import com.th3nekit.finegram.privacy.FGDeletedMark;

import org.telegram.messenger.R;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;

import com.th3nekit.finegram.core.configs.FinegramPrivacyConfig;
import com.th3nekit.finegram.preferences.BaseCGPreferencesEntry;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;
import com.th3nekit.finegram.privacy.FGDeletedExceptions;
import com.th3nekit.finegram.privacy.FGDeletedStore;

public class SavedMessagesPreferencesEntry extends BaseCGPreferencesEntry {

    private final int deletedRow = 1;
    private final int botsRow = 2, channelsRow = 3;
    private final int dimRow = 4;
    private final int logRow = 5;
    private final int editsRow = 6;
    private final int exceptionsRow = 7;
    private final int customMarkRow = 8, markTextRow = 9, markColorRow = 10;
    private static final int[] MARK_COLORS = {0, 0xffe85b55, 0xffe68a2e, 0xff40a868, 0xff448fe5, 0xffa16ce0};

    @Override
    public void onResume() {
        super.onResume();
        updateRows(false);
    }

    @Override
    public String getTitle() {
        return getString(R.string.FG_Saved);
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        items.add(UItem.asShadow(null));
        items.add(SettingsHelper.asSwitchCG(deletedRow, getString(R.string.FG_Saved))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getSaveDeleted()));
        items.add(UItem.asShadow(getString(R.string.FG_Saved_Desc)));

        final boolean on = FinegramPrivacyConfig.INSTANCE.getSaveDeleted();
        items.add(UItem.asHeader(getString(R.string.FG_Saved_Where)));
        items.add(UItem.asCheck(botsRow, getString(R.string.FG_Saved_Bots))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getSaveDeletedFromBots()).setEnabled(on));
        items.add(UItem.asCheck(channelsRow, getString(R.string.FG_Saved_Channels))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getSaveDeletedFromChannels()).setEnabled(on));
        items.add(UItem.asCheck(dimRow, getString(R.string.FG_DimDeleted))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getDimDeleted()).setEnabled(on));
        items.add(SettingsHelper.asSwitchCG(customMarkRow, getString(R.string.FG_CustomDeletedMark), getString(R.string.FG_CustomDeletedMark_Desc))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getCustomDeletedMark()).slug("customDeletedMark"));
        String text = FGDeletedMark.normalizeText(FinegramPrivacyConfig.INSTANCE.getDeletedMarkText());
        items.add(UItem.asButton(markTextRow, getString(R.string.FG_DeletedMarkText),
                text.isEmpty() ? getString(R.string.FG_Saved_Mark) : text).setEnabled(FinegramPrivacyConfig.INSTANCE.getCustomDeletedMark()).slug("deletedMarkText"));
        items.add(UItem.asButton(markColorRow, getString(R.string.FG_DeletedMarkColor), colorTitles().get(colorIndex())).setEnabled(FinegramPrivacyConfig.INSTANCE.getCustomDeletedMark()).slug("deletedMarkColor"));
        items.add(UItem.asShadow(null));

        if (on) {
            items.add(UItem.asButton(logRow, R.drawable.msg_viewchats,
                    getString(R.string.FG_Saved_Log), String.valueOf(FGDeletedStore.count(currentAccount))));
        }

        items.add(UItem.asButton(exceptionsRow, getString(R.string.FG_Saved_Exceptions),
                String.valueOf(FGDeletedExceptions.dialogs(currentAccount).size())));
        items.add(UItem.asShadow(getString(R.string.FG_Saved_Exceptions_Info)));

        items.add(SettingsHelper.asSwitchCG(editsRow, getString(R.string.FG_KeepEdits))
                .setChecked(FinegramPrivacyConfig.INSTANCE.getKeepEdits()));
        items.add(UItem.asShadow(getString(R.string.FG_KeepEdits_Desc)));
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.id == deletedRow) {
            FinegramPrivacyConfig.INSTANCE.setSaveDeleted(!FinegramPrivacyConfig.INSTANCE.getSaveDeleted());
            updateRows(true);
        } else if (item.id == botsRow) {
            FinegramPrivacyConfig.INSTANCE.setSaveDeletedFromBots(!FinegramPrivacyConfig.INSTANCE.getSaveDeletedFromBots());
            updateRows(false);
        } else if (item.id == channelsRow) {
            FinegramPrivacyConfig.INSTANCE.setSaveDeletedFromChannels(!FinegramPrivacyConfig.INSTANCE.getSaveDeletedFromChannels());
            updateRows(false);
        } else if (item.id == dimRow) {
            FinegramPrivacyConfig.INSTANCE.setDimDeleted(!FinegramPrivacyConfig.INSTANCE.getDimDeleted());
            updateRows(false);
        } else if (item.id == customMarkRow) {
            FinegramPrivacyConfig.INSTANCE.setCustomDeletedMark(!FinegramPrivacyConfig.INSTANCE.getCustomDeletedMark());
            refreshMark();
        } else if (item.id == markTextRow) {
            showMarkTextDialog();
        } else if (item.id == markColorRow) {
            PopupHelper.show(colorTitles(), getString(R.string.FG_DeletedMarkColor), colorIndex(), getContext(), i -> {
                FinegramPrivacyConfig.INSTANCE.setDeletedMarkColor(MARK_COLORS[i]);
                refreshMark();
            }, getResourceProvider());
        } else if (item.id == editsRow) {
            FinegramPrivacyConfig.INSTANCE.setKeepEdits(!FinegramPrivacyConfig.INSTANCE.getKeepEdits());
            SettingsHelper.updateCheckState(view, FinegramPrivacyConfig.INSTANCE.getKeepEdits());
        } else if (item.id == logRow) {
            final com.th3nekit.finegram.privacy.ui.FGDeletedLogFragment fragment =
                    new com.th3nekit.finegram.privacy.ui.FGDeletedLogFragment();
            fragment.setCurrentAccount(currentAccount);
            presentFragment(fragment);
        } else if (item.id == exceptionsRow) {
            final com.th3nekit.finegram.privacy.ui.FGDeletedExclusionsFragment fragment =
                    new com.th3nekit.finegram.privacy.ui.FGDeletedExclusionsFragment();
            fragment.setCurrentAccount(currentAccount);
            presentFragment(fragment);
        }
    }

    private ArrayList<String> colorTitles() {
        ArrayList<String> titles = new ArrayList<>();
        for (int id : new int[]{R.string.Default, R.string.Red, R.string.Orange, R.string.Green, R.string.Blue, R.string.Purple}) {
            titles.add(getString(id));
        }
        return titles;
    }

    private int colorIndex() {
        int color = FinegramPrivacyConfig.INSTANCE.getDeletedMarkColor();
        for (int i = 0; i < MARK_COLORS.length; i++) if (MARK_COLORS[i] == color) return i;
        return 0;
    }

    private void refreshMark() {
        updateRows(true);
        if (getParentLayout() != null) getParentLayout().rebuildAllFragmentViews(false, false);
    }

    private void showMarkTextDialog() {
        if (getParentActivity() == null) return;
        EditTextBoldCursor text = new EditTextBoldCursor(getParentActivity());
        text.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 16);
        text.setTextColor(getThemedColor(Theme.key_dialogTextBlack));
        text.setHintText(getString(R.string.FG_Saved_Mark));
        text.setHintColor(getThemedColor(Theme.key_dialogTextHint));
        text.setCursorColor(getThemedColor(Theme.key_chats_actionBackground));
        text.setSingleLine(true);
        text.setFilters(new InputFilter[]{new InputFilter.LengthFilter(48)});
        text.setText(FinegramPrivacyConfig.INSTANCE.getDeletedMarkText());
        text.setSelection(text.length());
        FrameLayout wrapper = new FrameLayout(getParentActivity());
        wrapper.addView(text, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, Gravity.TOP, 24, 8, 24, 8));
        AlertDialog dialog = new AlertDialog.Builder(getParentActivity(), getResourceProvider())
                .setTitle(getString(R.string.FG_DeletedMarkText))
                .setMessage(getString(R.string.FG_DeletedMarkText_Desc))
                .setView(wrapper)
                .setPositiveButton(getString(R.string.Save), (d, which) -> {
                    FinegramPrivacyConfig.INSTANCE.setDeletedMarkText(FGDeletedMark.normalizeText(text.getText().toString()));
                    refreshMark();
                })
                .setNegativeButton(getString(R.string.Cancel), null).create();
        dialog.setOnShowListener(d -> { text.requestFocus(); AndroidUtilities.showKeyboard(text); });
        showDialog(dialog);
    }

    @Override
    protected String getKey() {
        return "cg_saved_messages";
    }
}

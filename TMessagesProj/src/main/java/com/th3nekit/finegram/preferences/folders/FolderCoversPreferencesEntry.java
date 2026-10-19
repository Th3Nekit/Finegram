package com.th3nekit.finegram.preferences.folders;

import android.app.Activity;
import android.content.Intent;
import android.view.View;
import androidx.recyclerview.widget.RecyclerView;
import java.util.ArrayList;
import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.R;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import com.th3nekit.finegram.core.ui.folders.FolderCoverStore;
import com.th3nekit.finegram.core.ui.folders.FolderCoverView;
import com.th3nekit.finegram.helpers.ui.PopupHelper;
import com.th3nekit.finegram.preferences.BaseCGPreferencesEntry;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;
import static org.telegram.messenger.LocaleController.getString;

public final class FolderCoversPreferencesEntry extends BaseCGPreferencesEntry {
    private int folder;
    private FolderCoverView preview;
    private boolean importing;
    private boolean destroyed;
    private int requestedFolder;

    @Override protected CharSequence getTitle() { return getString(R.string.FG_FolderCovers); }
    @Override protected String getKey() { return "folderCovers"; }

    private String folderName() {
        for (MessagesController.DialogFilter filter : getMessagesController().getDialogFilters()) {
            if (filter.id == folder) return filter.isDefault() ? getString(R.string.FilterAllChats) : filter.name.toString();
        }
        return getString(R.string.FilterAllChats);
    }

    @Override protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        if (getContext() == null) return;
        if (preview == null) {
            preview = new FolderCoverView(getContext(), getResourceProvider());
            preview.setLayoutParams(new RecyclerView.LayoutParams(-1, AndroidUtilities.dp(112)));
        }
        preview.setFolder(currentAccount, folder, false);
        items.add(SettingsHelper.asCustomWithBackground(preview));
        items.add(UItem.asButton(1, getString(R.string.FG_CoverFolder), folderName()));
        items.add(UItem.asButton(2, getString(R.string.FG_CoverArtwork)));
        items.add(UItem.asButton(3, getString(R.string.FG_CoverImage)));
        items.add(UItem.asButton(4, getString(R.string.Reset)));
        items.add(UItem.asShadow(getString(importing ? R.string.FG_CoverImporting : R.string.FG_CoverInfo)));
    }

    private void refresh() {
        if (destroyed) return;
        if (preview != null) preview.setFolder(currentAccount, folder, true);
        getNotificationCenter().postNotificationName(NotificationCenter.dialogFiltersUpdated);
        updateRows(false);
    }

    @Override protected void onClick(UItem item, View view, int position, float x, float y) {
        if (importing) return;
        if (item.id == 1) {
            ArrayList<String> names = new ArrayList<>();
            ArrayList<Integer> ids = new ArrayList<>();
            names.add(getString(R.string.FilterAllChats)); ids.add(0);
            for (MessagesController.DialogFilter filter : getMessagesController().getDialogFilters()) {
                if (!filter.isDefault()) { names.add(filter.name.toString()); ids.add(filter.id); }
            }
            PopupHelper.show(names, getString(R.string.FG_CoverFolder), ids.indexOf(folder), getContext(), i -> { folder = ids.get(i); refresh(); });
        } else if (item.id == 2) {
            ArrayList<String> names = new ArrayList<>();
            names.add(getString(R.string.FG_CoverAutomatic));
            names.add(getString(R.string.FG_CoverGeometry));
            names.add(getString(R.string.FG_CoverLandscape));
            PopupHelper.show(names, getString(R.string.FG_CoverArtwork), Math.min(2, FolderCoverStore.get(currentAccount, folder).style), getContext(), i -> { FolderCoverStore.setStyle(currentAccount, folder, i); refresh(); });
        } else if (item.id == 3) {
            requestedFolder = folder;
            Intent intent = new Intent(Intent.ACTION_GET_CONTENT).setType("image/*").addCategory(Intent.CATEGORY_OPENABLE);
            startActivityForResult(Intent.createChooser(intent, getString(R.string.FG_CoverImage)), 731);
        } else if (item.id == 4) {
            FolderCoverStore.reset(currentAccount, folder); refresh();
        }
    }

    @Override public void onActivityResultFragment(int requestCode, int resultCode, Intent data) {
        super.onActivityResultFragment(requestCode, resultCode, data);
        if (requestCode != 731 || resultCode != Activity.RESULT_OK || data == null || data.getData() == null) return;
        importing = true;
        updateRows(false);
        FolderCoverStore.importImage(currentAccount, requestedFolder, data.getData(), success -> {
            importing = false;
            if (destroyed) return;
            if (!success) BulletinFactory.of(this).createErrorBulletin(getString(R.string.FG_CoverImportFailed)).show();
            refresh();
        });
    }

    @Override public void onFragmentDestroy() { destroyed = true; super.onFragmentDestroy(); }
}

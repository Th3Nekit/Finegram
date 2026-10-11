/*
 * Finegram plugin store.
 * Adapted from Kangel Plugins Manager (KangelPlugins).
 * Upstream: https://git.kangel.xyz/KangelPlugins/PluginManager
 * Licensed under GNU GPL v3; see LICENSE.PluginManager and NOTICE.
 */

package com.th3nekit.finegram.store;

import static org.telegram.messenger.LocaleController.formatString;
import static org.telegram.messenger.LocaleController.getString;

import android.view.Gravity;
import android.view.View;
import android.widget.EditText;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.ActionBar;
import org.telegram.ui.ActionBar.ActionBarMenu;
import org.telegram.ui.ActionBar.ActionBarMenuItem;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.ItemOptions;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import com.exteragram.messenger.plugins.ui.components.InstallPluginBottomSheet;
import com.th3nekit.finegram.core.configs.FinegramCoreConfig;
import com.th3nekit.finegram.preferences.BaseCGPreferencesEntry;
import com.th3nekit.finegram.helpers.ui.PopupHelper;

public class FGStoreActivity extends BaseCGPreferencesEntry {

    private static final int SORT_NEW = 0;
    private static final int SORT_NAME = 1;
    private static final int SORT_POPULAR = 2;
    private static final int SORT_AUTHOR = 3;
    private static final int SORT_INSTALLED = 4;

    private static final int ROW_BASE = 1000;

    private static final int FILTERS_HEIGHT_DP = 52;
    private static final int FEATURED_ROW = 11;

    private static final int FEATURED_COUNT = 10;

    private static final int UP_BUTTON_AFTER = 8;
    private static final int SKELETON_ROW_BASE = 20;
    private static final int MENU_SORT = 1;
    private static final int MENU_AUTO_UPDATE = 2;

    private final ArrayList<FGStore.Item> shown = new ArrayList<>();

    private Map<String, String> installed = Collections.emptyMap();

    private final ArrayList<FGStore.Category> categories = new ArrayList<>();

    private FGStore.Category category;
    private int sort = FinegramCoreConfig.INSTANCE.getStoreSort();
    private String query = "";
    private boolean loading;

    @Override
    protected CharSequence getTitle() {
        return getString(R.string.FG_Store);
    }

    @Override
    public boolean onFragmentCreate() {
        boolean created = super.onFragmentCreate();
        loading = FGStore.cached().isEmpty();
        FGStore.refresh(false, () -> {
            loading = false;
            updateRows(true);
            toTop();
        });
        return created;
    }

    @Override
    public View createView(android.content.Context context) {
        final ActionBarMenu menu = actionBar.createMenu();
        searchItem = menu.addItem(2, R.drawable.msg_search)
                .setIsSearchField(true)
                .setActionBarMenuItemSearchListener(new ActionBarMenuItem.ActionBarMenuItemSearchListener() {
                    @Override
                    public void onSearchCollapse() {
                        query = "";
                        updateRows(false);
                        toTop();
                    }

                    @Override
                    public void onTextChanged(EditText editText) {
                        query = editText.getText() == null ? ""
                                : editText.getText().toString().trim().toLowerCase(Locale.getDefault());
                        updateRows(false);
                        toTop();
                    }
                });
        searchItem.setSearchFieldHint(getString(R.string.Search));

        otherItem = menu.addItem(1, R.drawable.ic_ab_other);
        otherItem.setOnClickListener(view -> showItemOptions(otherItem));

        final View view = super.createView(context);
        addFilters(context);
        addUpButton(context);
        return view;
    }

    private void addFilters(android.content.Context context) {
        filtersView = new FGStoreFilters(context);
        filtersView.setBackgroundColor(getThemedColor(Theme.key_windowBackgroundGray));
        filtersView.setDelegate(index -> {
            category = index >= 0 && index < categories.size() ? categories.get(index) : null;
            updateRows(true);
            toTop();
        });
        contentView.addView(filtersView, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT,
                FILTERS_HEIGHT_DP, Gravity.TOP));

        actionBarContainer.addOnLayoutChangeListener((v, l, top, r, b, ol, ot, or, ob) -> {
            headerHeight = actionBarContainer.getMeasuredHeight();
            filtersView.setTranslationY(headerHeight);
            applyListPadding();
        });
        applyListPadding();
    }

    private void applyListPadding() {
        if (listView == null) {
            return;
        }
        final int header = headerHeight > 0 ? headerHeight : ActionBar.getCurrentActionBarHeight();
        final int top = header + (filtersShown ? AndroidUtilities.dp(FILTERS_HEIGHT_DP) : 0);
        if (listView.getPaddingTop() != top) {
            listView.setPadding(0, top, 0, 0);
        }
    }

    private void applyFilters() {
        if (filtersView == null) {
            return;
        }
        final boolean hasList = !categories.isEmpty() && query.isEmpty();
        filtersShown = hasList;
        filtersView.setVisibility(hasList ? View.VISIBLE : View.GONE);
        applyListPadding();
        if (!hasList) {
            return;
        }
        filtersView.setTitles(titlesOf(categories), Math.max(0, categories.indexOf(category)));
    }

    private void addUpButton(android.content.Context context) {
        upButton = new ImageView(context);
        upButton.setScaleType(ImageView.ScaleType.CENTER);
        upButton.setImageResource(R.drawable.msg_go_up);
        upButton.setColorFilter(getThemedColor(Theme.key_featuredStickers_buttonText));
        upButton.setBackground(Theme.createSimpleSelectorCircleDrawable(AndroidUtilities.dp(48),
                getThemedColor(Theme.key_featuredStickers_addButton),
                getThemedColor(Theme.key_featuredStickers_addButtonPressed)));
        upButton.setAlpha(0f);
        upButton.setScaleX(0.9f);
        upButton.setScaleY(0.9f);
        upButton.setVisibility(View.GONE);
        upButton.setOnClickListener(v -> {
            toTop();
        });
        contentView.addView(upButton, LayoutHelper.createFrame(48, 48,
                Gravity.RIGHT | Gravity.BOTTOM, 0, 0, 14, 28));

        listView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(@NonNull RecyclerView recyclerView, int dx, int dy) {
                showUpButton(layoutManager != null
                        && layoutManager.findFirstVisibleItemPosition() > UP_BUTTON_AFTER);
            }
        });
    }

    private void showUpButton(boolean show) {
        if (upButton == null || upShown == show) {
            return;
        }
        upShown = show;
        if (show) {
            upButton.setVisibility(View.VISIBLE);
        }
        upButton.animate().cancel();
        upButton.animate()
                .alpha(show ? 1f : 0f)
                .scaleX(show ? 1f : 0.9f)
                .scaleY(show ? 1f : 0.9f)
                .setDuration(180)
                .setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT)
                .withEndAction(() -> {
                    if (!upShown) {
                        upButton.setVisibility(View.GONE);
                    }
                })
                .start();
    }

    private ActionBarMenuItem otherItem;
    private FGStoreFilters filtersView;
    private boolean filtersShown = true;
    private int headerHeight;
    private ImageView upButton;
    private boolean upShown;

    private void showItemOptions(View button) {
        ItemOptions options = ItemOptions.makeOptions(this, button);
        options.add(R.drawable.msg_reorder, getString(R.string.FG_Store_Sort), this::showSortSelector);
        options.add(R.drawable.msg_download, getString(R.string.FG_Store_AutoUpdate), this::showAutoUpdateSelector);
        options.add(R.drawable.msg_retry_solar, getString(R.string.FG_Store_Refresh), () -> {
            loading = true;
            updateRows(false);
            FGStore.refresh(true, () -> {
                loading = false;
                updateRows(true);
                toTop();
            });
        });
        options.setBlur(false);
        options.setDrawScrim(false);

        options.setGravity(Gravity.RIGHT);
        options.translate(-AndroidUtilities.dp(8), 0);
        options.show();
    }

    @Override
    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        shown.clear();
        installed = FGStoreInstaller.installedVersions();

        final List<FGStore.Item> all = FGStore.cached();
        if (all.isEmpty()) {
            fillEmpty(items);
            return;
        }

        categories.clear();
        categories.add(null);
        for (FGStore.Item item : all) {
            if (!categories.contains(item.getCategory())) {
                categories.add(item.getCategory());
            }
        }
        if (!categories.contains(category)) {
            category = null;
        }

        AndroidUtilities.runOnUIThread(this::applyFilters);

        if (query.isEmpty()) {
            if (category == null) {
                final List<FGStore.Item> featured = featuredOf(all);
                if (!featured.isEmpty()) {
                    items.add(FGStoreFeatured.Factory.of(FEATURED_ROW,
                            new FGStoreFeatured.Factory.Data(featured, all.size(), this::onFeaturedPicked)));
                }
            }
        }

        final ArrayList<FGStore.Item> updates = new ArrayList<>();
        final ArrayList<FGStore.Item> rest = new ArrayList<>();
        for (FGStore.Item item : all) {
            if (!matchesQuery(item) || !matchesCategory(item)) {
                continue;
            }
            if (hasUpdate(item)) {
                updates.add(item);
            } else {
                rest.add(item);
            }
        }
        sortItems(updates);
        sortItems(rest);

        addSection(items, getString(R.string.FG_Store_Updates) + " · " + updates.size(), updates);
        addSection(items, null, rest);

        if (shown.isEmpty()) {
            items.add(UItem.asCenterShadow(getString(R.string.NoResult)));
        } else if (!FGStore.lastSource().isEmpty()) {
            items.add(UItem.asShadow(formatString(R.string.FG_Store_Source, hostOf(FGStore.lastSource()))));
        }
    }

    private void fillEmpty(ArrayList<UItem> items) {
        if (loading) {
            for (int i = 0; i < 6; i++) {
                items.add(FGStoreSkeleton.Factory.of(SKELETON_ROW_BASE + i));
            }
            return;
        }
        items.add(UItem.asTopView(getString(R.string.FG_Store_Desc), R.raw.code_laptop));
        items.add(UItem.asCenterShadow(FGStore.lastError().isEmpty()
                ? getString(R.string.FG_Store_Empty)
                : getString(R.string.FG_Store_Failed)));
    }

    private ArrayList<String> titlesOf(List<FGStore.Category> list) {
        final ArrayList<String> titles = new ArrayList<>(list.size());
        for (FGStore.Category value : list) {
            titles.add(value == null ? getString(R.string.FG_Store_Cat_All) : FGStoreTitles.of(value));
        }
        return titles;
    }

    private boolean matchesCategory(FGStore.Item item) {
        return category == null || item.getCategory() == category;
    }

    private static String hostOf(String url) {
        try {
            return new java.net.URL(url).getHost();
        } catch (Exception e) {
            return url;
        }
    }

    private void addSection(ArrayList<UItem> items, CharSequence title, List<FGStore.Item> list) {
        if (list.isEmpty()) {
            return;
        }
        if (title != null) {
            items.add(UItem.asHeader(title));
        }
        for (FGStore.Item item : list) {

            final int row = ROW_BASE + (item.getId().hashCode() & 0x3FFFFF);
            shown.add(item);
            items.add(FGStoreCell.Factory.of(row, new FGStoreCell.Factory.Data(
                    item, stateOf(item), () -> startInstall(item))));
        }
        items.add(UItem.asShadow(null));
    }

    private boolean matchesQuery(FGStore.Item item) {
        if (query.isEmpty()) {
            return true;
        }
        return item.getSearchKey().contains(query);
    }

    private void sortItems(ArrayList<FGStore.Item> list) {
        final Comparator<FGStore.Item> byName = (a, b) -> a.getName().compareToIgnoreCase(b.getName());
        switch (effectiveSort()) {
            case SORT_POPULAR:
                list.sort(((Comparator<FGStore.Item>) (a, b) ->
                        Integer.compare(b.getDownloads(), a.getDownloads())).thenComparing(byName));
                break;
            case SORT_NEW:
                list.sort(newestFirst().thenComparing(byName));
                break;
            case SORT_AUTHOR:
                list.sort(byAuthor().thenComparing(byName));
                break;
            case SORT_INSTALLED:
                list.sort(((Comparator<FGStore.Item>) (a, b) ->
                        Boolean.compare(isInstalled(b), isInstalled(a))).thenComparing(byName));
                break;
            case SORT_NAME:
            default:
                list.sort(byName);
                break;
        }
    }

    private Comparator<FGStore.Item> newestFirst() {
        if (hasDates()) {
            return (a, b) -> Long.compare(b.getUpdatedAt(), a.getUpdatedAt());
        }
        return (a, b) -> FGStoreInstaller.compareVersions(b.getMinAppVersion(), a.getMinAppVersion());
    }

    private Comparator<FGStore.Item> byAuthor() {
        return (a, b) -> {
            final boolean left = a.getAuthor().trim().isEmpty();
            final boolean right = b.getAuthor().trim().isEmpty();
            if (left != right) {
                return left ? 1 : -1;
            }
            return a.getAuthor().compareToIgnoreCase(b.getAuthor());
        };
    }

    private int effectiveSort() {
        if (sort == SORT_NEW && !hasDates() && !hasVersions()) {
            return SORT_NAME;
        }
        if (sort == SORT_POPULAR && !hasDownloads()) {
            return SORT_NAME;
        }
        if (sort == SORT_INSTALLED && installed.isEmpty()) {
            return SORT_NAME;
        }
        return sort;
    }

    private boolean hasDates() {
        for (FGStore.Item item : FGStore.cached()) {
            if (item.getUpdatedAt() > 0) {
                return true;
            }
        }
        return false;
    }

    private boolean hasVersions() {
        for (FGStore.Item item : FGStore.cached()) {
            if (!item.getMinAppVersion().isEmpty()) {
                return true;
            }
        }
        return false;
    }

    private boolean hasDownloads() {
        for (FGStore.Item item : FGStore.cached()) {
            if (item.getDownloads() > 0) {
                return true;
            }
        }
        return false;
    }

    private FGStoreCell.State stateOf(FGStore.Item item) {
        if (FGStoreInstaller.isRunning(item.getId())) {
            return FGStoreCell.State.RUNNING;
        }
        if (hasUpdate(item)) {
            return FGStoreCell.State.UPDATE;
        }
        if (isInstalled(item)) {
            return FGStoreCell.State.INSTALLED;
        }
        return FGStoreCell.State.INSTALL;
    }

    private boolean isInstalled(FGStore.Item item) {
        final String current = installed.get(item.getId());
        return current != null && !current.isEmpty();
    }

    private boolean hasUpdate(FGStore.Item item) {
        final String current = installed.get(item.getId());
        return current != null && !current.isEmpty() && !item.getVersion().isEmpty()
                && FGStoreInstaller.isNewer(item.getVersion(), current);
    }

    private void startInstall(FGStore.Item item) {
        FGStoreInstaller.prepare(item, file -> {
            updateRows(false);
            if (file == null) {
                reportFailed(item);
                return;
            }
            if (item.getKind() == FGStore.Kind.PLUGIN && getParentActivity() != null) {
                showInstallSheet(item, file);
            } else {
                finishInstall(item, file);
            }
        });

        updateRows(false);
    }

    private void showInstallSheet(FGStore.Item item, File file) {
        final InstallPluginBottomSheet.InstallParams params =
                new InstallPluginBottomSheet.InstallParams(file.getAbsolutePath());
        params.id = item.getId();
        params.name = item.getName();
        params.version = item.getVersion();
        params.author = item.getAuthor();
        params.description = item.getDescription();
        params.icon = item.getIcon();
        showDialog(new InstallPluginBottomSheet(getParentActivity(), getResourceProvider(),
                params, ignored -> finishInstall(item, file)));
    }

    private void finishInstall(FGStore.Item item, File file) {

        final org.telegram.ui.ActionBar.AlertDialog progress =
                new org.telegram.ui.ActionBar.AlertDialog(getContext(),
                        org.telegram.ui.ActionBar.AlertDialog.ALERT_TYPE_SPINNER);
        final boolean[] shown = new boolean[1];
        com.th3nekit.finegram.plugins.FGLibraryProgress.INSTANCE.listen((name, done, total) -> {
            if (!shown[0]) {
                shown[0] = true;
                progress.show();
            }
            progress.setMessage(total > 0
                    ? formatString(R.string.FG_Plugins_Library_Progress, name,
                            Math.min(100, (int) (done * 100 / total)))
                    : formatString(R.string.FG_Plugins_Library, name));
        });

        FGStoreInstaller.onBypassClash = name -> {
            if (getContext() != null) {
                BulletinFactory.of(this).createErrorBulletin(
                        formatString(R.string.FG_Store_BypassClash, name)).show();
            }
        };
        FGStoreInstaller.installReady(item, file, ok -> {
            FGStoreInstaller.onBypassClash = null;
            com.th3nekit.finegram.plugins.FGLibraryProgress.INSTANCE.listen(null);
            if (shown[0]) {
                try { progress.dismiss(); } catch (Throwable ignored) {}
            }
            updateRows(false);
            if (getContext() == null) {
                return;
            }
            if (ok) {
                BulletinFactory.of(this).createSimpleBulletin(R.raw.contact_check,
                        formatString(R.string.FG_Store_Done, item.getName())).show();
            } else {
                reportFailed(item);
            }
        });
        updateRows(false);
    }

    private void reportFailed(FGStore.Item item) {
        if (getContext() == null) {
            return;
        }
        BulletinFactory.of(this).createErrorBulletin(
                formatString(R.string.FG_Store_Fail, item.getName())).show();
    }

    private void showSortSelector() {
        final ArrayList<Integer> options = new ArrayList<>();
        final ArrayList<String> titles = new ArrayList<>();
        options.add(SORT_NAME);
        titles.add(getString(R.string.FG_Store_SortName));
        if (hasDates() || hasVersions()) {
            options.add(SORT_NEW);
            titles.add(getString(R.string.FG_Store_SortNew));
        }
        options.add(SORT_AUTHOR);
        titles.add(getString(R.string.FG_Store_SortAuthor));
        if (!installed.isEmpty()) {
            options.add(SORT_INSTALLED);
            titles.add(getString(R.string.FG_Store_SortInstalled));
        }
        if (hasDownloads()) {
            options.add(SORT_POPULAR);
            titles.add(getString(R.string.FG_Store_SortPopular));
        }
        final int selected = Math.max(0, options.indexOf(effectiveSort()));
        PopupHelper.show(titles, getString(R.string.FG_Store_Sort), selected, getContext(), value -> {
            sort = options.get(value);
            FinegramCoreConfig.INSTANCE.setStoreSort(sort);
            updateRows(true);

            toTop();
        });
    }

    private void showAutoUpdateSelector() {
        ArrayList<String> titles = new ArrayList<>();
        titles.add(getString(R.string.FG_Store_AutoUpdate_Manual));
        titles.add(getString(R.string.FG_Store_AutoUpdate_Ask));
        titles.add(getString(R.string.FG_Store_AutoUpdate_Silent));
        PopupHelper.show(titles, getString(R.string.FG_Store_AutoUpdate),
                FinegramCoreConfig.INSTANCE.getPluginAutoUpdate(), getContext(),
                value -> FinegramCoreConfig.INSTANCE.setPluginAutoUpdate(value));
    }

    private List<FGStore.Item> featuredOf(List<FGStore.Item> all) {
        final ArrayList<FGStore.Item> fit = new ArrayList<>();
        for (FGStore.Item item : all) {
            if (!item.getIcon().isEmpty() && !item.getDescription().trim().isEmpty()
                    && !isInstalled(item)) {
                fit.add(item);
            }
        }
        if (fit.size() <= FEATURED_COUNT) {
            return fit;
        }
        final int day = (int) (System.currentTimeMillis() / (24L * 60 * 60 * 1000));
        fit.sort((a, b) -> Integer.compare(seed(a.getId(), day), seed(b.getId(), day)));
        return new ArrayList<>(fit.subList(0, FEATURED_COUNT));
    }

    private static int seed(String id, int day) {
        int value = id.hashCode() ^ (day * 0x9E3779B9);
        value ^= value >>> 15;
        return value;
    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {
        if (item.object instanceof FGStoreCell.Factory.Data) {
            showDetails(((FGStoreCell.Factory.Data) item.object).item);
        }
    }

    private void toTop() {
        if (listView == null || layoutManager == null) {
            return;
        }

        listView.scrollToPosition(0);
        showUpButton(false);
        updateActionBarVisible();
    }

    private void onFeaturedPicked(FGStore.Item item) {
        showDetails(item);
    }

    private void showDetails(FGStore.Item item) {
        if (getContext() == null) {
            return;
        }
        showDialog(new FGStoreSheet(getContext(), getResourceProvider(), item, this::startInstall));
    }
}

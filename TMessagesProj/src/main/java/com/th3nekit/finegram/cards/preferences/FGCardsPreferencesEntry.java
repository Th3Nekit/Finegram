/* Licensed under GNU GPL v. 2 or later. */

package com.th3nekit.finegram.cards.preferences;

import android.util.SparseArray;
import android.view.View;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import org.telegram.messenger.LocaleController;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.R;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import com.th3nekit.finegram.preferences.BaseCGPreferencesEntry;
import com.th3nekit.finegram.preferences.helpers.SettingsHelper;
import com.th3nekit.finegram.helpers.ui.PopupHelper;
import com.th3nekit.finegram.cards.FGCardsRegistry;
import com.th3nekit.finegram.cards.FGCardType;
import com.th3nekit.finegram.cards.FGCardsConfig;

public class FGCardsPreferencesEntry extends BaseCGPreferencesEntry implements FGCardRow.Listener {

    private static final int ID_ENABLED            = 100;
    private static final int ID_INFINITE_SCROLL    = 101;
    private static final int ID_AUTO_SCROLL        = 102;
    private static final int ID_BRAND_COLORS       = 103;

    private static final int ID_CARD_BASE          = 1000;

    private int activeReorderSectionId = -1;

    private static final String[] CURRENCIES = {
            "AUTO", "USD", "EUR", "RUB", "GBP", "UAH", "KZT", "PLN", "TRY", "CNY", "JPY", "BYN", "INR"
    };

    private final SparseArray<FGCardRow> rows = new SparseArray<>();

    @Override
    public String getTitle() {
        return LocaleController.getString(R.string.FG_Cards_Title);
    }

    @Override
    public View createView(android.content.Context context) {
        View view = super.createView(context);
        listView.listenReorder(this::onReordered);
        listView.allowReorder(true);
        return view;
    }

    @Override
    public void onFragmentDestroy() {
        rows.clear();
        super.onFragmentDestroy();
    }

    private FGCardRow getRow(FGCardsRegistry.CardInfo info, boolean active) {
        FGCardRow row = rows.get(info.id);
        if (row == null) {
            row = new FGCardRow(getParentActivity(), getResourceProvider());
            row.setListener(this);
            rows.put(info.id, row);
        }
        row.bind(info, active, active && hasOptions(info.id));
        return row;
    }

    private static boolean hasOptions(int id) {
        return id == FGCardType.TON.id || id == FGCardType.BTC.id || id == FGCardType.USD.id;
    }

    @Override
    public void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {
        final boolean enabled = FGCardsConfig.isEnabled();

        items.add(UItem.asHeader(LocaleController.getString(R.string.FG_Cards_GeneralHeader)));
        items.add(UItem.asCheck(ID_ENABLED, LocaleController.getString(R.string.FG_Cards_Enable))
                .setChecked(enabled));

        if (enabled) {
            items.add(UItem.asCheck(ID_INFINITE_SCROLL,
                            LocaleController.getString(R.string.FG_Cards_InfiniteScroll))
                    .setChecked(FGCardsConfig.isInfiniteScrolling()));

            items.add(UItem.asCheck(ID_AUTO_SCROLL,
                            LocaleController.getString(R.string.FG_Cards_AutoScroll))
                    .setChecked(FGCardsConfig.isAutoScroll()));

            final List<Integer> active = FGCardsConfig.getActiveCards();
            final List<Integer> hidden = FGCardsConfig.getHiddenCards();

            items.add(UItem.asCheck(ID_BRAND_COLORS,
                            LocaleController.getString(R.string.FG_Cards_Colors))
                    .setChecked(FGCardsConfig.getColorMode() != FGCardsConfig.COLOR_MODE_THEME));
            items.add(UItem.asShadow(LocaleController.getString(R.string.FG_Cards_ColorsHint)));
            items.add(UItem.asHeader(LocaleController.getString(R.string.FG_Cards_ActiveHeader)));

            activeReorderSectionId = adapter.reorderSectionStart();
            for (int pillId : active) {
                FGCardsRegistry.CardInfo info = FGCardsRegistry.get(pillId);
                if (info == null) continue;
                items.add(UItem.asCustom(ID_CARD_BASE + pillId, getRow(info, true)));
            }
            adapter.reorderSectionEnd();

            if (!hidden.isEmpty()) {
                items.add(UItem.asShadow(null));
                items.add(UItem.asHeader(LocaleController.getString(R.string.FG_Cards_HiddenHeader)));
                for (int pillId : hidden) {
                    FGCardsRegistry.CardInfo info = FGCardsRegistry.get(pillId);
                    if (info == null) continue;
                    items.add(UItem.asCustom(ID_CARD_BASE + pillId, getRow(info, false)));
                }
            }
        }

        items.add(UItem.asShadow(LocaleController.getString(R.string.FG_Cards_Footer)));
    }

    @Override
    public void onClick(UItem item, View view, int position, float x, float y) {
        if (item == null) return;
        final int id = item.id;

        if (id == ID_ENABLED) {
            FGCardsConfig.setEnabled(!FGCardsConfig.isEnabled());
            SettingsHelper.updateCheckState(view, FGCardsConfig.isEnabled());
            reload();
        } else if (id == ID_INFINITE_SCROLL) {
            boolean v = !FGCardsConfig.isInfiniteScrolling();
            FGCardsConfig.setInfiniteScrolling(v);
            SettingsHelper.updateCheckState(view, v);
        } else if (id == ID_BRAND_COLORS) {
            boolean brand = FGCardsConfig.getColorMode() == FGCardsConfig.COLOR_MODE_THEME;
            FGCardsConfig.setColorMode(brand
                    ? FGCardsConfig.COLOR_MODE_CUSTOM : FGCardsConfig.COLOR_MODE_THEME);
            SettingsHelper.updateCheckState(view, brand);
        } else if (id == ID_AUTO_SCROLL) {
            boolean v = !FGCardsConfig.isAutoScroll();
            FGCardsConfig.setAutoScroll(v);
            SettingsHelper.updateCheckState(view, v);
            NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.infoCardsSettingsChanged);
        }
    }

    @Override
    public void onToggle(int pillId, boolean active) {
        if (active) {
            moveToActive(pillId);
        } else {
            moveToHidden(pillId);
        }
        reload();
    }

    @Override
    public void onBodyTap(int pillId) {
        if (!FGCardsConfig.isCardActive(pillId) || !hasOptions(pillId)) return;
        showCurrencyPicker(pillId);
    }

    private void showCurrencyPicker(int pillId) {
        ArrayList<String> labels = new ArrayList<>(Arrays.asList(CURRENCIES));
        labels.set(0, LocaleController.getString(R.string.Default));
        int selected = Math.max(0, Arrays.asList(CURRENCIES).indexOf(FGCardsConfig.getTargetCurrency(pillId)));
        PopupHelper.show(labels, LocaleController.getString(R.string.FG_Cards_Title), selected, getParentActivity(), i -> {
            FGCardsConfig.setTargetCurrency(pillId, CURRENCIES[i]);
            reload();
        });
    }

    private void moveToHidden(int pillId) {
        List<Integer> active = FGCardsConfig.getActiveCards();
        List<Integer> hidden = FGCardsConfig.getHiddenCards();
        if (active.size() <= 1) return;
        if (!active.remove((Integer) pillId)) return;
        if (!hidden.contains(pillId)) hidden.add(pillId);
        FGCardsConfig.setLayout(active, hidden);
    }

    private void moveToActive(int pillId) {
        List<Integer> active = FGCardsConfig.getActiveCards();
        List<Integer> hidden = FGCardsConfig.getHiddenCards();
        if (!hidden.remove((Integer) pillId)) return;
        if (!active.contains(pillId)) active.add(pillId);
        FGCardsConfig.setLayout(active, hidden);
    }

    private void onReordered(int sectionId, ArrayList<UItem> reordered) {
        if (sectionId != activeReorderSectionId) return;
        ArrayList<Integer> newActive = new ArrayList<>();
        for (UItem it : reordered) {
            if (it.id >= ID_CARD_BASE && it.id < ID_CARD_BASE + 1000) {
                newActive.add(it.id - ID_CARD_BASE);
            }
        }
        if (newActive.isEmpty()) return;
        FGCardsConfig.setLayout(newActive, FGCardsConfig.getHiddenCards());
    }

    private void reload() {
        if (listView != null && listView.adapter != null) {
            listView.adapter.update(true);
        }
    }

}

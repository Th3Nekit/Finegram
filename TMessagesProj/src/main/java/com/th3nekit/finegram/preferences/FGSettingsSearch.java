/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences;

import static org.telegram.messenger.LocaleController.getString;

import org.telegram.messenger.R;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;

import java.util.ArrayList;
import java.util.Locale;

import com.th3nekit.finegram.cards.preferences.FGCardsPreferencesEntry;
import com.th3nekit.finegram.preferences.folders.FoldersPreferencesEntry;
import com.th3nekit.finegram.preferences.tabs.MainTabsPreferencesEntry;

public final class FGSettingsSearch {

    private FGSettingsSearch() {
    }

    public interface ScreenFactory {
        BaseCGPreferencesEntry create();
    }

    public static final class Result {
        public final CharSequence screenTitle;
        public final CharSequence itemTitle;
        public final CharSequence itemSubtitle;
        public final int itemId;
        public final ScreenFactory screenFactory;

        Result(CharSequence screenTitle, CharSequence itemTitle, CharSequence itemSubtitle,
               int itemId, ScreenFactory screenFactory) {
            this.screenTitle = screenTitle;
            this.itemTitle = itemTitle;
            this.itemSubtitle = itemSubtitle;
            this.itemId = itemId;
            this.screenFactory = screenFactory;
        }
    }

    private static final class Screen {
        final ScreenFactory factory;

        Screen(ScreenFactory factory) {
            this.factory = factory;
        }
    }

    private static final Screen[] SCREENS = new Screen[]{
            new Screen(GeneralPreferencesEntry::new),
            new Screen(AppearancePreferencesEntry::new),
            new Screen(ChatsPreferencesEntry::new),
            new Screen(MessagesPreferencesEntry::new),
            new Screen(MessageMenuPreferencesEntry::new),
            new Screen(MessageFiltersPreferencesEntry::new),
            new Screen(CameraPreferencesEntry::new),
            new Screen(PrivacyPreferencesEntry::new),
            new Screen(GeminiPreferencesEntry::new),
            new Screen(FoldersPreferencesEntry::new),
            new Screen(MainTabsPreferencesEntry::new),
            new Screen(NetworkPreferencesEntry::new),
            new Screen(TextAnimationPreferencesEntry::new),
            new Screen(FGCardsPreferencesEntry::new),
            new Screen(PluginsPreferencesEntry::new),
            new Screen(AboutPreferencesEntry::new),
    };

    private static ArrayList<Result> index;

    public static void invalidate() {
        index = null;
    }

    private static ArrayList<Result> getIndex() {
        if (index != null) {
            return index;
        }
        ArrayList<Result> collected = new ArrayList<>();
        for (Screen screen : SCREENS) {
            try {
                BaseCGPreferencesEntry fragment = screen.factory.create();
                CharSequence screenTitle = fragment.getScreenTitleForSearch();
                for (UItem item : fragment.collectItemsForSearch()) {
                    if (item == null || item.id == 0) {
                        continue;
                    }
                    if (item.viewType == UniversalAdapter.VIEW_TYPE_HEADER || item.viewType == UniversalAdapter.VIEW_TYPE_SHADOW) {
                        continue;
                    }
                    CharSequence title = item.text;
                    if (title == null || title.length() == 0) {
                        continue;
                    }
                    collected.add(new Result(screenTitle, title, item.subtext, item.id, screen.factory));
                }
            } catch (Throwable ignored) {

            }
        }
        return index = collected;
    }

    public static ArrayList<Result> search(String query) {
        ArrayList<Result> results = new ArrayList<>();
        if (query == null) {
            return results;
        }
        String normalized = query.trim().toLowerCase(Locale.getDefault());
        if (normalized.isEmpty()) {
            return results;
        }
        String[] words = normalized.split("\\s+");
        for (Result result : getIndex()) {
            String haystack = (result.itemTitle + " " + (result.itemSubtitle != null ? result.itemSubtitle : "")
                    + " " + result.screenTitle).toLowerCase(Locale.getDefault());
            boolean matches = true;
            for (String word : words) {
                if (!haystack.contains(word)) {
                    matches = false;
                    break;
                }
            }
            if (matches) {
                results.add(result);
            }
        }
        return results;
    }

    public static CharSequence emptyText() {
        return getString(R.string.NoResult);
    }
}

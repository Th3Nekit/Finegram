/* Licensed under GNU GPL v. 2 or later. */

package com.th3nekit.finegram.cards;

import android.content.Context;
import androidx.annotation.StringRes;
import org.telegram.messenger.LocaleController;

import org.telegram.ui.ActionBar.Theme;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.LinkedHashMap;

public final class FGCardsRegistry {

    public interface CardCreator {
        FGCard create(Context context, Theme.ResourcesProvider resourcesProvider);
    }

    public static final class CardInfo {
        public final int id;
        public final CharSequence name;
        @StringRes public final int nameRes;
        public final int iconRes;
        public final int colorTop;
        public final int colorBottom;
        public final CardCreator creator;

        public CardInfo(int id, CharSequence name, int iconRes, int colorTop, int colorBottom, CardCreator creator) {
            this.id = id;
            this.name = name;
            this.nameRes = 0;
            this.iconRes = iconRes;
            this.colorTop = colorTop;
            this.colorBottom = colorBottom;
            this.creator = creator;
        }

        public CardInfo(int id, @StringRes int nameRes, int iconRes, int colorTop, int colorBottom, CardCreator creator) {
            this.id = id;
            this.name = null;
            this.nameRes = nameRes;
            this.iconRes = iconRes;
            this.colorTop = colorTop;
            this.colorBottom = colorBottom;
            this.creator = creator;
        }

        public CharSequence getName() {
            return nameRes != 0 ? LocaleController.getString(nameRes) : name;
        }
    }

    private static final LinkedHashMap<Integer, CardInfo> registry = new LinkedHashMap<>();
    private static boolean defaultsRegistered;

    public static synchronized void register(CardInfo pill) {
        if (pill != null) registry.put(pill.id, pill);
    }

    public static synchronized CardInfo get(int id) {
        ensureRegistered();
        return registry.get(id);
    }

    public static synchronized boolean isRegistered(int id) {
        ensureRegistered();
        return registry.containsKey(id);
    }

    public static synchronized Collection<CardInfo> all() {
        ensureRegistered();
        return Collections.unmodifiableCollection(new ArrayList<>(registry.values()));
    }

    public static FGCard create(int id, Context context, Theme.ResourcesProvider rp) {
        ensureRegistered();
        CardInfo info;
        synchronized (FGCardsRegistry.class) {
            info = registry.get(id);
        }
        if (info == null || info.creator == null) return null;
        FGCard pill = info.creator.create(context, rp);
        if (pill != null) {
            pill.setCardColors(info.colorTop, info.colorBottom);
            pill.setCardAccessibilityLabel(info.getName());
        }
        return pill;
    }

    public static synchronized void ensureRegistered() {
        if (defaultsRegistered) return;
        defaultsRegistered = true;

        register(new CardInfo(FGCardType.TON.id, org.telegram.messenger.R.string.FG_Cards_NameGram,
                org.telegram.messenger.R.drawable.menu_gram_24, -15702638, -15969141,
                (ctx, rp) -> new FGRateCard(ctx, rp, FGCardType.TON.id, "ton",
                        org.telegram.messenger.R.drawable.menu_gram_24)));

        register(new CardInfo(FGCardType.BTC.id, org.telegram.messenger.R.string.FG_Cards_NameBitcoin,
                org.telegram.messenger.R.drawable.pill_btc, -7051765, -7387125,
                (ctx, rp) -> new FGRateCard(ctx, rp, FGCardType.BTC.id, "btc",
                        org.telegram.messenger.R.drawable.pill_btc)));

        register(new CardInfo(FGCardType.USD.id, org.telegram.messenger.R.string.FG_Cards_NameUsd,
                org.telegram.messenger.R.drawable.pill_usd, -15641031, -15840201,
                (ctx, rp) -> new FGRateCard(ctx, rp, FGCardType.USD.id, null,
                        org.telegram.messenger.R.drawable.pill_usd)));

        register(new CardInfo(FGCardType.WEATHER.id,
                org.telegram.messenger.R.string.FG_Cards_NameWeather,
                org.telegram.messenger.R.drawable.pill_weather, -13141098, -14457201,
                (ctx, rp) -> new FGWeatherCard(ctx, rp,
                        org.telegram.messenger.R.drawable.pill_weather)));

        register(new CardInfo(FGCardType.CACHE.id,
                org.telegram.messenger.R.string.FG_Cards_NameStorage,
                org.telegram.messenger.R.drawable.pill_cache, -13610344, -14663537,
                (ctx, rp) -> new FGStorageCard(ctx, rp,
                        org.telegram.messenger.R.drawable.pill_cache)));

        register(new CardInfo(FGCardType.PROXY.id,
                org.telegram.messenger.R.string.FG_Cards_NameProxy,
                org.telegram.messenger.R.drawable.pill_proxy, -13337300, -15175904,
                (ctx, rp) -> new FGProxyCard(ctx, rp,
                        org.telegram.messenger.R.drawable.pill_proxy)));
    }

    private FGCardsRegistry() {}
}

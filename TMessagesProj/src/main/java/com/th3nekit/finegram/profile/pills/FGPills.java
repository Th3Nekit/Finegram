/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.profile.pills;

import android.content.Context;
import android.content.SharedPreferences;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.NotificationCenter;
import org.telegram.ui.ActionBar.Theme;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Карточки в профиле: что объявлено и что из этого показывается.
 *
 * Объявить карточку может кто угодно — и сам клиент, и плагин: он говорит, как её зовут,
 * каким значком обозначить и чем нарисовать. Показываются только те, что человек оставил;
 * их набор переживает перезапуск.
 */
public final class FGPills {

    /** Как создать карточку. Пишет тот, кто её объявляет. */
    public interface Creator {
        FGPill create(Context context, Theme.ResourcesProvider resourcesProvider);
    }

    /** Описание карточки: что показать и чем нарисовать. */
    public static final class Pill {
        public final int id;
        public final CharSequence title;
        public final int icon;

        /** Место в ряду: чем меньше, тем левее. */
        public final int order;

        /** Ширина: 1 — обычная, 2 — вдвое шире. */
        public final int span;

        public final Creator creator;

        public Pill(int id, CharSequence title, int icon, int order, int span, Creator creator) {
            this.id = id;
            this.title = title;
            this.icon = icon;
            this.order = order;
            this.span = span < 1 ? 1 : span;
            this.creator = creator;
        }
    }

    private static final String PREFS = "finegram_profile_pills";
    private static final String KEY_LAYOUT = "layout";

    private static final Map<Integer, Pill> declared = new LinkedHashMap<>();
    private static final List<Integer> shown = new ArrayList<>();

    /** Пока идёт правка пачкой, профиль не дёргаем на каждое изменение. */
    private static int batch;
    private static boolean pending;

    private static boolean restored;

    // --- объявление ---

    public static void declare(Pill pill) {
        if (pill == null) {
            return;
        }
        synchronized (declared) {
            declared.put(pill.id, pill);
        }
        restoreOnce();
        changed();
    }

    public static void forget(int id) {
        synchronized (declared) {
            declared.remove(id);
            shown.remove(Integer.valueOf(id));
        }
        changed();
    }

    // --- что показывается ---

    public static void show(int id) {
        synchronized (declared) {
            if (!declared.containsKey(id) || shown.contains(id)) {
                return;
            }
            shown.add(id);
        }
        changed();
    }

    public static void hide(int id) {
        synchronized (declared) {
            shown.remove(Integer.valueOf(id));
        }
        changed();
    }

    public static boolean isShown(int id) {
        synchronized (declared) {
            return shown.contains(id);
        }
    }

    /** Всё объявленное, по порядку. */
    public static List<Pill> all() {
        synchronized (declared) {
            List<Pill> out = new ArrayList<>(declared.values());
            Collections.sort(out, (a, b) -> Integer.compare(a.order, b.order));
            return out;
        }
    }

    /**
     * Карточки, которые сейчас в профиле, в том порядке, в каком их видит человек.
     *
     * Порядок берём из списка показанных, а не из числа, которое прислал плагин: иначе
     * перестановка руками жила бы до первой перерисовки, после чего карточки прыгали бы
     * обратно. Число плагина решает только там, где человек ещё ничего не трогал —
     * при первом появлении карточки.
     */
    public static List<Pill> visible() {
        synchronized (declared) {
            List<Pill> out = new ArrayList<>(shown.size());
            for (Integer id : shown) {
                Pill pill = declared.get(id);
                if (pill != null) {
                    out.add(pill);
                }
            }
            return out;
        }
    }

    /** Опознаватели показанных карточек, по порядку. */
    public static List<Integer> shownIds() {
        synchronized (declared) {
            return new ArrayList<>(shown);
        }
    }

    /** Опознаватели объявленных, но убранных карточек. */
    public static List<Integer> hiddenIds() {
        List<Integer> out = new ArrayList<>();
        synchronized (declared) {
            for (Integer id : declared.keySet()) {
                if (!shown.contains(id)) {
                    out.add(id);
                }
            }
        }
        return out;
    }

    /** Двигает карточку на одну позицию; направление — -1 вверх, +1 вниз. */
    public static void move(int id, int direction) {
        boolean changed = false;
        synchronized (declared) {
            int from = shown.indexOf(id);
            int to = from + direction;
            if (from >= 0 && to >= 0 && to < shown.size()) {
                Collections.swap(shown, from, to);
                changed = true;
            }
        }
        if (changed) {
            saveLayout();
            notifyProfiles();
        }
    }

    /** Показать или спрятать карточку по выбору человека. */
    public static void setShown(int id, boolean visible) {
        if (visible) {
            show(id);
        } else {
            hide(id);
        }
        saveLayout();
    }

    public static boolean isEmpty() {
        synchronized (declared) {
            return shown.isEmpty();
        }
    }

    // --- правка пачкой ---

    public static void beginBatch() {
        synchronized (declared) {
            batch++;
        }
    }

    public static void endBatch() {
        boolean notify;
        synchronized (declared) {
            if (batch > 0) {
                batch--;
            }
            notify = batch == 0 && pending;
            if (notify) {
                pending = false;
            }
        }
        if (notify) {
            notifyProfiles();
        }
    }

    // --- память между запусками ---

    private static SharedPreferences prefs() {
        return ApplicationLoader.applicationContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    /** Запоминает нынешний ряд. */
    public static void saveLayout() {
        StringBuilder out = new StringBuilder();
        synchronized (declared) {
            for (Integer id : shown) {
                if (out.length() > 0) {
                    out.append(',');
                }
                out.append(id);
            }
        }
        prefs().edit().putString(KEY_LAYOUT, out.toString()).apply();
    }

    public static List<Integer> savedLayout() {
        List<Integer> out = new ArrayList<>();
        String raw = prefs().getString(KEY_LAYOUT, "");
        if (raw == null || raw.isEmpty()) {
            return out;
        }
        for (String piece : raw.split(",")) {
            try {
                out.add(Integer.parseInt(piece.trim()));
            } catch (Throwable ignored) {
            }
        }
        return out;
    }

    /**
     * Возвращает ряд, каким он был в прошлый раз.
     *
     * Делается один раз — при первом объявлении карточки: раньше возвращать нечего, список
     * ещё пуст, а позже человек мог уже что-то поменять руками.
     */
    private static void restoreOnce() {
        synchronized (declared) {
            if (restored) {
                return;
            }
            restored = true;
        }
        for (Integer id : savedLayout()) {
            synchronized (declared) {
                if (declared.containsKey(id) && !shown.contains(id)) {
                    shown.add(id);
                }
            }
        }
    }

    private static void changed() {
        synchronized (declared) {
            if (batch > 0) {
                pending = true;
                return;
            }
        }
        notifyProfiles();
    }

    /** Открытые профили перерисовывают ряд. */
    private static void notifyProfiles() {
        AndroidUtilities.runOnUIThread(() -> {
            try {
                NotificationCenter.getGlobalInstance()
                        .postNotificationName(NotificationCenter.mainUserInfoChanged);
            } catch (Throwable ignored) {
                // профиль может быть ещё не открыт — он возьмёт список сам
            }
        });
    }

    private FGPills() {
    }
}

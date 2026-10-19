/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.icons.pack;

import android.content.res.Resources;

import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig;
import com.th3nekit.finegram.core.icons.icon_replaces.SolarIconReplace;

import org.telegram.messenger.ApplicationLoader;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Места, куда набор иконок может встать.
 *
 * Считать покрытие по числу картинок в наборе бессмысленно: автор может положить хоть
 * тысячу, и если их имена клиенту незнакомы, ни одна не встанет. Знаменатель здесь —
 * места, для которых замена вообще предусмотрена; их список берём из встроенного набора,
 * где каждая пара «что меняем → на что» уже перечислена.
 *
 * Список собирается один раз: имена ресурсов достаются по номерам, а это поход в таблицу
 * ресурсов на каждое имя.
 */
public final class IconSlots {

    private IconSlots() {
    }

    /** имя ресурса -> его номер; порядок сохраняем, чтобы сетка не прыгала между открытиями */
    private static volatile LinkedHashMap<String, Integer> slots;

    public static Map<String, Integer> all() {
        LinkedHashMap<String, Integer> known = slots;
        if (known != null) {
            return known;
        }
        synchronized (IconSlots.class) {
            if (slots != null) {
                return slots;
            }
            LinkedHashMap<String, Integer> collected = new LinkedHashMap<>();
            try {
                Resources resources = ApplicationLoader.applicationContext.getResources();
                List<Integer> ids = new ArrayList<>(new SolarIconReplace().getReplaces().keySet());
                // Номера ресурсов идут в том порядке, в каком их выдал набор, — он от сборки
                // к сборке не гарантирован. Упорядочиваем по имени: так сетка выглядит
                // одинаково всегда и рядом оказываются родственные иконки.
                List<String> names = new ArrayList<>(ids.size());
                LinkedHashMap<String, Integer> byName = new LinkedHashMap<>();
                for (Integer id : ids) {
                    if (id == null) {
                        continue;
                    }
                    try {
                        String name = resources.getResourceEntryName(id);
                        if (name != null && !byName.containsKey(name)) {
                            byName.put(name, id);
                            names.add(name);
                        }
                    } catch (Throwable ignore) {
                        // ресурса нет в этой сборке — место просто не показываем
                    }
                }
                Collections.sort(names);
                for (String name : names) {
                    collected.put(name, byName.get(name));
                }
            } catch (Throwable ignore) {
                // без списка мест покрытие показать не выйдет; сетка тогда покажет сам набор
            }
            slots = collected;
            return collected;
        }
    }

    public static int count() {
        return all().size();
    }

    /** Сколько мест из известных закрывает набор с такими именами картинок. */
    public static int coverage(Iterable<String> packIconNames) {
        Map<String, Integer> known = all();
        int covered = 0;
        for (String name : packIconNames) {
            if (known.containsKey(name)) {
                covered++;
            }
        }
        return covered;
    }

    /** Номер оригинальной иконки клиента для этого места. */
    public static int originalIcon(String name) {
        Integer id = all().get(name);
        return id == null ? 0 : id;
    }

    /** Заменяются ли иконки прямо сейчас: от этого зависит, что показывать как «сейчас». */
    public static boolean replacementsEnabled() {
        return FinegramAppearanceConfig.INSTANCE.getIconReplacement()
                != FinegramAppearanceConfig.ICON_REPLACE_NONE;
    }
}

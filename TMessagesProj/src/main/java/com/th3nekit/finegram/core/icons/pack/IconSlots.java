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

public final class IconSlots {

    private IconSlots() {
    }

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

                    }
                }
                Collections.sort(names);
                for (String name : names) {
                    collected.put(name, byName.get(name));
                }
            } catch (Throwable ignore) {

            }
            slots = collected;
            return collected;
        }
    }

    public static int count() {
        return all().size();
    }

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

    public static int originalIcon(String name) {
        Integer id = all().get(name);
        return id == null ? 0 : id;
    }

    public static boolean replacementsEnabled() {
        return FinegramAppearanceConfig.INSTANCE.getIconReplacement()
                != FinegramAppearanceConfig.ICON_REPLACE_NONE;
    }
}

/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.pillstack.core;

import android.content.Context;

import java.util.ArrayList;

import org.telegram.ui.ActionBar.Theme;

import com.exteragram.messenger.pillstack.ui.pills.BasePill;
import com.th3nekit.finegram.profile.pills.FGPill;
import com.th3nekit.finegram.profile.pills.FGPills;

public final class PillRegistry {

    public interface PillCreator {
        BasePill create(Context context, Theme.ResourcesProvider resourcesProvider);
    }

    public static final class PillInfo {
        public final int id;
        public final CharSequence title;
        public final int icon;
        public final int order;
        public final int span;
        public final PillCreator creator;

        public PillInfo(int id, CharSequence title, int icon, int order, int span, PillCreator creator) {
            this.id = id;
            this.title = title;
            this.icon = icon;
            this.order = order;
            this.span = span;
            this.creator = creator;
        }
    }

    public static void register(PillInfo info) {
        if (info == null) {
            return;
        }
        final PillCreator creator = info.creator;
        FGPills.declare(new FGPills.Pill(info.id, info.title, info.icon, info.order, info.span,
                creator == null ? null : new FGPills.Creator() {
                    @Override
                    public FGPill create(Context context, Theme.ResourcesProvider resourcesProvider) {
                        return creator.create(context, resourcesProvider);
                    }
                }));
    }

    public static void unregister(int id) {
        FGPills.forget(id);
    }

    public static void activatePill(int id) {
        FGPills.show(id);
    }

    public static void deactivatePill(int id) {
        FGPills.hide(id);
    }

    public static ArrayList<Integer> activePills() {
        ArrayList<Integer> out = new ArrayList<>();
        for (FGPills.Pill pill : FGPills.visible()) {
            out.add(pill.id);
        }
        return out;
    }

    public static ArrayList<Integer> hiddenPills() {
        ArrayList<Integer> out = new ArrayList<>();
        for (FGPills.Pill pill : FGPills.all()) {
            if (!FGPills.isShown(pill.id)) {
                out.add(pill.id);
            }
        }
        return out;
    }

    public static void beginTransaction() {
        FGPills.beginBatch();
    }

    public static void endTransaction() {
        FGPills.endBatch();
    }

    private PillRegistry() {
    }
}

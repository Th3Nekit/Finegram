/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.pillstack.core;

import androidx.annotation.NonNull;

import java.util.AbstractCollection;
import java.util.Iterator;
import java.util.List;

import com.th3nekit.finegram.profile.pills.FGPills;

public final class PillStackConfig {

    public static final LiveIds activePills = new LiveIds(true);

    public static final LiveIds hiddenPills = new LiveIds(false);

    public static void savePillsLayout() {
        FGPills.saveLayout();
    }

    public static final class LiveIds extends AbstractCollection<Integer> {

        private final boolean visible;

        LiveIds(boolean visible) {
            this.visible = visible;
        }

        private List<Integer> ids() {
            return visible ? FGPills.shownIds() : FGPills.hiddenIds();
        }

        @NonNull
        @Override
        public Iterator<Integer> iterator() {
            final Iterator<Integer> source = ids().iterator();
            return new Iterator<Integer>() {
                private Integer last;

                @Override
                public boolean hasNext() {
                    return source.hasNext();
                }

                @Override
                public Integer next() {
                    last = source.next();
                    return last;
                }

                @Override
                public void remove() {
                    if (last != null) {
                        FGPills.setShown(last, !visible);
                    }
                }
            };
        }

        @Override
        public int size() {
            return ids().size();
        }

        @Override
        public boolean add(Integer id) {
            if (id == null) return false;
            boolean had = ids().contains(id);
            FGPills.setShown(id, visible);
            return !had;
        }

        @Override
        public boolean remove(Object value) {
            if (!(value instanceof Integer)) return false;
            Integer id = (Integer) value;
            if (!ids().contains(id)) return false;
            FGPills.setShown(id, !visible);
            return true;
        }

        @Override
        public boolean contains(Object value) {
            return value instanceof Integer && ids().contains(value);
        }
    }

    private PillStackConfig() {
    }
}

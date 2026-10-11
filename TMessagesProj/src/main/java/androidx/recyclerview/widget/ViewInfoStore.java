/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package androidx.recyclerview.widget;

import org.telegram.messenger.FileLog;

import static androidx.recyclerview.widget.ViewInfoStore.InfoRecord.FLAG_APPEAR;
import static androidx.recyclerview.widget.ViewInfoStore.InfoRecord.FLAG_APPEAR_AND_DISAPPEAR;
import static androidx.recyclerview.widget.ViewInfoStore.InfoRecord.FLAG_APPEAR_PRE_AND_POST;
import static androidx.recyclerview.widget.ViewInfoStore.InfoRecord.FLAG_DISAPPEARED;
import static androidx.recyclerview.widget.ViewInfoStore.InfoRecord.FLAG_POST;
import static androidx.recyclerview.widget.ViewInfoStore.InfoRecord.FLAG_PRE;
import static androidx.recyclerview.widget.ViewInfoStore.InfoRecord.FLAG_PRE_AND_POST;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.collection.ArrayMap;
import androidx.collection.LongSparseArray;
import androidx.core.util.Pools;

class ViewInfoStore {

    private static final boolean DEBUG = false;

    @VisibleForTesting
    final ArrayMap<RecyclerView.ViewHolder, InfoRecord> mLayoutHolderMap = new ArrayMap<>();

    @VisibleForTesting
    final LongSparseArray<RecyclerView.ViewHolder> mOldChangedHolders = new LongSparseArray<>();

    void clear() {
        mLayoutHolderMap.clear();
        mOldChangedHolders.clear();
    }

    void addToPreLayout(RecyclerView.ViewHolder holder, RecyclerView.ItemAnimator.ItemHolderInfo info) {
        InfoRecord record = mLayoutHolderMap.get(holder);
        if (record == null) {
            record = InfoRecord.obtain();
            mLayoutHolderMap.put(holder, record);
        }
        record.preInfo = info;
        record.flags |= FLAG_PRE;
    }

    boolean isDisappearing(RecyclerView.ViewHolder holder) {
        final InfoRecord record = mLayoutHolderMap.get(holder);
        return record != null && ((record.flags & FLAG_DISAPPEARED) != 0);
    }

    @Nullable
    RecyclerView.ItemAnimator.ItemHolderInfo popFromPreLayout(RecyclerView.ViewHolder vh) {
        return popFromLayoutStep(vh, FLAG_PRE);
    }

    @Nullable
    RecyclerView.ItemAnimator.ItemHolderInfo popFromPostLayout(RecyclerView.ViewHolder vh) {
        return popFromLayoutStep(vh, FLAG_POST);
    }

    private RecyclerView.ItemAnimator.ItemHolderInfo popFromLayoutStep(RecyclerView.ViewHolder vh, int flag) {
        int index = mLayoutHolderMap.indexOfKey(vh);
        if (index < 0) {
            return null;
        }
        final InfoRecord record = mLayoutHolderMap.valueAt(index);
        if (record != null && (record.flags & flag) != 0) {
            record.flags &= ~flag;
            final RecyclerView.ItemAnimator.ItemHolderInfo info;
            if (flag == FLAG_PRE) {
                info = record.preInfo;
            } else if (flag == FLAG_POST) {
                info = record.postInfo;
            } else {
                throw new IllegalArgumentException("Must provide flag PRE or POST");
            }

            if ((record.flags & (FLAG_PRE | FLAG_POST)) == 0) {
                mLayoutHolderMap.removeAt(index);
                InfoRecord.recycle(record);
            }
            return info;
        }
        return null;
    }

    void addToOldChangeHolders(long key, RecyclerView.ViewHolder holder) {
        mOldChangedHolders.put(key, holder);
    }

    void addToAppearedInPreLayoutHolders(RecyclerView.ViewHolder holder, RecyclerView.ItemAnimator.ItemHolderInfo info) {
        InfoRecord record = mLayoutHolderMap.get(holder);
        if (record == null) {
            record = InfoRecord.obtain();
            mLayoutHolderMap.put(holder, record);
        }
        record.flags |= FLAG_APPEAR;
        record.preInfo = info;
    }

    boolean isInPreLayout(RecyclerView.ViewHolder viewHolder) {
        final InfoRecord record = mLayoutHolderMap.get(viewHolder);
        return record != null && (record.flags & FLAG_PRE) != 0;
    }

    RecyclerView.ViewHolder getFromOldChangeHolders(long key) {
        return mOldChangedHolders.get(key);
    }

    void addToPostLayout(RecyclerView.ViewHolder holder, RecyclerView.ItemAnimator.ItemHolderInfo info) {
        InfoRecord record = mLayoutHolderMap.get(holder);
        if (record == null) {
            record = InfoRecord.obtain();
            mLayoutHolderMap.put(holder, record);
        }
        record.postInfo = info;
        record.flags |= FLAG_POST;
    }

    void addToDisappearedInLayout(RecyclerView.ViewHolder holder) {
        InfoRecord record = mLayoutHolderMap.get(holder);
        if (record == null) {
            record = InfoRecord.obtain();
            mLayoutHolderMap.put(holder, record);
        }
        record.flags |= FLAG_DISAPPEARED;
    }

    void removeFromDisappearedInLayout(RecyclerView.ViewHolder holder) {
        InfoRecord record = mLayoutHolderMap.get(holder);
        if (record == null) {
            return;
        }
        record.flags &= ~FLAG_DISAPPEARED;
    }

    void process(ProcessCallback callback) {
        for (int index = mLayoutHolderMap.size() - 1; index >= 0; index--) {
            final RecyclerView.ViewHolder viewHolder = mLayoutHolderMap.keyAt(index);
            InfoRecord record = null;
            try {
                record = mLayoutHolderMap.removeAt(index);
            } catch (Exception e) {
                FileLog.e(e);
            }
            if (record == null) {
                continue;
            }
            if ((record.flags & FLAG_APPEAR_AND_DISAPPEAR) == FLAG_APPEAR_AND_DISAPPEAR) {

                callback.unused(viewHolder);
            } else if ((record.flags & FLAG_DISAPPEARED) != 0) {

                if (record.preInfo == null) {

                    callback.unused(viewHolder);
                } else {
                    callback.processDisappeared(viewHolder, record.preInfo, record.postInfo);
                }
            } else if ((record.flags & FLAG_APPEAR_PRE_AND_POST) == FLAG_APPEAR_PRE_AND_POST) {

                callback.processAppeared(viewHolder, record.preInfo, record.postInfo);
            } else if ((record.flags & FLAG_PRE_AND_POST) == FLAG_PRE_AND_POST) {

                callback.processPersistent(viewHolder, record.preInfo, record.postInfo);
            } else if ((record.flags & FLAG_PRE) != 0) {

                callback.processDisappeared(viewHolder, record.preInfo, null);
            } else if ((record.flags & FLAG_POST) != 0) {

                callback.processAppeared(viewHolder, record.preInfo, record.postInfo);
            } else if ((record.flags & FLAG_APPEAR) != 0) {

            } else if (DEBUG) {
                throw new IllegalStateException("record without any reasonable flag combination:/");
            }
            InfoRecord.recycle(record);
        }
    }

    void removeViewHolder(RecyclerView.ViewHolder holder) {
        for (int i = mOldChangedHolders.size() - 1; i >= 0; i--) {
            if (holder == mOldChangedHolders.valueAt(i)) {
                mOldChangedHolders.removeAt(i);
                break;
            }
        }
        final InfoRecord info = mLayoutHolderMap.get(holder);
        if (info != null) {
            mLayoutHolderMap.remove(holder);
            InfoRecord.recycle(info);
        }
    }

    void onDetach() {
        InfoRecord.drainCache();
    }

    public void onViewDetached(RecyclerView.ViewHolder viewHolder) {
        removeFromDisappearedInLayout(viewHolder);
    }

    interface ProcessCallback {
        void processDisappeared(RecyclerView.ViewHolder viewHolder, @NonNull RecyclerView.ItemAnimator.ItemHolderInfo preInfo,
                @Nullable RecyclerView.ItemAnimator.ItemHolderInfo postInfo);
        void processAppeared(RecyclerView.ViewHolder viewHolder, @Nullable RecyclerView.ItemAnimator.ItemHolderInfo preInfo,
                RecyclerView.ItemAnimator.ItemHolderInfo postInfo);
        void processPersistent(RecyclerView.ViewHolder viewHolder, @NonNull RecyclerView.ItemAnimator.ItemHolderInfo preInfo,
                @NonNull RecyclerView.ItemAnimator.ItemHolderInfo postInfo);
        void unused(RecyclerView.ViewHolder holder);
    }

    static class InfoRecord {

        static final int FLAG_DISAPPEARED = 1;

        static final int FLAG_APPEAR = 1 << 1;

        static final int FLAG_PRE = 1 << 2;

        static final int FLAG_POST = 1 << 3;
        static final int FLAG_APPEAR_AND_DISAPPEAR = FLAG_APPEAR | FLAG_DISAPPEARED;
        static final int FLAG_PRE_AND_POST = FLAG_PRE | FLAG_POST;
        static final int FLAG_APPEAR_PRE_AND_POST = FLAG_APPEAR | FLAG_PRE | FLAG_POST;
        int flags;
        @Nullable
        RecyclerView.ItemAnimator.ItemHolderInfo preInfo;
        @Nullable
        RecyclerView.ItemAnimator.ItemHolderInfo postInfo;
        static Pools.Pool<InfoRecord> sPool = new Pools.SimplePool<>(20);

        private InfoRecord() {
        }

        static InfoRecord obtain() {
            InfoRecord record = sPool.acquire();
            return record == null ? new InfoRecord() : record;
        }

        static void recycle(InfoRecord record) {
            record.flags = 0;
            record.preInfo = null;
            record.postInfo = null;
            sPool.release(record);
        }

        static void drainCache() {

            while (sPool.acquire() != null);
        }
    }
}

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

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.Collections;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.Executor;

public class AsyncListDiffer<T> {
    private final ListUpdateCallback mUpdateCallback;
    @SuppressWarnings("WeakerAccess")
    final AsyncDifferConfig<T> mConfig;
    Executor mMainThreadExecutor;

    private static class MainThreadExecutor implements Executor {
        final Handler mHandler = new Handler(Looper.getMainLooper());
        MainThreadExecutor() {}
        @Override
        public void execute(@NonNull Runnable command) {
            mHandler.post(command);
        }
    }

    private static final Executor sMainThreadExecutor = new MainThreadExecutor();

    public interface ListListener<T> {

        void onCurrentListChanged(@NonNull List<T> previousList, @NonNull List<T> currentList);
    }

    private final List<ListListener<T>> mListeners = new CopyOnWriteArrayList<>();

    public AsyncListDiffer(@NonNull RecyclerView.Adapter adapter,
            @NonNull DiffUtil.ItemCallback<T> diffCallback) {
        this(new AdapterListUpdateCallback(adapter),
            new AsyncDifferConfig.Builder<>(diffCallback).build());
    }

    @SuppressWarnings("WeakerAccess")
    public AsyncListDiffer(@NonNull ListUpdateCallback listUpdateCallback,
            @NonNull AsyncDifferConfig<T> config) {
        mUpdateCallback = listUpdateCallback;
        mConfig = config;
        if (config.getMainThreadExecutor() != null) {
            mMainThreadExecutor = config.getMainThreadExecutor();
        } else {
            mMainThreadExecutor = sMainThreadExecutor;
        }
    }

    @Nullable
    private List<T> mList;

    @NonNull
    private List<T> mReadOnlyList = Collections.emptyList();

    @SuppressWarnings("WeakerAccess")
    int mMaxScheduledGeneration;

    @NonNull
    public List<T> getCurrentList() {
        return mReadOnlyList;
    }

    @SuppressWarnings("WeakerAccess")
    public void submitList(@Nullable final List<T> newList) {
        submitList(newList, null);
    }

    @SuppressWarnings("WeakerAccess")
    public void submitList(@Nullable final List<T> newList,
            @Nullable final Runnable commitCallback) {

        final int runGeneration = ++mMaxScheduledGeneration;

        if (newList == mList) {

            if (commitCallback != null) {
                commitCallback.run();
            }
            return;
        }

        final List<T> previousList = mReadOnlyList;

        if (newList == null) {

            int countRemoved = mList.size();
            mList = null;
            mReadOnlyList = Collections.emptyList();

            mUpdateCallback.onRemoved(0, countRemoved);
            onCurrentListChanged(previousList, commitCallback);
            return;
        }

        if (mList == null) {
            mList = newList;
            mReadOnlyList = Collections.unmodifiableList(newList);

            mUpdateCallback.onInserted(0, newList.size());
            onCurrentListChanged(previousList, commitCallback);
            return;
        }

        final List<T> oldList = mList;
        mConfig.getBackgroundThreadExecutor().execute(new Runnable() {
            @Override
            public void run() {
                final DiffUtil.DiffResult result = DiffUtil.calculateDiff(new DiffUtil.Callback() {
                    @Override
                    public int getOldListSize() {
                        return oldList.size();
                    }

                    @Override
                    public int getNewListSize() {
                        return newList.size();
                    }

                    @Override
                    public boolean areItemsTheSame(int oldItemPosition, int newItemPosition) {
                        T oldItem = oldList.get(oldItemPosition);
                        T newItem = newList.get(newItemPosition);
                        if (oldItem != null && newItem != null) {
                            return mConfig.getDiffCallback().areItemsTheSame(oldItem, newItem);
                        }

                        return oldItem == null && newItem == null;
                    }

                    @Override
                    public boolean areContentsTheSame(int oldItemPosition, int newItemPosition) {
                        T oldItem = oldList.get(oldItemPosition);
                        T newItem = newList.get(newItemPosition);
                        if (oldItem != null && newItem != null) {
                            return mConfig.getDiffCallback().areContentsTheSame(oldItem, newItem);
                        }
                        if (oldItem == null && newItem == null) {
                            return true;
                        }

                        throw new AssertionError();
                    }

                    @Nullable
                    @Override
                    public Object getChangePayload(int oldItemPosition, int newItemPosition) {
                        T oldItem = oldList.get(oldItemPosition);
                        T newItem = newList.get(newItemPosition);
                        if (oldItem != null && newItem != null) {
                            return mConfig.getDiffCallback().getChangePayload(oldItem, newItem);
                        }

                        throw new AssertionError();
                    }
                });

                mMainThreadExecutor.execute(new Runnable() {
                    @Override
                    public void run() {
                        if (mMaxScheduledGeneration == runGeneration) {
                            latchList(newList, result, commitCallback);
                        }
                    }
                });
            }
        });
    }

    @SuppressWarnings("WeakerAccess")
    void latchList(
            @NonNull List<T> newList,
            @NonNull DiffUtil.DiffResult diffResult,
            @Nullable Runnable commitCallback) {
        final List<T> previousList = mReadOnlyList;
        mList = newList;

        mReadOnlyList = Collections.unmodifiableList(newList);
        diffResult.dispatchUpdatesTo(mUpdateCallback);
        onCurrentListChanged(previousList, commitCallback);
    }

    private void onCurrentListChanged(@NonNull List<T> previousList,
            @Nullable Runnable commitCallback) {

        for (ListListener<T> listener : mListeners) {
            listener.onCurrentListChanged(previousList, mReadOnlyList);
        }
        if (commitCallback != null) {
            commitCallback.run();
        }
    }

    public void addListListener(@NonNull ListListener<T> listener) {
        mListeners.add(listener);
    }

    public void removeListListener(@NonNull ListListener<T> listener) {
        mListeners.remove(listener);
    }
}

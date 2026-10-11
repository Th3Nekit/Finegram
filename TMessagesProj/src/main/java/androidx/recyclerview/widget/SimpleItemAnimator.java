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

import android.util.Log;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public abstract class SimpleItemAnimator extends RecyclerView.ItemAnimator {

    private static final boolean DEBUG = false;

    private static final String TAG = "SimpleItemAnimator";

    boolean mSupportsChangeAnimations = true;
    protected boolean alwaysCreateMoveAnimationIfPossible;
    protected boolean disabledMoveAnimations;

    @SuppressWarnings("unused")
    public boolean getSupportsChangeAnimations() {
        return mSupportsChangeAnimations;
    }

    public void setSupportsChangeAnimations(boolean supportsChangeAnimations) {
        mSupportsChangeAnimations = supportsChangeAnimations;
    }

    @Override
    public boolean canReuseUpdatedViewHolder(@NonNull RecyclerView.ViewHolder viewHolder) {
        return !mSupportsChangeAnimations || viewHolder.isInvalid();
    }

    @Override
    public boolean animateDisappearance(RecyclerView recyclerView, @NonNull RecyclerView.ViewHolder viewHolder,
            @NonNull ItemHolderInfo preLayoutInfo, @Nullable ItemHolderInfo postLayoutInfo) {
        int oldLeft = preLayoutInfo.left;
        int oldTop = preLayoutInfo.top;
        View disappearingItemView = viewHolder.itemView;
        int newLeft = postLayoutInfo == null ? disappearingItemView.getLeft() : postLayoutInfo.left;
        int newTop = postLayoutInfo == null ? disappearingItemView.getTop() : postLayoutInfo.top;
        if (!disabledMoveAnimations && !viewHolder.isRemoved() && (oldLeft != newLeft || oldTop != newTop)) {
            disappearingItemView.layout(newLeft, newTop,
                    newLeft + disappearingItemView.getWidth(),
                    newTop + disappearingItemView.getHeight());
            if (DEBUG) {
                Log.d(TAG, "DISAPPEARING: " + viewHolder + " with view " + disappearingItemView);
            }
            return animateMove(viewHolder, preLayoutInfo, oldLeft, oldTop, newLeft, newTop);
        } else {
            if (DEBUG) {
                Log.d(TAG, "REMOVED: " + viewHolder + " with view " + disappearingItemView);
            }
            final int notRemovedPositionBelow = getLastNotRemovedPosition(recyclerView, viewHolder.mOldOldPosition);
            viewHolder.mOldCompoundPosition = notRemovedPositionBelow * 1000 + (viewHolder.mOldOldPosition - notRemovedPositionBelow);
            return animateRemove(viewHolder, preLayoutInfo);
        }
    }

    private int getLastNotRemovedPosition(RecyclerView recyclerView, int belowOldPosition) {
        if (recyclerView == null || belowOldPosition == RecyclerView.NO_POSITION) return RecyclerView.NO_POSITION;
        int bestPosition = RecyclerView.NO_POSITION;
        for (int i = 0; i < recyclerView.getChildCount(); ++i) {
            final View child = recyclerView.getChildAt(i);
            if (child == null) continue;
            final RecyclerView.ViewHolder viewHolder = recyclerView.getChildViewHolder(child);
            if (viewHolder == null) continue;
            if (!viewHolder.isRemoved() && viewHolder.mOldOldPosition >= 0 && viewHolder.mOldOldPosition < belowOldPosition && viewHolder.mOldOldPosition > bestPosition) {
                bestPosition = viewHolder.mOldOldPosition;
            }
        }
        return bestPosition;
    }

    @Override
    public boolean animateAppearance(@NonNull RecyclerView.ViewHolder viewHolder,
            @Nullable ItemHolderInfo preLayoutInfo, @NonNull ItemHolderInfo postLayoutInfo) {
        if (!disabledMoveAnimations && preLayoutInfo != null && ((preLayoutInfo.left != postLayoutInfo.left
                || preLayoutInfo.top != postLayoutInfo.top) || alwaysCreateMoveAnimationIfPossible)) {

            if (DEBUG) {
                Log.d(TAG, "APPEARING: " + viewHolder + " with view " + viewHolder);
            }
            return animateMove(viewHolder,preLayoutInfo, preLayoutInfo.left, preLayoutInfo.top,
                    postLayoutInfo.left, postLayoutInfo.top);
        } else {
            if (DEBUG) {
                Log.d(TAG, "ADDED: " + viewHolder + " with view " + viewHolder);
            }
            return animateAdd(viewHolder);
        }
    }

    @Override
    public boolean animatePersistence(@NonNull RecyclerView.ViewHolder viewHolder,
            @NonNull ItemHolderInfo preInfo, @NonNull ItemHolderInfo postInfo) {
        if (!disabledMoveAnimations && (preInfo.left != postInfo.left || preInfo.top != postInfo.top)) {
            if (DEBUG) {
                Log.d(TAG, "PERSISTENT: " + viewHolder
                        + " with view " + viewHolder.itemView);
            }
            return animateMove(viewHolder, preInfo,
                    preInfo.left, preInfo.top, postInfo.left, postInfo.top);
        }
        dispatchMoveFinished(viewHolder);
        return false;
    }

    @Override
    public boolean animateChange(@NonNull RecyclerView.ViewHolder oldHolder, @NonNull RecyclerView.ViewHolder newHolder,
            @NonNull ItemHolderInfo preInfo, @NonNull ItemHolderInfo postInfo) {
        if (DEBUG) {
            Log.d(TAG, "CHANGED: " + oldHolder + " with view " + oldHolder.itemView);
        }
        final int fromLeft = preInfo.left;
        final int fromTop = preInfo.top;
        final int toLeft, toTop;
        if (newHolder.shouldIgnore()) {
            toLeft = preInfo.left;
            toTop = preInfo.top;
        } else {
            toLeft = postInfo.left;
            toTop = postInfo.top;
        }
        return animateChange(oldHolder, newHolder, preInfo, fromLeft, fromTop, toLeft, toTop);
    }

    public abstract boolean animateRemove(RecyclerView.ViewHolder holder, ItemHolderInfo info);

    public abstract boolean animateAdd(RecyclerView.ViewHolder holder);

    public abstract boolean animateMove(RecyclerView.ViewHolder holder, ItemHolderInfo info, int fromX, int fromY,
            int toX, int toY);

    public abstract boolean animateChange(RecyclerView.ViewHolder oldHolder,
            RecyclerView.ViewHolder newHolder, ItemHolderInfo info, int fromLeft, int fromTop, int toLeft, int toTop);

    public final void dispatchRemoveFinished(RecyclerView.ViewHolder item) {
        onRemoveFinished(item);
        dispatchAnimationFinished(item);
    }

    public final void dispatchMoveFinished(RecyclerView.ViewHolder item) {
        onMoveFinished(item);
        dispatchAnimationFinished(item);
    }

    public final void dispatchAddFinished(RecyclerView.ViewHolder item) {
        onAddFinished(item);
        dispatchAnimationFinished(item);
    }

    public final void dispatchChangeFinished(RecyclerView.ViewHolder item, boolean oldItem) {
        onChangeFinished(item, oldItem);
        dispatchAnimationFinished(item);
    }

    public final void dispatchRemoveStarting(RecyclerView.ViewHolder item) {
        onRemoveStarting(item);
    }

    public final void dispatchMoveStarting(RecyclerView.ViewHolder item) {
        onMoveStarting(item);
    }

    public final void dispatchAddStarting(RecyclerView.ViewHolder item) {
        onAddStarting(item);
    }

    public final void dispatchChangeStarting(RecyclerView.ViewHolder item, boolean oldItem) {
        onChangeStarting(item, oldItem);
    }

    @SuppressWarnings("UnusedParameters")
    public void onRemoveStarting(RecyclerView.ViewHolder item) {
    }

    public void onRemoveFinished(RecyclerView.ViewHolder item) {
    }

    @SuppressWarnings("UnusedParameters")
    public void onAddStarting(RecyclerView.ViewHolder item) {
    }

    public void onAddFinished(RecyclerView.ViewHolder item) {
    }

    @SuppressWarnings("UnusedParameters")
    public void onMoveStarting(RecyclerView.ViewHolder item) {
    }

    public void onMoveFinished(RecyclerView.ViewHolder item) {
    }

    @SuppressWarnings("UnusedParameters")
    public void onChangeStarting(RecyclerView.ViewHolder item, boolean oldItem) {
    }

    public void onChangeFinished(RecyclerView.ViewHolder item, boolean oldItem) {
    }
}

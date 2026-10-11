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

import android.view.View;

class LayoutState {

    static final int LAYOUT_START = -1;

    static final int LAYOUT_END = 1;

    static final int INVALID_LAYOUT = Integer.MIN_VALUE;

    static final int ITEM_DIRECTION_HEAD = -1;

    static final int ITEM_DIRECTION_TAIL = 1;

    boolean mRecycle = true;

    int mAvailable;

    int mCurrentPosition;

    int mItemDirection;

    int mLayoutDirection;

    int mStartLine = 0;

    int mEndLine = 0;

    boolean mStopInFocusable;

    boolean mInfinite;

    boolean hasMore(RecyclerView.State state) {
        return mCurrentPosition >= 0 && mCurrentPosition < state.getItemCount();
    }

    View next(RecyclerView.Recycler recycler) {
        final View view = recycler.getViewForPosition(mCurrentPosition);
        mCurrentPosition += mItemDirection;
        return view;
    }

    @Override
    public String toString() {
        return "LayoutState{"
                + "mAvailable=" + mAvailable
                + ", mCurrentPosition=" + mCurrentPosition
                + ", mItemDirection=" + mItemDirection
                + ", mLayoutDirection=" + mLayoutDirection
                + ", mStartLine=" + mStartLine
                + ", mEndLine=" + mEndLine
                + '}';
    }
}

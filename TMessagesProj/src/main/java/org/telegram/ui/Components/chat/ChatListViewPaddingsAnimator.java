package org.telegram.ui.Components.chat;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.FileLog;

public class ChatListViewPaddingsAnimator {
    private final RecyclerView recyclerView;

    public ChatListViewPaddingsAnimator(RecyclerView recyclerView) {
        this.recyclerView = recyclerView;
    }

    private int currentAdditionalHeight;
    public void setPaddings(
        int paddingTopTarget,
        float paddingBottomAnimated, boolean allowScrollCompensation
    ) {
        final int additionalHeight = 0;

        if (additionalHeight == 0 && currentAdditionalHeight != 0) {
            currentAdditionalHeight = 0;
            recyclerView.requestLayout();
        } else if (additionalHeight > currentAdditionalHeight) {
            currentAdditionalHeight = additionalHeight;
            recyclerView.requestLayout();
        }

        final int paddingTop = paddingTopTarget;
        final int paddingBottom = (int) paddingBottomAnimated;

        final int paddingTopOld = recyclerView.getPaddingTop();
        final int paddingBottomOld = recyclerView.getPaddingBottom();

        if (paddingTopOld != paddingTop || paddingBottomOld != paddingBottom) {
            final int dy = paddingTopOld - paddingTop;

            if (allowScrollCompensation && dy != 0) {
                final boolean canScrollDown = recyclerView.canScrollVertically(1);
                final boolean canScrollUp = recyclerView.canScrollVertically(-1);
                if (dy < 0 && !canScrollDown || dy > 0 && !canScrollUp) {

                } else {
                    AndroidUtilities.doOnLayout(recyclerView, () -> {
                        try {
                            recyclerView.scrollBy(0, dy);
                        } catch (Throwable t) {
                            FileLog.e(t);
                        }
                    });
                }
            }

            recyclerView.setPadding(
                    recyclerView.getPaddingLeft(),
                    paddingTop,
                    recyclerView.getPaddingRight(),
                    paddingBottom
            );
        }
    }

    public int getCurrentAdditionalHeight() {
        return currentAdditionalHeight;
    }
}

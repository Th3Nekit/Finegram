package org.telegram.ui.iv;

import android.content.Context;
import android.widget.FrameLayout;

public abstract class RichBlockCell extends FrameLayout implements RichInsetCell {

    protected BlockRow currentRow;

    private final RichBlockInset insetAnim = new RichBlockInset();
    private int blockInset;
    private int basePadLeft, basePadTop, basePadRight, basePadBottom;

    public RichBlockCell(Context context) {
        super(context);
    }

    protected void setBlockPadding(int left, int top, int right, int bottom) {
        basePadLeft = left;
        basePadTop = top;
        basePadRight = right;
        basePadBottom = bottom;
        RichBlockChrome.applyInsetPx(this, blockInset, basePadLeft, basePadTop, basePadRight, basePadBottom);
    }

    protected int blockInset() {
        return blockInset;
    }

    private void applyBlockInset(int px) {
        blockInset = px;
        onBlockInsetChanged(px);
    }

    protected int nestedContentMargin() {
        return 0;
    }

    protected void onBlockInsetChanged(int px) {

        final int endInset = RichBlockChrome.insetEndFor(currentRow);
        final int margin = (px > 0 || endInset > 0) ? nestedContentMargin() : 0;

        final int top = currentRow != null && currentRow.quoteFirst ? RichBlockChrome.quoteTopPad(currentRow) : basePadTop;
        final int bottom = currentRow != null && currentRow.quoteLast ? RichBlockChrome.quoteBottomPad(currentRow) : basePadBottom;
        RichBlockChrome.applyInsetPx(this, px + margin, endInset + margin, basePadLeft, top, basePadRight, bottom);
    }

    protected void bindBlockInset(BlockRow row) {
        insetAnim.apply(row, this::applyBlockInset);
    }

    @Override
    public void resyncBlockInset(boolean animated) {
        insetAnim.apply(currentRow, this::applyBlockInset, animated);
    }
}

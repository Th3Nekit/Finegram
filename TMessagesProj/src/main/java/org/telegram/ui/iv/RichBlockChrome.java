package org.telegram.ui.iv;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.text.TextPaint;
import android.view.View;

import org.telegram.messenger.LocaleController;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.ReplyMessageLine;

public class RichBlockChrome {

    public static void applyEditorQuoteColor(ReplyMessageLine line, Theme.ResourcesProvider resourcesProvider) {
        final boolean dark = resourcesProvider != null ? resourcesProvider.isDark() : Theme.isCurrentThemeDark();
        line.setSimpleColor(Theme.getColor(Theme.key_featuredStickers_addButton, resourcesProvider), dark);
    }

    public static final int INDENT_DP_PER_LEVEL = 24;
    public static final int MARKER_WIDTH_DP = 28;

    public static final int QUOTE_STEP_DP = 16;

    public static final int QUOTE_GUTTER_DP = 16;
    public static final int QUOTE_BAR_DP = 3;

    public static final int QUOTE_PAD_L_DP = 12;
    public static final int QUOTE_PAD_R_DP = 8;

    public static final int QUOTE_EDGE_VPAD_DP = 10;

    public static final int QUOTE_NEST_VPAD_DP = 16;

    public static int quoteEdgePad(int count) {
        return count <= 0 ? 0 : dp(QUOTE_EDGE_VPAD_DP + (count - 1) * QUOTE_NEST_VPAD_DP);
    }

    public static int quoteTopPad(BlockRow row) {
        return row == null ? 0 : quoteEdgePad(row.quoteTopEdge);
    }

    public static int quoteBottomPad(BlockRow row) {
        return row == null ? 0 : quoteEdgePad(row.quoteBottomEdge);
    }

    public static int insetForDepth(int depth) {
        if (depth <= 0) return 0;
        return dp(MARKER_WIDTH_DP + (depth - 1) * INDENT_DP_PER_LEVEL);
    }

    public static int quoteDepth(BlockRow row) {
        return row == null ? 0 : row.quoteIds.size();
    }

    public static int quoteInset(BlockRow row) {
        final int d = quoteDepth(row);
        return d <= 0 ? 0 : dp((d - 1) * QUOTE_STEP_DP + QUOTE_PAD_L_DP);
    }

    public static int quoteInsetEnd(BlockRow row) {
        final int d = quoteDepth(row);
        return d <= 0 ? 0 : dp((d - 1) * QUOTE_STEP_DP + QUOTE_PAD_R_DP);
    }

    public static int insetFor(BlockRow row) {
        return row == null ? 0 : quoteInset(row) + insetForDepth(Math.max(0, row.level));
    }

    public static int insetEndFor(BlockRow row) {
        return quoteInsetEnd(row);
    }

    public static boolean rtl() {
        return LocaleController.isRTL;
    }

    public static void applyInset(View cell, BlockRow row, int baseLeft, int baseTop, int baseRight, int baseBottom) {
        applyInsetPx(cell, insetFor(row), insetEndFor(row), baseLeft, baseTop, baseRight, baseBottom);
    }

    public static void applyInsetPx(View cell, int inset, int baseLeft, int baseTop, int baseRight, int baseBottom) {
        applyInsetPx(cell, inset, 0, baseLeft, baseTop, baseRight, baseBottom);
    }

    public static void applyInsetPx(View cell, int startInset, int endInset, int baseLeft, int baseTop, int baseRight, int baseBottom) {
        if (rtl()) {
            cell.setPadding(baseLeft + endInset, baseTop, baseRight + startInset, baseBottom);
        } else {
            cell.setPadding(baseLeft + startInset, baseTop, baseRight + endInset, baseBottom);
        }
    }

    private final TextPaint markerPaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);

    public RichBlockChrome() {
        markerPaint.setTextSize(dp(16));
        markerPaint.setTextAlign(Paint.Align.CENTER);
    }
}

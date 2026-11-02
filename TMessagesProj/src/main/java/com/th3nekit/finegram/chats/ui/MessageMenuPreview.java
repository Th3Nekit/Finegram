package com.th3nekit.finegram.chats.ui;

public final class MessageMenuPreview {
    public final float offset;
    public final float menuTop;
    public final float menuHeight;
    public final float visibleBottom;
    public final float offsetX;
    public final float menuLeft;
    public final boolean beside;

    private MessageMenuPreview(float offset, float menuTop, float menuHeight, float visibleBottom) {
        this(offset, menuTop, menuHeight, visibleBottom, 0, 0, false);
    }

    private MessageMenuPreview(float offset, float menuTop, float menuHeight, float visibleBottom,
                               float offsetX, float menuLeft, boolean beside) {
        this.offset = offset;
        this.menuTop = menuTop;
        this.menuHeight = menuHeight;
        this.visibleBottom = visibleBottom;
        this.offsetX = offsetX;
        this.menuLeft = menuLeft;
        this.beside = beside;
    }

    public static MessageMenuPreview fit(float top, float bottom, float viewportTop, float viewportBottom,
                                         float desiredMenuHeight, float minimumMenuHeight, float gap) {
        float height = bottom - top;
        float available = viewportBottom - viewportTop;
        if (!Float.isFinite(top) || !Float.isFinite(bottom) || !Float.isFinite(viewportTop)
                || !Float.isFinite(viewportBottom) || !Float.isFinite(available)
                || !Float.isFinite(desiredMenuHeight) || !Float.isFinite(minimumMenuHeight)
                || !Float.isFinite(gap) || !Float.isFinite(height) || available <= 0 || height <= 0) {
            return new MessageMenuPreview(0, viewportTop, 0, viewportTop);
        }
        gap = Math.max(0, gap);
        float menuHeight = Math.max(0, desiredMenuHeight);
        float targetTop = Math.max(viewportTop, Math.min(top, viewportBottom - menuHeight - gap - height));
        float visibleBottom = targetTop + height;
        return new MessageMenuPreview(targetTop - top, visibleBottom + gap, menuHeight, visibleBottom);
    }

    public static MessageMenuPreview fit(float left, float top, float right, float bottom,
                                         float viewportLeft, float viewportTop, float viewportRight, float viewportBottom,
                                         float menuWidth, float desiredMenuHeight, float minimumMenuHeight,
                                         float gap, boolean outgoing) {
        MessageMenuPreview stacked = fit(top, bottom, viewportTop, viewportBottom,
                desiredMenuHeight, minimumMenuHeight, gap);
        float width = right - left;
        float availableWidth = viewportRight - viewportLeft;
        if (!Float.isFinite(left) || !Float.isFinite(right) || !Float.isFinite(viewportLeft)
                || !Float.isFinite(viewportRight) || !Float.isFinite(menuWidth)
                || !Float.isFinite(availableWidth) || width <= 0 || menuWidth <= 0
                || stacked.menuHeight <= 0) return stacked;
        return new MessageMenuPreview(stacked.offset, stacked.menuTop, stacked.menuHeight, stacked.visibleBottom,
                0, menuLeft(left, right, menuWidth, viewportLeft, viewportRight, outgoing), false);
    }

    public static float scroll(float current, float delta, float contentHeight, float visibleHeight) {
        if (!Float.isFinite(current) || !Float.isFinite(delta) || !Float.isFinite(contentHeight)
                || !Float.isFinite(visibleHeight) || visibleHeight <= 0) return 0;
        return Math.max(-Math.max(0, contentHeight - visibleHeight), Math.min(0, current + delta));
    }

    public static float menuLeft(float bubbleLeft, float bubbleRight, float menuWidth,
                                 float viewportLeft, float viewportRight, boolean outgoing) {
        float left = outgoing ? bubbleRight - menuWidth : bubbleLeft;
        return Math.max(viewportLeft, Math.min(left, viewportRight - menuWidth));
    }
}

package com.th3nekit.finegram.chats.ui;

public final class MessageMenuPreview {
    public final float offset;
    public final float menuTop;
    public final float menuHeight;
    public final float visibleBottom;

    private MessageMenuPreview(float offset, float menuTop, float menuHeight, float visibleBottom) {
        this.offset = offset;
        this.menuTop = menuTop;
        this.menuHeight = menuHeight;
        this.visibleBottom = visibleBottom;
    }

    public static MessageMenuPreview fit(float top, float bottom, float viewportTop, float viewportBottom,
                                         float desiredMenuHeight, float minimumMenuHeight, float gap) {
        float height = bottom - top;
        float available = viewportBottom - viewportTop;
        if (!Float.isFinite(top) || !Float.isFinite(bottom) || !Float.isFinite(viewportTop)
                || !Float.isFinite(viewportBottom) || !Float.isFinite(available)
                || !Float.isFinite(desiredMenuHeight) || !Float.isFinite(minimumMenuHeight)
                || !Float.isFinite(gap) || available <= 0 || height <= 0) {
            return new MessageMenuPreview(0, viewportTop, 0, viewportTop);
        }
        gap = Math.max(0, Math.min(gap, available / 4));
        float minimum = Math.min(Math.max(0, minimumMenuHeight), (available - gap) / 2);
        float previewMinimum = Math.min(height, Math.min(minimumMenuHeight / 2, (available - gap) / 3));
        float menuHeight = Math.min(Math.max(minimum, desiredMenuHeight), available - gap - previewMinimum);
        float visibleHeight = Math.min(height, available - gap - menuHeight);
        float targetTop = Math.max(viewportTop, Math.min(top, viewportBottom - menuHeight - gap - visibleHeight));
        float visibleBottom = targetTop + visibleHeight;
        return new MessageMenuPreview(targetTop - top, visibleBottom + gap, menuHeight, visibleBottom);
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

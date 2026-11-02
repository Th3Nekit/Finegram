package com.th3nekit.finegram.core.ui;

public final class MusicPlayerLayout {
    public final int artwork;
    public final int panelTop;
    public final int titleTop;
    public final int authorTop;
    public final int seekTop;
    public final int timeTop;
    public final int timeHeight;
    public final int controlsTop;
    public final int toolsTop;
    public final int toolsHeight;
    public final int panelBottom;
    public final int height;
    public final int controlsHeight;
    private final int playSize;
    private final int skipSize;

    private MusicPlayerLayout(int width, int availableHeight, int artwork, int titleHeight, int authorHeight, int timeHeight, int extra) {
        this.artwork = artwork;
        playSize = width < 360 || availableHeight < 560 ? 56 : 64;
        skipSize = 48;
        controlsHeight = playSize;
        panelTop = artwork + 30;
        titleTop = panelTop + 20;
        authorTop = titleTop + titleHeight;
        seekTop = authorTop + authorHeight + 4;
        timeTop = seekTop + 42;
        this.timeHeight = timeHeight;
        controlsTop = timeTop + timeHeight + 8;
        toolsTop = controlsTop + controlsHeight + 8;
        toolsHeight = Math.max(48, timeHeight);
        panelBottom = toolsTop + toolsHeight + 16 + extra;
        height = panelBottom + 12;
    }

    public static MusicPlayerLayout create(int width, int availableHeight, float fontScale, boolean profileActions) {
        return create(width, availableHeight, fontScale, profileActions, false);
    }

    public static MusicPlayerLayout createScrollable(int width, int availableHeight, float fontScale, boolean profileActions) {
        return create(width, availableHeight, fontScale, profileActions, true);
    }

    private static MusicPlayerLayout create(int width, int availableHeight, float fontScale, boolean profileActions, boolean scrollable) {
        if (width < 280 || !Float.isFinite(fontScale) || fontScale <= 0) return null;
        int titleHeight = (int) Math.ceil(32 * Math.max(1f, fontScale));
        int authorHeight = (int) Math.ceil(24 * Math.max(1f, fontScale));
        int timeHeight = Math.max(24, (int) Math.ceil(20 * fontScale));
        int extra = profileActions ? 64 : 0;
        MusicPlayerLayout empty = new MusicPlayerLayout(width, availableHeight, 0, titleHeight, authorHeight, timeHeight, extra);
        int artworkSpace = availableHeight - empty.height;
        if (scrollable) artworkSpace = Math.max(104, artworkSpace);
        int artwork = Math.min(Math.min(width - 88, 320), artworkSpace);
        return artwork < 104 ? null : new MusicPlayerLayout(width, availableHeight, artwork, titleHeight, authorHeight, timeHeight, extra);
    }

    public static Compact compact(float fontScale, boolean profileActions) {
        return new Compact(Float.isFinite(fontScale) && fontScale > 0 ? Math.max(1f, fontScale) : 1f, profileActions);
    }

    public static int panelSideInset(int width) {
        return width < 320 ? 12 : 20;
    }

    public static int contentSideInset(int width) {
        return panelSideInset(width) + (width < 320 ? 12 : 16);
    }

    public static ToolRow toolRow(int width, int minimumQueueWidth) {
        int side = contentSideInset(width);
        int speedWidth = 72;
        int optionsWidth = 48;
        int gap = 8;
        int inlineQueueWidth = Math.max(0, width - side * 2 - speedWidth - optionsWidth - gap * 2);
        boolean stacked = minimumQueueWidth > inlineQueueWidth;
        int queueLeft = stacked ? side : side + speedWidth + gap + optionsWidth + gap;
        int queueWidth = stacked ? Math.max(0, width - side * 2) : inlineQueueWidth;
        return new ToolRow(side, side + speedWidth + gap, queueLeft, queueWidth, stacked);
    }

    public static final class ToolRow {
        public final int speedLeft;
        public final int optionsLeft;
        public final int queueLeft;
        public final int queueWidth;
        public final boolean stacked;

        private ToolRow(int speedLeft, int optionsLeft, int queueLeft, int queueWidth, boolean stacked) {
            this.speedLeft = speedLeft;
            this.optionsLeft = optionsLeft;
            this.queueLeft = queueLeft;
            this.queueWidth = queueWidth;
            this.stacked = stacked;
        }
    }

    public static final class Compact {
        public final int titleTop = 20;
        public final int titleHeight;
        public final int authorTop;
        public final int authorHeight;
        public final int seekTop;
        public final int timeTop;
        public final int timeHeight;
        public final int controlsTop;
        public final int toolsTop;
        public final int toolsHeight;
        public final int height;

        private Compact(float font, boolean profileActions) {
            titleHeight = (int) Math.ceil(24 * font);
            authorTop = titleTop + titleHeight + 3;
            authorHeight = (int) Math.ceil(20 * font);
            seekTop = authorTop + authorHeight;
            timeTop = seekTop + 44;
            timeHeight = Math.max(24, (int) Math.ceil(20 * font));
            controlsTop = timeTop + timeHeight + 8;
            toolsTop = controlsTop + 66 + 8;
            toolsHeight = Math.max(48, (int) Math.ceil(22 * font));
            height = toolsTop + toolsHeight + 12 + (profileActions ? 64 : 0);
        }
    }

    public int buttonLeft(int width, int index) {
        int center = width / 2;
        int gap = Math.max(0, (Math.min(width - 64, 440) - playSize - skipSize * 2 - 96) / 4);
        switch (index) {
            case 0: return center + playSize / 2 + skipSize + gap * 2;
            case 1: return center - playSize / 2 - skipSize - gap;
            case 2: return center - playSize / 2;
            case 3: return center + playSize / 2 + gap;
            case 4: return center - playSize / 2 - skipSize - 48 - gap * 2;
            default: throw new IllegalArgumentException();
        }
    }

    public int buttonSize(int index) {
        return index == 2 ? playSize : index == 1 || index == 3 ? skipSize : 48;
    }
}

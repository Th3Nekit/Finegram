package com.th3nekit.finegram.core.ui;

public final class MusicPlayerLayout {
    public final int artwork;
    public final int titleTop;
    public final int authorTop;
    public final int seekTop;
    public final int timeTop;
    public final int timeHeight;
    public final int controlsTop;
    public final int toolsTop;
    public final int height;
    public final int controlsHeight;
    private final int playSize;
    private final int skipSize;

    private MusicPlayerLayout(int width, int artwork, int titleHeight, int authorHeight, int timeHeight, int extra) {
        this.artwork = artwork;
        playSize = width < 360 ? 80 : 88;
        skipSize = width < 360 ? 52 : 56;
        controlsHeight = playSize;
        titleTop = artwork + 28;
        authorTop = titleTop + titleHeight;
        seekTop = authorTop + authorHeight + 4;
        timeTop = seekTop + 42;
        this.timeHeight = timeHeight;
        controlsTop = timeTop + timeHeight + 8;
        toolsTop = controlsTop + controlsHeight + 8;
        height = toolsTop + 60 + extra;
    }

    public static MusicPlayerLayout create(int width, int availableHeight, float fontScale, boolean profileActions) {
        if (width < 280 || !Float.isFinite(fontScale) || fontScale <= 0) return null;
        int titleHeight = (int) Math.ceil(32 * Math.max(1f, fontScale));
        int authorHeight = (int) Math.ceil(24 * Math.max(1f, fontScale));
        int timeHeight = Math.max(24, (int) Math.ceil(20 * fontScale));
        int extra = profileActions ? 52 : 0;
        MusicPlayerLayout empty = new MusicPlayerLayout(width, 0, titleHeight, authorHeight, timeHeight, extra);
        int artwork = Math.min(Math.min(width - 40, 400), availableHeight - empty.height);
        return artwork < 104 ? null : new MusicPlayerLayout(width, artwork, titleHeight, authorHeight, timeHeight, extra);
    }

    public int buttonLeft(int width, int index) {
        int center = width / 2;
        int gap = Math.max(0, (Math.min(width - 32, 440) - playSize - skipSize * 2 - 96) / 4);
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

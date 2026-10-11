package com.th3nekit.finegram.core.ui;

public final class MusicPlayerLayout {
    public final int artwork;
    public final int titleTop;
    public final int authorTop;
    public final int seekTop;
    public final int timeTop;
    public final int controlsTop;
    public final int toolsTop;
    public final int height;

    private MusicPlayerLayout(int artwork, int titleHeight, int authorHeight, int extra) {
        this.artwork = artwork;
        titleTop = artwork + 40;
        authorTop = titleTop + titleHeight;
        seekTop = authorTop + authorHeight + 12;
        timeTop = seekTop + 40;
        controlsTop = timeTop + 30;
        toolsTop = controlsTop + 108;
        height = toolsTop + 60 + extra;
    }

    public static MusicPlayerLayout create(int width, int availableHeight, float fontScale, boolean profileActions) {
        if (width < 336 || !Float.isFinite(fontScale) || fontScale <= 0) return null;
        int titleHeight = (int) Math.ceil(36 * Math.max(1f, fontScale));
        int authorHeight = (int) Math.ceil(28 * Math.max(1f, fontScale));
        int extra = profileActions ? 52 : 0;
        MusicPlayerLayout empty = new MusicPlayerLayout(0, titleHeight, authorHeight, extra);
        int artwork = Math.min(Math.min(width - 40, 360), availableHeight - empty.height);
        return artwork < 104 ? null : new MusicPlayerLayout(artwork, titleHeight, authorHeight, extra);
    }

    public int buttonLeft(int width, int index) {
        int center = width / 2;
        switch (index) {
            case 0: return width - 52;
            case 1: return center - 100 - (width - 312) / 2;
            case 2: return center - 44;
            case 3: return center + 44 + (width - 312) / 2;
            case 4: return 4;
            default: throw new IllegalArgumentException();
        }
    }

    public int buttonSize(int index) {
        return index == 2 ? 88 : index == 1 || index == 3 ? 56 : 48;
    }
}

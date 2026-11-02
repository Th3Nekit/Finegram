package com.th3nekit.finegram.core.ui;

public final class ProfileMusicLayout {
    public final float height;
    public final float coverLeft, coverTop, coverSize;
    public final float buttonLeft, buttonTop;
    public final float textLeft, textWidth, titleTop, titleHeight, authorTop, authorHeight;
    public final float progressLeft, progressRight, progressY;

    public static float height(float fontScale) {
        return 52f + Math.max(52f, 40f * Math.max(1f, fontScale) + 3f);
    }

    public ProfileMusicLayout(float width, float fontScale, boolean rtl) {
        height = height(fontScale);
        float scale = Math.max(1f, fontScale);
        coverSize = 52f;
        coverLeft = mirror(30f, coverSize, width, rtl);
        titleHeight = 22f * scale;
        authorHeight = 18f * scale;
        float mainHeight = Math.max(coverSize, titleHeight + 3f + authorHeight);
        coverTop = 22f + (mainHeight - coverSize) / 2f;
        titleTop = 22f + (mainHeight - titleHeight - 3f - authorHeight) / 2f;
        authorTop = titleTop + titleHeight + 3f;
        textWidth = Math.max(0f, width - 182f);
        textLeft = mirror(94f, textWidth, width, rtl);
        buttonLeft = mirror(width - 78f, 48f, width, rtl);
        buttonTop = 22f + (mainHeight - 48f) / 2f;
        progressLeft = 16f;
        progressRight = width - 16f;
        progressY = height - 9f;
    }

    private static float mirror(float left, float size, float width, boolean rtl) {
        return rtl ? width - left - size : left;
    }

    public static boolean isCurrentTrack(int account, long documentId, int playingAccount, long playingDocumentId) {
        return documentId != 0 && account == playingAccount && documentId == playingDocumentId;
    }
}

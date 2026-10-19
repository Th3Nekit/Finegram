package com.th3nekit.finegram.chats.ui;

public final class PredictiveBackTransform {
    public float scale = 1f;
    public float offsetX;
    public float offsetY;

    public void set(float scale, float shift, float pivotX, float pivotY) {
        this.scale = scale;
        offsetX = (1f - scale) * pivotX - shift;
        offsetY = (1f - scale) * pivotY;
    }

    public float mapX(float x) {
        return x * scale + offsetX;
    }

    public float mapY(float y) {
        return y * scale + offsetY;
    }
}

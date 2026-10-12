/*
 * Finegram plugin store.
 * Adapted from Kangel Plugins Manager (KangelPlugins).
 * Upstream: https://git.kangel.xyz/KangelPlugins/PluginManager
 * Licensed under GNU GPL v3; see LICENSE.PluginManager and NOTICE.
 */

package com.th3nekit.finegram.store;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;
import android.view.View;

import androidx.annotation.NonNull;

import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.RecyclerListView;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import org.telegram.ui.Components.UniversalRecyclerView;

public class FGStoreSkeleton extends View {

    private static final long SWEEP_MS = 1100;

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF rect = new RectF();
    private final Matrix matrix = new Matrix();

    private LinearGradient gradient;
    private int gradientWidth;
    private long startedAt;

    public FGStoreSkeleton(@NonNull Context context) {
        super(context);
        setWillNotDraw(false);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), dp(86));
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        startedAt = System.currentTimeMillis();
    }

    @Override
    protected void onDraw(@NonNull Canvas canvas) {
        final int width = getMeasuredWidth();
        if (width <= 0) {
            return;
        }
        if (gradient == null || gradientWidth != width) {
            gradientWidth = width;
            final int base = Theme.getColor(Theme.key_windowBackgroundGray);
            final int light = Theme.getColor(Theme.key_windowBackgroundWhite);
            gradient = new LinearGradient(0, 0, width / 2f, 0,
                    new int[]{base, light, base}, new float[]{0f, 0.5f, 1f}, Shader.TileMode.CLAMP);
            paint.setShader(gradient);
        }

        final float progress = ((System.currentTimeMillis() - startedAt) % SWEEP_MS) / (float) SWEEP_MS;
        matrix.setTranslate(width * (progress * 2f - 0.5f), 0);
        gradient.setLocalMatrix(matrix);

        final float centerY = getMeasuredHeight() / 2f;
        rect.set(dp(16), centerY - dp(23), dp(16 + 46), centerY + dp(23));
        canvas.drawRoundRect(rect, dp(14), dp(14), paint);

        rect.set(dp(76), centerY - dp(21), dp(76) + width * 0.34f, centerY - dp(9));
        canvas.drawRoundRect(rect, dp(6), dp(6), paint);

        rect.set(dp(76), centerY - dp(1), dp(76) + width * 0.46f, centerY + dp(9));
        canvas.drawRoundRect(rect, dp(5), dp(5), paint);

        rect.set(dp(76), centerY + dp(14), dp(76) + width * 0.28f, centerY + dp(24));
        canvas.drawRoundRect(rect, dp(5), dp(5), paint);

        rect.set(width - dp(16 + 84), centerY - dp(26), width - dp(16), centerY - dp(2));
        canvas.drawRoundRect(rect, dp(12), dp(12), paint);

        invalidate();
    }

    public static class Factory extends UItem.UItemFactory<FGStoreSkeleton> {
        static { setup(new Factory()); }

        @Override
        public boolean isClickable() {
            return false;
        }

        @Override
        public FGStoreSkeleton createView(Context context, RecyclerListView listView, int currentAccount,
                                          int classGuid, Theme.ResourcesProvider resourcesProvider) {
            return new FGStoreSkeleton(context);
        }

        @Override
        public void bindView(View view, UItem item, boolean divider, UniversalAdapter adapter, UniversalRecyclerView listView) {
        }

        public static UItem of(int id) {
            UItem item = UItem.ofFactory(Factory.class);
            item.id = id;
            return item;
        }
    }
}

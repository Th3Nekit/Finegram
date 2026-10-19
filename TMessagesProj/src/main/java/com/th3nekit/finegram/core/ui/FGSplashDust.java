/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.ui;

import android.animation.ValueAnimator;
import android.app.Activity;
import android.app.ActivityManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.PathInterpolator;
import android.widget.FrameLayout;

import androidx.annotation.RequiresApi;

import com.th3nekit.finegram.core.FinegramLogger;

import java.util.Random;

/**
 * Уход заставки: значок рассыпается в пыль, приложение проступает сквозь.
 *
 * Пыль не отвлечённая — это сам значок, разрезанный на клетки. Каждая летит от середины
 * наружу, уменьшается и гаснет, ближние к краю трогаются раньше. Отвлечённые точки поверх
 * значка выглядели бы как чужая мишура, а так распадается именно он.
 *
 * Своё окно рисуем поверх готового приложения, а системную заставку снимаем сразу: спорить
 * с ней за то, кто кого показывает, не нужно, и если наша отрисовка вдруг откажет, человек
 * увидит приложение, а не чёрный экран.
 *
 * Это самое чувствительное место в клиенте: код выполняется при каждом холодном запуске.
 * Поэтому любая осечка здесь заканчивается тем же, чем закончился бы отказ от затеи —
 * заставка просто исчезает.
 */
@RequiresApi(api = Build.VERSION_CODES.S)
public final class FGSplashDust {

    /** Клеток по большей стороне значка. Больше — мельче пыль и больше вызовов отрисовки. */
    private static final int GRID = 14;

    private static final long DURATION_MS = 620;

    /** Насколько далеко улетает клетка: доля от половины значка. */
    private static final float SPREAD = 2.6f;

    private FGSplashDust() {
    }

    public static void install(Activity activity) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) return;
        try {
            activity.getSplashScreen().setOnExitAnimationListener(view -> {
                try {
                    start(activity, view);
                } catch (Throwable t) {
                    FinegramLogger.e("FGSplash", () -> "заставка ушла без пыли", t);
                    try { view.remove(); } catch (Throwable ignored) {}
                }
            });
        } catch (Throwable t) {
            FinegramLogger.e("FGSplash", () -> "не удалось перехватить уход заставки", t);
        }
    }

    private static void start(Activity activity, android.window.SplashScreenView splash) {
        final View iconView = splash.getIconView();
        if (iconView == null || iconView.getWidth() <= 0 || iconView.getHeight() <= 0
                || isWeakDevice(activity)) {
            splash.remove();
            return;
        }

        final Bitmap icon = Bitmap.createBitmap(iconView.getWidth(), iconView.getHeight(),
                Bitmap.Config.ARGB_8888);
        iconView.draw(new Canvas(icon));

        final int[] iconAt = new int[2];
        iconView.getLocationInWindow(iconAt);
        final int[] rootAt = new int[2];
        final ViewGroup root = (ViewGroup) activity.getWindow().getDecorView();
        root.getLocationInWindow(rootAt);

        final Rect where = new Rect(
                iconAt[0] - rootAt[0], iconAt[1] - rootAt[1],
                iconAt[0] - rootAt[0] + iconView.getWidth(),
                iconAt[1] - rootAt[1] + iconView.getHeight());

        // Цвет берём из ресурса, а не у снимаемой заставки: её фон после remove трогать
        // нечестно, он может быть общим на несколько мест.
        final int background = activity.getColor(org.telegram.messenger.R.color.fg_splash_background);
        final DustView dust = new DustView(activity, icon, where, background);
        root.addView(dust, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        // Системную заставку убираем до начала: дальше показываем сами.
        splash.remove();
        dust.play(() -> {
            root.removeView(dust);
            icon.recycle();
        });
    }

    /**
     * На слабом телефоне пыль пропускаем.
     *
     * Она приходится ровно на тот момент, когда клиент доедает последние силы на запуск, и
     * дёрганая заставка хуже, чем её отсутствие.
     */
    private static boolean isWeakDevice(Activity activity) {
        try {
            final ActivityManager manager = activity.getSystemService(ActivityManager.class);
            return manager != null && manager.isLowRamDevice();
        } catch (Throwable t) {
            return false;
        }
    }

    /** Окно поверх приложения: фон заставки, который гаснет, и разлетающиеся клетки. */
    private static final class DustView extends View {

        /** Резкий старт с долгим оседанием: пыль срывается сразу, а тает не спеша. */
        private final PathInterpolator flight = new PathInterpolator(0.16f, 1f, 0.3f, 1f);

        private final Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG | Paint.ANTI_ALIAS_FLAG);
        private final Bitmap icon;
        private final Rect where;
        private final int background;

        private final int columns;
        private final int rows;
        private final float[] shiftX;
        private final float[] shiftY;
        private final float[] delay;

        private final Rect source = new Rect();
        private final RectF target = new RectF();

        private float progress;

        DustView(Activity activity, Bitmap icon, Rect where, int background) {
            super(activity);
            this.icon = icon;
            this.where = where;
            this.background = background;
            setWillNotDraw(false);

            columns = GRID;
            rows = Math.max(1, Math.round(GRID * (float) where.height() / where.width()));
            final int count = columns * rows;
            shiftX = new float[count];
            shiftY = new float[count];
            delay = new float[count];

            final Random random = new Random(icon.getGenerationId());
            for (int row = 0; row < rows; row++) {
                for (int column = 0; column < columns; column++) {
                    final int i = row * columns + column;
                    // от середины наружу, с разбросом — иначе пыль летит ровными лучами
                    final float dx = (column + 0.5f) / columns - 0.5f;
                    final float dy = (row + 0.5f) / rows - 0.5f;
                    final float jitterX = random.nextFloat() - 0.5f;
                    final float jitterY = random.nextFloat() - 0.5f;
                    shiftX[i] = (dx * 2f + jitterX * 0.6f) * SPREAD;
                    shiftY[i] = (dy * 2f + jitterY * 0.6f) * SPREAD - 0.25f;
                    // края срываются первыми, середина держится дольше
                    final float fromCenter = Math.min(1f, (float) Math.hypot(dx, dy) * 2f);
                    delay[i] = (1f - fromCenter) * 0.28f;
                }
            }
        }

        void play(Runnable whenDone) {
            final ValueAnimator animator = ValueAnimator.ofFloat(0f, 1f);
            animator.setDuration(DURATION_MS);
            animator.addUpdateListener(value -> {
                progress = (float) value.getAnimatedValue();
                invalidate();
            });
            animator.addListener(new android.animation.AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(android.animation.Animator animation) {
                    whenDone.run();
                }
            });
            animator.start();
        }

        @Override
        protected void onDraw(Canvas canvas) {
            // фон уходит раньше пыли, чтобы приложение проступило сквозь неё
            final float cover = 1f - clamp(progress / 0.55f);
            if (cover > 0f) {
                canvas.drawColor(androidx.core.graphics.ColorUtils.setAlphaComponent(
                        background, (int) (255 * cover)));
            }

            final float tileWidth = (float) where.width() / columns;
            final float tileHeight = (float) where.height() / rows;
            final float sourceWidth = (float) icon.getWidth() / columns;
            final float sourceHeight = (float) icon.getHeight() / rows;
            final float centerX = where.exactCenterX();
            final float centerY = where.exactCenterY();
            final float reach = where.width() / 2f;

            for (int row = 0; row < rows; row++) {
                for (int column = 0; column < columns; column++) {
                    final int i = row * columns + column;
                    final float own = clamp((progress - delay[i]) / (1f - delay[i]));
                    if (own >= 1f) continue;
                    final float eased = flight.getInterpolation(own);

                    final float alpha = 1f - own * own;
                    if (alpha <= 0.01f) continue;

                    final float scale = 1f - eased * 0.7f;
                    final float left = where.left + column * tileWidth;
                    final float top = where.top + row * tileHeight;
                    final float tileCenterX = left + tileWidth / 2f + shiftX[i] * reach * eased;
                    final float tileCenterY = top + tileHeight / 2f + shiftY[i] * reach * eased;
                    final float halfW = tileWidth * scale / 2f;
                    final float halfH = tileHeight * scale / 2f;

                    source.set(
                            (int) (column * sourceWidth), (int) (row * sourceHeight),
                            (int) Math.ceil((column + 1) * sourceWidth),
                            (int) Math.ceil((row + 1) * sourceHeight));
                    target.set(tileCenterX - halfW, tileCenterY - halfH,
                            tileCenterX + halfW, tileCenterY + halfH);

                    paint.setAlpha((int) (255 * alpha));
                    canvas.drawBitmap(icon, source, target, paint);
                }
            }

            // держим середину неподвижной чуть дольше: так видно, что рассыпается именно значок
            if (progress < 0.08f) {
                final float hold = 1f - progress / 0.08f;
                paint.setAlpha((int) (255 * hold));
                target.set(where);
                canvas.drawBitmap(icon, null, target, paint);
            }
        }

        private static float clamp(float value) {
            return value < 0f ? 0f : Math.min(value, 1f);
        }
    }
}

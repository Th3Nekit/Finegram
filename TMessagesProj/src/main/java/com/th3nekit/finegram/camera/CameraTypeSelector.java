/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.camera;

import static org.telegram.messenger.LocaleController.getString;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.RoundRectShape;
import android.os.Build;
import android.view.Gravity;
import android.view.HapticFeedbackConstants;
import android.widget.FrameLayout;
import android.view.MotionEvent;
import android.view.ViewParent;
import android.widget.LinearLayout;

import androidx.core.content.ContextCompat;
import androidx.core.graphics.ColorUtils;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.Easings;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.NumberPicker;
import org.telegram.ui.LaunchActivity;

import com.th3nekit.finegram.core.configs.FinegramCameraConfig;

public class CameraTypeSelector extends LinearLayout {

    String[] strings = new String[]{
            "Telegram",
            "CameraX",
            "Camera 2 (Telegram)",
            getString(R.string.CP_CameraTypeSystem),
    };

    /**
     * Значок под каждым видом камеры — по порядку списка выше.
     *
     * CameraX — библиотека Google, поэтому у неё их знак. «Telegram» и «Camera 2» — это
     * две съёмки самого Telegram, у обеих его самолётик. Прежде на месте CameraX стояла
     * вишня Cherrygram, а знак Google был у Camera 2: значки не совпадали с подписями.
     */
    int[] icons = new int[]{
            R.drawable.camera_icon_telegram,
            R.drawable.camera_icon_camerax,
            R.drawable.camera_icon_telegram,
            R.drawable.camera_icon_system
    };
    int currentIcon = FinegramCameraConfig.INSTANCE.getCameraType();

    private final NumberPicker numberPicker;
    private final FrameLayout preview;

    private final RectF rect = new RectF();
    private final Paint outlinePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint pickerDividersPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    private ValueAnimator animator;
    private float progress;

    // Предпросмотр перерисовывается на каждом кадре вращения колеса. Всё, что нужно для
    // отрисовки, живёт в полях: создавать фигуры на кадр — значит оставлять после себя
    // мусор ровно тогда, когда идёт анимация.
    private final Rect previewBounds = new Rect();
    private final int[] previewGradient = new int[2];
    /** Скругление сверху, прямые углы снизу — так выглядит корпус телефона на картинке. */
    private final ShapeDrawable previewShape = new ShapeDrawable(new RoundRectShape(
            new float[]{
                    AndroidUtilities.dp(20), AndroidUtilities.dp(20),
                    AndroidUtilities.dp(20), AndroidUtilities.dp(20),
                    0, 0, 0, 0
            }, null, null));
    private final GradientDrawable previewFade =
            new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM, new int[]{0, 0});
    private final CameraClusterDrawable cameraCluster = new CameraClusterDrawable();

    /** Жест начался на колесе — значит он до конца его, чей бы ни был следующий кадр. */
    private boolean gestureOnWheel;

    /** Работа, которая ждёт конца жеста: пересобирать список под пальцем нельзя. */
    private Runnable pendingAfterGesture;

    /** Сколько ждать после отпускания: колесо ещё доводится до ближайшего деления. */
    private static final long SETTLE_MS = 320;

    /**
     * Пока крутят колесо, список настроек под ним стоит на месте.
     *
     * Колесо и список ждут одного и того же вертикального движения. Запрет ставится на всё
     * время жеста и именно здесь: сюда касание приходит раньше, чем к колесу, и приходит
     * даже тогда, когда колесо от него отказалось.
     */
    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            gestureOnWheel = numberPicker != null && isInside(numberPicker, event);
        }
        if (gestureOnWheel) {
            final ViewParent parent = getParent();
            if (parent != null) {
                parent.requestDisallowInterceptTouchEvent(
                        action != MotionEvent.ACTION_UP && action != MotionEvent.ACTION_CANCEL);
            }
        }
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            gestureOnWheel = false;
            if (pendingAfterGesture != null) {
                final Runnable action1 = pendingAfterGesture;
                pendingAfterGesture = null;
                AndroidUtilities.runOnUIThread(action1, SETTLE_MS);
            }
        }
        return super.dispatchTouchEvent(event);
    }

    /**
     * Откладывает работу до конца жеста.
     *
     * Список настроек пересобирается при смене типа камеры, а пересборка во время жеста
     * отбирает касание у колеса: ячейку переиспользуют, и палец оказывается уже на списке.
     */
    public void runAfterGesture(Runnable action) {
        if (!gestureOnWheel) {
            action.run();
            return;
        }
        pendingAfterGesture = action;
    }

    /** Началось ли касание на самом колесе, а не рядом с ним. */
    private boolean isInside(android.view.View view, MotionEvent event) {
        final float x = event.getX();
        final float y = event.getY();
        return x >= view.getLeft() && x <= view.getRight()
                && y >= view.getTop() && y <= view.getBottom();
    }

    public CameraTypeSelector(Context context) {
        super(context);

        pickerDividersPaint.setStyle(Paint.Style.STROKE);
        pickerDividersPaint.setStrokeCap(Paint.Cap.ROUND);
        pickerDividersPaint.setStrokeWidth(AndroidUtilities.dp(2));

        outlinePaint.setStyle(Paint.Style.STROKE);
        outlinePaint.setColor(ColorUtils.setAlphaComponent(Theme.getColor(Theme.key_switchTrack), 0x3F));

        preview = new FrameLayout(context) {
            @Override
            @SuppressLint("DrawAllocation")
            protected void onDraw(Canvas canvas) {
                super.onDraw(canvas);
                int color = Theme.getColor(Theme.key_switchTrack);
                int r = Color.red(color);
                int g = Color.green(color);
                int b = Color.blue(color);

                int offsetX = AndroidUtilities.dp(10);
                int minSidePadding = AndroidUtilities.dp(4);
                int halfPhoneWidth = AndroidUtilities.dp(90);

                int previewWidth = getMeasuredWidth();

                int maxHalfPhoneWidth = Math.max(AndroidUtilities.dp(40), previewWidth / 2 - minSidePadding);
                if (halfPhoneWidth > maxHalfPhoneWidth) {
                    halfPhoneWidth = maxHalfPhoneWidth;
                }

                int maxOffsetX = Math.max(0, previewWidth / 2 - halfPhoneWidth - minSidePadding);
                if (offsetX > maxOffsetX) {
                    offsetX = maxOffsetX;
                }

                int centerX = previewWidth / 2 - offsetX;
                int left = centerX - halfPhoneWidth;
                int right = centerX + halfPhoneWidth;
                int top = AndroidUtilities.dp(10);
                int bottom = getMeasuredHeight() - top;

                outlinePaint.setStrokeWidth(Math.max(2, AndroidUtilities.dp(1)));

                float stroke = outlinePaint.getStrokeWidth() / 2;

                final Rect rect1 = previewBounds;
                final ShapeDrawable phoneDrawable = previewShape;

                rect.set(left, top, right, bottom);
                rect.round(rect1);
                phoneDrawable.setBounds(rect1);
                phoneDrawable.getPaint().setColor(Color.argb(20, r, g, b));
                phoneDrawable.draw(canvas);

                rect.set(left + stroke, top + stroke, right - stroke, bottom - stroke);
                rect.round(rect1);
                phoneDrawable.setBounds(rect1);
                phoneDrawable.getPaint().set(outlinePaint);
                phoneDrawable.draw(canvas);

                final GradientDrawable gd = previewFade;
                previewGradient[0] = 0x00;
                previewGradient[1] = Theme.getColor(Theme.key_windowBackgroundWhite);
                gd.setOrientation(GradientDrawable.Orientation.TOP_BOTTOM);
                gd.setColors(previewGradient);
                gd.setCornerRadius(0f);
                gd.setBounds((int) (left - stroke), (int) (3 * getMeasuredHeight() / 4 - stroke), (int) (right + stroke), (int) (bottom + stroke));
                gd.draw(canvas);

                Drawable d = ContextCompat.getDrawable(context, icons[currentIcon]);
                int ICON_WIDTH = AndroidUtilities.dp(16 + 2 * progress);
                int iconOffsetY = AndroidUtilities.dp(10);
                d.setBounds(
                        centerX - ICON_WIDTH,
                        getMeasuredHeight() / 2 + iconOffsetY,
                        centerX + ICON_WIDTH,
                        getMeasuredHeight() / 2 + iconOffsetY + 2 * ICON_WIDTH
                );
                d.setColorFilter(new PorterDuffColorFilter(ColorUtils.setAlphaComponent(Theme.getColor(Theme.key_switchTrack), (int) (0x4F * progress)), PorterDuff.Mode.MULTIPLY));
                d.draw(canvas);

                gd.setOrientation(GradientDrawable.Orientation.TOP_BOTTOM);
                previewGradient[1] = Color.argb(30, r, g, b);
                gd.setColors(previewGradient);
                gd.setCornerRadius(AndroidUtilities.dp(25));

                Theme.dialogs_onlineCirclePaint.setColor(ColorUtils.setAlphaComponent(Theme.getColor(Theme.key_switchTrack), 0x3F));
                outlinePaint.setStrokeWidth(Math.max(3, AndroidUtilities.dp(1.5f)));

                cameraCluster.draw(
                        canvas,
                        left + AndroidUtilities.dp(16),
                        top + AndroidUtilities.dp(16),
                        color,
                        Math.max(AndroidUtilities.dp(1.5f), 3)
                );
            }
        };
        preview.setWillNotDraw(false);
        addView(preview, new LayoutParams(0, LayoutParams.MATCH_PARENT, 1.0f));

        numberPicker = new NumberPicker(context, 13) {
            @Override
            protected void onDraw(Canvas canvas) {
                super.onDraw(canvas);
                float y = AndroidUtilities.dp(31);
                pickerDividersPaint.setColor(Theme.getColor(Theme.key_radioBackgroundChecked));
                canvas.drawLine(AndroidUtilities.dp(2), y, getMeasuredWidth() - AndroidUtilities.dp(2), y, pickerDividersPaint);

                y = getMeasuredHeight() - AndroidUtilities.dp(31);
                canvas.drawLine(AndroidUtilities.dp(2), y, getMeasuredWidth() - AndroidUtilities.dp(2), y, pickerDividersPaint);
            }
        };

        numberPicker.setMinValue(0);
        numberPicker.setDrawDividers(false);
        numberPicker.setMaxValue(strings.length - 1);
        // Колесо должно ходить по кругу. Задать это до установки границ нельзя:
        // setMaxValue пересчитывает разрешение на круг и, не зная общего числа значений,
        // отключает его. Тогда колесо упирается в крайний пункт, палец продолжает вести —
        // и дальше едет уже список настроек под ним.
        numberPicker.setAllItemsCount(strings.length);
        numberPicker.setWrapSelectorWheel(true);
        numberPicker.setFormatter(value -> strings[value]);
        numberPicker.setOnValueChangedListener((picker, oldVal, newVal) -> {
            // Отклик на каждое деление — так и задумано: палец чувствует шаг колеса.
            picker.performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP, HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING);
            // А вот выбор камеры откладываем до остановки. Колесо проходит через все
            // промежуточные пункты, и раньше на каждом из них заново собирался весь экран
            // и пускалась волна по окну: за один пролистанный оборот их набиралось
            // несколько, они накладывались, и настройки на глазах плыли.
            pendingCamera = newVal;
            AndroidUtilities.cancelRunOnUIThread(applySelection);
            AndroidUtilities.runOnUIThread(applySelection, 250);
        });
        int selectedButton = FinegramCameraConfig.INSTANCE.getCameraType();
        numberPicker.setValue(selectedButton);
        addView(numberPicker, LayoutHelper.createFrame(132, 102, Gravity.RIGHT, 0, 33, 21, 33));
        updateIcon(false);
    }

    private static class CameraClusterDrawable {
        private final Paint outlinePaint;
        private final Paint fillPaint;
        private final GradientDrawable fillDrawable;

        private final int bigSize;
        private final int bigRadius;
        private final int bigSpacing;

        private final int smallSize;
        private final int smallRadius;
        private final int smallExtraOffset;

        private final int flashRadius;

        private final int clusterPadding;

        public CameraClusterDrawable() {
            outlinePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
            outlinePaint.setStyle(Paint.Style.STROKE);

            fillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
            fillPaint.setStyle(Paint.Style.FILL);

            fillDrawable = new GradientDrawable();
            fillDrawable.setShape(GradientDrawable.OVAL);

            bigSize = AndroidUtilities.dp(28);
            bigRadius = bigSize / 2;
            bigSpacing = AndroidUtilities.dp(35);

            smallSize = bigSize / 2;
            smallRadius = smallSize / 2;
            smallExtraOffset = AndroidUtilities.dp(5);

            flashRadius = smallRadius / 2;

            clusterPadding = AndroidUtilities.dp(7);
        }

        /** Прямоугольник блока камер: считается на каждом кадре, создаётся один раз. */
        private final RectF clusterRect = new RectF();

        public void draw(Canvas canvas, int left, int top, int color, float strokeWidth) {
            int r = Color.red(color);
            int g = Color.green(color);
            int b = Color.blue(color);

            outlinePaint.setColor(ColorUtils.setAlphaComponent(color, 0x3F));
            outlinePaint.setStrokeWidth(strokeWidth);

            fillPaint.setColor(Color.argb(15, r, g, b));
            fillDrawable.setColor(Color.argb(30, r, g, b));

            int bigX = left;
            int smallX = bigX + bigSize + AndroidUtilities.dp(10) + smallExtraOffset;
            int flashX = smallX + smallRadius;

            float cy1 = top + bigRadius;
            float cy2 = cy1 + bigSpacing;
            float cy3 = cy2 + bigSpacing;

            final RectF clusterRect = this.clusterRect;
            clusterRect.set(
                    bigX - clusterPadding,
                    cy1 - bigRadius - clusterPadding,
                    bigX + bigSize + clusterPadding,
                    cy3 + bigRadius + clusterPadding
            );
            float clusterCorner = clusterRect.width() / 2f;

            canvas.drawRoundRect(clusterRect, clusterCorner, clusterCorner, fillPaint);
            canvas.drawRoundRect(clusterRect, clusterCorner, clusterCorner, outlinePaint);

            drawLens(canvas, bigX, cy1, bigSize, bigRadius);
            drawLens(canvas, bigX, cy2, bigSize, bigRadius);
            drawLens(canvas, bigX, cy3, bigSize, bigRadius);

            drawModule(canvas, smallX, cy1, smallSize, smallRadius);
            drawModule(canvas, smallX, cy2, smallSize, smallRadius);

            float flashCy = (cy1 + cy2) / 2f;
            canvas.drawCircle(flashX, flashCy, flashRadius, outlinePaint);
        }

        private void drawModule(Canvas canvas, int x, float cy, int size, int radius) {
            fillDrawable.setBounds(
                    x,
                    (int) (cy - radius),
                    x + size,
                    (int) (cy + radius)
            );
            fillDrawable.draw(canvas);

            canvas.drawCircle(
                    x + radius,
                    cy,
                    radius,
                    outlinePaint
            );
        }

        private void drawLens(Canvas canvas, int x, float cy, int size, int radius) {
            canvas.drawCircle(
                    x + radius,
                    cy,
                    radius,
                    outlinePaint
            );
        }
    }

    /** Пункт, на котором остановилось колесо; применяем его, когда оно замрёт. */
    private int pendingCamera = -1;

    private final Runnable applySelection = new Runnable() {
        @Override
        public void run() {
            if (pendingCamera < 0) return;
            final int selected = pendingCamera;
            pendingCamera = -1;
            onSelectedCamera(selected);
            updateIcon(true);
        }
    };

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        // Экран могли закрыть, пока колесо доезжало: применять выбор уже некуда.
        AndroidUtilities.cancelRunOnUIThread(applySelection);
        if (animator != null) {
            animator.cancel();
            animator = null;
        }
    }

    protected void onSelectedCamera(int cameraSelected) {
    }

    public void updateIcon(boolean animate) {
        if (animator != null) {
            // предыдущий ход не остановили — два хода меняли одно значение наперегонки
            animator.removeAllListeners();
            animator.cancel();
            animator = null;
        }
        if (animate) {
            animator = ValueAnimator.ofFloat(1f, 0f).setDuration(100);
            animator.setInterpolator(Easings.easeInOutQuad);
            animator.addUpdateListener(animation -> {
                progress = (Float) animation.getAnimatedValue();
                preview.invalidate();
            });
            animator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    super.onAnimationEnd(animation);
                    currentIcon = FinegramCameraConfig.INSTANCE.getCameraType();
                    animator.setFloatValues(0f, 1f);
                    animator.removeAllListeners();
                    animator.start();
                }
            });
            animator.start();
        } else {
            progress = 1f;
            preview.invalidate();
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(MeasureSpec.makeMeasureSpec(MeasureSpec.getSize(widthMeasureSpec), MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(AndroidUtilities.dp(168), MeasureSpec.EXACTLY));
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (numberPicker.getValue() == 1) {
            canvas.drawLine(AndroidUtilities.dp(8), getMeasuredHeight() - 1, getMeasuredWidth() - AndroidUtilities.dp(8), getMeasuredHeight() - 1, Theme.dividerPaint);
        }
    }
}

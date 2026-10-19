/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.drawer;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.FrameLayout;

import androidx.dynamicanimation.animation.FloatPropertyCompat;
import androidx.dynamicanimation.animation.SpringAnimation;
import androidx.dynamicanimation.animation.SpringForce;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.ui.ActionBar.Theme;

/**
 * Боковое меню: панель, выезжающая от края поверх списка чатов.
 *
 * Держит три вещи: панель с содержимым, затемнение под ней и жест, которым её тянут.
 *
 * Положение задаётся одним числом от нуля до единицы. От него считаются сдвиг панели,
 * плотность затемнения и сдвиг того, что лежит под ней. Одно число на всё, чтобы жест и
 * анимация не спорили за состояние.
 */
public class FGDrawer extends FrameLayout {

    /** Наибольшая ширина панели; на узких экранах оставляем край содержимого видимым. */
    private static final int MAX_WIDTH_DP = 300;
    private static final int EDGE_GAP_DP = 56;
    /** Скругление у внешнего края панели. */
    private static final int CORNER_DP = 20;
    /** Насколько затемняется то, что под панелью. */
    private static final int SCRIM_ALPHA = 102;

    private final FrameLayout panel;
    private final Paint scrimPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path clipPath = new Path();
    private final RectF clipRect = new RectF();
    private final float[] corners = new float[8];

    private float progress;
    private SpringAnimation spring;

    private View contentBehind;

    private VelocityTracker tracker;
    private boolean dragging;
    private float startX;
    private float startY;
    private float startProgress;
    private final int touchSlop;

    public FGDrawer(Context context) {
        super(context);
        touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        setWillNotDraw(false);
        setVisibility(GONE);

        panel = new FrameLayout(context);
        addView(panel, new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT));
    }

    /** Куда складывать содержимое меню: шапку и список пунктов. */
    public FrameLayout getPanel() {
        return panel;
    }

    /** Вид, который лежит под панелью: он слегка отъезжает, пока она открывается. */
    public void setContentBehind(View view) {
        contentBehind = view;
    }

    public boolean isOpened() {
        return progress > 0.5f;
    }

    private int panelWidth() {
        // Скрытую панель ни разу не мерили: в начале жеста берём ширину родителя.
        int width = getMeasuredWidth();
        if (width == 0 && getParent() instanceof View) {
            width = ((View) getParent()).getWidth();
        }
        return Math.min(AndroidUtilities.dp(MAX_WIDTH_DP),
                Math.max(AndroidUtilities.dp(MAX_WIDTH_DP) / 2,
                        width - AndroidUtilities.dp(EDGE_GAP_DP)));
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        panel.measure(
                MeasureSpec.makeMeasureSpec(panelWidth(), MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(getMeasuredHeight(), MeasureSpec.EXACTLY));
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        final int width = panelWidth();
        if (LocaleController.isRTL) {
            panel.layout(getMeasuredWidth() - width, 0, getMeasuredWidth(), getMeasuredHeight());
        } else {
            panel.layout(0, 0, width, getMeasuredHeight());
        }
        applyProgress();
    }

    /** Раскладывает текущее положение по всем, кого оно касается. */
    private void applyProgress() {
        final int width = panelWidth();
        final boolean rtl = LocaleController.isRTL;
        final float hidden = (1f - progress) * width;
        panel.setTranslationX(rtl ? hidden : -hidden);
        if (contentBehind != null) {
            // Содержимое отходит на треть ширины: полный сдвиг съедает экран, а без
            // движения вовсе панель выглядит наклейкой поверх.
            final float push = progress * width * 0.3f;
            contentBehind.setTranslationX(rtl ? -push : push);
        }
        setVisibility(progress <= 0.001f ? GONE : VISIBLE);
        invalidate();
    }

    private void setProgress(float value) {
        final boolean wasClosed = progress <= 0.001f;
        progress = Math.max(0f, Math.min(1f, value));
        if (wasClosed && progress > 0.001f && onOpening != null) {
            // Панель начала выезжать — неважно, по кнопке или пальцем. Содержимое надо
            // пересобрать именно сейчас: за время, пока она была закрыта, могло смениться
            // что угодно, вплоть до аккаунта.
            onOpening.run();
        }
        applyProgress();
    }

    /** Что сделать, когда панель начинает открываться. */
    public void setOnOpening(Runnable listener) {
        onOpening = listener;
    }

    private Runnable onOpening;

    private static final FloatPropertyCompat<FGDrawer> PROGRESS =
            new FloatPropertyCompat<FGDrawer>("drawerProgress") {
                @Override
                public float getValue(FGDrawer drawer) {
                    return drawer.progress * 100f;
                }

                @Override
                public void setValue(FGDrawer drawer, float value) {
                    drawer.setProgress(value / 100f);
                }
            };

    /**
     * Доводит панель до края.
     *
     * Пружина, а не отсчёт по времени: если человек отпустил палец на полпути, движение
     * продолжается с той же скоростью, с какой он вёл, и не выглядит рывком.
     */
    private void springTo(boolean open, float velocityPerSecond) {
        if (spring != null) {
            spring.cancel();
        }
        spring = new SpringAnimation(this, PROGRESS)
                .setSpring(new SpringForce(open ? 100f : 0f)
                        .setStiffness(900f)
                        .setDampingRatio(SpringForce.DAMPING_RATIO_NO_BOUNCY));
        if (velocityPerSecond != 0) {
            spring.setStartVelocity(velocityPerSecond / Math.max(1, panelWidth()) * 100f);
        }
        spring.start();
    }

    public void open() {
        springTo(true, 0);
    }

    public void close() {
        springTo(false, 0);
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        if (progress > 0.001f) {
            scrimPaint.setColor(Color.BLACK);
            scrimPaint.setAlpha((int) (SCRIM_ALPHA * progress));
            canvas.drawRect(0, 0, getMeasuredWidth(), getMeasuredHeight(), scrimPaint);
        }
        super.dispatchDraw(canvas);
    }

    @Override
    protected boolean drawChild(Canvas canvas, View child, long drawingTime) {
        if (child != panel) {
            return super.drawChild(canvas, child, drawingTime);
        }
        // Скругляем только внешний край: со стороны экрана панель прилегает вплотную.
        final float r = com.th3nekit.finegram.core.configs.FinegramAppearanceConfig.INSTANCE.getSideDrawerRounded()
                ? AndroidUtilities.dp(CORNER_DP) : 0f;
        final boolean rtl = LocaleController.isRTL;
        corners[0] = corners[1] = rtl ? r : 0f;
        corners[2] = corners[3] = rtl ? 0f : r;
        corners[4] = corners[5] = rtl ? 0f : r;
        corners[6] = corners[7] = rtl ? r : 0f;
        clipRect.set(panel.getLeft() + panel.getTranslationX(), panel.getTop(),
                panel.getRight() + panel.getTranslationX(), panel.getBottom());
        clipPath.rewind();
        clipPath.addRoundRect(clipRect, corners, Path.Direction.CW);
        canvas.save();
        canvas.clipPath(clipPath);
        final boolean result = super.drawChild(canvas, child, drawingTime);
        canvas.restore();

        // Край панели. На чёрной теме она одного цвета с тем, поверх чего выехала, и без
        // черты по внешнему краю неясно, где панель кончается. Черта только по внешнему
        // краю: обводка целиком ложилась и на верх с низом, прямо под часы и жесты.
        edgePaint.setColor(androidx.core.graphics.ColorUtils.setAlphaComponent(
                Theme.getColor(Theme.key_windowBackgroundWhiteBlackText), 30));
        edgePaint.setStrokeWidth(Math.max(1, AndroidUtilities.dp(0.66f)));
        edgePath.rewind();
        final float edgeX = rtl ? clipRect.left : clipRect.right;
        edgePath.moveTo(edgeX, clipRect.top + r);
        edgePath.lineTo(edgeX, clipRect.bottom - r);
        canvas.drawPath(edgePath, edgePaint);
        return result;
    }

    private final Paint edgePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path edgePath = new Path();

    {
        edgePaint.setStyle(Paint.Style.STROKE);
    }

    /**
     * Жест открытия ведёт список чатов: закрытая панель скрыта целиком и касаний не ловит,
     * иначе она отбирала бы их у экрана под собой. Список замечает движение вправо и
     * передаёт его сюда.
     */
    public void beginSwipe() {
        if (spring != null) {
            spring.cancel();
        }
        swipeStartProgress = progress;
    }

    public void swipeBy(float dx) {
        final float width = Math.max(1, panelWidth());
        setProgress(swipeStartProgress + (LocaleController.isRTL ? -dx : dx) / width);
    }

    public void endSwipe(float velocityX) {
        final float velocity = LocaleController.isRTL ? -velocityX : velocityX;
        final boolean open = Math.abs(velocity) > AndroidUtilities.dp(400)
                ? velocity > 0
                : progress > 0.35f;
        springTo(open, velocity);
    }

    private float swipeStartProgress;

    /**
     * Открытая панель забирает касания везде: по ней самой листают и нажимают, по
     * затемнению — закрывают тычком или смахиванием.
     */
    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            startX = event.getX();
            startY = event.getY();
            startProgress = progress;
            dragging = false;
            return false;
        }
        if (action != MotionEvent.ACTION_MOVE) {
            return false;
        }
        final float dx = event.getX() - startX;
        final float dy = event.getY() - startY;
        // Горизонтальное движение — наше; вертикальное оставляем списку пунктов.
        return Math.abs(dx) > touchSlop && Math.abs(dx) > Math.abs(dy);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        if (tracker == null) {
            tracker = VelocityTracker.obtain();
        }
        tracker.addMovement(event);

        final int width = Math.max(1, panelWidth());
        final boolean rtl = LocaleController.isRTL;

        switch (action) {
            case MotionEvent.ACTION_DOWN:
                startX = event.getX();
                startY = event.getY();
                startProgress = progress;
                dragging = false;
                return progress > 0.001f;
            case MotionEvent.ACTION_MOVE: {
                final float dx = event.getX() - startX;
                if (!dragging && Math.abs(dx) > touchSlop) {
                    dragging = true;
                    if (spring != null) {
                        spring.cancel();
                    }
                }
                if (dragging) {
                    setProgress(startProgress + (rtl ? -dx : dx) / width);
                }
                return true;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL: {
                tracker.computeCurrentVelocity(1000);
                final float velocity = rtl ? -tracker.getXVelocity() : tracker.getXVelocity();
                tracker.recycle();
                tracker = null;
                if (!dragging) {
                    // Короткий тычок мимо панели — просьба её убрать.
                    final boolean onPanel = rtl
                            ? startX > getMeasuredWidth() - width
                            : startX < width;
                    if (action == MotionEvent.ACTION_UP && progress > 0.5f && !onPanel) {
                        close();
                    }
                    return true;
                }
                dragging = false;
                // Решаем по скорости, а при медленном движении — по пройденному пути.
                final boolean open = Math.abs(velocity) > AndroidUtilities.dp(400)
                        ? velocity > 0
                        : progress > 0.5f;
                springTo(open, velocity);
                return true;
            }
            default:
                return true;
        }
    }
}

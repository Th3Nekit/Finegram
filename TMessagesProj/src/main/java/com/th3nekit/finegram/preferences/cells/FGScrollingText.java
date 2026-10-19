/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.preferences.cells;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.content.Context;
import android.widget.TextView;

import androidx.annotation.NonNull;

import org.telegram.messenger.AndroidUtilities;

/**
 * Строка, которая ездит туда и обратно, когда текст не помещается.
 *
 * Обычная бегущая строка Android уводит текст за левый край и начинает сначала: конец и
 * начало никогда не видны рядом, а глаз каждый раз ловит рывок возврата. Здесь текст
 * доезжает до конца, останавливается, и так же неспешно возвращается — читать длинное
 * название плагина этим способом можно, не дожидаясь нового круга.
 *
 * Пока текст помещается, вид ведёт себя как обычная надпись: ничего не считает и не
 * анимирует.
 */
public class FGScrollingText extends TextView {

    /** Сколько точек в секунду проезжает текст. Спокойнее шага чтения. */
    private static final float SPEED_DP_PER_SECOND = 28f;

    /** Сколько стоим на каждом краю, прежде чем поехать обратно. */
    private static final long EDGE_PAUSE_MS = 1200;

    private ValueAnimator animator;
    private int overflow;

    // Ссылки на метод создают новый объект при каждом упоминании, и отменить по ним ничего
    // нельзя — держим по одному экземпляру на каждое направление.
    private final Runnable forward = this::rideForward;
    private final Runnable back = this::rideBack;

    public FGScrollingText(@NonNull Context context) {
        super(context);
        setSingleLine(true);
        setHorizontallyScrolling(true);
        // Многоточие здесь только на случай, когда ездить нечему: например, вид ещё не
        // измерен, а текст уже надо чем-то ограничить.
        setEllipsize(null);
    }

    /**
     * Занимаем ровно то место, что дали.
     *
     * Горизонтальная прокрутка заставляет TextView просить ширину всего текста. В ячейке
     * это разносит раскладку: строка растягивает её, и переключатель с кнопкой меню уезжают
     * за край — соседние строки перестают стоять по одной вертикали.
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        int available = MeasureSpec.getSize(widthMeasureSpec);
        if (MeasureSpec.getMode(widthMeasureSpec) != MeasureSpec.UNSPECIFIED
                && getMeasuredWidth() > available) {
            setMeasuredDimension(available, getMeasuredHeight());
        }
    }

    @Override
    public void setText(CharSequence text, BufferType type) {
        super.setText(text, type);
        post(this::restart);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        post(this::restart);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        restart();
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        stop();
    }

    /** Заново считает, надо ли ездить, и запускает движение. */
    private void restart() {
        stop();
        scrollTo(0, 0);
        int visible = getWidth() - getPaddingLeft() - getPaddingRight();
        if (visible <= 0 || getLayout() == null) {
            return;
        }
        overflow = (int) Math.ceil(getLayout().getLineWidth(0)) - visible;
        if (overflow <= dp(2)) {
            overflow = 0;
            return;
        }
        AndroidUtilities.runOnUIThread(forward, EDGE_PAUSE_MS);
    }

    private void rideForward() {
        ride(0, overflow, back);
    }

    private void rideBack() {
        ride(overflow, 0, forward);
    }

    private void ride(int from, int to, Runnable next) {
        if (overflow <= 0 || !isAttachedToWindow()) {
            return;
        }
        stop();
        long duration = (long) (Math.abs(to - from) / (float) dp(SPEED_DP_PER_SECOND) * 1000);
        animator = ValueAnimator.ofInt(from, to);
        animator.setDuration(Math.max(300, duration));
        animator.addUpdateListener(a -> scrollTo((int) a.getAnimatedValue(), 0));
        animator.addListener(new AnimatorListenerAdapter() {
            private boolean cancelled;

            @Override
            public void onAnimationCancel(Animator animation) {
                cancelled = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                animator = null;
                if (!cancelled && isAttachedToWindow()) {
                    AndroidUtilities.runOnUIThread(next, EDGE_PAUSE_MS);
                }
            }
        });
        animator.start();
    }

    private void stop() {
        AndroidUtilities.cancelRunOnUIThread(forward);
        AndroidUtilities.cancelRunOnUIThread(back);
        if (animator != null) {
            animator.cancel();
            animator = null;
        }
    }
}

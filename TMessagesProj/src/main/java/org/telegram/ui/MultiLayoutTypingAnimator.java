package org.telegram.ui;

import static org.telegram.messenger.AndroidUtilities.dp;
import static org.telegram.messenger.AndroidUtilities.lerp;

import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Shader;
import android.text.Layout;
import android.view.Choreographer;
import android.view.View;

import androidx.core.math.MathUtils;

import java.util.ArrayList;
import java.util.List;

public final class MultiLayoutTypingAnimator implements Choreographer.FrameCallback {

    public interface Block {

        Layout getLayout();

        View getParentView();
    }

    public static final float MIN_SPEED_DP_PER_SEC = 40f;

    public static final float TARGET_DURATION_SEC = 1.05f;

    public static final float FADE_IN_SEC = 0.2f;

    private static final float EPS = 0.001f;

    private View invalidateTarget;
    private final Choreographer choreo = Choreographer.getInstance();

    private List<? extends Block> blocks = new ArrayList<Block>();

    private final ArrayList<Float> blockAlphas = new ArrayList<>();

    private int   curBlockIdx = 0;
    private int   curLineIdx  = 0;
    private float xPosition   = 0f;

    private boolean running   = false;
    private boolean finished  = true;
    private long lastFrameNs  = 0L;

    private float speedPxPerSec = dp(MIN_SPEED_DP_PER_SEC);

    public void setInvalidateTarget(View invalidateTarget) {
        this.invalidateTarget = invalidateTarget;
    }

    private View lastInvalidatedView;

    private void applyBlockAlphas(float dtSec) {

        View prev = null;
        for (int i = 0, n = blocks.size(); i < n; i++) {
            final Block b = blocks.get(i);

            final float target = (finished || i <= curBlockIdx) ? 1f : 0f;
            final float cur = i < blockAlphas.size() ? blockAlphas.get(i) : target;
            final float next;
            if (cur == target) {
                next = cur;
            } else if (dtSec <= 0f || FADE_IN_SEC <= 0f) {
                next = target;
            } else {
                final float step = dtSec / FADE_IN_SEC;
                next = target > cur ? Math.min(target, cur + step) : Math.max(target, cur - step);
            }
            if (i < blockAlphas.size() && next != cur) {
                blockAlphas.set(i, next);
            }

            final View v = b.getParentView();
            if (v == null || v == prev) continue;
            prev = v;
            if (v.getAlpha() != next) v.setAlpha(next);
        }
    }

    private void resetBlockAlphas() {
        for (int i = 0, n = blockAlphas.size(); i < n; i++) {
            blockAlphas.set(i, 1f);
        }
        View prev = null;
        for (int i = 0, n = blocks.size(); i < n; i++) {
            final View v = blocks.get(i).getParentView();
            if (v == null || v == prev) continue;
            prev = v;
            if (v.getAlpha() != 1f) v.setAlpha(1f);
        }
    }

    public float getBlockAlpha(Block block) {
        final int idx = indexOf(block);
        if (idx < 0 || idx >= blockAlphas.size()) return 1f;
        return blockAlphas.get(idx);
    }

    private void invalidate() {
        View v = null;
        if (curBlockIdx >= 0 && curBlockIdx < blocks.size()) {
            final Block b = blocks.get(curBlockIdx);
            if (b != null) v = b.getParentView();
        }
        if (v != null) {
            v.invalidate();

            if (lastInvalidatedView != null && lastInvalidatedView != v) {
                lastInvalidatedView.invalidate();
            }
            lastInvalidatedView = v;
        } else if (invalidateTarget != null) {
            invalidateTarget.invalidate();
        }
    }

    public void setBlocks(List<? extends Block> newBlocks) {
        if (!blocks.isEmpty() && curBlockIdx >= blocks.size()) {
            curBlockIdx = blocks.size() - 1;
            final Layout l = blocks.get(curBlockIdx).getLayout();
            curLineIdx = Math.max(0, l == null ? 0 : l.getLineCount() - 1);
            xPosition = l == null ? 0 : l.getLineWidth(curLineIdx);
        }

        this.blocks = (newBlocks != null) ? newBlocks : new ArrayList<Block>();

        recalcSpeed();

        finished = isAtAbsoluteEnd();

        while (blockAlphas.size() > this.blocks.size()) {
            blockAlphas.remove(blockAlphas.size() - 1);
        }
        for (int i = blockAlphas.size(); i < this.blocks.size(); i++) {
            blockAlphas.add((finished || i <= curBlockIdx) ? 1f : 0f);
        }

        if (!finished && !running) start();

        applyBlockAlphas(0f);
        invalidate();
    }

    public void reset() {
        curBlockIdx = 0;
        curLineIdx  = 0;
        xPosition   = 0f;
        finished    = blocks.isEmpty();
        speedPxPerSec = dp(MIN_SPEED_DP_PER_SEC);
        applyBlockAlphas(0f);
        invalidate();
    }

    public void start() {
        if (running) return;
        running = true;
        if (isAtAbsoluteEnd()) finished = true;
        lastFrameNs = 0L;
        choreo.postFrameCallback(this);
    }

    public void stop() {
        if (!running) return;
        running = false;
        choreo.removeFrameCallback(this);
        lastFrameNs = 0L;
        resetBlockAlphas();
        if (onFinishRunnable != null) {
            onFinishRunnable.run();
            onFinishRunnable = null;
        }
    }

    public boolean isRunning()  { return running; }
    public boolean isFinished() { return finished; }

    private Runnable onFinishRunnable;

    public void setOnFinishListener(Runnable onFinishRunnable) {
        this.onFinishRunnable = onFinishRunnable;
    }

    public int getCurrentBlockIndex() { return curBlockIdx; }

    public int   getFadeLineIndex(Block block) {
        return isFadeBlock(block) ? curLineIdx : -1;
    }
    public float getFadeXPosition(Block block) {
        return isFadeBlock(block) ? xPosition : 0f;
    }

    public boolean needDraw(Block block) {
        int idx = indexOf(block);
        if (idx < 0 || blocks.isEmpty()) return false;

        if (idx < curBlockIdx) return true;
        if (idx > curBlockIdx) return false;

        return true;
    }

    public boolean isFadeBlock(Block block) {
        int idx = indexOf(block);
        if (idx != curBlockIdx) return false;
        Layout l = block.getLayout();
        if (l == null || curLineIdx >= l.getLineCount()) return false;
        return true;
    }

    @Override public void doFrame(long frameTimeNanos) {
        if (!running) return;

        float dt = 0f;
        if (lastFrameNs != 0L) {
            dt = (frameTimeNanos - lastFrameNs) * 1e-9f;
            advance(dt);
        }
        lastFrameNs = frameTimeNanos;
        applyBlockAlphas(dt);
        invalidate();

        if (finished) {
            running = false;
            resetBlockAlphas();
            if (onFinishRunnable != null) {
                onFinishRunnable.run();
                onFinishRunnable = null;
            }
            return;
        }
        choreo.postFrameCallback(this);
    }

    private void advance(float dtSec) {
        if (blocks.isEmpty() || dtSec <= 0f) {
            finished = blocks.isEmpty();
            return;
        }

        float delta = speedPxPerSec * dtSec;

        while (delta > 0f) {
            if (curBlockIdx >= blocks.size()) { finished = true; break; }
            Layout lay = blocks.get(curBlockIdx).getLayout();
            if (lay == null || lay.getLineCount() == 0) {

                curBlockIdx++;
                curLineIdx = 0;
                xPosition = 0f;
                continue;
            }

            if (curLineIdx >= lay.getLineCount()) {

                curLineIdx = lay.getLineCount() - 1;
                xPosition = lineWidth(lay, curLineIdx);
            }

            float w = lineWidth(lay, curLineIdx);
            if (w <= EPS) {

                if (!nextLineOrBlock(lay)) continue;
                else break;
            }

            float remain = w - xPosition;
            if (remain <= EPS) {

                if (!nextLineOrBlock(lay)) continue;
                else break;
            }

            float step = (delta < remain) ? delta : remain;
            xPosition += step;
            delta -= step;

            if (w - xPosition <= EPS) {
                if (!nextLineOrBlock(lay)) {
                    delta = 0f;
                }
            }
        }

        finished = isAtAbsoluteEnd();
    }

    private boolean nextLineOrBlock(Layout lay) {
        curLineIdx++;
        xPosition = 0f;
        if (curLineIdx >= lay.getLineCount()) {
            curBlockIdx++;
            curLineIdx = 0;
            xPosition = 0f;
            return curBlockIdx >= blocks.size();
        }
        return false;
    }

    private void recalcSpeed() {
        float remainingPx = computeRemainingPixels();
        float minPxPerSec = dp(MIN_SPEED_DP_PER_SEC);

        if (remainingPx <= EPS) {
            speedPxPerSec = minPxPerSec;
            return;
        }

        float desired = remainingPx / TARGET_DURATION_SEC;
        speedPxPerSec = Math.max(minPxPerSec, desired);
    }

    private float computeRemainingPixels() {
        if (blocks.isEmpty()) return 0f;

        float total = 0f;

        for (int bi = curBlockIdx; bi < blocks.size(); bi++) {
            Layout l = blocks.get(bi).getLayout();
            if (l == null) continue;

            int startLine = 0;
            if (bi == curBlockIdx) {
                startLine = Math.min(Math.max(curLineIdx, 0), Math.max(0, l.getLineCount() - 1));
            }

            for (int li = startLine; li < l.getLineCount(); li++) {
                float w = lineWidth(l, li);
                if (w <= EPS) continue;

                if (bi == curBlockIdx && li == startLine) {
                    float remain = w - xPosition;
                    if (remain > EPS) total += remain;
                } else {
                    total += w;
                }
            }
        }

        return total;
    }

    private boolean isAtAbsoluteEnd() {
        if (blocks.isEmpty()) return true;
        int lastIdx = blocks.size() - 1;

        int i = lastIdx;
        Layout lay = null;
        for (; i >= 0; i--) {
            lay = blocks.get(i).getLayout();
            if (lay != null && lay.getLineCount() > 0) break;
        }
        if (i < 0 || lay == null) return true;

        if (curBlockIdx < i) return false;
        if (curBlockIdx > i) return true;

        int lastLine = lay.getLineCount() - 1;
        float w = lineWidth(lay, lastLine);
        if (curLineIdx < lastLine) return false;
        return xPosition >= w - EPS;
    }

    private float lineWidth(Layout l, int line) {
        float w = l.getLineRight(line) - l.getLineLeft(line);
        return (w >= 0f) ? w : -w;
    }

    public int indexOf(Block block) {
        for (int i = 0, n = blocks.size(); i < n; i++) {
            if (blocks.get(i) == block) return i;
        }
        return -1;
    }

    private static final float FADE_WIDTH_DP = 50f;
    private static final Paint MASK_PAINT;
    private static final LinearGradient GRADIENT;
    private static final Matrix GRAD_MTX = new Matrix();

    static {
        MASK_PAINT = new Paint(Paint.ANTI_ALIAS_FLAG);
        MASK_PAINT.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.DST_IN));
        GRADIENT = new LinearGradient(0f, 0f, 1f, 0f, 0xFFFFFFFF, 0x00FFFFFF, Shader.TileMode.CLAMP);
        MASK_PAINT.setShader(GRADIENT);
    }

    public interface Renderer {
        void draw(Canvas canvas);
    }

    public static void drawLayoutWithLastLineFade(
            Canvas canvas,
            Layout layout,
            int lineIndex,
            float xPosition
    ) {
        drawLayoutWithLastLineFade(canvas, layout, lineIndex, xPosition, null);
    }

    public static void drawLayoutWithLastLineFade(
            Canvas canvas,
            Layout layout,
            int lineIndex,
            float xPosition,
            Renderer drawer
    ) {
        if (layout == null) return;
        final int lineCount = layout.getLineCount();
        if (lineIndex < 0 || lineIndex >= lineCount) return;

        final Renderer paint = drawer != null ? drawer : layout::draw;

        final int width  = layout.getWidth();
        final int height = layout.getHeight();

        final int topLine    = layout.getLineTop(lineIndex);
        final int bottomLine = layout.getLineBottom(lineIndex);

        if (topLine > 0) {
            canvas.save();
            canvas.clipRect(0f, 0f, width, topLine);
            paint.draw(canvas);
            canvas.restore();
        }

        final float rawL = layout.getLineLeft(lineIndex);
        final float rawR = layout.getLineRight(lineIndex);
        final float lineL = Math.min(rawL, rawR);
        final float lineR = Math.max(rawL, rawR);
        if (lineR <= lineL) return;

        final int dir = layout.getParagraphDirection(lineIndex);
        final float lineW = (lineR - lineL);

        final float x = MathUtils.clamp(xPosition, 0f, lineW);
        final float position = x / lineW;

        if (x <= 0f) {
            return;
        }
        if (x >= lineW) {
            canvas.save();
            canvas.clipRect(0f, topLine, width, bottomLine);
            paint.draw(canvas);
            canvas.restore();
            return;
        }

        final float gradX1 = lerp(-dp(FADE_WIDTH_DP), lineW, position);
        final float gradX2 = gradX1 + dp(FADE_WIDTH_DP);

        final int sc = canvas.saveLayer(lineL, topLine, lineR, bottomLine, null);

        canvas.save();
        canvas.clipRect(lineL, topLine, lineR, bottomLine);
        paint.draw(canvas);
        canvas.restore();

        GRAD_MTX.reset();
        if (dir >= 0) {
            GRAD_MTX.setScale(dp(FADE_WIDTH_DP), 1f);
            GRAD_MTX.postTranslate(gradX1, 0f);
        } else {
            GRAD_MTX.setScale(-dp(FADE_WIDTH_DP), 1f);
            GRAD_MTX.postTranslate(lineW - gradX1, 0f);
        }
        GRADIENT.setLocalMatrix(GRAD_MTX);

        canvas.drawRect(lineL, topLine, lineR, bottomLine, MASK_PAINT);
        canvas.restoreToCount(sc);

    }

    public static void drawHorizontalFadeMask(
            Canvas canvas,
            float left, float top, float right, float bottom,
            float lineL, float lineW, float xPosition, int dir
    ) {
        if (lineW <= 0f) return;
        final float x = MathUtils.clamp(xPosition, 0f, lineW);
        final float position = x / lineW;
        final float gradX1 = lerp(-dp(FADE_WIDTH_DP), lineW, position);

        GRAD_MTX.reset();
        if (dir >= 0) {
            GRAD_MTX.setScale(dp(FADE_WIDTH_DP), 1f);
            GRAD_MTX.postTranslate(gradX1 + lineL, 0f);
        } else {
            GRAD_MTX.setScale(-dp(FADE_WIDTH_DP), 1f);
            GRAD_MTX.postTranslate(lineL + lineW - gradX1, 0f);
        }
        GRADIENT.setLocalMatrix(GRAD_MTX);

        canvas.drawRect(left, top, right, bottom, MASK_PAINT);
    }
}

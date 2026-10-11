package org.telegram.ui;

import static org.telegram.messenger.AndroidUtilities.dp;
import static org.telegram.messenger.AndroidUtilities.lerp;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewConfiguration;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.dynamicanimation.animation.DynamicAnimation;
import androidx.dynamicanimation.animation.FloatPropertyCompat;
import androidx.dynamicanimation.animation.SpringAnimation;
import androidx.dynamicanimation.animation.SpringForce;

import android.util.Log;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.AnimatedLinearLayout;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.glass.GlassTabView;

import me.vkryl.android.animator.BoolAnimator;
import me.vkryl.android.animator.ListAnimator;

import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig;
import java.util.HashMap;

@SuppressLint("ViewConstructor")
public class MainTabsLayout extends AnimatedLinearLayout {

    private final Theme.ResourcesProvider resourcesProvider;

    public MainTabsLayout(Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;
    }

    private static final float[] PASS_TEXT_SIZES_DP = {12f, 12f, 10f};
    private static final int[] PASS_PADDINGS_DP = {16, 8, 4};

    private int maxWidthPx;

    public void setMaxWidth(int maxWidthPx) {
        if (this.maxWidthPx != maxWidthPx) {
            this.maxWidthPx = maxWidthPx;
            requestLayout();
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = MeasureSpec.getSize(widthMeasureSpec);
        final int height = MeasureSpec.getSize(heightMeasureSpec);
        final int tabHeight = height - getPaddingTop() - getPaddingBottom();
        final int tabWidth = dp(FinegramAppearanceConfig.INSTANCE.getShowMainTabsTitle() ? 24 : 12);

        if (maxWidthPx > 0 && width > maxWidthPx) {
            width = maxWidthPx;
        }

        int screenWidth = getResources().getDisplayMetrics().widthPixels;
        int searchOffset = dp(DialogsActivity.MAIN_TABS_HEIGHT_WITH_MARGINS);
        int absoluteMaxLimit = screenWidth - searchOffset - dp(DialogsActivity.MAIN_TABS_MARGIN * 2);

        final int maxTotalWidthForTabs = Math.max(width, absoluteMaxLimit) - getPaddingLeft() - getPaddingRight();
        final int minTotalWidthForTabs = Math.min(dp(320) / 2, maxTotalWidthForTabs);

        int chosenPass = PASS_TEXT_SIZES_DP.length - 1;
        float lastMeasuredTextSize = -1;
        for (int pass = 0; pass < PASS_TEXT_SIZES_DP.length; pass++) {
            if (PASS_TEXT_SIZES_DP[pass] != lastMeasuredTextSize) {
                measureTabTexts(PASS_TEXT_SIZES_DP[pass]);
                lastMeasuredTextSize = PASS_TEXT_SIZES_DP[pass];
            }
            final int padding = dp(PASS_PADDINGS_DP[pass]);
            float total = 0;
            for (int a = 0, N = getChildCount(); a < N; a++) {
                if (!isViewVisible(getChildAt(a))) continue;
                final float withMargin = tabsTextWidth[a] + padding * 2;
                total += withMargin;
            }
            final boolean fits = total <= maxTotalWidthForTabs;
            if (fits || pass == PASS_TEXT_SIZES_DP.length - 1) {
                chosenPass = pass;
                break;
            }
        }

        applyPassTextSize(chosenPass);

        final int tabPadding = dp(PASS_PADDINGS_DP[chosenPass]);
        final int maxTabTextWidthIfEq = (maxTotalWidthForTabs / Math.max(1, visibleChildCount)) - tabPadding * 2;

        float totalWidth = 0;
        int totalWeight = 0;
        for (int a = 0, N = getChildCount(); a < N; a++) {
            final View child = getChildAt(a);
            if (!isViewVisible(child)) {
                tabsTextWidth[a] = tabsTextWidthWithMargin[a] = 0;
                tabsWeight[a] = 0;
                continue;
            }

            float baseTabWidth = tabWidth + dp(12) * 2;
            final float w = Math.max(tabsTextWidth[a], baseTabWidth);

            if (w > maxTabTextWidthIfEq) {
                tabsTextWidthWithMargin[a] = w + dp(13) * 2;
            } else {
                tabsTextWidthWithMargin[a] = w + dp(16) * 2;
            }

            tabsWeight[a] = 1;
            totalWidth += tabsTextWidthWithMargin[a];
            totalWeight += tabsWeight[a];
        }

        if (totalWeight == 0) {
            for (int a = 0, N = getChildCount(); a < N; a++) {
                tabsWeight[a] = isViewVisible(getChildAt(a)) ? 1 : 0;
            }
            totalWeight = visibleChildCount;
        }

        if (totalWidth > maxTotalWidthForTabs) {
            final float m = maxTotalWidthForTabs / totalWidth;
            for (int a = 0, N = getChildCount(); a < N; a++) {
                tabsTextWidthWithMargin[a] *= m;
            }
        } else if (totalWidth < minTotalWidthForTabs) {

            float targetWidth;

            if (visibleChildCount == 1) {
                targetWidth = totalWidth + tabWidth;
                targetWidth = Math.min(targetWidth, maxTotalWidthForTabs * 0.5f);
            } else {
                targetWidth = minTotalWidthForTabs;
            }

            final float growW = targetWidth - totalWidth;
            final float growP = growW / totalWeight;

            for (int a = 0, N = getChildCount(); a < N; a++) {
                tabsTextWidthWithMargin[a] += growP * tabsWeight[a];
            }
        }

        int l = 0;
        for (int a = 0, N = getChildCount(); a < N; a++) {
            if (!isViewVisible(getChildAt(a))) {
                continue;
            }

            tabsWidth[a] = Math.round(tabsTextWidthWithMargin[a]);
            tabsLeftPos[a] = l;
            l += tabsWidth[a];
        }
        setMeasuredDimension(l + getPaddingLeft() + getPaddingRight(), height);
        for (int a = 0, N = getChildCount(); a < N; a++) {
            final View child = getChildAt(a);
            child.measure(
                MeasureSpec.makeMeasureSpec(tabsWidth[a], MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(tabHeight, MeasureSpec.EXACTLY));
        }

        calculateTotalSizesAfterMeasure();
    }

    public interface Tab {
        float measureTextWidth();
        default float measureTextWidth(float textSizeDp) { return measureTextWidth(); }
        default void setTextSizeDp(float textSizeDp) {}
    }

    private float[] tabsTextWidth;
    private float[] tabsTextWidthWithMargin;
    private int[] tabsWeight;
    private int[] tabsWidth;

    private int[] tabsLeftPos;

    private int visibleChildCount;
    private int biggestTabTextWidth;

    private void measureTabTexts(float textSizeDp) {
        final int childCount = getChildCount();
        if (tabsTextWidth == null || tabsTextWidth.length < childCount) {
            tabsTextWidth = new float[childCount];
            tabsTextWidthWithMargin = new float[childCount];
            tabsWeight = new int[childCount];
            tabsLeftPos = new int[childCount];
            tabsWidth = new int[childCount];
        }

        float maxTabWidthF = 0;
        int index = 0;

        for (int a = 0; a < childCount; a++) {
            final View child = getChildAt(a);
            if (!isViewVisible(child)) {
                tabsTextWidth[a] = -1;
                continue;
            }

            final float tabWidth;
            if (child instanceof MainTabsLayout.Tab) {
                tabWidth = ((MainTabsLayout.Tab) child).measureTextWidth(textSizeDp);
            } else {
                tabWidth = 0;
            }

            tabsTextWidth[a] = tabWidth;
            maxTabWidthF = Math.max(maxTabWidthF, tabWidth);
            index++;
        }

        biggestTabTextWidth = (int) Math.ceil(maxTabWidthF);
        visibleChildCount = index;
    }

    private void applyPassTextSize(int pass) {
        final float textSizeDp = PASS_TEXT_SIZES_DP[pass];
        for (int a = 0, N = getChildCount(); a < N; a++) {
            final View child = getChildAt(a);
            if (child instanceof MainTabsLayout.Tab) {
                ((MainTabsLayout.Tab) child).setTextSizeDp(textSizeDp);
            }
        }
    }

    @Override
    protected void setChildVisibilityFactor(View view, float factor) {
        final float s = lerp(0.7f, 1f, factor);
        view.setAlpha(factor);
        view.setScaleX(s);
        view.setScaleY(s);
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        super.onLayout(changed, l, t, r, b);
        checkVisualWidth();
    }

    @Override
    protected void onItemsChanged() {
        super.onItemsChanged();
        checkVisualWidth();
    }

    private void checkVisualWidth() {
        for (int a = 0, N = getEntriesCount(); a < N; a++) {
            final ListAnimator.Entry<Holder> entry = getEntry(a);
            final float width = entry.getRectF().width();
            ((GlassTabView) entry.item.view).setVisualWidth(width);
        }
    }

    public void setTabSelected(View tab, boolean animated) {
        for (int a = 0, N = getChildCount(); a < N; a++) {
            final View child = getChildAt(a);
            if (child instanceof GlassTabView) {
                ((GlassTabView) child).setSelected(child == tab, animated);
            }
        }
    }

    private View findSelectedTab() {
        for (int a = 0, N = getChildCount(); a < N; a++) {
            final View child = getChildAt(a);
            if (child.getVisibility() != View.VISIBLE) {
                continue;
            }

            if (child instanceof GlassTabView) {
                if (((GlassTabView) child).isTabSelected()) {
                    return child;
                }
            }
        }
        return null;
    }

    private final Runnable restoreDrawSelector = () -> setSkipDrawSelector(false);

    private boolean drawCustomSelector;
    private void setSkipDrawSelector(boolean skipDrawSelector) {
        drawCustomSelector = skipDrawSelector;
        if (drawCustomSelector) {
            selectorPaint.setColor(Theme.multAlpha(Theme.getColor(Theme.key_glass_tabSelected, resourcesProvider), 0.09f));
        }
        for (int a = 0, N = getChildCount(); a < N; a++) {
            final View child = getChildAt(a);
            if (child.getVisibility() != View.VISIBLE) {
                continue;
            }

            if (child instanceof GlassTabView) {
                ((GlassTabView) child).setSkipDrawSelector(skipDrawSelector);
            }
        }
        invalidate();
    }

    @Override
    protected void dispatchDraw(@NonNull Canvas canvas) {
        if (drawCustomSelector) {
            final float x = animatedLongSelectedViewCenterX + animatedLongSelectedViewOffsetX;
            final float sWidth = getInterpolatedWidthByX(x, this);
            final float sHeight = getHeight() - getPaddingTop() - getPaddingBottom();

            canvas.drawRoundRect(
                    x - sWidth / 2f, (getHeight() - sHeight) / 2f,
                    x + sWidth / 2f, (getHeight() + sHeight) / 2f,
                    sHeight / 2f, sHeight / 2f, selectorPaint);
        }

        super.dispatchDraw(canvas);
    }

    final Paint selectorPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    final SpringAnimation scaleX = new SpringAnimation(this, DynamicAnimation.SCALE_X, 1f);
    final SpringAnimation scaleY = new SpringAnimation(this, DynamicAnimation.SCALE_Y, 1f);

    final SpringAnimation selectedTabPositionOffsetX = new SpringAnimation(this, new FloatPropertyCompat<MainTabsLayout>("selectedTabPositionOffsetX") {
        @Override
        public float getValue(MainTabsLayout object) {
            return object.animatedLongSelectedViewOffsetX;
        }

        @Override
        public void setValue(MainTabsLayout object, float value) {
            object.animatedLongSelectedViewOffsetX = value;
            object.invalidate();
        }
    });
    final SpringAnimation selectedTabPositionX = new SpringAnimation(this, new FloatPropertyCompat<MainTabsLayout>("selectedTabPositionX") {
        @Override
        public float getValue(MainTabsLayout object) {
            return object.animatedLongSelectedViewCenterX;
        }

        @Override
        public void setValue(MainTabsLayout object, float value) {
            object.animatedLongSelectedViewCenterX = value;
            object.invalidate();
        }
    });

    {
        selectedTabPositionOffsetX.setSpring(new SpringForce(1)
            .setStiffness(SpringForce.STIFFNESS_MEDIUM)
            .setDampingRatio(SpringForce.DAMPING_RATIO_LOW_BOUNCY));
        scaleX.setSpring(new SpringForce(1f)
            .setStiffness(250)
            .setDampingRatio(0.25f));
        scaleY.setSpring(new SpringForce(1f)
            .setStiffness(250)
            .setDampingRatio(0.25f));
        selectedTabPositionX.setSpring(new SpringForce(1f)
            .setStiffness(SpringForce.STIFFNESS_MEDIUM)
            .setDampingRatio(SpringForce.DAMPING_RATIO_LOW_BOUNCY));
    }

    private float animatedLongSelectedViewCenterX;
    private float animatedLongSelectedViewOffsetX;

    private boolean isInLongPress;
    private float lastLongSelectedViewCenterX;
    private float lastLongSelectedViewWidth;
    private View lastLongSelectedView;

    public static View findChildUnder(ViewGroup parent, float x, float y) {
        for (int i = parent.getChildCount() - 1; i >= 0; i--) {
            View child = parent.getChildAt(i);

            if (child.getVisibility() != View.VISIBLE) continue;

            if (x >= child.getLeft() && x <= child.getRight()
                    && y >= child.getTop() && y <= child.getBottom()) {
                return child;
            }
        }
        return null;
    }

    private void checkLongMove(float x_, float y, boolean start, boolean end) {
        final float x = clampXToChildrenCenters(x_, this);
        final View found = findNearestVisibleChildByX(x, this);
        if (start) {
            View selected = findSelectedTab();
            if (selected != null) {
                animatedLongSelectedViewCenterX = selected.getX() + selected.getWidth() / 2f;
                animatedLongSelectedViewOffsetX = animatedLongSelectedViewCenterX - x;
                selectedTabPositionOffsetX.animateToFinalPosition(0);
            }
            selectedTabPositionX.cancel();
        }

        if (!end) {
            animatedLongSelectedViewCenterX = x;
            invalidate();
        }

        if (found != null) {
            lastLongSelectedView = found;
            setTabSelected(found, true);

            if (end) {
                final float vw = found.getWidth();
                final float cx = found.getX() + vw / 2f;
                if (lastLongSelectedViewWidth != vw || lastLongSelectedViewCenterX != cx) {
                    selectedTabPositionX.animateToFinalPosition(cx);
                }
            }
        }
    }

    public void addTabToIgnoreClick(View v) {
    }

    private final BoolAnimator animatorIsScaled = new BoolAnimator(0, (a, factor, c, g) -> {
        setScaleX(lerp(1, 1.019f, factor));
        setScaleY(lerp(1, 1.019f, factor));
    }, CubicBezierInterpolator.EASE_OUT_QUINT, 380);

    private float touchDownX;
    private float touchDownY;
    private boolean dragCandidate;
    private View dragOrigin;
    private boolean dragMoved;
    private boolean consumeUntilUp;
    private long touchDownTime;
    private float holdX;
    private float holdY;
    private final HashMap<View, View.OnLongClickListener> tabLongActions = new HashMap<>();
    private final Runnable openHeldMenu = () -> {
        View tab = lastLongSelectedView;
        View.OnLongClickListener action = tabLongActions.get(tab);
        if (!isInLongPress || action == null || tab.getParent() != this || tab.getVisibility() != View.VISIBLE || !tab.isEnabled()) return;
        finishDrag(holdX, holdY, true);
        consumeUntilUp = true;
        action.onLongClick(tab);
    };

    public void setTabLongClickListener(View tab, View.OnLongClickListener action) {
        if (action == null) tabLongActions.remove(tab);
        else tabLongActions.put(tab, action);
        tab.setOnLongClickListener(v -> {
            if (v.getParent() != this || !v.isEnabled()) return false;
            if (!dragCandidate) return action != null && action.onLongClick(v);
            startDrag(touchDownX, touchDownY);
            scheduleHeldMenu(touchDownX, touchDownY, Math.max(1, 1000 - (SystemClock.uptimeMillis() - touchDownTime)));
            return true;
        });
    }

    private void scheduleHeldMenu(float x, float y, long delay) {
        AndroidUtilities.cancelRunOnUIThread(openHeldMenu);
        holdX = x;
        holdY = y;
        if (tabLongActions.containsKey(lastLongSelectedView)) AndroidUtilities.runOnUIThread(openHeldMenu, delay);
    }

    @Override
    public void onViewRemoved(View child) {
        tabLongActions.remove(child);
        if (isInLongPress && (child == dragOrigin || child == lastLongSelectedView)) finishDrag(holdX, holdY, true);
        super.onViewRemoved(child);
    }

    @Override
    public void requestDisallowInterceptTouchEvent(boolean disallowIntercept) {
        if (isInLongPress || consumeUntilUp) {
            super.requestDisallowInterceptTouchEvent(false);
            if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(true);
        } else {
            super.requestDisallowInterceptTouchEvent(disallowIntercept);
        }
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) consumeUntilUp = false;
        if (isInLongPress || consumeUntilUp) return true;
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                touchDownX = event.getX();
                touchDownY = event.getY();
                touchDownTime = SystemClock.uptimeMillis();
                dragCandidate = findChildUnder(this, touchDownX, touchDownY) != null;
                dragOrigin = findSelectedTab();
                dragMoved = false;
                break;
            case MotionEvent.ACTION_MOVE:
                if (dragCandidate && event.getPointerCount() == 1) {
                    float dx = Math.abs(event.getX() - touchDownX);
                    float dy = Math.abs(event.getY() - touchDownY);
                    int slop = ViewConfiguration.get(getContext()).getScaledTouchSlop();
                    if (dx > slop && dx > dy) {
                        dragMoved = true;
                        startDrag(event.getX(), event.getY());
                        return true;
                    }
                    if (dy > slop) dragCandidate = false;
                }
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
            case MotionEvent.ACTION_POINTER_DOWN:
                dragCandidate = false;
                dragOrigin = null;
                break;
        }
        return isInLongPress || super.onInterceptTouchEvent(event);
    }

    private void startDrag(float x, float y) {
        isInLongPress = true;
        lastLongSelectedView = null;
        checkPivot(this, x, y);
        AndroidUtilities.cancelRunOnUIThread(restoreDrawSelector);
        setSkipDrawSelector(true);
        checkLongMove(x, y, true, false);
        scheduleHeldMenu(x, y, 1000);
        animatorIsScaled.setValue(true, true);
        if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(true);
        invalidate();
    }

    private void finishDrag(float x, float y, boolean cancelled) {
        checkPivot(this, x, y);
        checkLongMove(x, y, false, true);
        View target = lastLongSelectedView;
        AndroidUtilities.cancelRunOnUIThread(openHeldMenu);
        isInLongPress = false;
        dragCandidate = false;
        if (cancelled || !dragMoved) {
            if (dragOrigin != null && dragOrigin.getParent() == this) setTabSelected(dragOrigin, true);
        } else if (target != null && target != dragOrigin && target.getParent() == this && target.isEnabled()) {
            target.performClick();
        }
        dragOrigin = null;
        lastLongSelectedView = null;
        AndroidUtilities.runOnUIThread(restoreDrawSelector, 450);
        animatorIsScaled.setValue(false, true);
        if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(false);
        invalidate();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (consumeUntilUp) {
            if (event.getActionMasked() == MotionEvent.ACTION_UP || event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
                consumeUntilUp = false;
                if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(false);
            }
            return true;
        }
        if (!isInLongPress) return super.onTouchEvent(event);
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_MOVE:
                float dx = Math.abs(event.getX() - touchDownX);
                float dy = Math.abs(event.getY() - touchDownY);
                int slop = ViewConfiguration.get(getContext()).getScaledTouchSlop();
                if (dy > slop && dy > dx) {
                    finishDrag(event.getX(), event.getY(), true);
                    consumeUntilUp = true;
                    break;
                }
                if (dx > slop) dragMoved = true;
                View previousTarget = lastLongSelectedView;
                checkPivot(this, event.getX(), event.getY());
                checkLongMove(event.getX(), event.getY(), false, false);
                if (previousTarget != lastLongSelectedView || Math.abs(event.getX() - holdX) > slop || Math.abs(event.getY() - holdY) > slop) {
                    scheduleHeldMenu(event.getX(), event.getY(), 1000);
                }
                break;
            case MotionEvent.ACTION_UP:
                finishDrag(event.getX(), event.getY(), false);
                break;
            case MotionEvent.ACTION_CANCEL:
            case MotionEvent.ACTION_POINTER_DOWN:
                finishDrag(event.getX(), event.getY(), true);
                consumeUntilUp = event.getActionMasked() != MotionEvent.ACTION_CANCEL;
                break;
        }
        return true;
    }

    @Override
    public void setScaleY(float scaleY) {
        super.setScaleY(scaleY);
        checkLayerType();
    }

    @Override
    protected void onDetachedFromWindow() {
        AndroidUtilities.cancelRunOnUIThread(restoreDrawSelector);
        AndroidUtilities.cancelRunOnUIThread(openHeldMenu);
        if (isInLongPress && dragOrigin != null) setTabSelected(dragOrigin, false);
        isInLongPress = false;
        dragCandidate = false;
        dragOrigin = null;
        lastLongSelectedView = null;
        consumeUntilUp = false;
        setSkipDrawSelector(false);
        animatorIsScaled.setValue(false, false);
        if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(false);
        super.onDetachedFromWindow();
    }

    @Override
    public void setScaleX(float scaleX) {
        super.setScaleX(scaleX);
        checkLayerType();
    }

    private void checkLayerType() {
        final int layerType = Math.abs(getScaleX() - 1f) < 0.0001f && Math.abs(getScaleY() - 1f) < 0.0001f ?
            View.LAYER_TYPE_NONE : View.LAYER_TYPE_HARDWARE;

        if (getLayerType() != layerType) {
            setLayerType(layerType, null);
            invalidate();
        }
    }

    private void checkPivot(View view, float x, float y) {
        float w = view.getWidth();
        float h = view.getHeight();

        if (w <= 0f || h <= 0f) {
            return;
        }

        float cx = w * 0.5f;
        float cy = h * 0.5f;

        float dx = x - cx;
        float dy = y - cy;

        float halfW = w * 0.5f;
        float halfH = h * 0.5f;

        float nx = dx / halfW;
        float ny = dy / halfH;

        float r = (float) Math.sqrt(nx * nx + ny * ny);

        float pivotX;
        float pivotY;

        if (r > 1e-4f) {
            float mappedR = 1.5f * r / (r + 0.5f);

            float scale = mappedR / r;
            pivotX = cx + dx * scale;
            pivotY = cy + dy * scale;
        } else {
            pivotX = cx;
            pivotY = cy;
        }

        pivotX = lerp(cx, pivotX, 1f);
        pivotY = lerp(cy, pivotY, 3f);

        view.setPivotX(pivotX);
        view.setPivotY(pivotY);
    }

    private static float clampXToChildrenCenters(float x, ViewGroup parent) {
        if (parent == null || parent.getChildCount() == 0) {
            return x;
        }

        float min = Float.MAX_VALUE;
        float max = -Float.MAX_VALUE;
        boolean found = false;

        for (int i = 0; i < parent.getChildCount(); i++) {
            View view = parent.getChildAt(i);
            if (view == null || view.getVisibility() != View.VISIBLE) {
                continue;
            }

            float centerX = view.getX() + view.getWidth() * 0.5f;

            if (centerX < min) min = centerX;
            if (centerX > max) max = centerX;

            found = true;
        }

        if (!found) {
            return x;
        }

        if (x < min) return min;
        if (x > max) return max;
        return x;
    }

    @Nullable
    private static View findNearestVisibleChildByX(float x, ViewGroup parent) {
        if (parent == null || parent.getChildCount() == 0) {
            return null;
        }

        View nearest = null;
        float nearestDistance = Float.MAX_VALUE;

        for (int i = 0; i < parent.getChildCount(); i++) {
            View view = parent.getChildAt(i);
            if (view == null || view.getVisibility() != View.VISIBLE) {
                continue;
            }

            float centerX = view.getX() + view.getWidth() * 0.5f;
            float distance = Math.abs(centerX - x);

            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearest = view;
            }
        }

        return nearest;
    }

    private static float getInterpolatedWidthByX(float x, ViewGroup parent) {
        if (parent == null || parent.getChildCount() == 0) {
            return 0f;
        }

        View left = null;
        View right = null;

        for (int i = 0; i < parent.getChildCount(); i++) {
            View view = parent.getChildAt(i);
            if (view == null || view.getVisibility() != View.VISIBLE) {
                continue;
            }

            float centerX = view.getX() + view.getWidth() * 0.5f;

            if (centerX <= x && (left == null || centerX > getCenterX(left))) {
                left = view;
            }

            if (centerX >= x && (right == null || centerX < getCenterX(right))) {
                right = view;
            }
        }

        if (left == null && right == null) {
            return 0f;
        }

        if (left == null) {
            return right.getWidth();
        }

        if (right == null) {
            return left.getWidth();
        }

        float leftX = getCenterX(left);
        float rightX = getCenterX(right);

        if (left == right || leftX == rightX) {
            return left.getWidth();
        }

        float ratio = (x - leftX) / (rightX - leftX);
        return lerp(left.getWidth(), right.getWidth(), ratio);
    }

    private static float getCenterX(View v) {
        return v.getX() + v.getWidth() * 0.5f;
    }
}

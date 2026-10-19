/* Licensed under GNU GPL v. 2 or later. */            
                                              
  
                                                                                                                                                                     
                                                                                                                                                                    
                                                         
   

package com.th3nekit.finegram.cards;

import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.Configuration;
import android.os.SystemClock;
import android.os.Build;
import android.os.PowerManager;

import androidx.dynamicanimation.animation.SpringAnimation;
import androidx.dynamicanimation.animation.SpringForce;
import androidx.dynamicanimation.animation.FloatPropertyCompat;
import android.graphics.Canvas;
import android.os.Bundle;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.SharedConfig;
import org.telegram.messenger.LiteMode;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.LayoutHelper;

import java.util.ArrayList;
import java.util.List;

public class FGCardsStrip extends FrameLayout implements NotificationCenter.NotificationCenterDelegate {

    private static final int DRAG_DISTANCE_DP = 36;
    private static final float COMMIT_FRACTION = 0.25f;   
    private static final int FLING_DP_PER_S = 700;        
    public static final long AUTO_SCROLL_MS = 15000;      

    private final Theme.ResourcesProvider resourcesProvider;
    private final ArrayList<FGCard> pills = new ArrayList<>();
    private org.telegram.ui.ActionBar.BaseFragment hostFragment;

    public void setHostFragment(org.telegram.ui.ActionBar.BaseFragment fragment) {
        hostFragment = fragment;
        for (FGCard pill : pills) pill.setHostFragment(fragment);
    }
    private int currentIndex = 0;

    private final int touchSlop;
    private VelocityTracker velocityTracker;
    private boolean dragging;
    private float downX, downY, touchDownY;
    private boolean stackPaused;
    private float dragProgress;   
    private boolean dragUp;       
    private int incomingIndex = -1;
    private SpringAnimation animator;
    private float releaseVelocity;
    private int activePointerId = -1;
    private boolean fullMotion;
    private final FloatPropertyCompat<FGCardsStrip> stackProgress = new FloatPropertyCompat<FGCardsStrip>("stackProgress") {
        @Override
        public float getValue(FGCardsStrip strip) { return strip.dragProgress * 100f; }
        @Override
        public void setValue(FGCardsStrip strip, float value) {
            strip.applyDrag(strip.incomingIndex, value / 100f, strip.dragUp, strip.dragHeight());
        }
    };
    private boolean settlingToNext;
    private int pendingActiveCardId = -1;
    private int measuredCardWidthLimit;

    private boolean potentialTap;
    private boolean longPressFired;
    private final Runnable longPressRunnable = new Runnable() {
        @Override
        public void run() {
            if (!potentialTap || dragging) return;
            finishPausedForPress();
            FGCard cur = current();
            if (cur != null) {
                longPressFired = cur.performLongClick();
                setCardsPressed(false);
            }
        }
    };

    private float visibilityFactor = 1f;
    private boolean opaqueCards;
    private boolean inlineFolderStyle;
    public void setInlineFolderStyle(boolean inline) {
        if (inlineFolderStyle == inline) return;
        inlineFolderStyle = inline;
        for (FGCard pill : pills) {
            pill.setInlineFolderStyle(inline);
        }
        requestLayout();
    }

    public FGCardsStrip(Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;
        this.touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        setClipChildren(false);
        setClipToPadding(false);
    }

    public void setOpaqueCards(boolean v) {
        opaqueCards = v;
        for (FGCard p : pills) p.setOpaqueFlat(v);
    }

    public void rebuildIfChanged() {
        if (!FGCardsConfig.isEnabled()) {
            if (!pills.isEmpty()) {
                cancelAnim();
                removeAllViews();
                pills.clear();
                currentIndex = 0;
                incomingIndex = -1;
                requestLayout();
            }
            disarmGlobalTicker();
            disarmRateTicker();
            return;
        }
        if (pills.isEmpty()) { rebuild(); return; }
        List<Integer> active = FGCardsConfig.getActiveCards();
        if (active.size() == pills.size()) {
            boolean same = true;
            for (int i = 0; i < active.size(); i++) {
                if (pills.get(i).getCardId() != active.get(i)) { same = false; break; }
            }
            if (same) {
                updateColors();
                return;
            }
        }
        rebuild();
    }

    public void rebuild() {
        resetForWindowLifecycle();
        pendingActiveCardId = -1;
        removeAllViews();
        int prevId = -1;
        FGCard prev = current();
        if (prev != null) prevId = prev.getCardId();
        pills.clear();
        currentIndex = 0;
        List<Integer> active = FGCardsConfig.getActiveCards();
        for (int id : active) {
            FGCard pill = FGCardsRegistry.create(id, getContext(), resourcesProvider);
            if (pill != null) {
                pill.setHostFragment(hostFragment);
                pill.setOpaqueFlat(opaqueCards); 
                pill.setInlineFolderStyle(inlineFolderStyle);
                pill.setAccessibilityDelegate(new View.AccessibilityDelegate() {
                    @Override
                    public void onInitializeAccessibilityNodeInfo(View host, AccessibilityNodeInfo info) {
                        super.onInitializeAccessibilityNodeInfo(host, info);
                        int index = pills.indexOf(host);
                        if (index >= 0) {
                            info.setCollectionItemInfo(AccessibilityNodeInfo.CollectionItemInfo.obtain(
                                    0, 1, index, 1, false, index == currentIndex));
                        }
                        boolean canForward = pills.size() > 1 && neighbor(true) >= 0;
                        boolean canBack = pills.size() > 1 && neighbor(false) >= 0;
                        info.setScrollable(canForward || canBack);
                        if (canForward) info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_FORWARD);
                        if (canBack) info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_BACKWARD);
                    }

                    @Override
                    public boolean performAccessibilityAction(View host, int action, Bundle args) {
                        if (action == AccessibilityNodeInfo.ACTION_SCROLL_FORWARD) {
                            return moveFromAccessibility(true);
                        }
                        if (action == AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD) {
                            return moveFromAccessibility(false);
                        }
                        return super.performAccessibilityAction(host, action, args);
                    }
                });
                pills.add(pill);
                int g = Gravity.CENTER_VERTICAL | (LocaleController.isRTL ? Gravity.LEFT : Gravity.RIGHT);
                addView(pill, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT,
                        LayoutHelper.WRAP_CONTENT, g));
            }
        }
        int target = prevId >= 0 ? prevId : FGCardsConfig.getLastActiveCardId();
        if (target >= 0) {
            for (int i = 0; i < pills.size(); i++) {
                if (pills.get(i).getCardId() == target) { currentIndex = i; break; }
            }
        }
        int cap = usableCardWidth();
        int hSpec = android.view.View.MeasureSpec.makeMeasureSpec(AndroidUtilities.dp(44), android.view.View.MeasureSpec.AT_MOST);
        for (FGCard p : pills) {
            p.setMaxChipWidth(cap); 
            p.measure(android.view.View.MeasureSpec.makeMeasureSpec(cap, android.view.View.MeasureSpec.AT_MOST), hSpec);
        }
        requestLayout();
        applyResting();
    }

    public boolean isLayoutSuppressed() {
        return dragging || stackPaused || animator != null;
    }

    public boolean canAnimateCardResize() {
        return !isLayoutSuppressed() && visibilityFactor > 0.999f;
    }
    private int carouselWidth() {
        FGCard cur = current();
        if (cur == null) return 0;
        int width = cur.getMeasuredWidth();
        if (incomingIndex >= 0 && incomingIndex < pills.size()) {
            width = Math.max(width, pills.get(incomingIndex).getMeasuredWidth());
        }
        return width;
    }

    public static int inlineWidthBudget(int availableWidth) {
        return Math.max(1, Math.min(AndroidUtilities.dp(152), (int) (availableWidth * 0.42f)));
    }

    private int usableCardWidth() {
        if (measuredCardWidthLimit > 0) {
            return measuredCardWidthLimit;
        }
        int parentW = 0;
        if (getParent() instanceof View) parentW = ((View) getParent()).getWidth();
        if (parentW <= 0) parentW = getWidth();
        if (parentW <= 0) parentW = AndroidUtilities.displaySize.x;
        if (parentW <= 0) parentW = AndroidUtilities.dp(400);
        return opaqueCards ? Math.max(1, parentW) : inlineWidthBudget(parentW);
    }

    private int usableCardWidth(int widthMeasureSpec) {
        int mode = View.MeasureSpec.getMode(widthMeasureSpec);
        int available = View.MeasureSpec.getSize(widthMeasureSpec);
        if (mode == View.MeasureSpec.UNSPECIFIED || available <= 0) {
            return usableCardWidth();
        }
        return opaqueCards ? Math.max(1, available) : inlineWidthBudget(available);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        measuredCardWidthLimit = usableCardWidth(widthMeasureSpec);
        for (FGCard pill : pills) {
            pill.setMaxChipWidth(measuredCardWidthLimit);
        }
        int mode = MeasureSpec.getMode(widthMeasureSpec);
        int constrainedWidth = mode == MeasureSpec.AT_MOST
                ? MeasureSpec.makeMeasureSpec(measuredCardWidthLimit, MeasureSpec.AT_MOST) : widthMeasureSpec;
        super.onMeasure(constrainedWidth, heightMeasureSpec);
        if (mode == MeasureSpec.AT_MOST) {
            setMeasuredDimension(Math.min(measuredCardWidthLimit, carouselWidth()), getMeasuredHeight());
        }
    }

    private void applyResting() {
        applyResting(true);
    }

    private void applyResting(boolean notifySelected) {
        int usable = usableCardWidth();
        for (int i = 0; i < pills.size(); i++) {
            FGCard p = pills.get(i);
            p.setMaxChipWidth(usable);
            boolean cur = i == currentIndex;
            p.setVisibility(cur ? VISIBLE : GONE);
            p.setAlpha(cur ? 1f : 0f);
            p.setScaleX(1f);
            p.setScaleY(1f);
            p.setRotation(0f);
            p.setStackDepth(1f);
            p.setTranslationX(0);
            p.setTranslationY(0);
        }
        incomingIndex = -1;
        FGCard cur = current();
        if (cur != null) setFrontCard(cur);
        if (notifySelected && cur != null) cur.onCardSelected();
    }

    private void resetForWindowLifecycle() {
        cancelAnim();
        dragging = false;
        stackPaused = false;
        dragProgress = 0f;
        incomingIndex = -1;
        pendingActiveCardId = -1;
        potentialTap = false;
        longPressFired = false;
        activePointerId = -1;
        releaseVelocity = 0f;
        removeCallbacks(longPressRunnable);
        setCardsPressed(false);
        releaseTracker();
        for (FGCard pill : pills) {
            pill.finishResizeAnimation();
        }
        applyResting(false);
        requestLayout();
        invalidate();
    }

    private FGCard current() {
        return (currentIndex >= 0 && currentIndex < pills.size()) ? pills.get(currentIndex) : null;
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        final int padTight = AndroidUtilities.dp(1);
        final int padLoose = AndroidUtilities.dp(20);
        final int padLeft = LocaleController.isRTL ? padTight : padLoose;
        final int padRight = LocaleController.isRTL ? padLoose : padTight;
        canvas.save();
        canvas.clipRect(-padLeft, -AndroidUtilities.dp(5), getWidth() + padRight, getHeight() + AndroidUtilities.dp(5));
        super.dispatchDraw(canvas);
        canvas.restore();
    }

    public void syncToActiveCard() {
        syncToActiveCard(true);
    }

    private void syncToActiveCard(boolean notifySelected) {
        if (pills.isEmpty()) return;
        cancelAnim();
        stackPaused = false;
        pendingActiveCardId = -1;
        dragging = false;
        dragProgress = 0;
        incomingIndex = -1;
        int target = FGCardsConfig.getLastActiveCardId();
        if (target >= 0) {
            for (int i = 0; i < pills.size(); i++) {
                if (pills.get(i).getCardId() == target) { currentIndex = i; break; }
            }
        }
        FGCard cur = current();
        applyResting(false);
        if (notifySelected && cur != null) cur.updateDataInstantly();
    }

    private int neighbor(boolean up) {
        int n = pills.size();
        if (n == 0) return -1;
        boolean inf = FGCardsConfig.isInfiniteScrolling();
        int idx = up ? currentIndex + 1 : currentIndex - 1;
        if (idx < 0) return inf ? n - 1 : -1;
        if (idx >= n) return inf ? 0 : -1;
        return idx;
    }

    private boolean moveFromAccessibility(boolean forward) {
        int next = neighbor(forward);
        if (next < 0 || next == currentIndex) return false;
        cancelAnimResume();
        stackPaused = false;
        FGCard old = current();
        if (old != null) old.onCardUnselected();
        currentIndex = next;
        dragProgress = 0;
        dragging = false;
        applyResting();
        FGCard selected = current();
        if (selected != null) {
            FGCardsConfig.setLastActiveCardId(selected.getCardId());
            selected.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_SELECTED);
        }
        NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.infoCardsActiveCardChanged);
        restartGlobalTicker();
        return true;
    }

    private float dragHeight() {
        return AndroidUtilities.dp(DRAG_DISTANCE_DP);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        return !pills.isEmpty();
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        if (pills.isEmpty()) return false;
        int action = ev.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            activePointerId = ev.getPointerId(0);
            releaseTracker();
        }
        int pointer = ev.findPointerIndex(activePointerId);
        if (pointer < 0) action = MotionEvent.ACTION_CANCEL;
        if (action == MotionEvent.ACTION_POINTER_UP && ev.getPointerId(ev.getActionIndex()) == activePointerId) {
            action = MotionEvent.ACTION_CANCEL;
        }
        float x = pointer >= 0 ? ev.getX(pointer) : downX;
        float y = pointer >= 0 ? ev.getY(pointer) : downY;
        if (velocityTracker == null) velocityTracker = VelocityTracker.obtain();
        velocityTracker.addMovement(ev);
        float h = dragHeight();
        switch (action) {
            case MotionEvent.ACTION_DOWN:
                downY = y;
                touchDownY = y;
                downX = x;
                stackPaused = animator != null;
                cancelAnim();
                dragging = false;
                fullMotion = shouldAnimate(getContext());
                restartGlobalTicker();
                if (stackPaused) downY += (dragUp ? 1f : -1f) * dragProgress * h;
                potentialTap = true;
                longPressFired = false;
                setCardsPressed(potentialTap);
                removeCallbacks(longPressRunnable);
                if (potentialTap) postDelayed(longPressRunnable, ViewConfiguration.getLongPressTimeout());
                return true;
            case MotionEvent.ACTION_MOVE: {
                if (longPressFired) break;
                if (!dragging) {
                    float dy = y - touchDownY, dx = x - downX;
                    if (Math.abs(dy) > touchSlop || Math.abs(dx) > touchSlop) {
                        potentialTap = false;
                        removeCallbacks(longPressRunnable);
                        setCardsPressed(false);
                    }
                    if (pills.size() >= 2 && Math.abs(dy) > touchSlop && Math.abs(dy) >= Math.abs(dx)) {
                        beginDrag(ev);
                    } else break;
                }
                float dy = y - downY;
                dragUp = dy < 0;
                int nb = neighbor(dragUp);
                ensureIncomingPrepared(nb);
                float prog;
                if (nb < 0) {
                    float raw = Math.abs(dy) / h;
                    prog = (float) (1.0 - 1.0 / (raw * 0.18f + 1.0));
                } else prog = Math.min(1f, Math.abs(dy) / h);
                applyDrag(nb, prog, dragUp, h);
                break;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL: {
                if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(false);
                removeCallbacks(longPressRunnable);
                boolean click = action == MotionEvent.ACTION_UP && potentialTap && !dragging && !longPressFired;
                potentialTap = false;
                activePointerId = -1;
                setCardsPressed(false);
                if (click) {
                    finishPausedForPress();
                    FGCard cur = current();
                    releaseTracker();
                    if (cur != null) cur.performClick();
                    return true;
                }
                if (!dragging && !stackPaused) {
                    releaseTracker();
                    return true;
                }
                velocityTracker.computeCurrentVelocity(1000);
                float vy = action == MotionEvent.ACTION_CANCEL ? 0f : velocityTracker.getYVelocity(ev.getPointerId(pointer));
                releaseTracker();
                releaseVelocity = (dragUp ? -vy : vy) / h * 100f;
                int nb = neighbor(dragUp);
                boolean flingMatches = Math.abs(vy) > AndroidUtilities.dp(FLING_DP_PER_S) && (vy < 0) == dragUp;
                boolean commit = action == MotionEvent.ACTION_UP && nb >= 0
                        && (dragProgress > COMMIT_FRACTION || flingMatches);
                dragging = false;
                stackPaused = false;
                if (commit) animateCommit(nb);
                else animateSnapBack(nb);
                break;
            }
        }
        return true;
    }

    private void setCardsPressed(boolean pressed) {
        FGCard cur = current();
        if (cur != null) cur.setPressed(pressed);
    }

    private void beginDrag(MotionEvent ev) {
        dragging = true;
        stackPaused = false;
        if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(true);
        potentialTap = false;
        removeCallbacks(longPressRunnable);
        setCardsPressed(false);
        FGCard cur = current();
        if (cur != null) cur.finishResizeAnimation();
    }

    private void finishPausedForPress() {
        if (!stackPaused) return;
        stackPaused = false;
        boolean commit = dragProgress >= 0.5f;
        pendingActiveCardId = -1;
        finishSettle(incomingIndex, commit);
        if (!commit && current() != null) {
            FGCardsConfig.setLastActiveCardId(current().getCardId());
            NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.infoCardsActiveCardChanged);
        }
    }

    private void ensureIncomingPrepared(int nb) {
        if (nb == incomingIndex) return;
        if (incomingIndex >= 0 && incomingIndex < pills.size() && incomingIndex != currentIndex) {
            FGCard old = pills.get(incomingIndex);
            old.setVisibility(GONE);
        }
        incomingIndex = nb;
        if (nb >= 0 && nb < pills.size()) {
            FGCard in = pills.get(nb);
            in.setVisibility(VISIBLE);
            try { in.updateDataInstantly(); } catch (Throwable ignore) {}
            in.finishResizeAnimation();
            int sw = getWidth(), sh = getHeight();
            if (sw > 0 && sh > 0) {
                int cap = usableCardWidth();
                in.setMaxChipWidth(cap); 
                in.measure(
                        android.view.View.MeasureSpec.makeMeasureSpec(cap, android.view.View.MeasureSpec.AT_MOST),
                        android.view.View.MeasureSpec.makeMeasureSpec(sh, android.view.View.MeasureSpec.AT_MOST));
                int mw = in.getMeasuredWidth(), mh = in.getMeasuredHeight();
                mw = Math.min(mw, cap);
                int top = Math.max(0, (sh - mh) / 2);
                if (LocaleController.isRTL) {
                    in.layout(0, top, mw, top + mh);
                } else {
                    in.layout(sw - mw, top, sw, top + mh); 
                }
            }
        }
        requestLayout();
    }

    private void applyDrag(int incomingIdx, float prog, boolean up, float h) {
        float p = Math.max(0f, Math.min(1f, prog));
        dragProgress = p;
        float direction = up ? -1f : 1f;
        float side = LocaleController.isRTL ? -1f : 1f;
        FGCard cur = current();
        if (incomingIdx < 0 || incomingIdx >= pills.size()) {
            if (cur != null) {
                cur.setTranslationY(fullMotion ? direction * h * 0.25f * p : 0f);
                cur.setTranslationX(0f);
                cur.setAlpha(1f);
                cur.setScaleX(1f);
                cur.setScaleY(1f);
                cur.setRotation(0f);
                cur.setStackDepth(1f);
            }
            return;
        }
        FGCard next = pills.get(incomingIdx);
        if (cur != null) {
            cur.setPivotX(LocaleController.isRTL ? cur.getWidth() : 0f);
            cur.setPivotY(cur.getHeight() * 0.5f);
            cur.setTranslationY(fullMotion ? direction * h * 0.55f * p : 0f);
            cur.setTranslationX(fullMotion ? -side * AndroidUtilities.dp(8) * p : 0f);
            cur.setRotation(fullMotion ? direction * side * 3f * p : 0f);
            cur.setScaleX(fullMotion ? 1f - 0.06f * p : 1f);
            cur.setScaleY(fullMotion ? 1f - 0.06f * p : 1f);
            cur.setAlpha(1f - p);
            cur.setStackDepth(1f - p);
        }
        next.setPivotX(LocaleController.isRTL ? next.getWidth() : 0f);
        next.setPivotY(next.getHeight() * 0.5f);
        next.setTranslationY(fullMotion ? -direction * AndroidUtilities.dp(6) * (1f - p) : 0f);
        next.setTranslationX(fullMotion ? side * AndroidUtilities.dp(8) * (1f - p) : 0f);
        next.setRotation(fullMotion ? -direction * side * 2f * (1f - p) : 0f);
        next.setScaleX(fullMotion ? 0.94f + 0.06f * p : 1f);
        next.setScaleY(fullMotion ? 0.94f + 0.06f * p : 1f);
        next.setAlpha(p);
        next.setStackDepth(p);
        if (p >= 0.5f) setFrontCard(next);
        else if (cur != null) setFrontCard(cur);
    }

    private void setFrontCard(FGCard card) {
        if (getChildCount() > 0 && getChildAt(getChildCount() - 1) != card) card.bringToFront();
    }

    private void animateCommit(final int incomingIdx) {
        settleStack(incomingIdx, true);
    }

    private void animateSnapBack(final int incomingIdx) {
        settleStack(incomingIdx, false);
    }

    static boolean shouldAnimate(Context context) {
        if (!SharedConfig.animationsEnabled() || LiteMode.isPowerSaverApplied()
                || (Build.VERSION.SDK_INT >= 26 && !ValueAnimator.areAnimatorsEnabled())) return false;
        PowerManager power = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        return power == null || !power.isPowerSaveMode();
    }

    private void settleStack(int incomingIdx, boolean commit) {
        cancelAnim();
        settlingToNext = commit;
        fullMotion = shouldAnimate(getContext());
        if (!fullMotion || Math.abs(dragProgress - (commit ? 1f : 0f)) < 0.001f) {
            finishSettle(incomingIdx, commit);
            return;
        }
        SpringAnimation spring = new SpringAnimation(this, stackProgress);
        spring.setSpring(new SpringForce(commit ? 100f : 0f)
                .setStiffness(900f).setDampingRatio(SpringForce.DAMPING_RATIO_NO_BOUNCY));
        spring.setMinimumVisibleChange(0.1f);
        spring.setMinValue(0f);
        spring.setMaxValue(100f);
        spring.setStartVelocity(Math.max(-1500f, Math.min(1500f, releaseVelocity)));
        releaseVelocity = 0f;
        spring.addEndListener((animation, cancelled, value, velocity) -> {
            if (cancelled || animator != animation) return;
            animator = null;
            finishSettle(incomingIdx, commit);
        });
        animator = spring;
        spring.start();
    }

    private void finishSettle(int incomingIdx, boolean commit) {
        stackPaused = false;
        if (commit && incomingIdx >= 0 && incomingIdx < pills.size()) {
            FGCard old = current();
            if (old != null) old.onCardUnselected();
            currentIndex = incomingIdx;
        }
        dragging = false;
        dragProgress = 0f;
        applyResting(false);
        requestLayout();
        if (reconcilePendingActiveCard()) return;
        applyResting();
        if (commit && current() != null) {
            FGCardsConfig.setLastActiveCardId(current().getCardId());
            NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.infoCardsActiveCardChanged);
        }
    }

    private boolean reconcilePendingActiveCard() {
        int pending = pendingActiveCardId;
        pendingActiveCardId = -1;
        FGCard cur = current();
        if (pending < 0 || (cur != null && cur.getCardId() == pending)) {
            return false;
        }
        syncToActiveCard();
        return true;
    }

    private void cancelAnim() {
        SpringAnimation running = animator;
        animator = null;
        if (running != null) running.cancel();
    }

    private void cancelAnimResume() {
        if (animator == null) return;
        cancelAnim();
        dragging = true;
    }

    private void releaseTracker() {
        if (velocityTracker != null) {
            velocityTracker.recycle();
            velocityTracker = null;
        }
    }

    public void setVisibilityFactor(float f) {
        if (visibilityFactor == f) return;
        float previousFactor = visibilityFactor;
        visibilityFactor = f;
        if (f < 0.999f && (dragging || stackPaused || animator != null)) {
            cancelAnimResume();     
            if (dragging || stackPaused) {
                dragging = false;
                stackPaused = false;
                dragProgress = 0;
                applyResting(false);
                if (!reconcilePendingActiveCard()) {
                    applyResting();
                }
            }
        }
        if (previousFactor >= 0.999f && f < 0.999f) {
            FGCard cur = current();
            if (cur != null) cur.finishResizeAnimation();
        }
        setAlpha(f);
        float scale = shouldAnimate(getContext()) ? AndroidUtilities.lerp(0.6f, 1.0f, f) : 1f;
        setScaleX(scale);
        setScaleY(scale);
        setVisibility(f <= 0.01f ? GONE : VISIBLE);
    }

    public float getVisibilityFactor() {
        return visibilityFactor;
    }

    public int getCardsCount() {
        return pills.size();
    }

    public void updateColors() {
        for (FGCard p : pills) {
            p.updateColors();
            p.applyColorMode();
        }
    }

    private static boolean globalTickerArmed = false;
    private static final Runnable GLOBAL_AUTOSCROLL = () -> {
        globalTickerArmed = false;
        if (!FGCardsConfig.isEnabled() || !FGCardsConfig.isAutoScroll()) return; 
        List<Integer> active = FGCardsConfig.getActiveCards();
        if (active.size() >= 2) {
            int curId = FGCardsConfig.getLastActiveCardId();
            int idx = active.indexOf(curId);
            int next = active.get(idx < 0 ? 0 : (idx + 1) % active.size());
            FGCardsConfig.setLastActiveCardId(next);
            NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.infoCardsActiveCardChanged);
        }
        armGlobalTicker(); 
    };

    /**
     * Сколько полос сейчас видно.
     *
     * Часы у нас общие на всех: одни отсчитывают смену карточки, другие — обновление
     * курсов. Пока они заведены, клиент раз в полторы минуты ходит в сеть и раз в
     * пятнадцать секунд будит отрисовку. Считать только прикреплённые полосы мало:
     * свёрнутое приложение своих видов не отцепляет, и часы продолжали бы идти в кармане.
     * Поэтому считаем те, что и прикреплены, и видны.
     */
    private static int visibleStrips;

    /** Учтена ли эта полоса как видимая: чтобы не посчитать её дважды и не потерять. */
    private boolean countedAsVisible;

    private void setCountedAsVisible(boolean visible) {
        if (countedAsVisible == visible) {
            return;
        }
        countedAsVisible = visible;
        if (visible) {
            visibleStrips++;
            armGlobalTicker();
            armRateTicker();
            return;
        }
        visibleStrips = Math.max(0, visibleStrips - 1);
        if (visibleStrips == 0) {
            disarmGlobalTicker();
            disarmRateTicker();
        }
    }

    /** Видна ли полоса прямо сейчас — и на экране, и в незакрытом окне. */
    private boolean isStripOnScreen() {
        return isAttachedToWindow() && getWindowVisibility() == View.VISIBLE;
    }

    static void armGlobalTicker() {
        if (visibleStrips <= 0) return;
        if (globalTickerArmed || !FGCardsConfig.isEnabled() || !FGCardsConfig.isAutoScroll()) return;
        globalTickerArmed = true;
        AndroidUtilities.runOnUIThread(GLOBAL_AUTOSCROLL, AUTO_SCROLL_MS);
    }

    static void restartGlobalTicker() {
        globalTickerArmed = false;
        AndroidUtilities.cancelRunOnUIThread(GLOBAL_AUTOSCROLL);
        armGlobalTicker();
    }

    static void disarmGlobalTicker() {
        globalTickerArmed = false;
        AndroidUtilities.cancelRunOnUIThread(GLOBAL_AUTOSCROLL);
    }

    private static final long RATE_REFRESH_MS = 90000;
    private static boolean rateTickerArmed = false;
    private static final Runnable GLOBAL_RATE_REFRESH = () -> {
        rateTickerArmed = false;
        if (!FGCardsConfig.isEnabled()) return; 
        if (hasActiveFGRateCard()) {
            lastRateRefreshAt = SystemClock.uptimeMillis();
            FGCardsRates.fetch(false, null); 
        }
        armRateTicker(); 
    };

    private static boolean hasActiveFGRateCard() {
        List<Integer> active = FGCardsConfig.getActiveCards();
        return active.contains(FGCardType.TON.id)
                || active.contains(FGCardType.BTC.id)
                || active.contains(FGCardType.USD.id);
    }

    /** Когда курсы обновлялись в последний раз; 0 — ещё ни разу. */
    private static long lastRateRefreshAt;

    /**
     * Заводит часы обновления курсов.
     *
     * Ждём не полный срок, а остаток от него. Часы останавливаются, когда полоса уходит с
     * экрана, и заводятся снова при возвращении — а уходит она каждый раз, когда человек
     * открывает чат. При полном сроке отсчёт начинался бы заново после каждого такого
     * захода, и у того, кто часто заглядывает в чаты, курсы не обновились бы вообще.
     */
    static void armRateTicker() {
        if (visibleStrips <= 0) return;
        if (rateTickerArmed || !FGCardsConfig.isEnabled()) return;
        rateTickerArmed = true;
        final long waited = lastRateRefreshAt == 0 ? 0 : SystemClock.uptimeMillis() - lastRateRefreshAt;
        final long left = Math.max(0, RATE_REFRESH_MS - waited);
        AndroidUtilities.runOnUIThread(GLOBAL_RATE_REFRESH, left);
    }

    static void disarmRateTicker() {
        rateTickerArmed = false;
        AndroidUtilities.cancelRunOnUIThread(GLOBAL_RATE_REFRESH);
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        resetForWindowLifecycle();
        for (FGCard pill : pills) {
            pill.updateLayoutDirection();
            android.widget.FrameLayout.LayoutParams lp =
                    (android.widget.FrameLayout.LayoutParams) pill.getLayoutParams();
            int gravity = Gravity.CENTER_VERTICAL | (LocaleController.isRTL ? Gravity.LEFT : Gravity.RIGHT);
            if (lp != null && lp.gravity != gravity) {
                lp.gravity = gravity;
                pill.setLayoutParams(lp);
            }
            FGCardsRegistry.CardInfo info = FGCardsRegistry.get(pill.getCardId());
            if (info != null) pill.setCardAccessibilityLabel(info.getName());
            try { pill.onUpdateData(false); } catch (Throwable ignored) {}
        }
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        NotificationCenter.getGlobalInstance().addObserver(this, NotificationCenter.infoCardsLayoutChanged);
        NotificationCenter.getGlobalInstance().addObserver(this, NotificationCenter.infoCardsSettingsChanged);
        NotificationCenter.getGlobalInstance().addObserver(this, NotificationCenter.infoCardsColorModeChanged);
        NotificationCenter.getGlobalInstance().addObserver(this, NotificationCenter.infoCardsActiveCardChanged);
        NotificationCenter.getGlobalInstance().addObserver(this, NotificationCenter.didSetNewTheme);
        setCountedAsVisible(isStripOnScreen());
        rebuildIfChanged();
        syncToActiveCard();
        armGlobalTicker(); 
        armRateTicker();   
    }

    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        resetForWindowLifecycle();
        setCountedAsVisible(isStripOnScreen());
        if (visibility == View.VISIBLE) {
            syncToActiveCard(false);
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        NotificationCenter.getGlobalInstance().removeObserver(this, NotificationCenter.infoCardsLayoutChanged);
        NotificationCenter.getGlobalInstance().removeObserver(this, NotificationCenter.infoCardsSettingsChanged);
        NotificationCenter.getGlobalInstance().removeObserver(this, NotificationCenter.infoCardsColorModeChanged);
        NotificationCenter.getGlobalInstance().removeObserver(this, NotificationCenter.infoCardsActiveCardChanged);
        NotificationCenter.getGlobalInstance().removeObserver(this, NotificationCenter.didSetNewTheme);
        resetForWindowLifecycle();
        potentialTap = false;
        setCountedAsVisible(false);
    }

    @Override
    public void didReceivedNotification(int id, int account, Object... args) {
        if (id == NotificationCenter.infoCardsLayoutChanged) {
            rebuildIfChanged();
        } else if (id == NotificationCenter.infoCardsColorModeChanged
                || id == NotificationCenter.didSetNewTheme) {
            updateColors();
        } else if (id == NotificationCenter.infoCardsSettingsChanged) {
            boolean refreshAll = args == null || args.length == 0;
            for (FGCard p : pills) {
                boolean affected = refreshAll;
                if (!affected) {
                    for (Object arg : args) {
                        if (arg instanceof Number && ((Number) arg).intValue() == p.getCardId()) {
                            affected = true;
                            break;
                        }
                    }
                }
                if (affected) {
                    try { p.onUpdateData(false); } catch (Throwable ignore) {}
                }
            }
            if (FGCardsConfig.isAutoScroll()) armGlobalTicker(); else disarmGlobalTicker();
        } else if (id == NotificationCenter.infoCardsActiveCardChanged) {
            FGCard cur = current();
            int targetId = FGCardsConfig.getLastActiveCardId();
            if (dragging || stackPaused || animator != null) {
                pendingActiveCardId = targetId;
            } else if (cur != null && cur.getCardId() == targetId) {
            } else if (getVisibility() == VISIBLE && isShown()
                    && getWindowVisibility() == View.VISIBLE && hasWindowFocus()
                    && visibilityFactor > 0.99f && pills.size() >= 2
                    && pills.get((currentIndex + 1) % pills.size()).getCardId() == targetId) {
                int nb = (currentIndex + 1) % pills.size();
                dragUp = true;
                fullMotion = shouldAnimate(getContext());
                releaseVelocity = 0f;
                ensureIncomingPrepared(nb);
                applyDrag(nb, 0f, dragUp, dragHeight());
                animateCommit(nb);             
            } else {
                syncToActiveCard();            
            }
        }
    }
}

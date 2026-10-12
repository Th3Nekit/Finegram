/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.icons.pack;

import static org.telegram.messenger.AndroidUtilities.dp;
import static org.telegram.messenger.LocaleController.formatString;
import static org.telegram.messenger.LocaleController.getString;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.util.LruCache;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.core.graphics.ColorUtils;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.FileLog;
import org.telegram.messenger.R;
import org.telegram.messenger.Utilities;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.ActionBar.BottomSheet;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.LaunchActivity;

import java.io.File;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;

public class IconPackPreviewSheet extends BottomSheet {

    private static final int COLUMNS = 8;
    private static final int CELL_HEIGHT_DP = 44;
    private static final int ICON_SIZE_DP = 26;

    private static final int VISIBLE_ROWS = 5;

    private static final int TILE_SIZE_DP = 56;
    private static final int TILE_ICON_DP = 30;
    private static final int SHOWCASE_SIZE = 5;

    private static final String[] SHOWCASE_PREFERRED = {
            "msg_settings", "chats_archive", "msg_search", "ic_ab_search", "msg_calls",
            "msg_saved", "msg_delete", "msg_download", "msg_photos", "msg_bot"
    };

    private static final float MISSING_ALPHA = 0.3f;

    private static final int FILTER_ALL = 0;
    private static final int FILTER_COVERED = 1;
    private static final int FILTER_MISSING = 2;

    private final IconPackPreview preview;
    private final Installer installer;
    private final Runnable onInstalled;

    public interface Installer {
        IconPack install();
    }

    private final List<Slot> allSlots = new ArrayList<>();
    private final List<Slot> shown = new ArrayList<>();
    private int filter = FILTER_ALL;

    private final TextView coverageText;
    private final ScrollView scroll;
    private final SlotGrid grid;
    private final FilterBar filterBar;
    private TransformStrip strip;

    private final AtomicBoolean closed = new AtomicBoolean();

    private static final class Slot {
        final String name;
        final int originalRes;
        final boolean covered;

        Bitmap fromPack;

        long arrivedAt;

        Slot(String name, int originalRes, boolean covered) {
            this.name = name;
            this.originalRes = originalRes;
            this.covered = covered;
        }
    }

    public IconPackPreviewSheet(Context context, Theme.ResourcesProvider resourcesProvider,
                                Installer installer, IconPackPreview preview, Runnable onInstalled) {
        super(context, false, resourcesProvider);
        this.installer = installer;
        this.preview = preview;
        this.onInstalled = onInstalled;

        fixNavigationBar();
        setApplyBottomPadding(false);

        buildSlots();

        final LinearLayout container = new LinearLayout(context);
        container.setOrientation(LinearLayout.VERTICAL);
        container.setPadding(dp(22), dp(18), dp(22), dp(12));

        final List<Slot> showcase = pickShowcase();
        if (showcase.size() >= 2) {
            strip = new TransformStrip(context, showcase);
            container.addView(strip, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT,
                    TILE_SIZE_DP, 0, 0, 0, 14));
        }

        final TextView title = new TextView(context);
        title.setTextColor(getThemedColor(Theme.key_dialogTextBlack));
        title.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 20);
        title.setTypeface(AndroidUtilities.bold());
        title.setText(preview.name);
        container.addView(title, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT,
                LayoutHelper.WRAP_CONTENT));

        coverageText = new TextView(context);
        coverageText.setTextColor(getThemedColor(Theme.key_dialogTextGray2));
        coverageText.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 14);
        coverageText.setText(subtitleFor(coveredCount()));
        container.addView(coverageText, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT,
                LayoutHelper.WRAP_CONTENT, 0, 4, 0, 0));

        container.addView(new CoverageBar(context),
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 4, 0, 10, 0, 0));

        filterBar = new FilterBar(context);
        container.addView(filterBar, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT,
                LayoutHelper.WRAP_CONTENT, 0, 14, 0, 0));

        grid = new SlotGrid(context);
        scroll = new ScrollView(context);
        scroll.setVerticalScrollBarEnabled(false);
        scroll.setOverScrollMode(View.OVER_SCROLL_NEVER);
        scroll.addView(grid, new ScrollView.LayoutParams(
                LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
        container.addView(scroll, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT,
                CELL_HEIGHT_DP * VISIBLE_ROWS, 0, 12, 0, 6));

        container.addView(createInstallButton(context),
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 48, 0, 14, 0, 0));

        setCustomView(container);

        final Set<String> wanted = new HashSet<>();
        for (Slot slot : allSlots) {
            if (slot.covered) {
                wanted.add(slot.name);
            }
        }
        IconPackManager.loadIcons(preview, wanted, dp(TILE_ICON_DP), closed, this::onIconsArrived);
    }

    @Override
    public void dismiss() {
        closed.set(true);
        super.dismiss();
    }

    private View createInstallButton(Context context) {
        final TextView button = new TextView(context) {
            @Override
            public boolean onTouchEvent(MotionEvent event) {

                final int action = event.getActionMasked();
                if (action == MotionEvent.ACTION_DOWN) {
                    animate().scaleX(0.97f).scaleY(0.97f).setDuration(160)
                            .setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT).start();
                } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                    animate().scaleX(1f).scaleY(1f).setDuration(160)
                            .setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT).start();
                }
                return super.onTouchEvent(event);
            }
        };
        button.setGravity(Gravity.CENTER);
        button.setTextColor(getThemedColor(Theme.key_featuredStickers_buttonText));
        button.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 15);
        button.setTypeface(AndroidUtilities.bold());
        button.setText(getString(R.string.FG_IconPack_Apply));
        button.setBackground(Theme.createSimpleSelectorRoundRectDrawable(dp(8),
                getThemedColor(Theme.key_featuredStickers_addButton),
                getThemedColor(Theme.key_featuredStickers_addButtonPressed)));
        button.setOnClickListener(v -> install());
        return button;
    }

    private void buildSlots() {
        final Map<String, Integer> known = IconSlots.all();
        final List<Slot> covered = new ArrayList<>();
        final List<Slot> missing = new ArrayList<>();
        for (Map.Entry<String, Integer> entry : known.entrySet()) {
            final Slot slot = new Slot(entry.getKey(), entry.getValue(),
                    preview.slots.containsKey(entry.getKey()));
            (slot.covered ? covered : missing).add(slot);
        }
        allSlots.addAll(covered);
        allSlots.addAll(missing);
        if (allSlots.isEmpty()) {

            for (String name : preview.slots.keySet()) {
                allSlots.add(new Slot(name, 0, true));
            }
        }
        selectShown();
    }

    private void onIconsArrived(Map<String, Bitmap> icons) {
        final long now = System.currentTimeMillis();
        boolean changed = false;
        for (Slot slot : allSlots) {
            final Bitmap bitmap = icons.get(slot.name);
            if (bitmap != null && slot.fromPack == null) {
                slot.fromPack = bitmap;
                slot.arrivedAt = now;
                changed = true;
            }
        }
        if (!changed) {
            return;
        }
        grid.playArrival();
        if (strip != null) {
            strip.invalidate();
        }
    }

    private void selectShown() {
        shown.clear();
        for (Slot slot : allSlots) {
            if (filter == FILTER_ALL
                    || (filter == FILTER_COVERED && slot.covered)
                    || (filter == FILTER_MISSING && !slot.covered)) {
                shown.add(slot);
            }
        }
    }

    private int coveredCount() {
        int count = 0;
        for (Slot slot : allSlots) {
            if (slot.covered) {
                count++;
            }
        }
        return count;
    }

    private List<Slot> pickShowcase() {
        final List<Slot> picked = new ArrayList<>();
        for (String name : SHOWCASE_PREFERRED) {
            for (Slot slot : allSlots) {
                if (slot.covered && slot.name.equals(name)) {
                    picked.add(slot);
                    break;
                }
            }
            if (picked.size() == SHOWCASE_SIZE) {
                return picked;
            }
        }
        for (Slot slot : allSlots) {
            if (picked.size() == SHOWCASE_SIZE) {
                break;
            }
            if (slot.covered && !picked.contains(slot)) {
                picked.add(slot);
            }
        }
        return picked;
    }

    private CharSequence subtitleFor(int covered) {
        final String author = preview.author == null || preview.author.isEmpty() ? "—" : preview.author;
        return formatString(R.string.FG_IconPack_Coverage, author, covered, allSlots.size());
    }

    private void applyFilter(int value) {
        if (filter == value) {
            return;
        }
        filter = value;
        filterBar.moveTo(value);
        grid.animate().alpha(0f).setDuration(110)
                .setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT)
                .withEndAction(() -> {
                    selectShown();
                    grid.requestLayout();
                    scroll.scrollTo(0, 0);
                    grid.setAlpha(1f);
                    grid.playReveal(16, 220);
                }).start();
    }

    private class TransformStrip extends View {

        private static final long CYCLE = 3600;

        private static final long FLIP = 320;
        private static final long STAGGER = 150;

        private static final long FIRST_FLIP = 500;

        private final List<Slot> tiles;
        private final Paint chipPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint iconPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        private final RectF chip = new RectF();
        private final Rect target = new Rect();
        private final LruCache<Integer, Drawable> originals = new LruCache<>(SHOWCASE_SIZE * 2);
        private final ColorFilter tint;

        private ValueAnimator loop;
        private float time;

        TransformStrip(Context context, List<Slot> tiles) {
            super(context);
            this.tiles = tiles;
            chipPaint.setColor(Theme.multAlpha(getThemedColor(Theme.key_dialogTextBlack), 0.06f));
            tint = new PorterDuffColorFilter(getThemedColor(Theme.key_dialogTextBlack),
                    PorterDuff.Mode.SRC_IN);
            iconPaint.setColorFilter(tint);
        }

        @Override
        protected void onAttachedToWindow() {
            super.onAttachedToWindow();
            loop = ValueAnimator.ofFloat(0f, CYCLE);
            loop.setDuration(CYCLE);
            loop.setRepeatCount(ValueAnimator.INFINITE);

            loop.setInterpolator(null);
            loop.addUpdateListener(a -> {
                time = (float) a.getAnimatedValue();
                invalidate();
            });
            loop.start();
        }

        @Override
        protected void onDetachedFromWindow() {
            super.onDetachedFromWindow();
            if (loop != null) {
                loop.cancel();
                loop = null;
            }
        }

        @Override
        protected void onDraw(@NonNull Canvas canvas) {
            final int count = tiles.size();
            if (count == 0) {
                return;
            }
            final float step = getMeasuredWidth() / (float) count;
            final int tile = dp(TILE_SIZE_DP);
            final int icon = dp(TILE_ICON_DP);
            final float radius = dp(16);

            for (int i = 0; i < count; i++) {
                final float cx = (i + 0.5f) * step;
                final float cy = getMeasuredHeight() / 2f;

                chip.set(cx - tile / 2f, cy - tile / 2f, cx + tile / 2f, cy + tile / 2f);
                canvas.drawRoundRect(chip, radius, radius, chipPaint);

                final float local = (time - i * STAGGER + CYCLE / 2f - FIRST_FLIP + CYCLE * 2) % CYCLE;
                final float toEdge = Math.min(Math.min(local, Math.abs(local - CYCLE / 2f)),
                        CYCLE - local);
                final float squeeze = Math.min(1f, toEdge / (FLIP / 2f));
                if (squeeze <= 0.01f) {
                    continue;
                }

                canvas.save();
                canvas.scale(squeeze, 1f, cx, cy);
                target.set(Math.round(cx - icon / 2f), Math.round(cy - icon / 2f),
                        Math.round(cx + icon / 2f), Math.round(cy + icon / 2f));

                final Bitmap replacement = tiles.get(i).fromPack;
                if (local >= CYCLE / 2f && replacement != null) {
                    iconPaint.setAlpha(255);
                    canvas.drawBitmap(replacement, null, target, iconPaint);
                } else {
                    final Drawable original = originalOf(tiles.get(i).originalRes);
                    if (original != null) {
                        original.setBounds(target);
                        original.setAlpha(255);
                        original.draw(canvas);
                    }
                }
                canvas.restore();
            }
        }

        private Drawable originalOf(int resource) {
            if (resource == 0) {
                return null;
            }
            Drawable known = originals.get(resource);
            if (known != null) {
                return known;
            }
            try {
                known = getContext().getResources().getDrawable(resource, null);
            } catch (Throwable ignore) {
                return null;
            }
            if (known == null) {
                return null;
            }
            known = known.mutate();
            known.setColorFilter(tint);
            originals.put(resource, known);
            return known;
        }
    }

    private class CoverageBar extends View {

        private final Paint trackPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint fillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF rect = new RectF();
        private float progress;

        CoverageBar(Context context) {
            super(context);
            trackPaint.setColor(Theme.multAlpha(getThemedColor(Theme.key_dialogTextBlack), 0.1f));
            fillPaint.setColor(getThemedColor(Theme.key_featuredStickers_addButton));

            final int covered = coveredCount();
            final float target = allSlots.isEmpty() ? 0f : (float) covered / allSlots.size();
            final ValueAnimator animator = ValueAnimator.ofFloat(0f, 1f);
            animator.setDuration(420);
            animator.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
            animator.addUpdateListener(a -> {
                final float value = (float) a.getAnimatedValue();
                progress = target * value;
                coverageText.setText(subtitleFor(Math.round(covered * value)));
                invalidate();
            });
            animator.setStartDelay(120);
            animator.start();
        }

        @Override
        protected void onDraw(@NonNull Canvas canvas) {
            final float r = getMeasuredHeight() / 2f;
            rect.set(0, 0, getMeasuredWidth(), getMeasuredHeight());
            canvas.drawRoundRect(rect, r, r, trackPaint);
            if (progress > 0) {
                rect.set(0, 0, Math.max(getMeasuredHeight(), getMeasuredWidth() * progress),
                        getMeasuredHeight());
                canvas.drawRoundRect(rect, r, r, fillPaint);
            }
        }
    }

    private class FilterBar extends LinearLayout {

        private final Paint pillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF pill = new RectF();
        private final int activeColor = getThemedColor(Theme.key_featuredStickers_buttonText);
        private final int idleColor = getThemedColor(Theme.key_dialogTextBlack);

        private float pillLeft;
        private float pillRight;
        private ValueAnimator move;

        FilterBar(Context context) {
            super(context);
            setOrientation(HORIZONTAL);
            setWillNotDraw(false);
            pillPaint.setColor(getThemedColor(Theme.key_featuredStickers_addButton));

            addChip(FILTER_ALL, getString(R.string.FG_IconPack_FilterAll), allSlots.size());
            addChip(FILTER_COVERED, getString(R.string.FG_IconPack_FilterCovered), coveredCount());
            addChip(FILTER_MISSING, getString(R.string.FG_IconPack_FilterMissing),
                    allSlots.size() - coveredCount());
        }

        private void addChip(int value, String label, int count) {
            final TextView chip = new TextView(getContext());
            chip.setText(label + " · " + count);
            chip.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 13);
            chip.setTypeface(AndroidUtilities.bold());
            chip.setGravity(Gravity.CENTER);
            chip.setPadding(dp(12), dp(6), dp(12), dp(6));
            chip.setTextColor(value == filter ? activeColor : idleColor);
            chip.setOnClickListener(v -> applyFilter(value));
            addView(chip, LayoutHelper.createLinear(LayoutHelper.WRAP_CONTENT,
                    LayoutHelper.WRAP_CONTENT, 0, 0, 8, 0));
        }

        void moveTo(int value) {
            final View destination = getChildAt(value);
            if (destination == null) {
                return;
            }
            if (move != null) {
                move.cancel();
            }
            final int previous = under();
            final float fromLeft = pillLeft;
            final float fromRight = pillRight;
            final float toLeft = destination.getLeft();
            final float toRight = destination.getRight();
            move = ValueAnimator.ofFloat(0f, 1f);
            move.setDuration(260);
            move.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
            move.addUpdateListener(a -> {
                final float t = (float) a.getAnimatedValue();
                pillLeft = fromLeft + (toLeft - fromLeft) * t;
                pillRight = fromRight + (toRight - fromRight) * t;
                paint(value, t);
                if (previous >= 0 && previous != value) {
                    paint(previous, 1f - t);
                }
                invalidate();
            });
            move.start();
        }

        private void paint(int index, float toward) {
            final View child = getChildAt(index);
            if (child instanceof TextView) {
                ((TextView) child).setTextColor(ColorUtils.blendARGB(idleColor, activeColor,
                        Math.max(0f, Math.min(1f, toward))));
            }
        }

        private int under() {
            for (int i = 0; i < getChildCount(); i++) {
                if (Math.abs(getChildAt(i).getLeft() - pillLeft) < 1f) {
                    return i;
                }
            }
            return -1;
        }

        @Override
        protected void onLayout(boolean changed, int l, int t, int r, int b) {
            super.onLayout(changed, l, t, r, b);
            final View active = getChildAt(filter);
            if (active != null && (move == null || !move.isRunning())) {
                pillLeft = active.getLeft();
                pillRight = active.getRight();
            }
        }

        @Override
        protected void dispatchDraw(@NonNull Canvas canvas) {
            if (pillRight > pillLeft) {
                pill.set(pillLeft, 0, pillRight, getMeasuredHeight());
                final float r = getMeasuredHeight() / 2f;
                canvas.drawRoundRect(pill, r, r, pillPaint);
            }
            super.dispatchDraw(canvas);
        }
    }

    private class SlotGrid extends View {

        private static final long STAGGER = 22;
        private static final long CELL_REVEAL = 260;

        private static final long ARRIVAL = 260;

        private final Paint iconPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        private final Rect target = new Rect();
        private final Rect clip = new Rect();
        private final ColorFilter tint;

        private final LruCache<Integer, Drawable> originals = new LruCache<>(120);

        private ValueAnimator reveal;
        private ValueAnimator arrival;
        private float revealTime = -1f;
        private long stagger = STAGGER;
        private long cellReveal = CELL_REVEAL;

        private int pressedIndex = -1;
        private float pressScale = 1f;
        private ValueAnimator press;

        SlotGrid(Context context) {
            super(context);
            tint = new PorterDuffColorFilter(getThemedColor(Theme.key_dialogTextBlack),
                    PorterDuff.Mode.SRC_IN);
            iconPaint.setColorFilter(tint);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            final int width = MeasureSpec.getSize(widthMeasureSpec);
            final int rows = (shown.size() + COLUMNS - 1) / COLUMNS;
            setMeasuredDimension(width, Math.max(dp(CELL_HEIGHT_DP), rows * dp(CELL_HEIGHT_DP)));
        }

        void playReveal(long staggerMs, long cellMs) {
            stagger = staggerMs;
            cellReveal = cellMs;
            if (reveal != null) {
                reveal.cancel();
            }
            final long total = (VISIBLE_ROWS + COLUMNS) * stagger + cellReveal;
            reveal = ValueAnimator.ofFloat(0f, total);
            reveal.setDuration(total);
            reveal.setInterpolator(null);
            reveal.addUpdateListener(a -> {
                revealTime = (float) a.getAnimatedValue();
                invalidate();
            });
            reveal.start();
        }

        @Override
        protected void onAttachedToWindow() {
            super.onAttachedToWindow();
            if (revealTime < 0f) {
                playReveal(STAGGER, CELL_REVEAL);
            }
        }

        @Override
        protected void onDraw(@NonNull Canvas canvas) {
            if (!canvas.getClipBounds(clip)) {
                return;
            }
            final int cellHeight = dp(CELL_HEIGHT_DP);
            final float cellWidth = getMeasuredWidth() / (float) COLUMNS;
            final int size = dp(ICON_SIZE_DP);

            final int firstRow = Math.max(0, clip.top / cellHeight);
            final int lastRow = Math.min((shown.size() - 1) / COLUMNS, clip.bottom / cellHeight);

            for (int row = firstRow; row <= lastRow; row++) {
                for (int col = 0; col < COLUMNS; col++) {
                    final int index = row * COLUMNS + col;
                    if (index >= shown.size()) {
                        break;
                    }
                    final float appear = appearOf(row, col);
                    if (appear <= 0f) {
                        continue;
                    }

                    float scale = 0.9f + 0.1f * appear;
                    if (index == pressedIndex) {
                        scale *= pressScale;
                    }
                    final int drawn = Math.round(size * scale);
                    final int cx = Math.round((col + 0.5f) * cellWidth);
                    final int cy = row * cellHeight + cellHeight / 2;
                    target.set(cx - drawn / 2, cy - drawn / 2, cx + drawn / 2, cy + drawn / 2);
                    drawSlot(canvas, shown.get(index), appear);
                }
            }
        }

        private void drawSlot(Canvas canvas, Slot slot, float appear) {
            final float arrival = slot.fromPack == null ? 0f : arrivalOf(slot);

            if (arrival < 1f && slot.originalRes != 0) {

                final Drawable drawable = originalOf(slot.originalRes);
                if (drawable != null) {
                    drawable.setBounds(target);
                    drawable.setAlpha(Math.round(255 * MISSING_ALPHA * appear * (1f - arrival)));
                    drawable.draw(canvas);
                }
            }
            if (slot.fromPack != null && arrival > 0f) {
                iconPaint.setAlpha(Math.round(255 * appear * arrival));
                canvas.drawBitmap(slot.fromPack, null, target, iconPaint);
            }
        }

        private float arrivalOf(Slot slot) {
            final long passed = System.currentTimeMillis() - slot.arrivedAt;
            if (passed >= ARRIVAL) {
                return 1f;
            }
            return CubicBezierInterpolator.EASE_OUT_QUINT.getInterpolation(passed / (float) ARRIVAL);
        }

        void playArrival() {
            if (arrival != null) {
                arrival.cancel();
            }
            arrival = ValueAnimator.ofFloat(0f, 1f);
            arrival.setDuration(ARRIVAL);
            arrival.setInterpolator(null);
            arrival.addUpdateListener(a -> invalidate());
            arrival.start();
        }

        private Drawable originalOf(int resource) {
            Drawable known = originals.get(resource);
            if (known != null) {
                return known;
            }
            try {
                known = getContext().getResources().getDrawable(resource, null);
            } catch (Throwable ignore) {
                return null;
            }
            if (known == null) {
                return null;
            }
            known = known.mutate();
            known.setColorFilter(tint);
            originals.put(resource, known);
            return known;
        }

        private float appearOf(int row, int col) {
            if (revealTime < 0f) {
                return 0f;
            }
            if (row >= VISIBLE_ROWS) {
                return 1f;
            }
            final float t = (revealTime - (row + col) * stagger) / cellReveal;
            if (t <= 0f) {
                return 0f;
            }
            if (t >= 1f) {
                return 1f;
            }
            return CubicBezierInterpolator.EASE_OUT_QUINT.getInterpolation(t);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            final int action = event.getActionMasked();
            if (action == MotionEvent.ACTION_DOWN) {
                pressedIndex = indexAt(event.getX(), event.getY());
                if (pressedIndex < 0) {
                    return false;
                }
                animatePress(0.86f, 120);
                return true;
            }
            if (pressedIndex < 0) {
                return false;
            }
            if (action == MotionEvent.ACTION_UP) {
                final BaseFragment fragment = LaunchActivity.getLastFragment();
                if (fragment != null) {
                    BulletinFactory.of(fragment).createSimpleBulletin(R.drawable.msg_info,
                            shown.get(pressedIndex).name).show();
                }
                animatePress(1f, 200);
            } else if (action == MotionEvent.ACTION_CANCEL) {
                animatePress(1f, 160);
            }
            return true;
        }

        private void animatePress(float to, long duration) {
            if (press != null) {
                press.cancel();
            }
            press = ValueAnimator.ofFloat(pressScale, to);
            press.setDuration(duration);
            press.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
            press.addUpdateListener(a -> {
                pressScale = (float) a.getAnimatedValue();
                invalidate();
            });
            press.start();
        }

        private int indexAt(float x, float y) {
            final int col = (int) (x / (getMeasuredWidth() / (float) COLUMNS));
            final int row = (int) (y / dp(CELL_HEIGHT_DP));
            if (col < 0 || col >= COLUMNS || row < 0) {
                return -1;
            }
            final int index = row * COLUMNS + col;
            return index < shown.size() ? index : -1;
        }
    }

    private void install() {
        dismiss();
        Utilities.globalQueue.postRunnable(() -> {
            final IconPack installed = installer.install();
            AndroidUtilities.runOnUIThread(() -> finishInstall(installed));
        });
    }

    private void finishInstall(IconPack installed) {
        final BaseFragment fragment = LaunchActivity.getLastFragment();
        if (installed == null) {
            if (fragment != null) {
                BulletinFactory.of(fragment).createErrorBulletin(
                        getString(R.string.FG_IconPack_Failed)).show();
            }
            return;
        }
        IconPackManager.INSTANCE.setActive(installed);
        if (LaunchActivity.instance != null) {
            LaunchActivity.instance.reloadResources();
        }
        if (fragment != null) {
            BulletinFactory.of(fragment).createSimpleBulletin(
                    R.raw.done,
                    formatString(R.string.FG_IconPack_Installed, installed.getName())
            ).show();
        }
        if (onInstalled != null) {
            onInstalled.run();
        }
    }

    public static class IconPackPreview {
        public final String name;
        public final String author;
        public final int iconCount;

        public final LinkedHashMap<String, String> slots;

        public final byte[] source;

        public IconPackPreview(String name, String author, int iconCount,
                               LinkedHashMap<String, String> slots, byte[] source) {
            this.name = name;
            this.author = author;
            this.iconCount = iconCount;
            this.slots = slots;
            this.source = source;
        }
    }

    public static boolean showForUri(Uri uri, BaseFragment fragment, Runnable onInstalled) {
        if (uri == null || fragment == null || fragment.getContext() == null) {
            return false;
        }
        try {
            final IconPackPreview preview = IconPackManager.INSTANCE.readPreview(uri);
            if (preview == null) {
                return false;
            }
            new IconPackPreviewSheet(fragment.getContext(), fragment.getResourceProvider(),
                    () -> IconPackManager.INSTANCE.install(uri), preview, onInstalled).show();
            return true;
        } catch (Throwable e) {
            FileLog.e(e);
            return false;
        }
    }

    public static boolean showIfIconPack(File file, String fileName, BaseFragment fragment) {
        if (file == null || !file.exists() || fragment == null || fragment.getContext() == null) {
            android.util.Log.w("FGOpen", "icons: file " + (file != null && file.exists()) + ", screen " + fragment);
            return false;
        }
        final String name = fileName != null ? fileName : file.getName();
        if (!name.toLowerCase().endsWith(".icons")) {
            return false;
        }
        try {
            final IconPackPreview preview = IconPackManager.INSTANCE.readPreview(file);
            if (preview == null) {
                android.util.Log.w("FGOpen", "icons: pack not readable: " + name);
                return false;
            }
            new IconPackPreviewSheet(fragment.getContext(), fragment.getResourceProvider(),
                    () -> IconPackManager.INSTANCE.install(file), preview, null).show();
            return true;
        } catch (Throwable e) {
            android.util.Log.w("FGOpen", "icons: sheet failed", e);
            FileLog.e(e);
            return false;
        }
    }
}

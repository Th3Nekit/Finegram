/*
 * Finegram plugin store.
 * Adapted from Kangel Plugins Manager (KangelPlugins).
 * Upstream: https://git.kangel.xyz/KangelPlugins/PluginManager
 * Licensed under GNU GPL v3; see LICENSE.PluginManager and NOTICE.
 */

package com.th3nekit.finegram.store;

import static org.telegram.messenger.AndroidUtilities.dp;
import static org.telegram.messenger.LocaleController.formatString;
import static org.telegram.messenger.LocaleController.getString;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.BackupImageView;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.RecyclerListView;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import org.telegram.ui.Components.UniversalRecyclerView;

import com.th3nekit.finegram.plugins.FGPluginIcon;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class FGStoreFeatured extends LinearLayout {

    private static final int CARD_W = 150;
    private static final int CARD_H = 172;

    public interface Delegate {
        void onPicked(FGStore.Item item);
    }

    private final TextView titleView;
    private final TextView countView;
    private final LinearLayout strip;
    private final HorizontalScrollView scroller;

    private List<String> shownIds = Collections.emptyList();
    private boolean shownOnce;

    public FGStoreFeatured(@NonNull Context context) {
        super(context);
        setOrientation(VERTICAL);

        final LinearLayout head = new LinearLayout(context);
        head.setOrientation(HORIZONTAL);
        head.setGravity(Gravity.CENTER_VERTICAL);

        titleView = new TextView(context);
        titleView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 15);
        titleView.setTypeface(AndroidUtilities.bold());
        titleView.setText(getString(R.string.FG_Store_Featured));
        head.addView(titleView, LayoutHelper.createLinear(0, LayoutHelper.WRAP_CONTENT, 1f));

        countView = new TextView(context);
        countView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 13);
        head.addView(countView, LayoutHelper.createLinear(LayoutHelper.WRAP_CONTENT, LayoutHelper.WRAP_CONTENT));

        addView(head, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                16, 14, 16, 10));

        scroller = new HorizontalScrollView(context);
        scroller.setHorizontalScrollBarEnabled(false);
        scroller.setClipToPadding(false);
        scroller.setPadding(dp(16), 0, dp(16), 0);
        scroller.setOverScrollMode(OVER_SCROLL_NEVER);

        scroller.setFocusable(false);
        scroller.setDescendantFocusability(FOCUS_BLOCK_DESCENDANTS);

        strip = new LinearLayout(context);
        strip.setOrientation(HORIZONTAL);
        scroller.addView(strip, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT, CARD_H));

        addView(scroller, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, CARD_H, 0, 0, 0, 12));
    }

    public void set(List<FGStore.Item> items, int total, Delegate delegate) {
        titleView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlueHeader));
        countView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteGrayText2));
        countView.setText(formatString(R.string.FG_Store_Total, total));

        final List<String> ids = new ArrayList<>(items.size());
        for (FGStore.Item item : items) {
            ids.add(item.getId());
        }
        if (ids.equals(shownIds)) {

            for (int i = 0; i < strip.getChildCount() && i < items.size(); i++) {
                ((Card) strip.getChildAt(i)).refresh(items.get(i));
            }
            return;
        }
        shownIds = ids;

        strip.removeAllViews();
        scroller.scrollTo(0, 0);
        for (int i = 0; i < items.size(); i++) {
            final FGStore.Item item = items.get(i);
            final Card card = new Card(getContext());
            card.setFocusable(false);
            card.set(item, delegate);
            strip.addView(card, LayoutHelper.createLinear(CARD_W, CARD_H, i == 0 ? 0 : 10, 0, 0, 0));
            if (!shownOnce) {
                appear(card, i);
            }
        }
        shownOnce = true;
    }

    private void appear(View card, int index) {
        card.setAlpha(0f);
        card.setTranslationY(dp(10));
        card.animate()
                .alpha(1f)
                .translationY(0)
                .setStartDelay(Math.min(index, 6) * 45L)
                .setDuration(240)
                .setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT)
                .start();
    }

    private static class Card extends FrameLayout {

        private final Paint backPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint tagPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF rect = new RectF();
        private final RectF tagRect = new RectF();

        private final TextView letterView;
        private final BackupImageView iconView;
        private final TextView nameView;
        private final TextView authorView;
        private final TextView tagView;

        private int color;
        private int gradientHeight;
        private float pressScale = 1f;
        private ValueAnimator pressAnimator;

        Card(@NonNull Context context) {
            super(context);
            setWillNotDraw(false);

            letterView = new TextView(context);
            letterView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 22);
            letterView.setTypeface(AndroidUtilities.bold());
            letterView.setGravity(Gravity.CENTER);
            addView(letterView, LayoutHelper.createFrame(52, 52, Gravity.CENTER_HORIZONTAL | Gravity.TOP, 0, 16, 0, 0));

            iconView = new BackupImageView(context);
            iconView.setRoundRadius(dp(14));
            iconView.setVisibility(GONE);
            addView(iconView, LayoutHelper.createFrame(52, 52, Gravity.CENTER_HORIZONTAL | Gravity.TOP, 0, 16, 0, 0));

            nameView = new TextView(context);
            nameView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 14);
            nameView.setTypeface(AndroidUtilities.bold());
            nameView.setGravity(Gravity.CENTER);
            nameView.setLines(1);
            nameView.setEllipsize(TextUtils.TruncateAt.END);
            addView(nameView, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                    Gravity.TOP, 8, 76, 8, 0));

            authorView = new TextView(context);
            authorView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 11);
            authorView.setGravity(Gravity.CENTER);
            authorView.setLines(1);
            authorView.setEllipsize(TextUtils.TruncateAt.END);
            addView(authorView, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                    Gravity.TOP, 8, 97, 8, 0));

            tagView = new TextView(context);
            tagView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 11);
            tagView.setTypeface(AndroidUtilities.bold());
            tagView.setGravity(Gravity.CENTER);
            tagView.setLines(1);
            tagView.setEllipsize(TextUtils.TruncateAt.END);
            addView(tagView, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, 28,
                    Gravity.BOTTOM, 16, 0, 16, 16));
        }

        void set(FGStore.Item item, Delegate delegate) {
            color = FGPluginIcon.accentFor(item.getId());
            gradientHeight = 0;
            letterView.setText("");
            letterView.setBackground(new com.th3nekit.finegram.plugins.FGPluginAvatar(getContext(),
                    item.getId(), item.getIcon(), 0.3f));
            letterView.setVisibility(VISIBLE);
            iconView.setVisibility(GONE);
            FGPluginIcon.load(iconView, item.getIcon(), 52, () -> letterView.setVisibility(GONE));

            nameView.setText(item.getName());
            authorView.setText(item.getAuthor());

            setOnClickListener(v -> {
                if (delegate != null) {
                    delegate.onPicked(item);
                }
            });
            refresh(item);
        }

        void refresh(FGStore.Item item) {
            nameView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlackText));
            authorView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteGrayText2));

            final int text;
            if (FGStoreInstaller.isRunning(item.getId())) {
                text = R.string.FG_Store_Installing;
            } else if (FGStoreInstaller.hasUpdate(item)) {
                text = R.string.FG_Store_Update;
            } else if (FGStoreInstaller.isInstalled(item)) {
                text = R.string.FG_Store_Installed;
            } else {
                text = R.string.FG_Store_Install;
            }
            tagView.setText(getString(text));
            tagView.setTextColor(color);
            invalidate();
        }

        private void animatePress(boolean pressed) {
            if (pressAnimator != null) {
                pressAnimator.cancel();
            }
            pressAnimator = ValueAnimator.ofFloat(pressScale, pressed ? 0.96f : 1f);
            pressAnimator.setDuration(150);
            pressAnimator.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
            pressAnimator.addUpdateListener(animation -> {
                pressScale = (float) animation.getAnimatedValue();
                invalidate();
            });
            pressAnimator.start();
        }

        @Override
        public boolean onTouchEvent(@NonNull MotionEvent event) {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    animatePress(true);
                    break;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    animatePress(false);
                    break;
            }
            return super.onTouchEvent(event);
        }

        @Override
        protected void onDetachedFromWindow() {
            super.onDetachedFromWindow();
            if (pressAnimator != null) {
                pressAnimator.cancel();
                pressAnimator = null;
            }
        }

        @Override
        protected void dispatchDraw(@NonNull Canvas canvas) {
            final int w = getMeasuredWidth();
            final int h = getMeasuredHeight();
            canvas.save();
            canvas.scale(pressScale, pressScale, w / 2f, h / 2f);

            if (gradientHeight != h) {
                gradientHeight = h;
                backPaint.setShader(new LinearGradient(0, 0, 0, h,
                        Theme.multAlpha(color, 0.22f), Theme.multAlpha(color, 0.07f), Shader.TileMode.CLAMP));
            }
            rect.set(0, 0, w, h);
            canvas.drawRoundRect(rect, dp(18), dp(18), backPaint);

            tagRect.set(dp(16), h - dp(44), w - dp(16), h - dp(16));
            tagPaint.setColor(Theme.multAlpha(color, 0.18f));
            canvas.drawRoundRect(tagRect, dp(14), dp(14), tagPaint);

            super.dispatchDraw(canvas);
            canvas.restore();
        }
    }

    public static class Factory extends UItem.UItemFactory<FGStoreFeatured> {
        static { setup(new Factory()); }

        @Override
        public boolean isClickable() {
            return false;
        }

        @Override
        public FGStoreFeatured createView(Context context, RecyclerListView listView, int currentAccount,
                                          int classGuid, Theme.ResourcesProvider resourcesProvider) {
            return new FGStoreFeatured(context);
        }

        @Override
        public void bindView(View view, UItem item, boolean divider, UniversalAdapter adapter, UniversalRecyclerView listView) {
            final Data data = (Data) item.object;
            ((FGStoreFeatured) view).set(data.items, data.total, data.delegate);
        }

        public static UItem of(int id, Data data) {
            UItem item = UItem.ofFactory(Factory.class);
            item.id = id;
            item.object = data;
            return item;
        }

        public static class Data {
            public final List<FGStore.Item> items;
            public final int total;
            public final Delegate delegate;

            public Data(List<FGStore.Item> items, int total, Delegate delegate) {
                this.items = items;
                this.total = total;
                this.delegate = delegate;
            }
        }
    }
}

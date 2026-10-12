/*
 * Finegram plugin store.
 * Adapted from Kangel Plugins Manager (KangelPlugins).
 * Upstream: https://git.kangel.xyz/KangelPlugins/PluginManager
 * Licensed under GNU GPL v3; see LICENSE.PluginManager and NOTICE.
 */

package com.th3nekit.finegram.store;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
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

public class FGStoreCell extends FrameLayout {

    public enum State { INSTALL, UPDATE, INSTALLED, RUNNING }

    private static final long PRESS_MS = 160;
    private static final long SPIN_MS = 900;

    private static final int ICON_DP = 46;
    private static final int ICON_RADIUS_DP = 14;
    private static final int HEIGHT_MIN_DP = 76;
    private static final int HEIGHT_MAX_DP = 94;
    private static final int DESCRIPTION_LINES = 3;
    private static final int TEXT_LEFT_DP = 16 + ICON_DP + 14;

    private final Paint avatarPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint buttonPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint buttonStroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint spinnerPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint dividerPaint = new Paint();
    private final RectF avatar = new RectF();
    private final RectF buttonRect = new RectF();
    private final RectF spinnerRect = new RectF();

    private final TextView letterView;
    private final TextView nameView;
    private final TextView statusView;
    private final BackupImageView iconView;
    private final TextView buttonText;

    private final Theme.ResourcesProvider resourcesProvider;

    private State state = State.INSTALL;
    private boolean needDivider;
    private boolean hasOwnIcon;
    private Runnable onAction;

    private CharSequence lastButtonText;
    private boolean pressedOnButton;
    private float pressScale = 1f;
    private ValueAnimator pressAnimator;
    private ValueAnimator spinAnimator;
    private float spinAngle;

    public FGStoreCell(@NonNull Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;
        setWillNotDraw(false);

        spinnerPaint.setStyle(Paint.Style.STROKE);
        spinnerPaint.setStrokeCap(Paint.Cap.ROUND);
        spinnerPaint.setStrokeWidth(dp(2));

        buttonStroke.setStyle(Paint.Style.STROKE);
        buttonStroke.setStrokeWidth(dp(1.5f));

        letterView = new TextView(context);
        letterView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 19);
        letterView.setTypeface(AndroidUtilities.bold());
        letterView.setTextColor(0xFFFFFFFF);
        letterView.setGravity(Gravity.CENTER);
        addView(letterView, LayoutHelper.createFrame(ICON_DP, ICON_DP,
                Gravity.LEFT | Gravity.CENTER_VERTICAL, 16, 0, 0, 0));

        iconView = new BackupImageView(context);
        iconView.setRoundRadius(dp(ICON_RADIUS_DP));
        iconView.setVisibility(GONE);
        addView(iconView, LayoutHelper.createFrame(ICON_DP, ICON_DP,
                Gravity.LEFT | Gravity.CENTER_VERTICAL, 16, 0, 0, 0));

        nameView = new TextView(context);
        nameView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 16);
        nameView.setTypeface(AndroidUtilities.bold());
        nameView.setLines(1);
        nameView.setEllipsize(TextUtils.TruncateAt.END);
        addView(nameView, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT, LayoutHelper.WRAP_CONTENT,
                Gravity.LEFT | Gravity.TOP, TEXT_LEFT_DP, 12, 0, 0));

        statusView = new TextView(context);
        statusView.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 13);
        statusView.setMaxLines(DESCRIPTION_LINES);
        statusView.setLineSpacing(dp(1), 1f);
        statusView.setEllipsize(TextUtils.TruncateAt.END);
        addView(statusView, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT, LayoutHelper.WRAP_CONTENT,
                Gravity.LEFT | Gravity.TOP, TEXT_LEFT_DP, 34, 0, 0));

        buttonText = new TextView(context);
        buttonText.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 12);
        buttonText.setTypeface(AndroidUtilities.bold());
        buttonText.setGravity(Gravity.CENTER);
        buttonText.setSingleLine(true);

        buttonText.setPadding(dp(14), 0, dp(14), 0);
        addView(buttonText, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT, 28,
                Gravity.RIGHT | Gravity.TOP, 0, 12, 16, 0));
    }

    public void set(FGStore.Item item, State state, boolean divider) {
        this.state = state;
        this.needDivider = divider;

        nameView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider));
        statusView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteGrayText2, resourcesProvider));
        dividerPaint.setColor(Theme.getColor(Theme.key_divider, resourcesProvider));

        nameView.setText(item.getName());
        statusView.setText(subtitle(item));

        hasOwnIcon = false;
        letterView.setText("");
        letterView.setBackground(new com.th3nekit.finegram.plugins.FGPluginAvatar(getContext(),
                item.getId(), item.getIcon(), ICON_RADIUS_DP / (float) ICON_DP));
        letterView.setVisibility(VISIBLE);
        iconView.setVisibility(GONE);
        iconView.setImageDrawable(null);
        FGPluginIcon.load(iconView, item.getIcon(), ICON_DP, () -> {
            letterView.setVisibility(GONE);
            hasOwnIcon = true;
            invalidate();
        });

        applyState();
    }

    private CharSequence subtitle(FGStore.Item item) {
        if (!item.getDescription().trim().isEmpty()) {
            return item.getDescription().trim();
        }
        StringBuilder text = new StringBuilder();
        if (!item.getVersion().isEmpty()) {
            text.append(item.getVersion());
        }
        if (!item.getAuthor().isEmpty()) {
            if (text.length() > 0) {
                text.append(" · ");
            }
            text.append(item.getAuthor());
        }
        if (item.getSizeBytes() > 0) {
            if (text.length() > 0) {
                text.append(" · ");
            }
            text.append(AndroidUtilities.formatFileSize(item.getSizeBytes()));
        }
        return text;
    }

    private void applyState() {
        final int accent = Theme.getColor(Theme.key_featuredStickers_addButton, resourcesProvider);
        switch (state) {
            case UPDATE:
                buttonText.setText(LocaleController.getString(R.string.FG_Store_Update));
                buttonText.setTextColor(0xFFFFFFFF);
                buttonPaint.setColor(accent);
                buttonStroke.setColor(0x00000000);
                break;
            case INSTALLED:
                buttonText.setText(LocaleController.getString(R.string.FG_Store_Installed));
                buttonText.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteGrayText2, resourcesProvider));
                buttonPaint.setColor(0x00000000);
                buttonStroke.setColor(Theme.multAlpha(
                        Theme.getColor(Theme.key_windowBackgroundWhiteGrayText2, resourcesProvider), 0.35f));
                break;
            case RUNNING:
                buttonText.setText("");
                spinnerPaint.setColor(accent);
                buttonPaint.setColor(Theme.multAlpha(accent, 0.12f));
                buttonStroke.setColor(0x00000000);
                startSpin();
                break;
            case INSTALL:
            default:
                buttonText.setText(LocaleController.getString(R.string.FG_Store_Install));
                buttonText.setTextColor(0xFFFFFFFF);
                buttonPaint.setColor(accent);
                buttonStroke.setColor(0x00000000);
                break;
        }
        if (state != State.RUNNING) {
            stopSpin();
        }

        final CharSequence now = buttonText.getText();
        if (!TextUtils.equals(now, lastButtonText)) {
            lastButtonText = now;
            requestLayout();
        }
        invalidate();
    }

    public void setOnAction(Runnable action) {
        this.onAction = action;
    }

    private void startSpin() {
        if (spinAnimator != null) {
            return;
        }
        spinAnimator = ValueAnimator.ofFloat(0f, 360f);
        spinAnimator.setDuration(SPIN_MS);
        spinAnimator.setRepeatCount(ValueAnimator.INFINITE);
        spinAnimator.setInterpolator(null);
        spinAnimator.addUpdateListener(animation -> {
            spinAngle = (float) animation.getAnimatedValue();
            invalidate();
        });
        spinAnimator.start();
    }

    private void stopSpin() {
        if (spinAnimator != null) {
            spinAnimator.cancel();
            spinAnimator = null;
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        stopSpin();
        if (pressAnimator != null) {
            pressAnimator.cancel();
            pressAnimator = null;
        }
    }

    private void animatePress(boolean pressed) {
        if (pressAnimator != null) {
            pressAnimator.cancel();
        }
        pressAnimator = ValueAnimator.ofFloat(pressScale, pressed ? 0.95f : 1f);
        pressAnimator.setDuration(PRESS_MS);
        pressAnimator.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
        pressAnimator.addUpdateListener(animation -> {
            pressScale = (float) animation.getAnimatedValue();
            invalidate();
        });
        pressAnimator.start();
    }

    private float buttonWidth() {
        final CharSequence text = buttonText.getText();
        if (text == null || text.length() == 0) {
            return dp(52);
        }
        return buttonText.getPaint().measureText(text.toString()) + dp(28);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        final int width = MeasureSpec.getSize(widthMeasureSpec);

        final int textWidth = Math.max(dp(40),
                (int) (width - dp(TEXT_LEFT_DP) - buttonWidth() - dp(16) - dp(12)));
        getLayoutParams(nameView).width = textWidth;
        getLayoutParams(statusView).width = textWidth;

        statusView.measure(MeasureSpec.makeMeasureSpec(textWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        final int needed = dp(34) + statusView.getMeasuredHeight() + dp(12);
        final int height = Math.max(dp(HEIGHT_MIN_DP), Math.min(dp(HEIGHT_MAX_DP), needed));
        super.onMeasure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));
    }

    private static FrameLayout.LayoutParams getLayoutParams(View view) {
        return (FrameLayout.LayoutParams) view.getLayoutParams();
    }

    @Override
    protected void onDraw(@NonNull Canvas canvas) {
        final float centerY = getMeasuredHeight() / 2f;

        final float width = buttonWidth();
        final float top = dp(12);
        buttonRect.set(getMeasuredWidth() - dp(16) - width, top,
                getMeasuredWidth() - dp(16), top + dp(28));
        canvas.save();
        canvas.scale(pressScale, pressScale, buttonRect.centerX(), buttonRect.centerY());
        if (buttonPaint.getColor() != 0) {
            canvas.drawRoundRect(buttonRect, dp(14), dp(14), buttonPaint);
        }
        if (buttonStroke.getColor() != 0) {
            canvas.drawRoundRect(buttonRect, dp(14), dp(14), buttonStroke);
        }
        canvas.restore();

        if (state == State.RUNNING) {
            final float r = dp(8);
            spinnerRect.set(buttonRect.centerX() - r, buttonRect.centerY() - r,
                    buttonRect.centerX() + r, buttonRect.centerY() + r);
            canvas.drawArc(spinnerRect, spinAngle, 100, false, spinnerPaint);
        }

        if (needDivider) {
            canvas.drawLine(dp(TEXT_LEFT_DP), getMeasuredHeight() - 1,
                    getMeasuredWidth() - dp(16), getMeasuredHeight() - 1, dividerPaint);
        }
    }

    @Override
    public boolean onTouchEvent(@NonNull MotionEvent event) {
        boolean inside = buttonRect.contains(event.getX(), event.getY());
        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                if (!inside || state == State.INSTALLED || state == State.RUNNING) {
                    return false;
                }
                pressedOnButton = true;
                animatePress(true);
                return true;
            case MotionEvent.ACTION_UP:
                if (pressedOnButton) {
                    pressedOnButton = false;
                    animatePress(false);
                    if (inside && onAction != null) {
                        onAction.run();
                    }
                    return true;
                }
                return false;
            case MotionEvent.ACTION_CANCEL:
                if (pressedOnButton) {
                    pressedOnButton = false;
                    animatePress(false);
                }
                return false;
        }
        return pressedOnButton;
    }

    public static class Factory extends UItem.UItemFactory<FGStoreCell> {
        static { setup(new Factory()); }

        @Override
        public FGStoreCell createView(Context context, RecyclerListView listView, int currentAccount,
                                      int classGuid, Theme.ResourcesProvider resourcesProvider) {
            return new FGStoreCell(context, resourcesProvider);
        }

        @Override
        public void bindView(View view, UItem item, boolean divider, UniversalAdapter adapter, UniversalRecyclerView listView) {
            Data data = (Data) item.object;
            FGStoreCell cell = (FGStoreCell) view;
            cell.set(data.item, data.state, divider);
            cell.setOnAction(data.onAction);
        }

        public static UItem of(int id, Data data) {
            UItem item = UItem.ofFactory(Factory.class);
            item.id = id;
            item.object = data;
            return item;
        }

        public static class Data {
            public final FGStore.Item item;
            public final State state;
            public final Runnable onAction;

            public Data(FGStore.Item item, State state, Runnable onAction) {
                this.item = item;
                this.state = state;
                this.onAction = onAction;
            }
        }
    }
}

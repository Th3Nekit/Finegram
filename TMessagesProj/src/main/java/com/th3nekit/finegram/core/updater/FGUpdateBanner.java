/*
 * Finegram for Android, licensed under GNU GPL v. 2 or later.
 */
package com.th3nekit.finegram.core.updater;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.WindowInsets;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.Theme;

import java.util.function.IntConsumer;

/** A reserved row below the app content, shared by every chat and settings screen. */
public final class FGUpdateBanner extends FrameLayout {
    private final TextView label;
    private final Paint progressPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final IntConsumer onHeightChanged;
    private final FGUpdateDownload.Listener listener = this::update;
    private FGUpdateDownload.State state;
    private int bottomInset;
    private int reservedHeight;

    public FGUpdateBanner(Context context, Runnable onOpen, IntConsumer onHeightChanged) {
        super(context);
        this.onHeightChanged = onHeightChanged;
        setWillNotDraw(false);
        setVisibility(GONE);
        label = new TextView(context);
        label.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        label.setSingleLine(true);
        label.setEllipsize(TextUtils.TruncateAt.END);
        label.setGravity(Gravity.CENTER_VERTICAL);
        label.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        LayoutParams params = new LayoutParams(LayoutParams.MATCH_PARENT, AndroidUtilities.dp(40), Gravity.TOP);
        params.leftMargin = params.rightMargin = AndroidUtilities.dp(16);
        addView(label, params);
        setOnClickListener(v -> onOpen.run());
        setFocusable(true);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        FGUpdateDownload.addListener(listener);
        update(FGUpdateDownload.getState());
    }

    @Override
    protected void onDetachedFromWindow() {
        FGUpdateDownload.removeListener(listener);
        super.onDetachedFromWindow();
    }

    private void update(FGUpdateDownload.State value) {
        state = value;
        boolean visible = value.update != null && value.phase != FGUpdateDownload.Phase.IDLE;
        int nextHeight = visible ? AndroidUtilities.dp(40) : 0;
        setVisibility(visible ? VISIBLE : GONE);
        if (reservedHeight != nextHeight) {
            reservedHeight = nextHeight;
            onHeightChanged.accept(nextHeight);
        }
        if (visible) {
            setBackgroundColor(Theme.getColor(Theme.key_windowBackgroundWhite));
            label.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlackText));
            String text = LocaleController.getString(R.string.UP_Category_Updates) + " · " + statusText(value, false);
            label.setText(text);
            setContentDescription(text);
            invalidate();
        }
    }

    @Override
    public WindowInsets onApplyWindowInsets(WindowInsets insets) {
        WindowInsetsCompat compat = WindowInsetsCompat.toWindowInsetsCompat(insets, this);
        Insets bars = compat.getInsets(WindowInsetsCompat.Type.systemBars());
        if (getPaddingLeft() != bars.left || getPaddingRight() != bars.right) {
            setPadding(bars.left, 0, bars.right, 0);
            invalidate();
        }
        int next = compat.isVisible(WindowInsetsCompat.Type.ime()) ? 0 : bars.bottom;
        if (bottomInset != next) {
            bottomInset = next;
            requestLayout();
        }
        return insets;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(AndroidUtilities.dp(40) + bottomInset, MeasureSpec.EXACTLY));
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        super.dispatchDraw(canvas);
        if (state == null) return;
        float y = AndroidUtilities.dp(40) - AndroidUtilities.dp(3);
        float left = getPaddingLeft();
        float width = Math.max(0, getWidth() - getPaddingLeft() - getPaddingRight());
        int accent = Theme.getColor(Theme.key_windowBackgroundWhiteBlueText);
        progressPaint.setColor(Theme.multAlpha(accent, .16f));
        canvas.drawRect(left, y, left + width, y + AndroidUtilities.dp(3), progressPaint);
        float fraction = state.phase == FGUpdateDownload.Phase.READY ? 1
                : state.total > 0 ? Math.max(0, Math.min(1, (float) state.downloaded / state.total)) : 0;
        progressPaint.setColor(accent);
        canvas.drawRect(left, y, left + width * fraction, y + AndroidUtilities.dp(3), progressPaint);
    }

    public static String statusText(FGUpdateDownload.State state, boolean details) {
        String bytes = AndroidUtilities.formatFileSize(state.downloaded);
        if (state.total > 0) bytes += " / " + AndroidUtilities.formatFileSize(state.total);
        switch (state.phase) {
            case DOWNLOADING:
                return state.total > 0
                        ? LocaleController.formatString(R.string.FG_Update_Downloading,
                                Math.min(100, (int) (state.downloaded * 100L / state.total))) + (details ? " · " + bytes : "")
                        : LocaleController.formatString(R.string.FG_Update_Bytes, bytes);
            case VERIFYING:
                return LocaleController.getString(R.string.FG_Update_Verifying);
            case PAUSED:
                return LocaleController.getString(R.string.FG_Update_Paused) + (details ? " · " + bytes : "");
            case READY:
                return LocaleController.getString(R.string.FG_Update_Ready);
            case FAILED:
                return LocaleController.getString(R.string.FG_Update_DownloadFailed)
                        + (details && !TextUtils.isEmpty(state.error) ? "\n" + state.error : "")
                        + (details && state.downloaded > 0 ? "\n" + LocaleController.getString(R.string.FG_Update_RetryHint) : "");
            default:
                return "";
        }
    }
}

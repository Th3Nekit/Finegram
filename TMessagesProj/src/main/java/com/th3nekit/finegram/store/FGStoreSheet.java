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
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.NonNull;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.BuildVars;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.BottomSheet;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.BackupImageView;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.LayoutHelper;

import com.th3nekit.finegram.plugins.FGPluginIcon;

import java.util.ArrayList;
import java.util.List;

public class FGStoreSheet extends BottomSheet {

    public interface Delegate {
        void onInstall(FGStore.Item item);
    }

    private static final int DESCRIPTION_MAX_DP = 210;

    private final FGStore.Item item;
    private final int color;

    private TextView letter;

    public FGStoreSheet(Context context, Theme.ResourcesProvider resourcesProvider,
                        FGStore.Item item, Delegate delegate) {
        super(context, false, resourcesProvider);
        this.item = item;
        this.color = FGPluginIcon.accentFor(item.getId());

        fixNavigationBar();
        setApplyBottomPadding(false);

        final LinearLayout container = new LinearLayout(context);
        container.setOrientation(LinearLayout.VERTICAL);
        container.setPadding(dp(22), dp(16), dp(22), dp(12));

        container.addView(createHead(context),
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 64));

        final List<String[]> facts = factsOf();
        if (!facts.isEmpty()) {
            container.addView(createFacts(context, facts),
                    LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 58, 0, 16, 0, 0));
        }

        final TextView description = new TextView(context);
        description.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 15);
        description.setLineSpacing(dp(2), 1f);
        description.setTextColor(getThemedColor(Theme.key_dialogTextBlack));
        description.setText(item.getDescription().trim().isEmpty()
                ? getString(R.string.FG_Store_NoDesc) : item.getDescription().trim());

        final ScrollView scroll = new ScrollView(context) {
            @Override
            protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(
                        dp(DESCRIPTION_MAX_DP), MeasureSpec.AT_MOST));
            }
        };
        scroll.setVerticalScrollBarEnabled(false);
        scroll.setOverScrollMode(View.OVER_SCROLL_NEVER);
        scroll.addView(description, new ScrollView.LayoutParams(
                LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
        container.addView(scroll, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT,
                LayoutHelper.WRAP_CONTENT, 0, 16, 0, 0));

        addNote(context, container, item.getRequirements(), R.string.FG_Store_Requires);
        addNote(context, container, item.getDependencies(), R.string.FG_Store_Depends);

        final String needs = item.getMinAppVersion();
        final boolean tooOld = !needs.isEmpty()
                && FGStoreInstaller.isNewer(needs, BuildVars.BUILD_VERSION_STRING);

        final TextView safety = new TextView(context);
        safety.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 13);
        safety.setTextColor(getThemedColor(tooOld ? Theme.key_text_RedBold : Theme.key_dialogTextGray2));
        safety.setText(tooOld
                ? formatString(R.string.FG_Store_NeedsApp, needs)
                : getString(item.getSignature().isEmpty()
                        ? R.string.FG_Store_Unverified : R.string.FG_Store_Signed));
        container.addView(safety, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT,
                LayoutHelper.WRAP_CONTENT, 0, 14, 0, 0));

        final TextView button = createButton(context, delegate);
        container.addView(button, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 48, 0, 14, 0, 0));

        setCustomView(container);
    }

    private View createHead(Context context) {
        final FrameLayout head = new FrameLayout(context);

        final FrameLayout iconWrap = new FrameLayout(context) {
            private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
            private final RectF rect = new RectF();

            @Override
            protected void dispatchDraw(@NonNull Canvas canvas) {
                super.dispatchDraw(canvas);
            }
        };
        iconWrap.setWillNotDraw(false);

        letter = new TextView(context);
        letter.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 26);
        letter.setTypeface(AndroidUtilities.bold());
        letter.setTextColor(0xFFFFFFFF);
        letter.setGravity(Gravity.CENTER);
        letter.setText("");
        letter.setBackground(new com.th3nekit.finegram.plugins.FGPluginAvatar(context,
                item.getId(), item.getIcon(), 18f / 64f));
        iconWrap.addView(letter, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.MATCH_PARENT));

        final BackupImageView icon = new BackupImageView(context);
        icon.setRoundRadius(dp(18));
        icon.setVisibility(View.GONE);
        iconWrap.addView(icon, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.MATCH_PARENT));
        FGPluginIcon.load(icon, item.getIcon(), 64, () -> {
            letter.setVisibility(View.GONE);
            iconWrap.invalidate();
        });

        head.addView(iconWrap, LayoutHelper.createFrame(64, 64, Gravity.LEFT | Gravity.TOP));

        final TextView name = new TextView(context);
        name.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 19);
        name.setTypeface(AndroidUtilities.bold());
        name.setTextColor(getThemedColor(Theme.key_dialogTextBlack));
        name.setLines(1);
        name.setEllipsize(TextUtils.TruncateAt.END);
        name.setText(item.getName());
        head.addView(name, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                Gravity.LEFT | Gravity.TOP, 78, 8, 0, 0));

        final TextView author = new TextView(context);
        author.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 14);
        author.setTextColor(getThemedColor(Theme.key_dialogTextGray2));
        author.setLines(1);
        author.setEllipsize(TextUtils.TruncateAt.END);
        author.setText(item.getAuthor().isEmpty()
                ? FGStoreTitles.of(item.getCategory()) : item.getAuthor());
        head.addView(author, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                Gravity.LEFT | Gravity.TOP, 78, 34, 0, 0));

        return head;
    }

    private List<String[]> factsOf() {
        final List<String[]> facts = new ArrayList<>();
        if (!item.getVersion().isEmpty()) {
            facts.add(new String[]{getString(R.string.FG_Store_Version), item.getVersion()});
        }
        if (item.getSizeBytes() > 0) {
            facts.add(new String[]{getString(R.string.FG_Store_Size),
                    AndroidUtilities.formatFileSize(item.getSizeBytes())});
        }
        facts.add(new String[]{getString(R.string.FG_Store_Section), FGStoreTitles.of(item.getCategory())});
        return facts;
    }

    private View createFacts(Context context, List<String[]> facts) {
        final LinearLayout row = new LinearLayout(context);
        row.setOrientation(LinearLayout.HORIZONTAL);
        for (int i = 0; i < facts.size(); i++) {
            row.addView(createFact(context, facts.get(i)),
                    LayoutHelper.createLinear(0, LayoutHelper.MATCH_PARENT, 1f,
                            i == 0 ? 0 : 8, 0, 0, 0));
        }
        return row;
    }

    private View createFact(Context context, String[] fact) {
        final LinearLayout cell = new LinearLayout(context) {
            private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
            private final RectF rect = new RectF();

            @Override
            protected void dispatchDraw(@NonNull Canvas canvas) {
                rect.set(0, 0, getMeasuredWidth(), getMeasuredHeight());
                paint.setColor(Theme.multAlpha(color, 0.14f));
                canvas.drawRoundRect(rect, dp(14), dp(14), paint);
                super.dispatchDraw(canvas);
            }
        };
        cell.setWillNotDraw(false);
        cell.setOrientation(LinearLayout.VERTICAL);
        cell.setGravity(Gravity.CENTER);

        final TextView value = new TextView(context);
        value.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 15);
        value.setTypeface(AndroidUtilities.bold());
        value.setTextColor(getThemedColor(Theme.key_dialogTextBlack));
        value.setGravity(Gravity.CENTER);
        value.setLines(1);
        value.setEllipsize(TextUtils.TruncateAt.END);
        value.setText(fact[1]);
        cell.addView(value, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                6, 0, 6, 0));

        final TextView title = new TextView(context);
        title.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 11);
        title.setTextColor(getThemedColor(Theme.key_dialogTextGray2));
        title.setGravity(Gravity.CENTER);
        title.setLines(1);
        title.setText(fact[0]);
        cell.addView(title, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT,
                6, 2, 6, 0));

        return cell;
    }

    private void addNote(Context context, LinearLayout container, List<String> values, int pattern) {
        if (values == null || values.isEmpty()) {
            return;
        }
        final TextView note = new TextView(context);
        note.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 13);
        note.setTextColor(getThemedColor(Theme.key_dialogTextGray2));
        note.setText(formatString(pattern, TextUtils.join(", ", values)));
        container.addView(note, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT,
                LayoutHelper.WRAP_CONTENT, 0, 12, 0, 0));
    }

    private TextView createButton(Context context, Delegate delegate) {
        final TextView view = new TextView(context) {
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
        view.setGravity(Gravity.CENTER);
        view.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 15);
        view.setTypeface(AndroidUtilities.bold());

        final boolean installed = FGStoreInstaller.isInstalled(item) && !FGStoreInstaller.hasUpdate(item);
        if (installed) {

            view.setText(getString(R.string.FG_Store_Installed));
            view.setTextColor(getThemedColor(Theme.key_dialogTextGray2));
            view.setBackground(Theme.createRoundRectDrawable(dp(10),
                    Theme.multAlpha(getThemedColor(Theme.key_dialogTextGray2), 0.12f)));
        } else {
            view.setText(getString(FGStoreInstaller.hasUpdate(item)
                    ? R.string.FG_Store_Update : R.string.FG_Store_Install));
            view.setTextColor(getThemedColor(Theme.key_featuredStickers_buttonText));
            view.setBackground(Theme.createSimpleSelectorRoundRectDrawable(dp(10),
                    getThemedColor(Theme.key_featuredStickers_addButton),
                    getThemedColor(Theme.key_featuredStickers_addButtonPressed)));
            view.setOnClickListener(v -> {
                dismiss();
                if (delegate != null) {
                    delegate.onInstall(item);
                }
            });
        }
        return view;
    }
}

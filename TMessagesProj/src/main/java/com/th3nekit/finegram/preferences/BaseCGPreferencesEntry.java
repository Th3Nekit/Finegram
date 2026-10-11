/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.preferences;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.os.Parcelable;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import android.net.Uri;
import org.telegram.ui.Components.ItemOptions;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.messenger.SharedConfig;
import org.telegram.ui.ActionBar.ActionBar;
import org.telegram.ui.ActionBar.ActionBarMenuItem;
import org.telegram.ui.ActionBar.BackDrawable;
import org.telegram.ui.ActionBar.INavigationLayout;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.ActionBar.ThemeDescription;
import androidx.core.graphics.ColorUtils;
import org.telegram.ui.ChatActivity;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.SizeNotifierFrameLayout;
import org.telegram.ui.Components.UItem;
import org.telegram.ui.Components.UniversalAdapter;
import org.telegram.ui.Components.UniversalFragment;
import org.telegram.ui.Components.UniversalRecyclerView;
import org.telegram.ui.Components.blur3.DownscaleScrollableNoiseSuppressor;
import org.telegram.ui.Components.blur3.ViewGroupPartRenderer;
import org.telegram.ui.Components.blur3.RenderNodeWithHash;
import org.telegram.ui.Components.blur3.capture.IBlur3Hash;
import org.telegram.ui.Components.blur3.capture.IBlur3Capture;
import org.telegram.ui.Components.blur3.source.BlurredBackgroundSourceRenderNode;

import java.util.ArrayList;

import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig;
import com.th3nekit.finegram.core.ui.FGBulletinCreator;

public class BaseCGPreferencesEntry extends UniversalFragment {

    protected SizeNotifierFrameLayout contentView;
    protected UniversalRecyclerView listView;
    protected LinearLayoutManager layoutManager;
    protected Theme.ResourcesProvider resourcesProvider;
    protected View actionBarBackground;
    protected FrameLayout actionBarContainer;
    protected ActionBarMenuItem searchItem;
    private Parcelable savedListState;

    public BaseCGPreferencesEntry() {
        super();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            scrollableViewNoiseSuppressor = new DownscaleScrollableNoiseSuppressor();
            iBlur3SourceGlassFrosted = new BlurredBackgroundSourceRenderNode(null);
            iBlur3SourceGlassFrosted.setupRenderer(new RenderNodeWithHash.Renderer() {
                @Override
                public void renderNodeCalculateHash(IBlur3Hash hash) {
                    hash.add(getThemedColor(Theme.key_windowBackgroundWhite));
                    hash.add(SharedConfig.chatBlurEnabled());
                }

                @Override
                public void renderNodeUpdateDisplayList(Canvas canvas) {
                    canvas.drawColor(getThemedColor(Theme.key_windowBackgroundWhite));
                    if (SharedConfig.chatBlurEnabled()) {
                        scrollableViewNoiseSuppressor.draw(canvas, DownscaleScrollableNoiseSuppressor.DRAW_FROSTED_GLASS);
                    }
                }
            });
        } else {
            scrollableViewNoiseSuppressor = null;
            iBlur3SourceGlassFrosted = null;
        }
    }

    @Override
    public View createView(Context context) {
        contentView = new SizeNotifierFrameLayout(context) {
            @Override
            protected void dispatchDraw(Canvas canvas) {
                if (Build.VERSION.SDK_INT >= 31 && scrollableViewNoiseSuppressor != null) {
                    blur3_InvalidateBlur();

                    final int width = getMeasuredWidth();
                    final int height = getMeasuredHeight();
                    if (iBlur3SourceGlassFrosted != null && !iBlur3SourceGlassFrosted.inRecording()) {
                        iBlur3SourceGlassFrosted.setSize(width, height);
                        iBlur3SourceGlassFrosted.updateDisplayListIfNeeded();
                    }
                }
                super.dispatchDraw(canvas);
            }

            @Override
            public void drawBlurRect(Canvas canvas, float y, Rect rectTmp, Paint blurScrimPaint, boolean top) {
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q || !SharedConfig.chatBlurEnabled() || iBlur3SourceGlassFrosted == null) {
                    canvas.drawRect(rectTmp, blurScrimPaint);
                    return;
                }

                canvas.save();
                canvas.translate(0, -y);
                iBlur3SourceGlassFrosted.draw(canvas, rectTmp.left, rectTmp.top + y, rectTmp.right, rectTmp.bottom + y);
                canvas.restore();

                final int oldScrimAlpha = blurScrimPaint.getAlpha();
                blurScrimPaint.setAlpha(ChatActivity.ACTION_BAR_BLUR_ALPHA);
                canvas.drawRect(rectTmp, blurScrimPaint);
                blurScrimPaint.setAlpha(oldScrimAlpha);
            }
        };
        contentView.setBackgroundColor(getThemedColor(Theme.key_windowBackgroundGray));

        listView = new UniversalRecyclerView(this, this::fillItems, this::onClick, this::onSettingLongClick);

        if (listView.getLayoutManager() instanceof LinearLayoutManager) {
            layoutManager = (LinearLayoutManager) listView.getLayoutManager();
        }
        listView.adapter.setApplyBackground(false);
        listView.setClipToPadding(false);
        listView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(@NonNull RecyclerView recyclerView, int dx, int dy) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && scrollableViewNoiseSuppressor != null) {
                    scrollableViewNoiseSuppressor.onScrolled(dx, dy);
                    blur3_InvalidateBlur();
                }
            }
        });
        iBlur3Capture = new ViewGroupPartRenderer(listView, contentView, listView::drawChild);
        listView.addEdgeEffectListener(() -> listView.postOnAnimation(this::blur3_InvalidateBlur));
        listView.setSections();
        listView.setPadding(0, needActionBarPadding() ? ActionBar.getCurrentActionBarHeight() : AndroidUtilities.dp(16), 0, 0);
        contentView.addView(listView, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.MATCH_PARENT, Gravity.FILL));

        actionBarBackground = new View(context) {
            private final Paint blurScrimPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
            private final Paint edgePaint = new Paint(Paint.ANTI_ALIAS_FLAG);

            @Override
            protected void onDraw(@NonNull Canvas canvas) {
                var top = actionBarContainer.getHeight();
                AndroidUtilities.rectTmp2.set(0, 0, getMeasuredWidth(), top);
                blurScrimPaint.setColor(getThemedColor(Theme.key_windowBackgroundWhite));
                contentView.drawBlurRect(canvas, 0, AndroidUtilities.rectTmp2, blurScrimPaint, true);
                edgePaint.setColor(getThemedColor(Theme.key_windowBackgroundWhiteBlueText));
                edgePaint.setAlpha(Theme.isCurrentThemeDark() ? 44 : 26);
                canvas.drawRect(AndroidUtilities.dp(16), top - AndroidUtilities.dpf2(0.75f), getMeasuredWidth() - AndroidUtilities.dp(16), top, edgePaint);
                if (getParentLayout() != null) {
                    getParentLayout().drawHeaderShadow(canvas, top);
                }
            }
        };
        contentView.addView(actionBarBackground, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, 200, Gravity.TOP));
        actionBarContainer = new FrameLayout(context);
        actionBarContainer.addView(actionBar, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, Gravity.FILL_HORIZONTAL | Gravity.BOTTOM));
        contentView.addView(actionBarContainer, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, Gravity.FILL_HORIZONTAL | Gravity.TOP));

        listView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(@NonNull RecyclerView recyclerView, int dx, int dy) {
                updateActionBarVisible();
            }
        });

        updateActionBarVisible(true, false);

        restoreListPosition();

        if (pendingSettingSlug != null) {
            AndroidUtilities.runOnUIThread(this::scrollToPendingSlug, 300);
        }
        if (pendingSearchItemId != 0) {
            AndroidUtilities.runOnUIThread(this::scrollToPendingSetting, 300);
        }

        return fragmentView = contentView;
    }

    @Override
    public void clearViews() {
        if (layoutManager != null) {
            savedListState = layoutManager.onSaveInstanceState();
        }
        super.clearViews();
        listView = null;
        layoutManager = null;
    }

    private void restoreListPosition() {
        if (savedListState != null && layoutManager != null) {
            layoutManager.onRestoreInstanceState(savedListState);
            savedListState = null;
        }
    }

    protected boolean isSearchFieldVisible() {
        return searchItem != null && searchItem.isSearchFieldVisible2();
    }

    @Override
    public boolean onBackPressed(boolean invoked) {
        if (isSearchFieldVisible()) {
            if (invoked) actionBar.closeSearchField();
            return false;
        }
        return super.onBackPressed(invoked);
    }

    private boolean actionBarVisible;
    private ValueAnimator actionBarVisibleAnimator;

    protected void updateActionBarVisible() {
        updateActionBarVisible(false, true);
    }

    private void updateActionBarVisible(boolean force, boolean animated) {
        final boolean visible;
        if (isSearchFieldVisible()) {
            visible = true;
        } else if (listView.getChildCount() > 0) {
            var firstChild = listView.getChildAt(0);
            visible = needActionBarPadding() ? listView.canScrollVertically(-1) : (
                    listView.getChildAdapterPosition(firstChild) > 0 ||
                    firstChild.getY() + firstChild.getHeight() < actionBar.getHeight()
            );
        } else {
            visible = false;
        }
        if (actionBarVisible == visible && !force) return;

        actionBarVisible = visible;
        if (actionBarVisibleAnimator != null) {
            actionBarVisibleAnimator.cancel();
            actionBarVisibleAnimator = null;
        }
        if (!animated) {
            if (!needActionBarPadding())
                actionBar.getTitlesContainer().setAlpha(visible ? 1.0f : 0.0f);
            actionBarBackground.setAlpha(visible ? 1.0f : 0.0f);
        } else {
            actionBarVisibleAnimator = ValueAnimator.ofFloat(actionBarBackground.getAlpha(), visible ? 1.0f : 0.0f);
            actionBarVisibleAnimator.addUpdateListener(a -> {
                final float t = (float) a.getAnimatedValue();
                if (!needActionBarPadding()) actionBar.getTitlesContainer().setAlpha(t);
                actionBarBackground.setAlpha(t);
            });
            actionBarVisibleAnimator.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
            actionBarVisibleAnimator.setDuration(420);
            actionBarVisibleAnimator.start();
        }
    }

    protected CharSequence getTitle() {
        return null;
    }

    protected void fillItems(ArrayList<UItem> items, UniversalAdapter adapter) {

    }

    @Override
    protected void onClick(UItem item, View view, int position, float x, float y) {

    }

    private String pendingSettingSlug;
    private Runnable pendingSettingUnknown;

    protected String getKey() { return null; }

    public void scrollToRow(String slug, Runnable unknown) {
        pendingSettingSlug = slug;
        pendingSettingUnknown = unknown;
        if (listView != null) scrollToPendingSlug();
    }

    private void scrollToPendingSlug() {
        if (pendingSettingSlug == null || listView == null || layoutManager == null) return;
        String slug = pendingSettingSlug;
        Runnable unknown = pendingSettingUnknown;
        pendingSettingSlug = null;
        pendingSettingUnknown = null;
        int position = listView.findPositionByItemSlug(slug);
        if (position < 0) {
            if (unknown != null) unknown.run();
            return;
        }
        layoutManager.scrollToPositionWithOffset(position, AndroidUtilities.dp(80));
        listView.highlightRow(() -> listView.findPositionByItemSlug(slug));
    }

    private boolean onSettingLongClick(UItem item, View view, int position, float x, float y) {
        if (onLongClick(item, view, position, x, y)) return true;
        String page = getKey();
        if (page == null || item.slug == null || item.slug.isEmpty() || !item.enabled) return false;
        Uri.Builder link = new Uri.Builder().scheme("https").authority("t.me").appendPath("fgSettings");
        if (item.slug.startsWith("cg_")) {
            link.appendPath(item.slug.substring(3));
        } else {
            link.appendPath(page.startsWith("cg_") ? page.substring(3) : page).appendQueryParameter("r", item.slug);
        }
        ItemOptions.makeOptions(this, view)
                .setScrimViewBackground(listView.getClipBackground(view))
                .add(R.drawable.msg_copy, LocaleController.getString(R.string.CopyLink), () -> {
                    AndroidUtilities.addToClipboard(link.build().toString());
                    BulletinFactory.of(this).createCopyLinkBulletin().show();
                }).setMinWidth(190).show();
        return true;
    }

    private int pendingSearchItemId = 0;

    public BaseCGPreferencesEntry openAtSetting(int itemId) {
        pendingSearchItemId = itemId;
        return this;
    }

    public CharSequence getScreenTitleForSearch() {
        try {
            return getTitle();
        } catch (Throwable ignored) {
            return "";
        }
    }

    public ArrayList<UItem> collectItemsForSearch() {
        ArrayList<UItem> items = new ArrayList<>();
        try {
            fillItems(items, null);
        } catch (Throwable ignored) {

        }
        return items;
    }

    protected void scrollToPendingSetting() {
        final int itemId = pendingSearchItemId;
        pendingSearchItemId = 0;
        if (itemId == 0 || listView == null || listView.adapter == null || layoutManager == null) {
            return;
        }
        int position = listView.findPositionByItemId(itemId);
        if (position < 0 || position >= listView.adapter.getItemCount()) {
            return;
        }
        layoutManager.scrollToPositionWithOffset(position, AndroidUtilities.dp(80));
        listView.highlightRow(() -> listView.findPositionByItemId(itemId));
    }

    @Override
    protected boolean onLongClick(UItem item, View view, int position, float x, float y) {
        return false;
    }

    @Override
    public void setParentLayout(INavigationLayout layout) {
        if (layout != null && layout.getLastFragment() != null) {
            resourcesProvider = layout.getLastFragment().getResourceProvider();
        }
        super.setParentLayout(layout);
    }

    @Override
    public Theme.ResourcesProvider getResourceProvider() {
        return resourcesProvider;
    }

    @Override
    public ActionBar createActionBar(Context context) {
        var actionBar = super.createActionBar(context);
        actionBar.setBackgroundColor(Color.TRANSPARENT);
        actionBar.setAddToContainer(false);
        actionBar.setUseContainerForTitles();
        actionBar.setOccupyStatusBar(false);
        actionBar.setTitle(getTitle(), null, true);
        actionBar.setTitleColor(getThemedColor(Theme.key_windowBackgroundWhiteBlackText));
        actionBar.setItemsColor(getThemedColor(Theme.key_windowBackgroundWhiteBlackText), false);
        actionBar.setActionBarMenuOnItemClick(new ActionBar.ActionBarMenuOnItemClick() {
            @Override
            public void onItemClick(int id) {
                if (id == -1) {
                    finishFragment();
                }
            }
        });
        BackDrawable backDrawable = new BackDrawable(false);
        backDrawable.setShowStick(!FinegramAppearanceConfig.INSTANCE.getCenterTitle());
        actionBar.setBackButtonDrawable(backDrawable);
        return actionBar;
    }

    protected boolean needActionBarPadding() {
        return true;
    }

    protected void showRestartBulletin() {
        FGBulletinCreator.INSTANCE.createRestartBulletin(this);
    }

    protected void showSuccessBulletin() {
        FGBulletinCreator.INSTANCE.createDebugSuccessBulletin(this);
    }

    public void reloadRows() {
        updateRows(true);
    }

    protected void updateRows(boolean animated) {
        if (listView != null && listView.adapter != null) listView.adapter.update(animated);
    }

    @Override
    public void onResume() {
        super.onResume();
        updateRows(false);
        FGSettingsTheme.refresh(listView, getResourceProvider());
        updateSettingsColors();
    }

    @Override
    public ArrayList<ThemeDescription> getThemeDescriptions() {
        return FGSettingsTheme.descriptions(listView, this::updateSettingsColors);
    }

    private void updateSettingsColors() {
        if (contentView == null || actionBar == null) return;
        contentView.setBackgroundColor(getThemedColor(Theme.key_windowBackgroundGray));
        actionBar.setTitleColor(getThemedColor(Theme.key_windowBackgroundWhiteBlackText));
        actionBar.setItemsColor(getThemedColor(Theme.key_windowBackgroundWhiteBlackText), false);
        actionBar.setItemsBackgroundColor(getThemedColor(Theme.key_listSelector), false);
        actionBar.setSearchTextColor(getThemedColor(Theme.key_windowBackgroundWhiteBlackText), false);
        actionBar.setSearchTextColor(getThemedColor(Theme.key_windowBackgroundWhiteGrayText2), true);
        if (searchItem != null) searchItem.updateColor();
        FGSettingsTheme.refreshRows(listView);
        if (actionBarBackground != null) actionBarBackground.invalidate();
        contentView.invalidate();
    }

    @Override
    public boolean isLightStatusBar() {
        return ColorUtils.calculateLuminance(getThemedColor(Theme.key_windowBackgroundWhite)) > 0.7f;
    }

    @Override
    public boolean isSupportEdgeToEdge() {
        return true;
    }

    @Override
    public void onInsets(int left, int top, int right, int bottom) {
        var topPadding = needActionBarPadding() ? ActionBar.getCurrentActionBarHeight() : AndroidUtilities.dp(16);
        listView.setPadding(0, top + topPadding, 0, bottom);
        actionBarContainer.setPadding(0, top, 0, 0);
        super.onInsets(left, top, right, bottom);
    }

    private final @Nullable DownscaleScrollableNoiseSuppressor scrollableViewNoiseSuppressor;
    private final @Nullable BlurredBackgroundSourceRenderNode iBlur3SourceGlassFrosted;

    private IBlur3Capture iBlur3Capture;

    private final ArrayList<RectF> iBlur3Positions = new ArrayList<>();
    private final RectF iBlur3PositionActionBar = new RectF();

    {
        iBlur3Positions.add(iBlur3PositionActionBar);
    }

    private void blur3_InvalidateBlur() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S || scrollableViewNoiseSuppressor == null
                || fragmentView == null || actionBarContainer == null || iBlur3Capture == null) {
            return;
        }

        final int additionalList = AndroidUtilities.dp(48);
        iBlur3PositionActionBar.set(0, -additionalList, fragmentView.getMeasuredWidth(), actionBarContainer.getMeasuredHeight() + additionalList);

        scrollableViewNoiseSuppressor.setupRenderNodes(iBlur3Positions, 1);
        scrollableViewNoiseSuppressor.invalidateResultRenderNodes(iBlur3Capture, fragmentView.getMeasuredWidth(), fragmentView.getMeasuredHeight());
    }

}

/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.ui.folders

import android.content.Context
import android.graphics.RectF
import android.os.Build
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import androidx.annotation.RequiresApi
import org.telegram.messenger.AndroidUtilities.dp
import org.telegram.ui.ActionBar.Theme
import org.telegram.ui.Components.FilterTabsView
import org.telegram.ui.Components.LayoutHelper
import org.telegram.ui.Components.blur3.BlurredBackgroundDrawableViewFactory
import org.telegram.ui.Components.blur3.DownscaleScrollableNoiseSuppressor
import org.telegram.ui.Components.blur3.capture.IBlur3Capture
import org.telegram.ui.Components.blur3.drawable.color.impl.BlurredBackgroundProviderImpl
import org.telegram.ui.DialogsActivity
import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig

object FoldersHelper {

    private const val TABS_HEIGHT = 36 + 7 + 7

    fun moveFoldersToBottom(): Boolean {
        return FinegramAppearanceConfig.foldersAtBottom
    }

    fun getFloatingButtonsOffset(filterTabsView: FilterTabsView?): Int {
        if (!moveFoldersToBottom() || filterTabsView == null) return 0
        if (filterTabsView.visibility != View.VISIBLE) return 0
        return (dp(TABS_HEIGHT.toFloat()) * filterTabsView.alpha).toInt()
    }

    fun setupFilterTabs(
        context: Context,
        contentView: ViewGroup,
        filterTabsView: FilterTabsView,
        resourceProvider: Theme.ResourcesProvider?,
        iBlur3FactoryLiquidGlass: BlurredBackgroundDrawableViewFactory,
        iBlur3FactoryFade: BlurredBackgroundDrawableViewFactory,
        inForwardMode: Boolean
    ) {
        val background = iBlur3FactoryLiquidGlass.create(
            filterTabsView,
            BlurredBackgroundProviderImpl.foldersPanel(resourceProvider)
        )
        background.setRadius(dp(18f).toFloat())
        background.setPadding(dp(6.666f))

        filterTabsView.setPadding(0, dp(7f), 0, dp(7f))
        filterTabsView.setBlurredBackground(background)

        contentView.addView(
            filterTabsView,
            LayoutHelper.createFrame(
                LayoutHelper.MATCH_PARENT, TABS_HEIGHT.toFloat(),
                Gravity.BOTTOM, 4f, 0f, 4f, 0f
            )
        )
    }

    fun updateFoldersOffset(
        dialogsActivity: DialogsActivity,
        inForwardMode: Boolean,
    ) {
        if (!moveFoldersToBottom()) return
        val filterTabsView = dialogsActivity.filterTabsView ?: return

        var offset = dialogsActivity.navigationBarHeight
        if (dialogsActivity.hasMainTabs || FinegramAppearanceConfig.bottomTabsVisible()) {
            offset += dp(DialogsActivity.MAIN_TABS_HEIGHT.toFloat()) + DialogsActivity.MAIN_TABS_MARGIN
        }
        filterTabsView.translationY = -offset.toFloat()
    }

    @RequiresApi(Build.VERSION_CODES.S)
    @Suppress("FunctionName")
    fun blur3_InvalidateBlur(
        dialogsActivity: DialogsActivity,
        iBlur3Capture: IBlur3Capture,
        iBlur3PositionActionBar: RectF,
        iBlur3PositionFolders: RectF,
        iBlur3PositionMainTabs: RectF,
        iBlur3Positions: ArrayList<RectF>,
        scrollableViewNoiseSuppressor: DownscaleScrollableNoiseSuppressor
    ) {
        val fragmentView = dialogsActivity.fragmentView ?: return
        val actionBar = dialogsActivity.actionBar ?: return
        val filterTabsView = dialogsActivity.filterTabsView

        val width = fragmentView.measuredWidth
        val height = fragmentView.measuredHeight

        iBlur3PositionActionBar.set(
            0f, -dp(48f).toFloat(), width.toFloat(),
            (actionBar.measuredHeight + dp(48f)).toFloat()
        )

        val mainTabsBottom = height - dialogsActivity.navigationBarHeight
        val mainTabsTop = if (dialogsActivity.hasMainTabs || FinegramAppearanceConfig.bottomTabsVisible()) {
            mainTabsBottom - dp(DialogsActivity.MAIN_TABS_HEIGHT.toFloat()) - DialogsActivity.MAIN_TABS_MARGIN
        } else {
            mainTabsBottom
        }
        iBlur3PositionMainTabs.set(0f, mainTabsTop.toFloat(), width.toFloat(), mainTabsBottom.toFloat())

        if (filterTabsView != null && filterTabsView.visibility == View.VISIBLE) {
            val top = filterTabsView.y
            iBlur3PositionFolders.set(0f, top, width.toFloat(), top + filterTabsView.height)
        } else {
            iBlur3PositionFolders.setEmpty()
        }

        scrollableViewNoiseSuppressor.setupRenderNodes(iBlur3Positions, 2)
        scrollableViewNoiseSuppressor.invalidateResultRenderNodes(iBlur3Capture, width, height)
    }

}

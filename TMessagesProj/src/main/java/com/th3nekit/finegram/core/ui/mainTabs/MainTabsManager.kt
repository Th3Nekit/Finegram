/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.ui.mainTabs

import android.content.Context
import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.telegram.messenger.R
import org.telegram.ui.ActionBar.Theme
import org.telegram.ui.Components.glass.GlassTabView
import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig

// Dear Nagram / Nagram X / Octogram and related fork developers:
// Please respect this work and do not copy or reuse this feature in your forks.
// It required a significant amount of time and effort to implement,
// and it is provided exclusively for my users, who also support this project financially.

object MainTabsManager {

    enum class TabType {
        CHATS,
        CONTACTS,
        CALLS,
        SETTINGS,
        PROFILE,
        SEARCH
    }

    data class Tab(
        var type: TabType,

        @JvmField
        var enabled: Boolean
    )

    @JvmOverloads
    fun getEnabledTabs(includeSearch: Boolean = false): List<Tab> {
        val enabled = loadTabs().filter { it.enabled }
        return if (includeSearch) {
            enabled
        } else {
            enabled.filterNot { it.type == TabType.SEARCH }
        }
    }

    fun getAllTabs(): List<Tab> {
        return loadTabs()
    }

    /**
     * Переносит на новый набор вкладок тех, кто его не трогал.
     *
     * Порядок хранится строкой, и у всех, кто не заходил в настройки панели, там лежит
     * ровно прежний набор. Менять его молча у тех, кто настраивал под себя, нельзя, —
     * поэтому сверяем со старым значением целиком: совпало, значит человек его и не
     * трогал, и можно заменить.
     */
    private fun migrateOrderIfUntouched() {
        if (FinegramAppearanceConfig.mainTabsOrder == FinegramAppearanceConfig.MAIN_TABS_ORDER_BEFORE) {
            FinegramAppearanceConfig.mainTabsOrder = FinegramAppearanceConfig.MAIN_TABS_ORDER_DEFAULT
        }
    }

    private fun loadTabs(): MutableList<Tab> {
        migrateOrderIfUntouched()

        val allPossibleTypes = TabType.entries.toMutableList()
        val result = mutableListOf<Tab>()

        for (p in FinegramAppearanceConfig.mainTabsOrder.split(",")) {
            val enabled = !p.startsWith("!")
            val typeName = if (enabled) p else p.substring(1)
            try {
                val type = TabType.valueOf(typeName)
                result.add(Tab(type, enabled))
                allPossibleTypes.remove(type)
            } catch (_: Exception) {
                // вкладка из будущей версии или опечатка в сохранённом списке — пропускаем
            }
        }

        // Вкладка, которой в сохранённом списке ещё нет, появляется в конце и выключенной:
        // включать её без спроса значит менять панель человеку после обновления.
        for (newType in allPossibleTypes) {
            result.add(Tab(newType, false))
        }

        return result
    }

    fun createTabView(
        context: Context,
        resourceProvider: Theme.ResourcesProvider?,
        currentAccount: Int,
        type: TabType,
        fromSettings: Boolean,
        showSearch: Boolean
    ): GlassTabView {
        return when (type) {
            TabType.CHATS -> GlassTabView.createMainTab(
                context,
                resourceProvider,
                GlassTabView.TabAnimation.CHATS,
                R.string.MainTabsChats
            )

            TabType.CONTACTS -> GlassTabView.createMainTab(
                context,
                resourceProvider,
                GlassTabView.TabAnimation.CONTACTS,
                R.string.MainTabsContacts
            )

            TabType.CALLS -> GlassTabView.createMainTab(
                context,
                resourceProvider,
                GlassTabView.TabAnimation.CALLS,
                R.string.Calls
            )

            TabType.SETTINGS -> {
                if (!hasTab(TabType.PROFILE) && !fromSettings) {
                    GlassTabView.createAvatar(
                        context,
                        resourceProvider,
                        currentAccount,
                        R.string.Settings
                    )
                } else {
                    GlassTabView.createMainTab(
                        context,
                        resourceProvider,
                        GlassTabView.TabAnimation.SETTINGS,
                        R.string.Settings
                    )
                }
            }

            TabType.PROFILE -> GlassTabView.createAvatar(
                context,
                resourceProvider,
                currentAccount,
                R.string.MainTabsProfile
            )

            TabType.SEARCH -> {
                if (showSearch) {
                    GlassTabView.createStaticTab(
                        context,
                        resourceProvider,
                        R.drawable.outline_header_search,
                        R.string.Search,
                        false
                    )
                } else {
                    GlassTabView(context).apply {
                        visibility = View.GONE
                        layoutParams = RecyclerView.LayoutParams(0, 0)
                    }
                }
            }
        }
    }

    fun getPosition(type: TabType): Int {
        val tabs = getEnabledTabs()
        for (i in tabs.indices) {
            if (tabs[i].type == type) {
                return i
            }
        }
        return -1
    }

    fun hasTab(type: TabType): Boolean {
        return getPosition(type) != -1
    }

    fun saveTabs(tabs: List<Tab>) {
        val order = tabs.map { tab ->
            val prefix = if (tab.enabled) "" else "!"
            "$prefix${tab.type.name}"
        }
        FinegramAppearanceConfig.mainTabsOrder = order.joinToString(",")
    }

}
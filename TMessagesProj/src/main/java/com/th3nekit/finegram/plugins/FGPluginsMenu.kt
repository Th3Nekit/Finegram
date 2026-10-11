/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins

import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.NotificationCenter
import android.content.Context
import org.telegram.messenger.R
import org.telegram.ui.Components.ItemOptions
import com.th3nekit.finegram.core.FinegramLogger
import org.telegram.ui.ChatActivity
import org.telegram.ui.ActionBar.AlertDialog
import org.telegram.messenger.LocaleController.getString
import org.telegram.messenger.ChatObject

object FGPluginsMenu {

    const val DRAWER = "drawer_menu"

    const val MAIN = "main_menu"
    const val CHAT_ACTION = "chat_action_menu"
    const val MESSAGE_CONTEXT = "message_context_menu"
    const val PROFILE_ACTION = "profile_action_menu"

    const val CHAT_ITEM_BASE = 2500

    const val MESSAGE_ITEM_BASE = 2600

    @Volatile
    private var cached: List<FGPluginsController.MenuItem> = emptyList()
    @Volatile
    private var declared: List<FGPluginsController.MenuItem> = emptyList()

    @Volatile
    private var empty = true

    @Volatile
    private var refreshing = false
    private val refreshLock = Any()
    private var refreshRevision = 0L

    private var chatItems: List<FGPluginsController.MenuItem> = emptyList()
    private var chatContext: Map<String, Any?>? = null
    private var messageItems: List<FGPluginsController.MenuItem> = emptyList()
    private var messageContext: Map<String, Any?>? = null

    @JvmStatic
    fun invalidate() {
        synchronized(refreshLock) {
            refreshRevision++
            if (refreshing) return
            refreshing = true
        }
        FGPluginsController.queue.postRunnable {
            while (true) {
                val revision = synchronized(refreshLock) { refreshRevision }
                try {
                    val items = FGPluginsController.menuItems(null, null)
                    declared = FGPluginsController.menuItems(null, null, true)
                    cached = items
                    empty = items.isEmpty()
                } catch (e: Throwable) {
                    FinegramLogger.e("FGPlugins", { "пункты меню не перечитались" }, e)
                }
                val done = synchronized(refreshLock) {
                    if (revision == refreshRevision) {
                        refreshing = false
                        true
                    } else false
                }
                if (done) {
                    AndroidUtilities.runOnUIThread {
                        NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.finegramPluginMenuUpdated)
                    }
                    break
                }
            }
        }
    }

    private fun iconOf(context: Context?, name: String?): Int {
        if (name.isNullOrEmpty() || context == null) return R.drawable.msg_settings
        return try {
            val id = context.resources.getIdentifier(name, "drawable", context.packageName)
            if (id != 0) id else R.drawable.msg_settings
        } catch (e: Throwable) {
            R.drawable.msg_settings
        }
    }

    private fun itemsFor(kind: String): List<FGPluginsController.MenuItem> {
        if (empty) return emptyList()
        return cached.filter { it.menuType == kind }
    }

    @JvmStatic
    @JvmOverloads
    fun inject(options: ItemOptions?, kind: String, context: Context?, payload: Map<String, Any?>? = null) {
        if (options == null || empty) return
        val items = itemsFor(kind)
        if (items.isEmpty()) return
        for (item in items) {
            options.add(iconOf(context, item.icon), item.text) {
                click(item, payload)
            }
        }
    }

    class Row(@JvmField val icon: Int, @JvmField val text: CharSequence, @JvmField val action: Runnable)

    @JvmStatic
    fun rows(kind: String, context: Context?, payload: Map<String, Any?>?): List<Row> {
        if (empty) return emptyList()
        return itemsFor(kind).map { item -> Row(iconOf(context, item.icon), item.text, Runnable { click(item, payload) }) }
    }

    @JvmStatic
    fun has(kind: String): Boolean = declared.any { it.menuType == kind }

    @JvmStatic
    fun showChatItems(fragment: ChatActivity) {
        val activity = fragment.parentActivity ?: return
        val user = fragment.currentUser
        val chat = fragment.currentChat
        val payload = hashMapOf<String, Any?>(
            "fragment" to fragment,
            "dialog_id" to fragment.dialogId,
            "account" to fragment.currentAccount,
            "userId" to (user?.id ?: 0L),
            "chatId" to (chat?.id ?: 0L),
            "is_user" to (user != null),
            "is_group" to (chat != null && (!ChatObject.isChannel(chat) || chat.megagroup)),
            "is_channel" to (chat != null && ChatObject.isChannel(chat) && !chat.megagroup),
            "user" to user,
            "chat" to chat
        )
        val loading = AlertDialog(activity, 3, fragment.resourceProvider)
        val cancelled = java.util.concurrent.atomic.AtomicBoolean(false)
        loading.setOnCancelListener { cancelled.set(true) }
        loading.showDelayed(200)
        FGPluginsController.queue.postRunnable {
            val items = FGPluginsController.menuItems(CHAT_ACTION, payload)
            AndroidUtilities.runOnUIThread {
                loading.dismiss()
                if (cancelled.get() || fragment.parentActivity !== activity || fragment.fragmentView?.isAttachedToWindow != true) return@runOnUIThread
                val builder = AlertDialog.Builder(activity, fragment.resourceProvider).setTitle(getString(R.string.FG_Plugins))
                if (items.isEmpty()) builder.setMessage(getString(R.string.FG_Plugins_NoChatActions)).setPositiveButton(getString(R.string.OK), null)
                else builder.setItems(items.map { it.text }.toTypedArray(), items.map { iconOf(activity, it.icon) }.toIntArray()) { _, index -> click(items[index], payload) }
                fragment.showDialog(builder.create())
            }
        }
    }

    @JvmStatic
    fun prepareChatItems(context: Map<String, Any?>?): List<FGPluginsController.MenuItem> {
        if (empty) {
            chatItems = emptyList()
            return chatItems
        }
        chatItems = itemsFor(CHAT_ACTION)
        chatContext = context
        return chatItems
    }

    @JvmStatic
    fun iconFor(context: Context?, item: FGPluginsController.MenuItem): Int = iconOf(context, item.icon)

    @JvmStatic
    fun injectMessageItems(
        items: java.util.ArrayList<CharSequence?>,
        options: java.util.ArrayList<Int?>,
        icons: java.util.ArrayList<Int?>,
        context: Context?,
        payload: Map<String, Any?>?
    ) {
        if (empty) {
            messageItems = emptyList()
            return
        }
        messageItems = itemsFor(MESSAGE_CONTEXT)
        messageContext = payload
        messageItems.forEachIndexed { index, item ->
            items.add(item.text)
            options.add(MESSAGE_ITEM_BASE + index)
            icons.add(iconOf(context, item.icon))
        }
    }

    @JvmStatic
    fun onMessageItemClick(id: Int): Boolean {
        val index = id - MESSAGE_ITEM_BASE
        val items = messageItems
        if (index < 0 || index >= items.size) return false
        click(items[index], messageContext)
        return true
    }

    @JvmStatic
    fun onChatItemClick(id: Int): Boolean {
        val index = id - CHAT_ITEM_BASE
        val items = chatItems
        if (index < 0 || index >= items.size) return false
        click(items[index], chatContext)
        return true
    }

    private fun click(item: FGPluginsController.MenuItem, payload: Map<String, Any?>?) {
        AndroidUtilities.runOnUIThread {
            try {
                FGPluginsController.menuClick(item, payload)
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "пункт плагина ${item.pluginId} упал" }, e)
            }
        }
    }
}

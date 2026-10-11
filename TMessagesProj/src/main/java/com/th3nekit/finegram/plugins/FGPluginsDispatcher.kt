/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins

import com.th3nekit.finegram.core.FinegramLogger
import com.th3nekit.finegram.plugins.api.PluginEvents
import org.telegram.tgnet.TLObject
import org.telegram.tgnet.TLRPC

object FGPluginsDispatcher {

    @Volatile
    private var namesActive = false

    @Volatile
    private var exactNames: Set<String> = emptySet()

    @Volatile
    private var nameParts: List<String> = emptyList()

    @Volatile
    private var sendMessageActive = false

    private val dexListeners = LinkedHashMap<String, PluginEvents>()

    @JvmStatic
    fun subscribeDex(pluginId: String, events: PluginEvents) {
        synchronized(dexListeners) { dexListeners[pluginId] = events }
    }

    @JvmStatic
    fun unsubscribeDex(pluginId: String) {
        synchronized(dexListeners) { dexListeners.remove(pluginId) }
    }

    private fun dexSubscribers(): List<PluginEvents> =
        if (dexListeners.isEmpty()) emptyList() else synchronized(dexListeners) { dexListeners.values.toList() }

    @Volatile
    private var fileHooksActive = false

    @JvmStatic
    fun setFileHooksActive(active: Boolean) {
        fileHooksActive = active
    }

    @JvmStatic
    fun onFileOpened(fileName: String?, path: String?, message: Any?): Boolean {
        if (!fileHooksActive || fileName.isNullOrEmpty()) return false
        val module = FGPluginsController.runtimeOrNull() ?: return false
        return try {
            module.callAttr("dispatch_file", fileName, path, message).toBoolean()
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "плагины не обработали файл" }, e)
            false
        }
    }

    @Volatile
    private var intentsActive = false

    @JvmStatic
    fun setIntentsActive(active: Boolean) {
        intentsActive = active
    }

    @JvmStatic
    fun onIntent(intent: Any?): Boolean {
        if (!intentsActive || intent == null) return false
        val module = FGPluginsController.runtimeOrNull() ?: return false
        return try {
            module.callAttr("dispatch_intent", intent).toBoolean()
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "плагины не обработали намерение" }, e)
            false
        }
    }

    @JvmStatic
    fun onAppEvent(event: String) {
        val module = FGPluginsController.runtimeOrNull() ?: return
        FGPluginsController.queue.postRunnable {
            try {
                module.callAttr("dispatch_app_event", event)
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "плагины не обработали событие приложения" }, e)
            }
        }
    }

    @JvmStatic
    fun setDispatchNames(names: String, parts: String, sendMessages: Boolean) {
        exactNames = if (names.isEmpty()) emptySet() else names.split(',').toHashSet()
        nameParts = if (parts.isEmpty()) emptyList() else parts.split(',')
        namesActive = exactNames.isNotEmpty() || nameParts.isNotEmpty()
        sendMessageActive = sendMessages
    }

    private fun wanted(name: String): Boolean {
        if (exactNames.contains(name)) return true
        for (part in nameParts) {
            if (name.contains(part)) return true
        }
        return false
    }

    class Verdict(@JvmField val cancel: Boolean, @JvmField val replacement: Any?) {
        companion object {
            @JvmField
            val PASS = Verdict(false, null)
        }
    }

    private fun verdict(result: com.chaquo.python.PyObject?): Verdict {
        if (result == null) return Verdict.PASS
        return try {
            val pair = result.asList()
            if (pair.size < 2) return Verdict.PASS
            val cancel = pair[0]?.toBoolean() == true
            val replacement = pair[1]?.toJava(Any::class.java)
            if (!cancel && replacement == null) Verdict.PASS else Verdict(cancel, replacement)
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "плагины ответили непонятно" }, e)
            Verdict.PASS
        }
    }

    @JvmStatic
    fun onSendRequestHooked(account: Int, request: TLObject?): Verdict {
        if (request == null || !namesActive && dexListeners.isEmpty()) return Verdict.PASS
        val name = request.javaClass.simpleName
        var cancel = false

        for (listener in dexSubscribers()) {
            try {
                if (listener.onSendRequest(account, name, request)) cancel = true
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "dex-плагин не обработал запрос" }, e)
            }
        }
        if (cancel) return Verdict(true, null)

        if (namesActive && wanted(name)) {
            val module = FGPluginsController.runtimeOrNull()
            if (module != null) {
                try {
                    return verdict(module.callAttr("dispatch_request", name, account, request))
                } catch (e: Throwable) {
                    FinegramLogger.e("FGPlugins", { "плагины не обработали запрос" }, e)
                }
            }
        }
        return Verdict.PASS
    }

    @JvmStatic
    fun onSendRequest(account: Int, request: TLObject?): Boolean =
        onSendRequestHooked(account, request).cancel

    @JvmStatic
    fun onRequestResponse(account: Int, request: TLObject?, response: TLObject?, error: TLRPC.TL_error?) {
        if (request == null || !namesActive && dexListeners.isEmpty()) return
        val name = request.javaClass.simpleName

        for (listener in dexSubscribers()) {
            try {
                listener.onRequestResponse(account, name, response, error)
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "dex-плагин не обработал ответ" }, e)
            }
        }

        if (namesActive && wanted(name)) {
            val module = FGPluginsController.runtimeOrNull() ?: return
            try {
                module.callAttr("dispatch_response", name, account, response, error)
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "плагины не обработали ответ" }, e)
            }
        }
    }

    @JvmStatic
    fun onAppStart() = onAppEvent("app_start")

    @JvmStatic
    fun onAppPause() = onAppEvent("app_pause")

    @JvmStatic
    fun onAppResume() = onAppEvent("app_resume")

    @JvmStatic
    fun onAppStop() = onAppEvent("app_stop")

    @JvmStatic
    fun onUpdates(account: Int, updates: MutableList<TLRPC.Update>?) {
        if (updates.isNullOrEmpty() || !namesActive && dexListeners.isEmpty()) return
        val listeners = dexSubscribers()
        val module = if (namesActive) FGPluginsController.runtimeOrNull() else null
        var index = 0
        while (index < updates.size) {
            val update = updates[index]
            if (update == null) {
                index++
                continue
            }
            val name = update.javaClass.simpleName
            for (listener in listeners) {
                try {
                    listener.onUpdate(account, name, update)
                } catch (e: Throwable) {
                    FinegramLogger.e("FGPlugins", { "dex-плагин не обработал обновление" }, e)
                }
            }
            if (module == null) {
                index++
                continue
            }
            if (!wanted(name)) {
                index++
                continue
            }
            val answer = try {
                verdict(module.callAttr("dispatch_update", name, account, update))
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "плагины не обработали обновление" }, e)
                Verdict.PASS
            }
            when {
                answer.cancel -> updates.removeAt(index)
                answer.replacement is TLRPC.Update -> {
                    updates[index] = answer.replacement
                    index++
                }
                else -> index++
            }
        }
    }

    @JvmStatic
    fun onSendMessageHooked(account: Int, params: Any?): Verdict {
        if (params == null || !sendMessageActive && dexListeners.isEmpty()) return Verdict.PASS
        var cancel = false

        for (listener in dexSubscribers()) {
            try {
                if (listener.onSendMessage(account, params)) cancel = true
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "dex-плагин не обработал отправку" }, e)
            }
        }
        if (cancel) return Verdict(true, null)

        if (sendMessageActive) {
            val module = FGPluginsController.runtimeOrNull()
            if (module != null) {
                try {
                    return verdict(module.callAttr("dispatch_send_message", account, params))
                } catch (e: Throwable) {
                    FinegramLogger.e("FGPlugins", { "плагины не обработали отправку" }, e)
                }
            }
        }
        return Verdict.PASS
    }

    @JvmStatic
    fun onSendMessage(account: Int, params: Any?): Boolean =
        onSendMessageHooked(account, params).cancel
}

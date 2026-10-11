/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins

import android.content.Context
import com.chaquo.python.PyObject
import com.th3nekit.finegram.core.FinegramLogger
import com.th3nekit.finegram.plugins.api.PluginMethodCall
import com.th3nekit.finegram.plugins.api.PluginMethodHook
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.ApplicationLoader
import org.telegram.messenger.MessagesController
import org.telegram.messenger.UserConfig
import org.telegram.messenger.Utilities
import org.telegram.messenger.DispatchQueue
import org.telegram.messenger.FileLoader
import org.telegram.tgnet.ConnectionsManager
import org.telegram.tgnet.TLObject
import org.telegram.ui.ActionBar.AlertDialog
import org.telegram.ui.Components.BulletinFactory
import org.telegram.ui.LaunchActivity
import dalvik.system.InMemoryDexClassLoader
import java.nio.ByteBuffer
import com.th3nekit.finegram.hook.ArtHook
import com.th3nekit.finegram.hook.HookCallback
import com.th3nekit.finegram.hook.HookFrame
import com.th3nekit.finegram.hook.HookHandle
import java.lang.reflect.Field
import java.lang.reflect.Member
import java.lang.reflect.Method
import java.util.concurrent.atomic.AtomicInteger

object FGHookBridge {

    private val hooks = HashMap<String, MutableList<HookHandle>>()

    private val singleHooks = HashMap<Int, HookHandle>()
    private val hookCounter = AtomicInteger(1)

    private fun ensureEngine(): Boolean {
        if (ArtHook.init()) return true
        FinegramLogger.e("FGPlugins", { "движок хуков не поднялся" })
        return false
    }

    @JvmStatic
    fun hook(
        pluginId: String,
        className: String,
        methodName: String,
        paramTypes: Array<String>,
        callback: PyObject
    ): Boolean {
        if (!ensureEngine()) {
            return false
        }
        return try {
            val loader = FGHookBridge::class.java.classLoader
            val target = Class.forName(className, false, loader)
            val types = paramTypes.map { resolveType(it, loader) }.toTypedArray()
            val method: Method = target.getDeclaredMethod(methodName, *types)
            hookMember(pluginId, method, callback) != 0
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "плагин $pluginId не смог перехватить $className.$methodName" }, e)
            false
        }
    }

    @JvmStatic
    fun hookMember(pluginId: String, member: Member, callback: PyObject): Int {
        if (!ensureEngine()) {
            return 0
        }
        return try {
            if (member is Method) {
                member.isAccessible = true
            }
            val hook = object : HookCallback() {
                override fun before(frame: HookFrame) {
                    invokeCallback(pluginId, callback, "before", frame)
                }

                override fun after(frame: HookFrame) {
                    invokeCallback(pluginId, callback, "after", frame)
                }
            }
            val token = register(pluginId, ArtHook.hook(member, hook))
            FinegramLogger.d("FGPlugins") { "плагин $pluginId перехватил ${member.declaringClass.name}.${member.name}" }
            token
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "плагин $pluginId не смог перехватить ${member.name}" }, e)
            0
        }
    }

    @JvmStatic
    fun hookJava(pluginId: String, member: Member, hook: PluginMethodHook): Int {
        if (!ensureEngine()) {
            return 0
        }
        return try {
            if (member is Method) {
                member.isAccessible = true
            }
            val callback = object : HookCallback() {
                override fun before(frame: HookFrame) {
                    safely(pluginId, "before") { hook.before(FGCallFrame(frame)) }
                }

                override fun after(frame: HookFrame) {
                    safely(pluginId, "after") { hook.after(FGCallFrame(frame)) }
                }
            }
            register(pluginId, ArtHook.hook(member, callback))
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "плагин $pluginId не смог перехватить ${member.name}" }, e)
            0
        }
    }

    private inline fun safely(pluginId: String, stage: String, block: () -> Unit) {
        try {
            block()
        } catch (e: Throwable) {

            FinegramLogger.e("FGPlugins", { "плагин $pluginId: обработчик $stage упал" }, e)
        }
    }

    private class FGCallFrame(private val frame: HookFrame) : PluginMethodCall {
        override fun thisObject(): Any? = frame.thisObject
        override fun args(): Array<Any?> = frame.args
        override fun result(): Any? = frame.result
        override fun setResult(value: Any?) {
            frame.result = value
        }
    }

    private fun register(pluginId: String, unhook: HookHandle): Int {
        val token = hookCounter.getAndIncrement()
        synchronized(hooks) {
            hooks.getOrPut(pluginId) { mutableListOf() }.add(unhook)
            singleHooks[token] = unhook
        }
        return token
    }

    @JvmStatic
    fun unhookOne(token: Int) {
        val unhook = synchronized(hooks) { singleHooks.remove(token) } ?: return
        try {
            unhook.unhook()
            synchronized(hooks) {
                hooks.values.forEach { list -> list.remove(unhook) }
            }
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "не снять перехват $token" }, e)
        }
    }

    private fun invokeCallback(pluginId: String, callback: PyObject, stage: String, frame: HookFrame) {
        try {

            if (stage == "before" && callback.containsKey("replace_hooked_method")) {
                val returned = callback.callAttr("replace_hooked_method", frame)
                frame.result = returned?.toJava(Any::class.java)
                return
            }
            val handler = when {
                callback.containsKey(stage + "_hooked_method") -> stage + "_hooked_method"
                callback.containsKey(stage) -> stage
                else -> return
            }
            callback.callAttr(handler, frame)
        } catch (e: Throwable) {

            FinegramLogger.e("FGPlugins", { "плагин $pluginId: обработчик $stage упал" }, e)
        }
    }

    @JvmStatic
    fun unhookAll(pluginId: String) {

        try {
            com.exteragram.messenger.plugins.PluginsController.removeAllXposedHooks(pluginId)
        } catch (e: Throwable) {

        }
        val list = synchronized(hooks) { hooks.remove(pluginId) } ?: return
        list.forEach { unhook ->
            try {
                unhook.unhook()
                synchronized(hooks) { singleHooks.values.remove(unhook) }
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "не снять перехват плагина $pluginId" }, e)
            }
        }
        FinegramLogger.d("FGPlugins") { "перехваты плагина $pluginId сняты: ${list.size}" }
    }

    private fun resolveType(name: String, loader: ClassLoader?): Class<*> = when (name) {
        "int" -> Int::class.javaPrimitiveType!!
        "long" -> Long::class.javaPrimitiveType!!
        "boolean" -> Boolean::class.javaPrimitiveType!!
        "float" -> Float::class.javaPrimitiveType!!
        "double" -> Double::class.javaPrimitiveType!!
        "byte" -> Byte::class.javaPrimitiveType!!
        "char" -> Char::class.javaPrimitiveType!!
        "short" -> Short::class.javaPrimitiveType!!
        else -> Class.forName(name, false, loader)
    }

    @JvmStatic
    fun setDispatchNames(names: String, parts: String, sendMessages: Boolean) {
        FGPluginsDispatcher.setDispatchNames(names, parts, sendMessages)
    }

    @JvmStatic
    fun menuItemsChanged() {
        FGPluginsMenu.invalidate()
    }

    @JvmStatic
    fun setFileHooksActive(active: Boolean) {
        FGPluginsDispatcher.setFileHooksActive(active)
    }

    @JvmStatic
    fun setIntentsActive(active: Boolean) {
        FGPluginsDispatcher.setIntentsActive(active)
    }

    @JvmStatic
    fun reloadPluginSettings(pluginId: String) {
        AndroidUtilities.runOnUIThread {
            try {
                val fragment = LaunchActivity.getLastFragment()
                if (fragment is com.th3nekit.finegram.preferences.PluginDetailsPreferencesEntry) {

                    if (fragment.pluginId == pluginId) fragment.reloadPluginSettings()
                } else if (fragment is com.th3nekit.finegram.preferences.BaseCGPreferencesEntry) {
                    fragment.reloadRows()
                }
            } catch (e: Throwable) {
                FinegramLogger.d("FGPlugins") { "настройки плагина $pluginId не перерисованы" }
            }
        }
    }

    @JvmStatic
    fun runOnUiThread(func: PyObject, delay: Int) {
        val runnable = Runnable { safeCall(func) }
        if (delay > 0) {
            AndroidUtilities.runOnUIThread(runnable, delay.toLong())
        } else {
            AndroidUtilities.runOnUIThread(runnable)
        }
    }

    @JvmStatic
    fun runOnQueue(func: PyObject, delay: Int) {
        runOnQueue(func, delay, "plugins")
    }

    @JvmStatic
    fun runOnQueue(func: PyObject, delay: Int, queueName: String) {
        getQueue(queueName).postRunnable(Runnable { safeCall(func) }, delay.coerceAtLeast(0).toLong())
    }

    @JvmStatic

    fun getQueue(name: String): DispatchQueue {
        val key = name.trim().lowercase(java.util.Locale.ROOT).removeSuffix("queue")
        return when (key) {
            "plugins" -> Utilities.pluginsQueue
            "stage" -> Utilities.stageQueue
            "search" -> Utilities.searchQueue
            "phonebook" -> Utilities.phoneBookQueue
            "theme" -> Utilities.themeQueue
            "externalnetwork" -> Utilities.externalNetworkQueue
            "cacheclear" -> Utilities.cacheClearQueue
            "file" -> FileLoader.getInstance(UserConfig.selectedAccount).fileLoaderQueue
            else -> Utilities.globalQueue
        }
    }

    private fun safeCall(func: PyObject) {
        try {
            func.call()
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "код плагина упал" }, e)
        }
    }

    @JvmStatic
    fun findClass(name: String): Class<*>? = try {
        Class.forName(name, false, FGHookBridge::class.java.classLoader)
    } catch (e: Throwable) {
        null
    }

    @JvmStatic
    fun getPrivateField(target: Any?, fieldName: String): Any? {
        val field = resolveField(target, fieldName) ?: return null
        return try {
            field.get(target)
        } catch (e: Throwable) {
            null
        }
    }

    @JvmStatic
    fun setPrivateField(target: Any?, fieldName: String, value: Any?) {
        val field = resolveField(target, fieldName) ?: return
        try {
            field.set(target, value)
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "не записать поле $fieldName" }, e)
        }
    }

    private fun resolveField(target: Any?, fieldName: String): Field? {
        if (target == null) return null
        var current: Class<*>? = if (target is Class<*>) target else target.javaClass
        while (current != null) {
            try {
                val field = current.getDeclaredField(fieldName)
                field.isAccessible = true
                return field
            } catch (e: NoSuchFieldException) {
                current = current.superclass
            }
        }
        return null
    }

    @JvmStatic
    fun loadDex(data: ByteArray, key: String?): ClassLoader? {
        if (data.isEmpty()) {
            return null
        }
        val cacheKey = key?.takeIf { it.isNotEmpty() } ?: data.size.toString() + ":" + data.contentHashCode()
        synchronized(dexLoaders) {
            dexLoaders[cacheKey]?.let { return it }
        }
        return try {
            val parent = ApplicationLoader.applicationContext.classLoader
                ?: FGHookBridge::class.java.classLoader
            val loader = InMemoryDexClassLoader(ByteBuffer.wrap(data), parent)
            synchronized(dexLoaders) { dexLoaders[cacheKey] = loader }
            FinegramLogger.d("FGPlugins") { "плагин подгрузил dex (${data.size} байт)" }
            loader
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "не загрузить dex плагина" }, e)
            null
        }
    }

    @JvmStatic
    fun loadDexClass(loader: ClassLoader?, className: String): Class<*>? = try {
        if (loader == null) null else loader.loadClass(className)
    } catch (e: Throwable) {
        FinegramLogger.e("FGPlugins", { "в dex плагина нет класса $className" }, e)
        null
    }

    private val dexLoaders = HashMap<String, ClassLoader>()

    @JvmStatic
    fun getLastFragment(): Any? = try {
        LaunchActivity.getLastFragment()
    } catch (e: Throwable) {
        null
    }

    @JvmStatic
    fun applicationContext(): Context? = ApplicationLoader.applicationContext

    @JvmStatic
    fun getUserConfig(account: Int): UserConfig = UserConfig.getInstance(resolveAccount(account))

    @JvmStatic
    fun getMessagesController(account: Int): MessagesController =
        MessagesController.getInstance(resolveAccount(account))

    private fun resolveAccount(account: Int): Int =
        if (account < 0 || account >= UserConfig.MAX_ACCOUNT_COUNT) UserConfig.selectedAccount else account

    @JvmStatic
    fun sendRequest(request: TLObject, callback: PyObject?, account: Int): Int {
        val currentAccount = resolveAccount(account)
        return ConnectionsManager.getInstance(currentAccount).sendRequest(request) { response, error ->
            if (callback != null) {
                try {
                    if (callback.containsKey("onResponse")) {
                        callback.callAttr("onResponse", response, error)
                    } else {
                        callback.call(response, error)
                    }
                } catch (e: Throwable) {
                    FinegramLogger.e("FGPlugins", { "обработчик ответа упал" }, e)
                }
            }
        }
    }

    @JvmStatic
    fun copyToClipboard(text: String) {
        AndroidUtilities.addToClipboard(text)
    }

    private fun bulletinIcon(kind: String, icon: Int): Int {
        if (icon != 0) return icon
        return when (kind) {
            "error" -> org.telegram.messenger.R.raw.error
            "copy" -> org.telegram.messenger.R.raw.copy
            "success" -> org.telegram.messenger.R.raw.done
            else -> org.telegram.messenger.R.raw.info
        }
    }

    @JvmStatic
    @JvmOverloads
    fun showBulletin(text: String, kind: String, icon: Int = 0) {
        AndroidUtilities.runOnUIThread {
            try {
                val fragment = LaunchActivity.getLastFragment() ?: return@runOnUIThread
                val factory = BulletinFactory.of(fragment)
                when {
                    icon != 0 -> factory.createSimpleBulletin(icon, text).show()
                    kind == "error" -> factory.createErrorBulletin(text).show()
                    kind == "success" -> factory.createSuccessBulletin(text).show()
                    else -> factory.createSimpleBulletin(bulletinIcon(kind, 0), text).show()
                }
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "не показать сообщение плагина" }, e)
            }
        }
    }

    @JvmStatic
    @JvmOverloads
    fun showBulletinTwoLine(title: String, subtitle: String, kind: String, icon: Int = 0) {
        AndroidUtilities.runOnUIThread {
            try {
                val fragment = LaunchActivity.getLastFragment() ?: return@runOnUIThread
                BulletinFactory.of(fragment)
                    .createSimpleBulletin(bulletinIcon(kind, icon), title, subtitle).show()
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "не показать сообщение плагина" }, e)
            }
        }
    }

    @JvmStatic
    @JvmOverloads
    fun showBulletinWithButton(text: String, buttonText: String, action: PyObject?,
                               icon: Int = 0, duration: Int = 5000) {
        AndroidUtilities.runOnUIThread {
            try {
                val fragment = LaunchActivity.getLastFragment() ?: return@runOnUIThread
                BulletinFactory.of(fragment)
                    .createSimpleBulletin(bulletinIcon("info", icon), text, buttonText) {
                        if (action != null) safeCall(action)
                    }
                    .setDuration(duration)
                    .show()
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "не показать сообщение с кнопкой" }, e)
            }
        }
    }

    @JvmStatic
    fun showBulletinUndo(text: String, subtitle: String?, action: PyObject?) {
        AndroidUtilities.runOnUIThread {
            try {
                val fragment = LaunchActivity.getLastFragment() ?: return@runOnUIThread
                val factory = BulletinFactory.of(fragment)
                val undo = Runnable { if (action != null) safeCall(action) }
                if (subtitle.isNullOrEmpty()) {
                    factory.createUndoBulletin(text, undo, null).show()
                } else {
                    factory.createUndoBulletin(text, subtitle, undo, null).show()
                }
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "не показать отмену" }, e)
            }
        }
    }

    @JvmStatic
    fun showErrorDialog(text: String) {
        AndroidUtilities.runOnUIThread {
            try {
                val fragment = LaunchActivity.getLastFragment() ?: return@runOnUIThread
                val activity = fragment.parentActivity ?: return@runOnUIThread
                val builder = AlertDialog.Builder(activity, fragment.resourceProvider)
                builder.setTitle(org.telegram.messenger.LocaleController.getString(org.telegram.messenger.R.string.ErrorOccurred))
                builder.setMessage(text)
                builder.setPositiveButton(org.telegram.messenger.LocaleController.getString(org.telegram.messenger.R.string.Copy)) { _, _ ->
                    AndroidUtilities.addToClipboard(text)
                }
                builder.setNegativeButton(org.telegram.messenger.LocaleController.getString(org.telegram.messenger.R.string.Close), null)
                fragment.showDialog(builder.create())
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "не показать окно ошибки" }, e)
            }
        }
    }

    @JvmStatic
    fun showDialog(
        title: String,
        message: String,
        positiveText: String?,
        positiveAction: PyObject?,
        negativeText: String?
    ) {
        AndroidUtilities.runOnUIThread {
            try {
                val fragment = LaunchActivity.getLastFragment() ?: return@runOnUIThread
                val activity = fragment.parentActivity ?: return@runOnUIThread
                val builder = AlertDialog.Builder(activity, fragment.resourceProvider)
                builder.setTitle(title)
                builder.setMessage(message)
                if (positiveText != null) {
                    builder.setPositiveButton(positiveText) { _, _ ->
                        if (positiveAction != null) safeCall(positiveAction)
                    }
                }
                if (negativeText != null) {
                    builder.setNegativeButton(negativeText, null)
                }
                fragment.showDialog(builder.create())
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "не показать окно плагина" }, e)
            }
        }
    }
}

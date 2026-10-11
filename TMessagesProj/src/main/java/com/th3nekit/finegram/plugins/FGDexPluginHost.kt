/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins

import android.content.Context
import com.th3nekit.finegram.core.FinegramLogger
import com.th3nekit.finegram.plugins.api.PluginEvents
import com.th3nekit.finegram.plugins.api.PluginHost
import com.th3nekit.finegram.plugins.api.PluginMethodHook
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.ApplicationLoader
import org.telegram.messenger.Utilities
import java.lang.reflect.Member

class FGDexPluginHost(private val pluginId: String) : PluginHost {

    private val preferences = ApplicationLoader.applicationContext
        .getSharedPreferences("finegram_plugin_$pluginId", Context.MODE_PRIVATE)

    override fun id(): String = pluginId

    override fun context(): Context = ApplicationLoader.applicationContext

    override fun log(message: String) {
        FinegramLogger.d("FGPlugins") { "[$pluginId] $message" }
    }

    override fun showBulletin(text: String, kind: String) {
        FGHookBridge.showBulletin(text, kind)
    }

    override fun runOnUiThread(action: Runnable) {
        AndroidUtilities.runOnUIThread { runSafely(action) }
    }

    override fun runInBackground(action: Runnable) {
        Utilities.globalQueue.postRunnable { runSafely(action) }
    }

    private fun runSafely(action: Runnable) {
        try {
            action.run()
        } catch (e: Throwable) {
            FinegramLogger.e("FGPlugins", { "плагин $pluginId: код упал" }, e)
        }
    }

    override fun getBoolean(key: String, fallback: Boolean): Boolean = preferences.getBoolean(key, fallback)

    override fun setBoolean(key: String, value: Boolean) {
        preferences.edit().putBoolean(key, value).apply()
    }

    override fun getInt(key: String, fallback: Int): Int = preferences.getInt(key, fallback)

    override fun setInt(key: String, value: Int) {
        preferences.edit().putInt(key, value).apply()
    }

    override fun getString(key: String, fallback: String?): String? = preferences.getString(key, fallback)

    override fun setString(key: String, value: String?) {
        preferences.edit().putString(key, value).apply()
    }

    override fun hook(method: Member?, hook: PluginMethodHook?): Any? {
        if (method == null || hook == null) {
            return null
        }
        val token = FGHookBridge.hookJava(pluginId, method, hook)
        return if (token == 0) null else token
    }

    override fun unhook(token: Any?) {
        if (token is Int) {
            FGHookBridge.unhookOne(token)
        }
    }

    override fun findClass(className: String?): Class<*>? =
        if (className == null) null else FGHookBridge.findClass(className)

    override fun getField(target: Any?, fieldName: String?): Any? =
        if (fieldName == null) null else FGHookBridge.getPrivateField(target, fieldName)

    override fun setField(target: Any?, fieldName: String?, value: Any?) {
        if (fieldName != null) {
            FGHookBridge.setPrivateField(target, fieldName, value)
        }
    }

    override fun subscribe(events: PluginEvents?) {
        if (events != null) {
            FGPluginsDispatcher.subscribeDex(pluginId, events)
        }
    }

    override fun unsubscribe() {
        FGPluginsDispatcher.unsubscribeDex(pluginId)
    }
}

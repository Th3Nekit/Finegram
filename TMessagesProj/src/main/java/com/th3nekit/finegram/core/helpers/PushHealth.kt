/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.helpers

import android.content.Context
import android.content.SharedPreferences
import androidx.core.content.edit
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.ApplicationLoader
import org.telegram.messenger.UserConfig
import org.telegram.tgnet.ConnectionsManager
import com.th3nekit.finegram.core.FinegramLogger
import com.th3nekit.finegram.core.configs.FinegramCoreConfig

object PushHealth {

    private const val KEY_REGISTERED = "registered_at"
    private const val KEY_LAST = "last_push_at"
    private const val KEY_FALLBACK = "fallback_done"
    private const val SILENCE_MS = 2 * 24 * 60 * 60 * 1000L
    private const val WRITE_EVERY_MS = 60 * 60 * 1000L

    private val prefs: SharedPreferences
        get() = ApplicationLoader.applicationContext.getSharedPreferences("fg_push", Context.MODE_PRIVATE)

    @JvmStatic
    fun onRegistered() {
        if (!prefs.contains(KEY_REGISTERED)) {
            prefs.edit { putLong(KEY_REGISTERED, System.currentTimeMillis()) }
        }
    }

    @JvmStatic
    fun onPushReceived() {
        val now = System.currentTimeMillis()
        if (now - prefs.getLong(KEY_LAST, 0) > WRITE_EVERY_MS) {
            prefs.edit { putLong(KEY_LAST, now) }
        }
    }

    @JvmStatic
    fun lastPushAt(): Long = prefs.getLong(KEY_LAST, 0)

    @JvmStatic
    fun firebaseSilent(): Boolean {
        val registered = prefs.getLong(KEY_REGISTERED, 0)
        return registered != 0L && lastPushAt() == 0L && System.currentTimeMillis() - registered > SILENCE_MS
    }

    @JvmStatic
    fun check() {
        if (!firebaseSilent() || prefs.getBoolean(KEY_FALLBACK, false)) {
            return
        }
        prefs.edit { putBoolean(KEY_FALLBACK, true) }
        if (FinegramCoreConfig.residentChosenByUser() || FinegramCoreConfig.residentNotification) {
            return
        }
        FinegramLogger.d { "push: за двое суток от Firebase ничего не пришло, включаю своё соединение" }
        FinegramCoreConfig.residentNotification = true
        AndroidUtilities.runOnUIThread {
            ApplicationLoader.startPushService()
            refreshPushConnection()
        }
    }

    @JvmStatic
    fun refreshPushConnection() {
        for (a in 0 until UserConfig.MAX_ACCOUNT_COUNT) {
            if (UserConfig.getInstance(a).isClientActivated) {
                val connections = ConnectionsManager.getInstance(a)
                connections.setPushConnectionEnabled(connections.isPushConnectionEnabled)
            }
        }
    }
}

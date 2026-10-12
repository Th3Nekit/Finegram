/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core

import android.util.Log
import org.telegram.messenger.BuildVars

object FinegramLogger {

    private const val TAG: String = "finegramLogger"

    private fun isDebuggable(): Boolean {
        return BuildVars.LOGS_ENABLED
    }

    @JvmStatic
    fun d(message: () -> String) {
        if (!isDebuggable()) return
        Log.d(TAG, message())
    }

    @JvmStatic
    fun d(t: Throwable) {
        Log.d(TAG, t.message, t)
    }

    @JvmStatic
    fun d(tag: String, message: () -> String) {
        if (!isDebuggable()) return
        Log.d(tag, message())
    }

    @JvmStatic
    fun d(message: () -> String, t: Throwable) {
        if (!isDebuggable()) return
        Log.d(TAG, message(), t)
    }

    @JvmStatic
    fun d(tag: String, message: () -> String, t: Throwable) {
        if (!isDebuggable()) return
        Log.d(tag, message(), t)
    }

    @JvmStatic
    fun i(message: () -> String) {
        if (!isDebuggable()) return
        Log.i(TAG, message())
    }

    @JvmStatic
    fun i(t: Throwable) {
        Log.i(TAG, t.message, t)
    }

    @JvmStatic
    fun i(tag: String, message: () -> String) {
        if (!isDebuggable()) return
        Log.i(tag, message())
    }

    @JvmStatic
    fun i(message: () -> String, t: Throwable) {
        if (!isDebuggable()) return
        Log.i(TAG, message(), t)
    }

    @JvmStatic
    fun i(tag: String, message: () -> String, t: Throwable) {
        if (!isDebuggable()) return
        Log.i(tag, message(), t)
    }

    @JvmStatic
    fun w(message: () -> String) {
        if (!isDebuggable()) return
        Log.w(TAG, message())
    }

    @JvmStatic
    fun w(t: Throwable) {
        Log.w(TAG, t.message, t)
    }

    @JvmStatic
    fun w(tag: String, message: () -> String) {
        if (!isDebuggable()) return
        Log.w(tag, message())
    }

    @JvmStatic
    fun w(message: () -> String, t: Throwable) {
        if (!isDebuggable()) return
        Log.w(TAG, message(), t)
    }

    @JvmStatic
    fun w(tag: String, message: () -> String, t: Throwable) {
        if (!isDebuggable()) return
        Log.w(tag, message(), t)
    }

    @JvmStatic
    @JvmOverloads
    fun e(message: () -> String, showOnlyInDev: Boolean = false) {
        if (!showOnlyInDev) Log.e(TAG, message())
    }

    @JvmStatic
    @JvmOverloads
    fun e(t: Throwable, showOnlyInDev: Boolean = false) {
        if (!showOnlyInDev) Log.e(TAG, t.message, t)
    }

    @JvmStatic
    @JvmOverloads
    fun e(tag: String, message: () -> String, showOnlyInDev: Boolean = false) {
        if (!showOnlyInDev) Log.e(tag, message())
    }

    @JvmStatic
    @JvmOverloads
    fun e(message: () -> String, t: Throwable, showOnlyInDev: Boolean = false) {
        if (!showOnlyInDev) Log.e(TAG, message(), t)
    }

    @JvmStatic
    @JvmOverloads
    fun e(tag: String, message: () -> String, t: Throwable, showOnlyInDev: Boolean = false) {
        if (!showOnlyInDev) Log.e(tag, message(), t)
    }

}
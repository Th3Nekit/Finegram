/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.plugins

import android.util.Log

object FGPluginLogSink {

    private const val TAG = "FGPlugins"

    @JvmStatic
    fun onPluginLog(text: String?) {
        if (text.isNullOrEmpty()) {
            return
        }
        Log.i(TAG, text)
    }
}

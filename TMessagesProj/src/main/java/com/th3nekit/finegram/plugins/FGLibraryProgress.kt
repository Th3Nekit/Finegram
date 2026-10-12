/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.plugins

import org.telegram.messenger.AndroidUtilities

object FGLibraryProgress {

    fun interface Listener {

        fun onLibrary(name: String, done: Long, total: Long)
    }

    @Volatile
    private var listener: Listener? = null

    fun listen(listener: Listener?) {
        this.listener = listener
    }

    @JvmStatic
    fun onLibraryProgress(name: String?, done: Int, total: Int) {
        val to = listener ?: return
        val safeName = name ?: return
        AndroidUtilities.runOnUIThread {
            to.onLibrary(safeName, done.toLong(), total.toLong())
        }
    }
}

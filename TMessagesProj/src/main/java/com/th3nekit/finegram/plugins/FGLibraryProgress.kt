/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.plugins

import org.telegram.messenger.AndroidUtilities

/**
 * Что сейчас доезжает для плагина.
 *
 * Библиотека на мегабайт по слабой связи едет ощутимо долго, и без отчёта установка
 * выглядит как зависший клиент: человек видит крутящийся кружок и не знает, идёт работа
 * или уже нет.
 *
 * Слушателя ровно один — экран установки. Держать список незачем: два экрана установки
 * одновременно не открыть.
 */
object FGLibraryProgress {

    fun interface Listener {
        /** @param done сколько уже скачано, @param total сколько всего, ноль — неизвестно */
        fun onLibrary(name: String, done: Long, total: Long)
    }

    @Volatile
    private var listener: Listener? = null

    fun listen(listener: Listener?) {
        this.listener = listener
    }

    /** Зовётся из Python. Имя метода менять нельзя — оно записано на той стороне. */
    @JvmStatic
    fun onLibraryProgress(name: String?, done: Int, total: Int) {
        val to = listener ?: return
        val safeName = name ?: return
        AndroidUtilities.runOnUIThread {
            to.onLibrary(safeName, done.toLong(), total.toLong())
        }
    }
}

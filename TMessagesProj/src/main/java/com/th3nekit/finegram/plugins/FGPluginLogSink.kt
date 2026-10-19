/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.plugins

import android.util.Log

/**
 * Куда попадает то, что пишут плагины.
 *
 * Раньше их строки жили только в кольцевом буфере внутри приложения и были видны на экране
 * плагина. Пока человек рядом — этого хватает, но разбирать чужую поломку по пересказу
 * невозможно: нужен системный журнал, который снимается кабелем.
 *
 * Пишем всегда, не спрашивая общую настройку журналирования: строк от плагинов немного, а
 * выключенный журнал ровно в тот момент, когда что-то сломалось, — это потерянный вечер.
 */
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

/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.helpers

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import org.telegram.messenger.MediaController
import com.th3nekit.finegram.core.configs.FinegramCoreConfig

class SleepHelper : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {
        if (!MediaController.getInstance().isMessagePaused) {
            MediaController.getInstance().pauseMessage(MediaController.getInstance().playingMessageObject)
        }
        FinegramCoreConfig.sleepTimer = false
    }

}
/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.configs

import android.app.Activity
import android.content.SharedPreferences
import org.telegram.messenger.ApplicationLoader
import com.th3nekit.finegram.preferences.boolean
import com.th3nekit.finegram.preferences.int

object FinegramDebugConfig {

    private val sharedPreferences: SharedPreferences = ApplicationLoader.applicationContext.getSharedPreferences("mainconfig", Activity.MODE_PRIVATE)

    var showRPCErrors by sharedPreferences.boolean("EP_ShowRPCErrors", false)
    var oldTimeStyle by sharedPreferences.boolean("CP_OldTimeStyle", false)

    var replacePunctuationMarks by sharedPreferences.boolean("replacePunctuationMarks", true)
    var editTextSuggestionsFix by sharedPreferences.boolean("editTextSuggestionsFix", false)

    const val AUDIO_SOURCE_DEFAULT = 0
    const val AUDIO_SOURCE_CAMCORDER = 1
    const val AUDIO_SOURCE_MIC = 2
    const val AUDIO_SOURCE_REMOTE_SUBMIX = 3
    const val AUDIO_SOURCE_UNPROCESSED = 4
    const val AUDIO_SOURCE_VOICE_CALL = 5
    const val AUDIO_SOURCE_VOICE_COMMUNICATION = 6
    const val AUDIO_SOURCE_VOICE_DOWNLINK = 7
    const val AUDIO_SOURCE_VOICE_PERFORMANCE = 8
    const val AUDIO_SOURCE_VOICE_RECOGNITION = 9
    const val AUDIO_SOURCE_VOICE_UPLINK = 10
    var audioSource by sharedPreferences.int("audioSource", AUDIO_SOURCE_DEFAULT)

    var sendVideosAtMaxQuality by sharedPreferences.boolean("sendVideosMaxQuality", true)
    var playGIFsAsVideos by sharedPreferences.boolean("CP_PlayGIFsAsVideos1", true)
    var hideVideoTimestamp by sharedPreferences.boolean("CP_HideVideoTimestamp", true)

}
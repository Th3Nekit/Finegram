/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.configs

import android.app.Activity
import android.content.SharedPreferences
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.ApplicationLoader
import com.th3nekit.finegram.preferences.boolean
import com.th3nekit.finegram.preferences.int

object FinegramPlayerConfig {

    private val sharedPreferences: SharedPreferences = ApplicationLoader.applicationContext.getSharedPreferences("mainconfig", Activity.MODE_PRIVATE)

    const val COVER_FLAT = 0
    const val COVER_TILT = 1
    const val COVER_VINYL = 2

    const val BACKGROUND_ARTWORK = 0
    const val BACKGROUND_LIVE = 1
    const val BACKGROUND_THEME = 2

    const val ACCENT_ARTWORK = 0
    const val ACCENT_THEME = 1

    const val CARD_ARTWORK = 0
    const val CARD_VIVID = 1
    const val CARD_GLASS = 2
    const val CARD_PLAIN = 3
    const val CARD_STOCK = 4
    const val CARD_CAPSULE = 5

    const val PROGRESS_LINE = 0
    const val PROGRESS_WAVE = 1
    const val PROGRESS_NONE = 2

    var fullscreenPlayer by sharedPreferences.boolean("FG_PlayerFullscreen", true)
    var coverStyle by sharedPreferences.int("FG_PlayerCoverStyle", COVER_FLAT)
    var coverBreath by sharedPreferences.boolean("FG_PlayerCoverBreath", true)
    var background by sharedPreferences.int("FG_PlayerBackground", BACKGROUND_ARTWORK)
    var accent by sharedPreferences.int("FG_PlayerAccent", ACCENT_ARTWORK)
    var wavyProgress by sharedPreferences.boolean("FG_PlayerWavyProgress", true)

    var miniPlayerShowCover by sharedPreferences.boolean("FG_MiniPlayerShowCover", true)
    var miniPlayerNextTrack by sharedPreferences.boolean("FG_MiniPlayerNextTrack", true)
    var miniPlayerGlass by sharedPreferences.boolean("FG_MiniPlayerGlass", true)
    var miniPlayerArtworkTint by sharedPreferences.boolean("FG_MiniPlayerArtworkTint", true)

    var profileCardStyle by sharedPreferences.int("FG_ProfileMusicCard", CARD_CAPSULE)
    var profileRoundCover by sharedPreferences.boolean("FG_ProfileMusicRoundCover", false)
    var profileProgress by sharedPreferences.int("FG_ProfileMusicProgress", PROGRESS_LINE)
    var profileEqualizer by sharedPreferences.boolean("FG_ProfileMusicEqualizer", true)

    @JvmStatic
    fun liveBackground(): Boolean = background == BACKGROUND_LIVE

    @JvmStatic
    fun themeBackground(): Boolean = background == BACKGROUND_THEME

    @JvmStatic
    fun stockProfileCard(): Boolean = profileCardStyle == CARD_STOCK

    @JvmStatic
    fun capsuleProfileCard(): Boolean = profileCardStyle == CARD_CAPSULE

    class Observer(private val prefix: String, private val update: Runnable) {
        private var active = false
        private var known = snapshot()
        private val pending = Runnable {
            if (active) {
                val current = snapshot()
                if (known != current) {
                    known = current
                    update.run()
                }
            }
        }
        private val listener = SharedPreferences.OnSharedPreferenceChangeListener { _, key ->
            if (active && (key == null || key.startsWith(prefix))) schedule()
        }

        private fun snapshot(): Map<String, *> = sharedPreferences.all.filterKeys { it.startsWith(prefix) }

        private fun schedule() {
            AndroidUtilities.cancelRunOnUIThread(pending)
            AndroidUtilities.runOnUIThread(pending, 1)
        }

        fun start() {
            if (active) return
            active = true
            sharedPreferences.registerOnSharedPreferenceChangeListener(listener)
            schedule()
        }

        fun stop() {
            active = false
            sharedPreferences.unregisterOnSharedPreferenceChangeListener(listener)
            AndroidUtilities.cancelRunOnUIThread(pending)
        }
    }

}

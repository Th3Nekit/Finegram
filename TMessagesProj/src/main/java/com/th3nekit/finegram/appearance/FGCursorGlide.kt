/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.appearance

import android.view.View
import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig
import org.telegram.messenger.AndroidUtilities
import java.util.WeakHashMap
import kotlin.math.abs
import kotlin.math.exp

object FGCursorGlide {

    private val tau: Float
        get() = 950f / FinegramAppearanceConfig.composerCursorSpeed.coerceIn(5, 60)

    private class State {
        var line = -1
        var x = 0f
        var lastFrame = 0L
    }

    private val states = WeakHashMap<View, State>()

    @JvmStatic
    fun horizontal(view: View?, line: Int, target: Float): Float {
        if (view == null || !FinegramAppearanceConfig.composerCursorGlide) return target

        val state = states.getOrPut(view) { State() }
        val now = System.currentTimeMillis()
        val far = abs(target - state.x) > AndroidUtilities.dp(160f)

        if (state.line != line || far || state.lastFrame == 0L) {
            state.line = line
            state.x = target
            state.lastFrame = now
            return target
        }

        val dt = (now - state.lastFrame).coerceIn(0L, 64L).toFloat()
        state.lastFrame = now
        state.x += (target - state.x) * (1f - exp(-dt / tau))

        if (abs(target - state.x) < 0.5f) {
            state.x = target
        } else {
            view.invalidate()
        }
        return state.x
    }

    @JvmStatic
    fun reset(view: View?) {
        if (view == null) return
        states.remove(view)
    }
}

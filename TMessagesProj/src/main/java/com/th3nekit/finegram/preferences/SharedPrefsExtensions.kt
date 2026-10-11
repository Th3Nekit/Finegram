/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.preferences

import android.content.SharedPreferences
import kotlin.properties.ReadWriteProperty
import kotlin.reflect.KProperty
import androidx.core.content.edit
import java.util.concurrent.atomic.AtomicReference

sealed class CachedPreference<T>(
        protected val preferences: SharedPreferences,
        protected val key: String,
) {
    private class Value(val cached: Any?)
    private val value = AtomicReference(Value(null))

    init {
        PreferenceCache.register(preferences, key, this)
    }

    protected abstract fun readFromPreferences(): T

    @Suppress("UNCHECKED_CAST")
    protected fun read(): T {
        while (true) {
            val snapshot = value.get()
            val known = snapshot.cached
            if (known != null) {
                return known as T
            }
            val fresh = readFromPreferences()
            if (value.compareAndSet(snapshot, Value(fresh))) {
                return fresh
            }
        }
    }

    internal fun forget() {
        value.set(Value(null))
    }
}

private object PreferenceCache {

    private val watched = HashMap<SharedPreferences, MutableMap<String, MutableList<CachedPreference<*>>>>()
    private val listeners = HashMap<SharedPreferences, SharedPreferences.OnSharedPreferenceChangeListener>()

    @Synchronized
    fun register(preferences: SharedPreferences, key: String, preference: CachedPreference<*>) {
        val keys = watched.getOrPut(preferences) {
            val listener = SharedPreferences.OnSharedPreferenceChangeListener { changed, changedKey ->
                onChanged(changed, changedKey)
            }
            listeners[preferences] = listener
            preferences.registerOnSharedPreferenceChangeListener(listener)
            HashMap()
        }
        keys.getOrPut(key) { ArrayList(1) }.add(preference)
    }

    @Synchronized
    private fun onChanged(preferences: SharedPreferences, key: String?) {
        val keys = watched[preferences] ?: return
        if (key == null) {

            keys.values.forEach { list -> list.forEach { it.forget() } }
            return
        }
        keys[key]?.forEach { it.forget() }
    }
}

class StringPreference(
        preferences: SharedPreferences,
        key: String,
        private val defaultValue: String,
) : CachedPreference<String>(preferences, key), ReadWriteProperty<Any, String> {
    override fun readFromPreferences(): String = preferences.getString(key, defaultValue) ?: defaultValue
    override fun getValue(thisRef: Any, property: KProperty<*>): String = read()
    override fun setValue(thisRef: Any, property: KProperty<*>, value: String) {
        preferences.edit { putString(key, value) }
        forget()
    }
}

class IntPreference(
        preferences: SharedPreferences,
        key: String,
        private val defaultValue: Int,
) : CachedPreference<Int>(preferences, key), ReadWriteProperty<Any, Int> {
    override fun readFromPreferences(): Int = preferences.getInt(key, defaultValue)
    override fun getValue(thisRef: Any, property: KProperty<*>): Int = read()
    override fun setValue(thisRef: Any, property: KProperty<*>, value: Int) {
        preferences.edit { putInt(key, value) }
        forget()
    }
}

class FloatPreference(
        preferences: SharedPreferences,
        key: String,
        private val defaultValue: Float,
) : CachedPreference<Float>(preferences, key), ReadWriteProperty<Any, Float> {
    override fun readFromPreferences(): Float = preferences.getFloat(key, defaultValue)
    override fun getValue(thisRef: Any, property: KProperty<*>): Float = read()
    override fun setValue(thisRef: Any, property: KProperty<*>, value: Float) {
        preferences.edit { putFloat(key, value) }
        forget()
    }
}

class BooleanPreference(
        preferences: SharedPreferences,
        key: String,
        private val defaultValue: Boolean,
) : CachedPreference<Boolean>(preferences, key), ReadWriteProperty<Any, Boolean> {
    override fun readFromPreferences(): Boolean = preferences.getBoolean(key, defaultValue)
    override fun getValue(thisRef: Any, property: KProperty<*>): Boolean = read()
    override fun setValue(thisRef: Any, property: KProperty<*>, value: Boolean) {
        preferences.edit { putBoolean(key, value) }
        forget()
    }
}

class LongPreference(
        preferences: SharedPreferences,
        key: String,
        private val defaultValue: Long,
) : CachedPreference<Long>(preferences, key), ReadWriteProperty<Any, Long> {
    override fun readFromPreferences(): Long = preferences.getLong(key, defaultValue)
    override fun getValue(thisRef: Any, property: KProperty<*>): Long = read()
    override fun setValue(thisRef: Any, property: KProperty<*>, value: Long) {
        preferences.edit { putLong(key, value) }
        forget()
    }
}

fun SharedPreferences.int(key: String, defaultValue: Int): ReadWriteProperty<Any, Int> = IntPreference(this, key, defaultValue)
fun SharedPreferences.float(key: String, defaultValue: Float): ReadWriteProperty<Any, Float> = FloatPreference(this, key, defaultValue)
fun SharedPreferences.boolean(key: String, defaultValue: Boolean): ReadWriteProperty<Any, Boolean> = BooleanPreference(this, key, defaultValue)
fun SharedPreferences.string(key: String, defaultValue: String): ReadWriteProperty<Any, String> = StringPreference(this, key, defaultValue)
fun SharedPreferences.long(key: String, defaultValue: Long): ReadWriteProperty<Any, Long> = LongPreference(this, key, defaultValue)

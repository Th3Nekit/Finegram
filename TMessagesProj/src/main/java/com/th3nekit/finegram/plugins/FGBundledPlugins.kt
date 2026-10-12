/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.plugins

import android.content.Context
import org.telegram.messenger.ApplicationLoader
import com.th3nekit.finegram.core.FinegramLogger
import java.io.File

object FGBundledPlugins {

    private val BUNDLED = emptyMap<String, String>()

    private const val PREFS = "finegram_plugins"

    @JvmStatic
    fun isBundled(id: String?): Boolean = id != null && BUNDLED.containsKey(id)

    @JvmStatic
    fun unpack() {
        val context = ApplicationLoader.applicationContext ?: return
        val root = File(ApplicationLoader.getFilesDirFixed(), "plugins")
        if (!root.exists()) root.mkdirs()

        val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        for ((id, asset) in BUNDLED) {
            try {
                val target = File(root, "$id.plugin")
                val version = readAssetVersion(context, asset)
                val known = prefs.getString("bundled_version_$id", null)
                if (target.exists() && known == version) {
                    continue
                }
                val firstTime = known == null
                context.assets.open(asset).use { input ->
                    target.outputStream().use { output -> input.copyTo(output) }
                }
                prefs.edit().putString("bundled_version_$id", version).apply()
                FinegramLogger.d("FGPlugins") { "встроенный плагин $id распакован, версия $version" }

                if (firstTime) {
                    FinegramLogger.d("FGPlugins") { "встроенный плагин $id готов к работе" }
                }
            } catch (e: Throwable) {
                FinegramLogger.e("FGPlugins", { "встроенный плагин $id не распаковался" }, e)
            }
        }
    }

    private fun readAssetVersion(context: Context, asset: String): String {
        return try {
            context.assets.open(asset).use { input ->
                val head = ByteArray(2048)
                val read = input.read(head)
                if (read <= 0) return ""
                val text = String(head, 0, read, Charsets.UTF_8)
                Regex("""__version__\s*=\s*["']([^"']+)["']""").find(text)?.groupValues?.get(1) ?: ""
            }
        } catch (e: Throwable) {
            ""
        }
    }

}

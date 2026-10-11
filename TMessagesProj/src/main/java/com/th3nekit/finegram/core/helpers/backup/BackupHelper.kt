/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.helpers.backup

import com.th3nekit.finegram.core.ui.FGTitles
import android.app.Activity
import android.content.Context
import android.content.DialogInterface
import android.widget.TextView
import android.widget.Toast
import androidx.core.content.edit
import com.google.gson.JsonObject
import com.google.gson.JsonParser
import org.json.JSONObject
import org.telegram.messenger.AndroidUtilities
import org.telegram.messenger.LocaleController.getString
import org.telegram.messenger.R
import org.telegram.messenger.SendMessagesHelper
import org.telegram.ui.ActionBar.AlertDialog
import com.th3nekit.finegram.helpers.FGFileSender
import org.telegram.ui.ActionBar.BaseFragment
import org.telegram.ui.ActionBar.Theme
import com.th3nekit.finegram.core.PermissionsUtils
import com.th3nekit.finegram.core.helpers.AppRestartHelper
import java.io.File
import java.io.InputStreamReader
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object BackupHelper {

    const val FILE_TYPE_FG_BACKUP = 1390

    fun backupSettings(fragment: BaseFragment?) {
        if (fragment == null || fragment.parentActivity == null || fragment.context == null) return

        val context = fragment.context ?: return

        try {
            val formattedDate = SimpleDateFormat("yyyyMMdd_HHmmss_SSS", Locale.US).format(Date())
            val fileName = "$formattedDate-settings.finegram"
            val dir = File(context.cacheDir, "settings-exports")
            val file = File(dir, fileName)
            writeUtf8String(backupSettingsJson(context), file)
            shareFile(fragment, file)
        } catch (e: Exception) {
            handleError(context, e)
        }
    }

    fun importSettings(fragment: BaseFragment?) {
        if (fragment == null || fragment.parentActivity == null || fragment.context == null) return

        if (!PermissionsUtils.isStoragePermissionGranted()) {
            PermissionsUtils.requestStoragePermission(fragment.parentActivity)
            return
        }

        val context = fragment.context ?: return

        val importActivity = BackupFileImportActivity().apply {
            setMaxSelectedFiles(1)
            setDelegate(object : BackupFileImportActivity.DocumentSelectActivityDelegate {
                override fun didSelectFiles(
                    activity: BackupFileImportActivity,
                    files: ArrayList<String>,
                    caption: String,
                    notify: Boolean,
                    scheduleDate: Int
                ) {
                    activity.finishFragment()
                    if (files.isEmpty()) return
                    importSettings(File(files.first()), context)
                }

                override fun didSelectPhotos(
                    photos: ArrayList<SendMessagesHelper.SendingMediaInfo>,
                    notify: Boolean,
                    scheduleDate: Int
                ) {}

                override fun startDocumentSelectActivity() {}
            })
        }

        fragment.presentFragment(importActivity)
    }

    fun importSettings(file: File, context: Context) {
        if (!file.exists() || !file.canRead()) {
            handleError(context, Exception("File not accessible"))
            return
        }

        AlertDialog.Builder(context).apply {
            setTitle(getString(R.string.FG_ImportSettings))
            setMessage(getString(R.string.FG_ImportSettingsAlert))
            setNegativeButton(getString(R.string.Cancel), null)
            setPositiveButton(getString(R.string.OK)) { _, _ ->
                importSettingsConfirmed(file, context)
            }
            val dialog = show()
            val button = dialog.getButton(DialogInterface.BUTTON_POSITIVE) as? TextView
            button?.setTextColor(Theme.getColor(Theme.key_text_RedBold))
        }
    }

    private fun importSettingsConfirmed(file: File, context: Context) {
        try {
            val json = readJsonObjectWithGson(file)
            restoreSharedPreferences(json, context)

            val dialog = AlertDialog(context, 0)
            dialog.setTitle(FGTitles.appName())
            dialog.setMessage(getString(R.string.FG_RestartToApply))
            dialog.setPositiveButton(getString(R.string.BotUnblock)) { _, _ ->
                AppRestartHelper.restartApp(context)
            }
            dialog.show()
        } catch (e: Exception) {
            handleError(context, e)
        }
    }

    private fun shareFile(fragment: BaseFragment?, fileToShare: File, caption: String = "") {
        if (fragment == null || !fileToShare.exists()) return
        try {
            FGFileSender.pickChatAndSend(fragment, fileToShare, caption)
        } catch (e: Exception) {
            fragment.context?.let { handleError(it, e) }
        }
    }

    private fun writeUtf8String(text: String, file: File) {
        try {
            file.parentFile?.let { initDir(it) }
            file.writeText(text, Charsets.UTF_8)
        } catch (e: Exception) {
            throw e
        }
    }

    private fun readJsonObjectWithGson(file: File): JsonObject {
        file.inputStream().buffered().use { inputStream ->
            InputStreamReader(inputStream, Charsets.UTF_8).use { reader ->
                return JsonParser.parseReader(reader).asJsonObject
            }
        }
    }

    private fun restoreSharedPreferences(json: JsonObject, context: Context) {
        for ((spName, data) in json.entrySet()) {
            val prefs = context.getSharedPreferences(spName, Activity.MODE_PRIVATE)
            prefs.edit {
                for ((keyRaw, valueElement) in data.asJsonObject.entrySet()) {
                    var key = keyRaw

                    val plain = key.removeSuffix("_long").removeSuffix("_float")
                    if (!isOwnKey(plain)) continue
                    val value = valueElement.asJsonPrimitive
                    when {
                        value.isBoolean -> putBoolean(key, value.asBoolean)
                        value.isNumber -> {
                            when {
                                key.endsWith("_long") -> {
                                    key = key.removeSuffix("_long")
                                    putLong(key, value.asLong)
                                }
                                key.endsWith("_float") -> {
                                    key = key.removeSuffix("_float")
                                    putFloat(key, value.asFloat)
                                }
                                else -> putInt(key, value.asInt)
                            }
                        }
                        else -> putString(key, value.asString)
                    }
                }
            }
        }
    }

    private fun initDir(dir: File) {
        try {
            if (dir.exists() && dir.isFile) {
                dir.delete()
            }
            if (!dir.exists()) {
                dir.mkdirs()
            }
        } catch (_: Exception) {}
    }

    private fun handleError(context: Context, e: Exception) {
        try {
            AndroidUtilities.addToClipboard(e.toString())
            Toast.makeText(context, e.toString(), Toast.LENGTH_SHORT).show()
        } catch (_: Exception) {}
    }

    private val OWN_PREFIXES = listOf("FG_", "CP_", "AP_", "SP_", "EP_")

    private val SECRET_KEYS = setOf("CP_GeminiApiKey")

    private fun isOwnKey(key: String): Boolean =
        key !in SECRET_KEYS && OWN_PREFIXES.any { key.startsWith(it) }

    private fun backupSettingsJson(context: Context): String {
        val json = JSONObject()
        spToJSON("mainconfig", json, context)
        return json.toString(4)
    }

    private fun spToJSON(name: String, target: JSONObject, context: Context) {
        val prefs = context.getSharedPreferences(name, Activity.MODE_PRIVATE)
        val jsonPrefs = JSONObject()

        for ((keyRaw, value) in prefs.all) {
            var key = keyRaw
            if (!isOwnKey(key)) continue
            when (value) {
                is Long -> key += "_long"
                is Float -> key += "_float"
            }
            jsonPrefs.put(key, value)
        }

        target.put(name, jsonPrefs)
    }

}

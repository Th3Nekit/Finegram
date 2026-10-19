/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.chats.helpers

import android.os.SystemClock
import android.text.SpannableStringBuilder
import android.text.Spanned
import androidx.core.content.edit
import com.google.gson.Gson
import com.google.gson.reflect.TypeToken
import org.telegram.messenger.BaseController
import org.telegram.messenger.ApplicationLoader
import org.telegram.messenger.NotificationCenter
import org.telegram.messenger.SharedConfig
import org.telegram.messenger.DialogObject
import org.telegram.messenger.FingerprintController
import org.telegram.messenger.MessageObject
import org.telegram.messenger.UserConfig
import org.telegram.tgnet.TLRPC
import org.telegram.tgnet.TLRPC.MessageEntity
import org.telegram.tgnet.TLRPC.TL_messageEntitySpoiler
import org.telegram.ui.Components.TextStyleSpan
import org.telegram.ui.Components.ForegroundDetector
import com.th3nekit.finegram.core.FGBiometricPrompt
import com.th3nekit.finegram.core.FinegramLogger
import com.th3nekit.finegram.core.configs.FinegramPrivacyConfig
import kotlin.random.Random

class ChatsPasswordHelper private constructor(num: Int) : BaseController(num) {

    companion object {

        /** Когда за этот чат последний раз показывали отпечаток. */
        private val unlockedAt = java.util.concurrent.ConcurrentHashMap<String, Long>()

        private val instances = arrayOfNulls<ChatsPasswordHelper>(UserConfig.MAX_ACCOUNT_COUNT)

        @JvmStatic
        fun getInstance(num: Int): ChatsPasswordHelper {
            return instances[num] ?: synchronized(ChatsPasswordHelper::class.java) {
                instances[num] ?: ChatsPasswordHelper(num).also { instances[num] = it }
            }
        }
    }

    private object ArchiveSession {
        var generation = 0L
            private set
        private var selectedAccount = UserConfig.selectedAccount
        private var observing = false
        private val observer = NotificationCenter.NotificationCenterDelegate { _, _, _ -> invalidate() }

        fun invalidate() {
            generation++
            selectedAccount = UserConfig.selectedAccount
        }

        fun isAvailable(): Boolean {
            val detector = ForegroundDetector.getInstance()
            if (detector == null) {
                invalidate()
                return false
            }
            if (!observing) {
                detector.addListener(object : ForegroundDetector.Listener {
                    override fun onBecameForeground() {}
                    override fun onBecameBackground() { invalidate() }
                })
                val global = NotificationCenter.getGlobalInstance()
                global.addObserver(observer, NotificationCenter.activeAccountChanged)
                global.addObserver(observer, NotificationCenter.passcodeDismissed)
                for (account in 0 until UserConfig.MAX_ACCOUNT_COUNT) {
                    NotificationCenter.getInstance(account).addObserver(observer, NotificationCenter.appDidLogout)
                }
                observing = true
            }
            if (selectedAccount != UserConfig.selectedAccount || detector.isBackground ||
                ApplicationLoader.mainInterfacePaused || SharedConfig.appLocked || SharedConfig.isWaitingForPasscodeEnter) {
                invalidate()
                return false
            }
            return true
        }
    }

    private var archiveUnlockedGeneration = -1L
    private var archiveUnlockedUserId = 0L

    fun getArchiveAuthGeneration(): Long {
        return if (ArchiveSession.isAvailable() && currentAccount == UserConfig.selectedAccount)
            ArchiveSession.generation else -1L
    }

    fun rememberArchiveUnlocked(generation: Long): Boolean {
        if (generation < 0 || getArchiveAuthGeneration() != generation) return false
        archiveUnlockedGeneration = generation
        archiveUnlockedUserId = userConfig.clientUserId
        return true
    }

    fun isArchiveLocked(): Boolean {
        val available = ArchiveSession.isAvailable()
        if (!FinegramPrivacyConfig.askBiometricsToOpenArchive) {
            archiveUnlockedGeneration = -1L
            return false
        }
        return !available || currentAccount != UserConfig.selectedAccount ||
            archiveUnlockedGeneration != ArchiveSession.generation || archiveUnlockedUserId != userConfig.clientUserId
    }

    fun canAccessDialog(dialogId: Long): Boolean {
        return canAccessDialog(messagesController.getDialog(dialogId))
    }

    fun canAccessDialog(dialog: TLRPC.Dialog?): Boolean {
        return dialog == null || dialog.folder_id != 1 || !isArchiveLocked()
    }

    private var lockedChatsCache: HashSet<String>? = null

    fun getPasscodeArray(): String = "locked_chats_list"

    fun saveArrayList(list: ArrayList<String>, key: String) {
        FinegramLogger.d { "запросил saveArrayList" }

        if (key == getPasscodeArray()) {
            lockedChatsCache = HashSet(list)
        }

        messagesController.mainSettings
            .edit {
                putString(key, Gson().toJson(list))
            }
    }

    fun getArrayList(key: String): ArrayList<String> {
        FinegramLogger.d { "запросил кешированный getArrayList для паролей" }

        if (key == getPasscodeArray() && lockedChatsCache != null) {
            return ArrayList(lockedChatsCache!!)
        }

        FinegramLogger.d { "запросил getArrayList для паролей" }

        val json = messagesController.mainSettings.getString(key, null)
        val list: ArrayList<String> = Gson().fromJson(json, object : TypeToken<ArrayList<String>>() {}.type)
            ?: arrayListOf()

        if (key == getPasscodeArray()) {
            lockedChatsCache = HashSet(list)
        }

        return list
    }

    fun isChatLocked(chatId: Long): Boolean {
        FinegramLogger.d { "запросил isChatLocked" }
        if (chatId == 0L || !FinegramPrivacyConfig.askBiometricsToOpenChat) return false

        if (lockedChatsCache == null) {
            getArrayList(getPasscodeArray())
        }

        val idStr = chatId.toString()
        return lockedChatsCache?.contains(idStr) == true || lockedChatsCache?.contains("-$idStr") == true
    }

    fun isChatLocked(messageObject: MessageObject): Boolean {
        FinegramLogger.d { "запросил isChatLocked2" }
        return FinegramPrivacyConfig.askBiometricsToOpenChat && messageObject.messageOwner.message != null
                && !messageObject.isStoryReactionPush && !messageObject.isStoryPush
                && !messageObject.isStoryMentionPush && !messageObject.isStoryPushHidden
                && isChatLocked(messageObject.chatId)
    }

    fun isEncryptedChat(chatId: Long): Boolean {
        FinegramLogger.d { "запросил isEncryptedChat" }
        if (FinegramPrivacyConfig.askBiometricsToOpenEncrypted) {
            val encID = DialogObject.getEncryptedChatId(chatId)
            val encryptedChat = messagesController.getEncryptedChat(encID)
            return encryptedChat != null
        } else {
            return false
        }
    }

    fun isEncryptedChat(messageObject: MessageObject): Boolean {
        FinegramLogger.d { "запросил isEncryptedChat2" }
        if (FinegramPrivacyConfig.askBiometricsToOpenEncrypted) {
            val encID = DialogObject.getEncryptedChatId(messageObject.dialogId)
            val encryptedChat = messagesController.getEncryptedChat(encID)
            return messageObject.messageOwner.message != null
                    && !messageObject.isStoryReactionPush && !messageObject.isStoryPush
                    && !messageObject.isStoryMentionPush && !messageObject.isStoryPushHidden
                    && encryptedChat != null
        } else {
            return false
        }
    }

    fun checkLockedChatsEntities(messageObject: MessageObject): ArrayList<MessageEntity>? {
        FinegramLogger.d { "запросил checkLockedChatsEntities" }
        return checkLockedChatsEntities(messageObject, messageObject.messageOwner.entities)
    }

    fun checkLockedChatsEntities(messageObject: MessageObject, original: ArrayList<MessageEntity>?): ArrayList<MessageEntity>? {
        FinegramLogger.d { "запросил checkLockedChatsEntities2" }
        return if (isChatLocked(messageObject) || isEncryptedChat(messageObject)) {
            val entities = original?.let { ArrayList(it) }
            val spoiler = TL_messageEntitySpoiler()
            spoiler.offset = 0
            spoiler.length = messageObject.messageOwner.message.length
            entities?.add(spoiler)
            entities
        } else {
            original
        }
    }

    private var spoilerChars: CharArray = charArrayOf(
        '⠌', '⡢', '⢑', '⠨', '⠥', '⠮', '⡑'
    )

    fun replaceStringToSpoilers(originalText: String?, force: Boolean): String? {
        FinegramLogger.d { "запросил replaceStringToSpoilers" }
        if (originalText == null) {
            return null
        }
        return if (FinegramPrivacyConfig.askBiometricsToOpenArchive || force) {
            val useRandomBraille = /*Random.nextBoolean()*/ false
            if (useRandomBraille) {
                toBrailleSpoiler(originalText)
            } else {
                val stringBuilder = StringBuilder(originalText)
                for (i in originalText.indices) {
                    stringBuilder.setCharAt(i, spoilerChars[i % spoilerChars.size])
                }
                stringBuilder.toString()
            }
        } else {
            originalText
        }
    }

    private fun toBrailleSpoiler(input: String?): String {
        if (input.isNullOrEmpty()) return ""

        val sb = StringBuilder(input.length)
        for (char in input) {
            if (char.isWhitespace()) {
                sb.append(char)
            } else {
                val brailleCode = 0x2801 + Random.nextInt(255)
                sb.append(brailleCode.toChar())
            }
        }
        return sb.toString()
    }

    fun replaceStringToSpoilers(originalText: CharSequence?): SpannableStringBuilder {
        if (originalText == null) {
            return SpannableStringBuilder("")
        }

        val spannable = SpannableStringBuilder(originalText)

        val run = TextStyleSpan.TextStyleRun()
        run.flags = run.flags or TextStyleSpan.FLAG_STYLE_SPOILER
        run.start = 0
        run.end = spannable.length

        spannable.setSpan(
            TextStyleSpan(run),
            0,
            spannable.length,
            Spanned.SPAN_EXCLUSIVE_EXCLUSIVE
        )

        return spannable
    }

    fun getLockedChatsCount(): Int {
        FinegramLogger.d { "запросил getLockedChatsCount" }
        return getArrayList(getPasscodeArray()).size
    }

    fun shouldRequireBiometrics(userID: Long, chatID: Long, encID: Long): Boolean {
        FinegramLogger.d { "запросил shouldRequireBiometrics" }
        val lockedChat = (userID != 0L && isChatLocked(userID)) || (chatID != 0L && isChatLocked(chatID))

        val encryptedChat = encID != 0L && isEncryptedChat(encID)

        val required = (lockedChat && shouldRequireBiometricsToOpenChats()) ||
                (encryptedChat && shouldRequireBiometricsToOpenEncryptedChats())
        if (!required) return false
        return !isRecentlyUnlocked(dialogKey(userID, chatID, encID))
    }

    /**
     * Один и тот же чат за короткое время не спрашивает отпечаток дважды.
     *
     * Выйти из чата и тут же вернуться — та же сессия, а не новая попытка доступа. Срок
     * задаёт человек; ноль означает «спрашивать всегда», и тогда ничего не запоминается.
     * Память живёт до перезапуска: сняли приложение из недавних — снова замок.
     */
    fun rememberUnlocked(userID: Long, chatID: Long, encID: Long) {
        if (FinegramPrivacyConfig.lockedChatsRememberSeconds <= 0) return
        unlockedAt[dialogKey(userID, chatID, encID)] = SystemClock.elapsedRealtime()
    }

    private fun isRecentlyUnlocked(key: String): Boolean {
        val seconds = FinegramPrivacyConfig.lockedChatsRememberSeconds
        if (seconds <= 0) return false
        val at = unlockedAt[key] ?: return false
        val passed = SystemClock.elapsedRealtime() - at
        if (passed in 0..(seconds * 1000L)) return true
        unlockedAt.remove(key)
        return false
    }

    private fun dialogKey(userID: Long, chatID: Long, encID: Long): String = "$userID:$chatID:$encID"

    fun shouldRequireBiometricsToOpenChats(): Boolean {
        FinegramLogger.d { "запросил shouldRequireBiometricsToOpenChats" }
        return FinegramPrivacyConfig.askBiometricsToOpenChat && checkBiometricAvailable()
    }

    fun shouldRequireBiometricsToOpenEncryptedChats(): Boolean {
        FinegramLogger.d { "запросил shouldRequireBiometricsToOpenEncryptedChats" }
        return FinegramPrivacyConfig.askBiometricsToOpenEncrypted && checkBiometricAvailable()
    }

    fun askPasscodeBeforeDelete(): Boolean {
        FinegramLogger.d { "запросил askPasscodeBeforeDelete" }
        return FinegramPrivacyConfig.askPasscodeBeforeDelete && checkBiometricAvailable()
    }

    fun checkBiometricAvailable(): Boolean {
        FinegramLogger.d { "запросил checkBiometricAvailable" }

        val hasBiometrics = FGBiometricPrompt.hasBiometricEnrolled()
        if (!hasBiometrics) return false

        val hasFingerprints = FGBiometricPrompt.hasEnrolledFingerprints()
        return if (hasFingerprints) {
            FingerprintController.isKeyReady() && !FingerprintController.checkDeviceFingerprintsChanged()
        } else {
            true
        }
    }

}

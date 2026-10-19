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
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import org.telegram.messenger.ApplicationLoader
import com.th3nekit.finegram.preferences.boolean
import com.th3nekit.finegram.preferences.int

object FinegramPrivacyConfig: CoroutineScope by CoroutineScope(
    context = SupervisorJob() + Dispatchers.Default
) {

    private val sharedPreferences: SharedPreferences = ApplicationLoader.applicationContext.getSharedPreferences("mainconfig", Activity.MODE_PRIVATE)

    /** Privacy start */
    /**
     * Режим призрака: клиент перестаёт рассказывать о себе.
     *
     * Общий выключатель поверх остальных. Так задумано: человек хочет «стать невидимым» одним
     * движением, а разбираться, что именно из четырёх пунктов его выдаёт, — потом и по желанию.
     */
    var ghostMode by sharedPreferences.boolean("FG_GhostMode", false)

    /** Не сообщать, что вы в сети. Запрос уходит, но говорит «меня нет». */
    var ghostHideOnline by sharedPreferences.boolean("FG_GhostHideOnline", true)

    /** Не сообщать, что вы набираете текст. */
    var ghostHideTyping by sharedPreferences.boolean("FG_GhostHideTyping", true)

    /** Не отмечать сообщения прочитанными у собеседника. */
    var ghostHideReading by sharedPreferences.boolean("FG_GhostHideReading", true)

    /** Не отмечаться среди просмотревших историю. */
    var ghostHideStoryViews by sharedPreferences.boolean("FG_GhostHideStoryViews", true)

    /** Уходить в офлайн сразу после отправки: иначе «в сети» висит ещё несколько минут. */
    var ghostOfflineAfterSend by sharedPreferences.boolean("FG_GhostOfflineAfterSend", false)

    /**
     * Сохранять удалённые собеседником сообщения.
     *
     * Работает не копированием, а отказом удалять: сообщение остаётся в базе клиента как
     * было, со вложениями и оформлением, и отдельно помечается удалённым.
     */
    var saveDeleted by sharedPreferences.boolean("FG_SaveDeleted", false)

    /** Ботов обычно не сохраняют: они сыплют служебным и сами же его убирают. */
    var saveDeletedFromBots by sharedPreferences.boolean("FG_SaveDeletedFromBots", false)

    /** В каналах удаляют свои же записи редакторы — это правка, а не пропажа. */
    var saveDeletedFromChannels by sharedPreferences.boolean("FG_SaveDeletedFromChannels", false)

    var hideProxySponsor by sharedPreferences.boolean("SP_NoProxySponsor", true)
    /** Privacy finish */

    /** Через сколько секунд снова спрашивать за защищённый чат; 0 — каждый раз. */
    var lockedChatsRememberSeconds by sharedPreferences.int("FG_LockedChatsRemember", 0)

    /** Passcode lock start */
    var hideArchiveFromChatsList by sharedPreferences.boolean("SP_HideArchiveFromChatsList", false)
    var askBiometricsToOpenArchive by sharedPreferences.boolean("SP_AskBiometricsToOpenArchive", false)
    var askBiometricsToOpenEncrypted by sharedPreferences.boolean("SP_AskBiometricsToOpenEncrypted", false)
    var askBiometricsToOpenChat by sharedPreferences.boolean("SP_AskBiometricsToOpenChat", false)
    var askPasscodeBeforeDelete by sharedPreferences.boolean("SP_AskPinBeforeDelete", false)
    var allowSystemPasscode by sharedPreferences.boolean("SP_AllowSystemPasscode", false)
    /** Passcode lock finish */

    /** Misc **/
    var hideArchivedStories by sharedPreferences.boolean("CP_HideArchivedStories", false)
    /** Misc **/

    /** Анонимность: приёмы, взятые из re:extera. Реализация своя. */

    /** Клиент считает вас подписчиком Premium. Сервер об этом не знает. */
    var localPremium by sharedPreferences.boolean("FG_LocalPremium", false)

    /** Снимать запрет на скриншоты, который клиент ставит в секретных чатах. */
    var allowScreenshots by sharedPreferences.boolean("FG_AllowScreenshots", false)

    /** Удалённые собеседником сообщения показывать приглушённо. */
    var dimDeleted by sharedPreferences.boolean("FG_DimDeleted", false)

    /** Пересылать оттуда, где пересылка запрещена, — копией от своего имени. */
    var pseudoForward by sharedPreferences.boolean("FG_PseudoForward", false)

    /** Хранить прежние версии изменённых сообщений. */
    var keepEdits by sharedPreferences.boolean("FG_KeepEdits", false)

}
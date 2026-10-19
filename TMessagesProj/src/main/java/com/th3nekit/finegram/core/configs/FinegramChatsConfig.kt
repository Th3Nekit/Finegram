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
import kotlinx.coroutines.launch
import org.telegram.messenger.ApplicationLoader
import com.th3nekit.finegram.preferences.boolean
import com.th3nekit.finegram.preferences.int

object FinegramChatsConfig: CoroutineScope by CoroutineScope(
    context = SupervisorJob() + Dispatchers.Default
) {

    private val sharedPreferences: SharedPreferences = ApplicationLoader.applicationContext.getSharedPreferences("mainconfig", Activity.MODE_PRIVATE)

    /** Appearance start */
    /** Не показывать подсказки про отложенную отправку и беззвучный режим. */
    /** Формулы между знаками доллара показываются набранными. */
    var renderFormulas by sharedPreferences.boolean("FG_RenderFormulas", false)

    var disableSendHints by sharedPreferences.boolean("FG_DisableSendHints", false)

    /**
     * Убирать спонсорские сообщения из каналов и ботов.
     *
     * Прячем не показ, а запрос: без него Telegram не узнает, что рекламу могли бы
     * показать, и не засчитает её. Промо-чат в списке и видеореклама идут другим путём,
     * их это не касается.
     */
    var hideSponsoredMessages by sharedPreferences.boolean("FG_HideSponsoredMessages", false)

    /** Прятать статус собеседника («был(а) в сети») в шапке чата. */
    var hideActionBarStatus by sharedPreferences.boolean("FG_HideActionBarStatus", false)

    /** Убирать кнопку звука из шапки каналов. */
    var hideMuteButton by sharedPreferences.boolean("FG_HideMuteButton", false)

    /** День недели рядом с датой-разделителем в переписке. */
    var weekdayNearDate by sharedPreferences.boolean("FG_WeekdayNearDate", false)

    var glareEffects by sharedPreferences.boolean("CP_GlareEffects", true)
    var centerChatTitle by sharedPreferences.boolean("AP_CenterChatTitle_v2", true)
    var centerChatTitle_AdaptiveWidth by sharedPreferences.boolean("AP_CenterChatTitle_AdaptiveWidth_v2", true)
    var unreadBadgeOnBackButton by sharedPreferences.boolean("CP_UnreadBadgeOnBackButton", centerChatTitle)
    var unreadBadgeOnBackButton_iOS by sharedPreferences.boolean("CP_UnreadBadgeOnBackButton_iOS", centerChatTitle)

    /** Chat shortcuts start */
    var shortcut_JumpToBegin by sharedPreferences.boolean("CP_Shortcut_JumpToBegin", true)
    var shortcut_DeleteAll by sharedPreferences.boolean("CP_Shortcut_DeleteAll", true)
    var shortcut_SavedMessages by sharedPreferences.boolean("CP_Shortcut_SavedMessages", false)
    var shortcut_Browser by sharedPreferences.boolean("CP_Shortcut_Browser", false)
    /** Chat shortcuts finish */

    /** Admin shortcuts start */
    var admins_Reactions by sharedPreferences.boolean("CP_Admins_Reactions", false)
    var admins_Permissions by sharedPreferences.boolean("CP_Admins_Permissions", false)
    var admins_Administrators by sharedPreferences.boolean("CP_Admins_Administrators", false)
    var admins_Members by sharedPreferences.boolean("CP_Admins_Members", false)
    var admins_Statistics by sharedPreferences.boolean("CP_Admins_Statistics", false)
    var admins_RecentActions by sharedPreferences.boolean("CP_Admins_RecentActions", false)
    /** Admin shortcuts finish */

    var customWallpapers by sharedPreferences.boolean("CP_CustomWallpapers", true)
    var drawSnowInChat by sharedPreferences.boolean("AP_DrawSnowInChat", false)
    var discussInsteadOfMute by sharedPreferences.boolean("CP_DiscussInsteadOfMute", true)
    var iOSMessageInputField by sharedPreferences.boolean("CP_iOSMessageInputField", true)
    var hideSendAsChannel by sharedPreferences.boolean("CP_HideSendAsChannel", false)
    var hideMuteUnmuteButton by sharedPreferences.boolean("CP_HideMuteUnmuteButton", false)
    var slider_RecentEmojisAmplifier by sharedPreferences.int("CP_Slider_RecentEmojisAmplifier", 45)
    var slider_RecentStickersAmplifier by sharedPreferences.int("CP_Slider_RecentStickersAmplifier", 20)
    /** Appearance finish */

    var customChatForSavedMessages by sharedPreferences.boolean("CP_CustomChatForSavedMessages", false)
    var hideKeyboardOnScrollIntensity by sharedPreferences.int("CP_HideKeyboardOnScrollIntensity", 5)

    /** Actions start */
    var autoQuoteReplies by sharedPreferences.boolean("CP_AutoQuoteReplies", false)
    var disableSwipeToNext by sharedPreferences.boolean("CP_DisableSwipeToNext", false)
    var disableVibration by sharedPreferences.boolean("CP_DisableVibration", false)
    var openLinksInIV by sharedPreferences.boolean("CP_OpenLinksInIV", false)
    /** Actions finish */

    /** Media start */
    /**
     * Писать голос со всех микрофонов, какие даёт устройство.
     *
     * Второй микрофон слышит иначе, и из двух дорожек выбирается та, где голос
     * громче, — шум с дальней стороны при этом отсекается.
     */
    /** Показывать в меню чата поиск по номеру сообщения. */
    var showIdSearch by sharedPreferences.boolean("FG_ShowIdSearch", true)

    /**
     * Показывать точное время последнего появления вместо «20 минут назад».
     *
     * Сервер отдаёт именно момент, а не промежуток, так что обе записи одинаково точны —
     * разница только в том, что удобнее читать.
     */
    var lastSeenExactTime by sharedPreferences.boolean("FG_LastSeenExactTime", false)

    var recordMultiMic by sharedPreferences.boolean("CP_RecordMultiMic", true)

    /** Оставлять обе дорожки как есть, а не сводить их в одну. */
    var recordInStereo by sharedPreferences.boolean("CP_RecordInStereo", false)

    var voiceMessagesAutoPlay by sharedPreferences.boolean("CP_VoiceMessagesAutoPlay", true)
    var playVideoOnVolume by sharedPreferences.boolean("CP_PlayVideo", false)
    var autoPauseVideo by sharedPreferences.boolean("CP_AutoPauseVideo", false)
    var videoSeekDuration by sharedPreferences.int("CP_VideoSeekDuration", 10)
    /** Media finish */

    /** Notifications start */
    const val NOTIF_SOUND_DISABLE = 0
    const val NOTIF_SOUND_DEFAULT = 1
    const val NOTIF_SOUND_IOS = 2
    var notificationSound by sharedPreferences.int("CP_Notification_Sound", NOTIF_SOUND_IOS)

    const val VIBRATION_DISABLE = 0
    const val VIBRATION_CLICK = 1
    const val VIBRATION_WAVE_FORM = 2
    const val VIBRATION_KEYBOARD_TAP = 3
    const val VIBRATION_LONG = 4
    var vibrateInChats by sharedPreferences.int("CP_VibrationInChats", VIBRATION_DISABLE)
    /** Notifications finish */

    /** Misc start */
    /** Direct share start */
    var forwardAuthorship by sharedPreferences.boolean("FG_ForwardAuthorship", true)
    var forwardCaptions by sharedPreferences.boolean("FG_ForwardCaptions", true)
    var forwardNotify by sharedPreferences.boolean("FG_ForwardNotify", true)
    /** Direct share finish */

    /** Bottom buttons forward start */
    var noAuthorship by sharedPreferences.boolean("FG_NoAuthorship", false)
    var noCaptions by sharedPreferences.boolean("FG_NoCaptions", false)
    /** Bottom buttons forward finish */

    /** Search Filter start */
    const val FILTER_NONE = 0
    const val FILTER_PHOTOS = 1
    const val FILTER_VIDEOS = 2
    const val FILTER_VOICE_MESSAGES = 3
    const val FILTER_VIDEO_MESSAGES = 4
    const val FILTER_FILES = 5
    const val FILTER_MUSIC = 6
    const val FILTER_GIFS = 7
    const val FILTER_GEO = 8
    const val FILTER_CONTACTS = 9
    const val FILTER_MENTIONS = 10
    var messagesSearchFilter by sharedPreferences.int("messagesSearchFilter", FILTER_NONE)
    /** Search Filter finish */

    var unarchiveOnSwipe by sharedPreferences.boolean("FG_UnarchiveOnSwipe", false)
    var sortByUnread by sharedPreferences.boolean("FG_SortByUnread", false)
    /** Misc finish */

}
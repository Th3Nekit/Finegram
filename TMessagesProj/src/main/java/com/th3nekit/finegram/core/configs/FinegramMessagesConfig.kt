/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core.configs

import org.telegram.messenger.SharedConfig
import android.app.Activity
import android.content.SharedPreferences
import android.os.Build
import org.telegram.messenger.ApplicationLoader
import org.telegram.messenger.LocaleController
import org.telegram.messenger.UserConfig
import com.th3nekit.finegram.chats.gemini.GeminiButtonsLayout
import com.th3nekit.finegram.preferences.boolean
import com.th3nekit.finegram.preferences.int
import com.th3nekit.finegram.preferences.string

object FinegramMessagesConfig {

    private val sharedPreferences: SharedPreferences = ApplicationLoader.applicationContext.getSharedPreferences("mainconfig", Activity.MODE_PRIVATE)

    /** Gemini AI start */
    var geminiApiKey by sharedPreferences.string("CP_GeminiApiKey", " ")
    var geminiModelName by sharedPreferences.string("CP_GeminiModelName", " ")
    var geminiSystemPrompt by sharedPreferences.string("CP_GeminiSystemPrompt", " ")
    var geminiTemperatureValue by sharedPreferences.int("CP_GeminiTemperature", 5)

    const val TRANSCRIPTION_PROVIDER_TELEGRAM = 0
    const val TRANSCRIPTION_PROVIDER_GEMINI = 1
    var voiceTranscriptionProvider by sharedPreferences.int("CP_VoiceTranscriptionProvider", TRANSCRIPTION_PROVIDER_TELEGRAM)
    /** Gemini AI finish */

    /** Appearance start */
    /** Message menu start */
    var hideBubbleTail by sharedPreferences.boolean("FG_HideBubbleTail", false)

    var blurMessageMenuBackground by sharedPreferences.boolean("CP_BlurMessageMenuBackground", false)
    var msgMenuUnifiedScroll by sharedPreferences.boolean("CP_MsgMenuUnifiedScrollForce", true)
    var msgMenuFixedHeight by sharedPreferences.boolean("CP_MsgMenuFixedHeightForce", true)
    var msgMenuNativeBlur by sharedPreferences.boolean("CP_MsgMenuNativeBlur", Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
    // Ключ у Cherrygram совпадал с ключом нативного размытия: один переключатель менял две
    // настройки разом. Свой ключ, значение по умолчанию прежнее.
    /** Message menu items start */
    var showSaveForNotifications by sharedPreferences.boolean("CP_ShowSaveForNotifications", true)
    var showGemini by sharedPreferences.boolean("CP_ShowGemini", GeminiButtonsLayout.geminiButtonsVisible())
    var showReply by sharedPreferences.boolean("CP_ShowReply", true)
    var showSaveToGallery by sharedPreferences.boolean("CP_ShowSaveToGallery", true)
    var showCopyPhoto by sharedPreferences.boolean("CP_ShowCopyPhoto", true)
    var showCopyPhotoAsSticker by sharedPreferences.boolean("CP_ShowCopyPhotoAsSticker", true)
    var showSaveToDownloads by sharedPreferences.boolean("CP_ShowSaveToDownloads", true)
    var showShare by sharedPreferences.boolean("CP_ShowShare", true)
    var showClearFromCache by sharedPreferences.boolean("CP_ShowClearFromCache", true)
    var showForward by sharedPreferences.boolean("CP_ShowForward", false)
    var showForwardWoAuthorship by sharedPreferences.boolean("CP_ShowForward_WO_Authorship", false)
    var showViewHistory by sharedPreferences.boolean("CP_ShowViewHistory", true)
    var showSaveMessage by sharedPreferences.boolean("CP_ShowSaveMessage", false)
    var showReport by sharedPreferences.boolean("CP_ShowReport", true)
    var openInExternalApp by sharedPreferences.boolean("CP_OpenInExternalApp", true)

    var showJSON by sharedPreferences.boolean("CP_ShowJSON", false)
    var jacksonJSON_Provider by sharedPreferences.boolean("CP_JacksonJSON_Provider", Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)

    var msgMenuItemsCompactView by sharedPreferences.boolean("CP_MsgMenuItemsCompactView", false)

    /**
     * Компактный вид меню для отдельного чата.
     *
     * общий переключатель задаёт привычку, но в отдельной переписке
     * бывает нужно наоборот. Храним два списка — где включено вопреки общему правилу и
     * где выключено.
     */
    @JvmStatic
    fun isCompactForChat(dialogId: Long): Boolean {
        if (compactOverride("On").contains(dialogId)) return true
        if (compactOverride("Off").contains(dialogId)) return false
        return msgMenuItemsCompactView
    }

    /** Перебирает три состояния чата: как у всех → включено → выключено. */
    @JvmStatic
    @Synchronized
    fun cycleCompactForChat(dialogId: Long): Boolean {
        val on = compactOverride("On").toMutableSet()
        val off = compactOverride("Off").toMutableSet()
        when {
            on.contains(dialogId) -> {
                on.remove(dialogId)
                off.add(dialogId)
            }
            off.contains(dialogId) -> {
                off.remove(dialogId)
            }
            else -> {
                on.add(dialogId)
            }
        }
        sharedPreferences.edit()
            .putString("FG_ChatCompactOverrideOn", on.joinToString(","))
            .putString("FG_ChatCompactOverrideOff", off.joinToString(","))
            .apply()
        return isCompactForChat(dialogId)
    }

    private fun compactOverride(suffix: String): Set<Long> {
        val raw = sharedPreferences.getString("FG_ChatCompactOverride$suffix", "") ?: ""
        if (raw.isEmpty()) return emptySet()
        return raw.split(",").mapNotNull { it.trim().toLongOrNull() }.toSet()
    }
    /** Message menu items finish */
    /** Message menu finish */

    /** Messages size start */
    var largerVoiceMessagesLayout by sharedPreferences.boolean("CP_LargerVoiceMessagesLayout", true)
    // Раньше сторона 2560 включалась молча, по классу устройства: на середнячке фото уходили
    // тяжёлыми, и человек не мог это отменить. Класс устройства остался значением по умолчанию.
    var largePhotos by sharedPreferences.boolean(
        "FG_LargePhotos",
        SharedConfig.getDevicePerformanceClass() >= SharedConfig.PERFORMANCE_CLASS_AVERAGE
    )
    var slider_mediaAmplifier by sharedPreferences.int("CP_Slider_MediaAmplifier", 100)
    var slider_stickerAmplifier by sharedPreferences.int("CP_Slider_StickerAmplifier", 100)
    var slider_gifsAmplifier by sharedPreferences.int("CP_Slider_GifsAmplifier", 100)
    /** Messages size finish */

    /** Direct share start */
    var shareDrawStoryButton by sharedPreferences.boolean("CP_ShareDrawStoryButton", true)
    var usersDrawShareButton by sharedPreferences.boolean("CP_UsersDrawShareButton", false)
    var supergroupsDrawShareButton by sharedPreferences.boolean("CP_SupergroupsDrawShareButton", false)
    var channelsDrawShareButton by sharedPreferences.boolean("CP_ChannelsDrawShareButton", true)
    var botsDrawShareButton by sharedPreferences.boolean("CP_BotsDrawShareButton", true)
    var stickersDrawShareButton by sharedPreferences.boolean("CP_StickersDrawShareButton", false)
    /** Direct share finish */

    var wideMessagesLayout by sharedPreferences.boolean("CP_WideMessagesLayout", false)
    var hideStickerTime by sharedPreferences.boolean("CP_TimeOnStick", false)
    var msgForwardDate by sharedPreferences.boolean("CP_ForwardMsgDate", true)
    var showPencilIcon by sharedPreferences.boolean("AP_PencilIcon", true)
    /** Appearance finish */

    /** Actions start */
    /** Messages filter start */
    var enableMsgFilters by sharedPreferences.boolean("CP_EnableMsgFilter", false)
    var msgFiltersElements by sharedPreferences.string("CP_MsgFiltersElements", "")
    var msgFiltersDetectTranslit by sharedPreferences.boolean("CP_MsgFiltersDetectTranslit", false)
    var msgFiltersMatchExactWord by sharedPreferences.boolean("CP_MsgFiltersMatchExactWord", false)
    var msgFiltersDetectEntities by sharedPreferences.boolean("CP_MsgFiltersDetectEntities", false)
    var msgFiltersHideFromBlocked by sharedPreferences.boolean("CP_MsgFiltersHideFromBlocked1", false)
    var msgFiltersHideAllUnderSpoiler by sharedPreferences.boolean("CP_MsgFiltersHideAll", false)
    var msgFiltersCollapseAutomatically by sharedPreferences.boolean("CP_MsgFiltersCollapseAutomatically", false)

    /**
     * Регулярные выражения в фильтре сообщений.
     *
     * одними словами не всегда получается описать то, что хочется
     * спрятать. Выражения пишутся по одному в строке.
     */
    var msgFiltersUseRegex by sharedPreferences.boolean("FG_MsgFiltersUseRegex", false)
    var msgFiltersRegexPatterns by sharedPreferences.string("FG_MsgFiltersRegexPatterns", "")

    /** Совпало и слово, и выражение (И) либо достаточно одного из них (ИЛИ). */
    var msgFiltersLogic by sharedPreferences.int("FG_MsgFiltersLogic", MSG_FILTERS_LOGIC_OR)

    /**
     * Списки чатов: где фильтр работает только в них, и где не работает никогда.
     *
     * Хранятся у каждого аккаунта свои: чужие чаты в чужом списке
     * ничего не значат.
     */
    @JvmStatic
    fun msgFiltersChatList(key: String, account: Int): String =
        sharedPreferences.getString(chatListKey(key, account), "") ?: ""

    @JvmStatic
    fun setMsgFiltersChatList(key: String, account: Int, value: String?) {
        sharedPreferences.edit().putString(chatListKey(key, account), value ?: "").apply()
    }

    private fun chatListKey(key: String, account: Int): String {
        val owner = try {
            UserConfig.getInstance(account).clientUserId
        } catch (e: Throwable) {
            0L
        }
        return if (owner == 0L) "FG_$key" else "FG_${key}_$owner"
    }

    const val MSG_FILTERS_LOGIC_OR = 0
    const val MSG_FILTERS_LOGIC_AND = 1
    var msgFilterTransparentMsg by sharedPreferences.boolean("CP_MsgFilterTransparentMsg", false)
    /** Messages filter finish */

    const val LEFT_BUTTON_FORWARD_WO_AUTHORSHIP = 0
    const val LEFT_BUTTON_REPLY = 1
    const val LEFT_BUTTON_SAVE_MESSAGE= 2
    const val LEFT_BUTTON_DIRECT_SHARE = 3
    const val LEFT_BUTTON_FORWARD_WO_CAPTION = 4
    var leftBottomButton by sharedPreferences.int("CP_LeftBottomButtonAction", LEFT_BUTTON_FORWARD_WO_AUTHORSHIP)

    const val DOUBLE_TAP_ACTION_NONE = 0
    const val DOUBLE_TAP_ACTION_REACTION = 1
    const val DOUBLE_TAP_ACTION_REPLY = 2
    const val DOUBLE_TAP_ACTION_SAVE = 3
    const val DOUBLE_TAP_ACTION_EDIT = 4
    const val DOUBLE_TAP_ACTION_TRANSLATE = 5
    const val DOUBLE_TAP_ACTION_TRANSLATE_GEMINI = 6
    var doubleTapAction by sharedPreferences.int("CP_DoubleTapAction", DOUBLE_TAP_ACTION_REACTION)

    const val MESSAGE_SLIDE_ACTION_REPLY = 0
    const val MESSAGE_SLIDE_ACTION_SAVE = 1
    const val MESSAGE_SLIDE_ACTION_TRANSLATE = 2
    const val MESSAGE_SLIDE_ACTION_DIRECT_SHARE = 3
    const val MESSAGE_SLIDE_ACTION_TRANSLATE_GEMINI = 4
    var messageSlideAction by sharedPreferences.int("CP_MessageSlideAction", MESSAGE_SLIDE_ACTION_REPLY)

    var deleteForAll by sharedPreferences.boolean("CP_DeleteForAll", false)
    var confirmVoiceBeforeSend by sharedPreferences.boolean("FG_ConfirmVoiceBeforeSend", false)
    var confirmRoundBeforeSend by sharedPreferences.boolean("FG_ConfirmRoundBeforeSend", false)
    /** Actions finish */

    /** Telegram Premium start */
    var disableReactionsOverlay by sharedPreferences.boolean("CP_DisableReactionsOverlay", false)
    var disableReactionAnim by sharedPreferences.boolean("CP_DisableReactionAnim", false)
    var disablePremStickAnim by sharedPreferences.boolean("CP_DisablePremStickAnim", false)
    var disablePremStickAutoPlay by sharedPreferences.boolean("CP_DisablePremStickAutoPlay", false)
    /** Telegram Premium finish */

    /** Misc start */
    var gifSpoilers by sharedPreferences.boolean("FG_GifSpoiler", false)
    var roundGalleryEditor by sharedPreferences.boolean("FG_RoundGalleryEditor", true)
    var photoAsSticker by sharedPreferences.boolean("FG_PhotoAsSticker", false)
    var motionPhotosEnabled by sharedPreferences.boolean("FG_MotionPhotosEnabled", false)

    /** Translator start */
    var translationKeyboardTarget by sharedPreferences.string("translationKeyboardTarget", "app")
    var translationTarget by sharedPreferences.string("translationTarget", "app")
    var translationTargetGemini by sharedPreferences.string("translationTargetGemini", LocaleController.getInstance().currentLocale.language)
    /** Translator finish */
    /** Misc finish */


    /** Пункт «Создать цитату» в меню сообщения: рисует картинку из выбранного. */
    var createQuoteImage by sharedPreferences.boolean("FG_CreateQuoteImage", true)

    /**
     * Пересылка без подписи к медиа.
     *
     * обычная пересылка тянет подпись автора вместе с фото, а иногда
     * нужно отправить только само медиа.
     */
    var forwardWithoutCaption by sharedPreferences.boolean("FG_ForwardWoCaption", false)

    /**
     * «Забрать эти реакции» — достать из сообщения набор нестандартных эмодзи.
     *
     */
    var getCustomReactions by sharedPreferences.boolean("FG_GetCustomReactions", false)

    /**
     * Скачать стикер из сообщения.
     *
     */

    /**
     * Отклик на долгое нажатие по сообщению.
     *
     * короткая вибрация в момент, когда меню вот-вот появится.
     */
    var messageMenuHaptic by sharedPreferences.boolean("FG_MessageMenuHaptic", true)

    /**
     * Свой порядок пунктов меню сообщения.
     *
     * Храним списком номеров пунктов; пустой список значит «как у
     * Telegram» — тогда меню собирается без нашего вмешательства.
     */
    @JvmStatic
    fun messageMenuOrder(): List<Int> {
        val raw = sharedPreferences.getString("FG_MessageMenuOrder", "") ?: ""
        if (raw.isEmpty()) return emptyList()
        return raw.split(",").mapNotNull { it.trim().toIntOrNull() }
    }

    @JvmStatic
    fun setMessageMenuOrder(order: List<Int>?) {
        if (order.isNullOrEmpty()) {
            resetMessageMenuOrder()
            return
        }
        sharedPreferences.edit()
            .putString("FG_MessageMenuOrder", order.joinToString(","))
            .apply()
    }

    @JvmStatic
    fun resetMessageMenuOrder() {
        sharedPreferences.edit().remove("FG_MessageMenuOrder").apply()
    }

    /**
     * Премиум-эмодзи без Telegram Premium.
     *
     * эмодзи уходит обычной ссылкой `tg://emoji?id=`, а при показе
     * снова разбирается в эмодзи. У кого такого же клиента нет, увидит ссылку — зато
     * отправить получается у всех.
     */
    var localPremiumEmojis by sharedPreferences.boolean("FG_LocalPremiumEmojis", false)
}

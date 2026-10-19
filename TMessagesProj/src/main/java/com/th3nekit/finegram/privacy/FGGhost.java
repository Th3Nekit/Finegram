/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Приём — разбирать исходящие запросы в одной точке — взят из re:extera
 * (GPL-3.0, Copyright the re:extera authors, https://github.com/fossSquad/re-extera).
 * Там он сделан перехватом снаружи; здесь мы у себя дома и правим место напрямую.
 */

package com.th3nekit.finegram.privacy;

import org.telegram.messenger.Utilities;
import org.telegram.tgnet.ConnectionsManager;
import org.telegram.tgnet.TLObject;
import org.telegram.tgnet.TLRPC;
import org.telegram.tgnet.tl.TL_account;
import org.telegram.tgnet.tl.TL_stories;

import com.th3nekit.finegram.core.configs.FinegramPrivacyConfig;

import java.util.Collections;
import java.util.Set;
import java.util.WeakHashMap;

/**
 * Режим призрака: клиент перестаёт рассказывать о себе.
 *
 * Telegram сообщает о вас не отдельной «телеметрией», а обычными запросами: «я в сети», «я
 * печатаю», «я прочитал до такого-то сообщения», «я посмотрел историю». Каждый из них
 * уходит через {@link ConnectionsManager}, поэтому разбирать их проще всего в одном месте —
 * там, где запрос уже собран и ещё не отправлен.
 *
 * Отсюда и устройство: не запрет на нажатия в интерфейсе, а решение о каждом исходящем
 * запросе. Интерфейс при этом продолжает работать как обычно — человек читает переписку,
 * а собеседник об этом не узнаёт.
 *
 * Важно, чего мы не делаем: не подменяем ответы сервера и не притворяемся, будто чужие
 * сообщения прочитаны. Мы только не отправляем своё.
 */
public final class FGGhost {

    private static final Set<TLObject> attentionReads = Collections.synchronizedSet(
            Collections.newSetFromMap(new WeakHashMap<>()));

    private FGGhost() {
    }

    /** Ничего не отправлять, будто запроса и не было. */
    public static final int PASS = 0;
    public static final int DROP = 1;

    private static boolean on() {
        return FinegramPrivacyConfig.INSTANCE.getGhostMode();
    }

    /**
     * Решение по исходящему запросу.
     *
     * Запросы про чтение и набор текста гасим целиком: ответ на них клиенту не нужен, он
     * ничего с ним не делает. С историями иначе — там ответ ждут, и молчание подвесило бы
     * показ; поэтому историю обслуживает {@link #fakeStoryAnswer}.
     */
    public static int decide(TLObject request) {
        if (attentionReads.remove(request)) return PASS;
        if (request == null || !on()) return PASS;

        // Для кого сделано исключение, всё уходит как обычно.
        if (!FGGhostExceptions.empty() && FGGhostExceptions.excluded(peerOf(request))) {
            return PASS;
        }

        if (FinegramPrivacyConfig.INSTANCE.getGhostHideReading() && isReading(request)) {
            return DROP;
        }
        if (FinegramPrivacyConfig.INSTANCE.getGhostHideTyping() && isTyping(request)) {
            return DROP;
        }
        if (FinegramPrivacyConfig.INSTANCE.getGhostHideStoryViews() && isStoryView(request)) {
            return DROP;
        }
        return PASS;
    }

    public static void allowAttentionRead(TLObject request) {
        if (request instanceof TLRPC.TL_messages_readMessageContents
                || request instanceof TLRPC.TL_channels_readMessageContents) {
            attentionReads.add(request);
        }
    }

    /**
     * Правит запрос перед отправкой.
     *
     * Единственный случай — «я в сети»: запрос нужен серверу, иначе сессия считается
     * брошенной, но сказать в нём можно и «меня нет».
     */
    public static void amend(TLObject request) {
        if (request == null || !on()) return;
        if (FinegramPrivacyConfig.INSTANCE.getGhostHideOnline()
                && request instanceof TL_account.updateStatus) {
            ((TL_account.updateStatus) request).offline = true;
        }
    }

    /**
     * Ответ вместо похода на сервер, когда запрос погашен.
     *
     * Нужен там, где клиент ждёт ответа и без него застрянет. Возвращаем «да», как если бы
     * сервер согласился: для просмотра истории это ровно то, что он и ответил бы.
     */
    public static TLObject fakeAnswer(TLObject request) {
        if (isStoryView(request)) {
            return new TLRPC.TL_boolTrue();
        }
        return null;
    }

    /**
     * Уйти в офлайн сразу после отправки сообщения.
     *
     * Telegram отмечает отправителя как «в сети» на несколько минут вперёд, и по этому
     * следу видно, что человек только что писал. Своим «меня нет» сразу вслед за
     * сообщением след обрывается.
     */
    public static void afterSend(int account, TLObject request) {
        if (request == null || !on()) return;
        if (!FinegramPrivacyConfig.INSTANCE.getGhostOfflineAfterSend()) return;
        if (!isSending(request)) return;
        Utilities.globalQueue.postRunnable(() -> {
            try {
                final TL_account.updateStatus offline = new TL_account.updateStatus();
                offline.offline = true;
                ConnectionsManager.getInstance(account).sendRequest(offline, (response, error) -> {});
            } catch (Throwable ignored) {
            }
        });
    }

    // --- что есть что ---

    /**
     * Кому адресован запрос. Ноль означает «непонятно кому» — тогда решение принимается
     * как обычно, без исключений: молчать наугад хуже, чем сработать по общему правилу.
     */
    private static long peerOf(TLObject request) {
        try {
            if (request instanceof TLRPC.TL_messages_readHistory) {
                return peerId(((TLRPC.TL_messages_readHistory) request).peer);
            }
            if (request instanceof TLRPC.TL_messages_setTyping) {
                return peerId(((TLRPC.TL_messages_setTyping) request).peer);
            }
            if (request instanceof TLRPC.TL_messages_readMentions) {
                return peerId(((TLRPC.TL_messages_readMentions) request).peer);
            }
            if (request instanceof TLRPC.TL_messages_readReactions) {
                return peerId(((TLRPC.TL_messages_readReactions) request).peer);
            }
            if (request instanceof TLRPC.TL_channels_readHistory) {
                final TLRPC.TL_channels_readHistory typed = (TLRPC.TL_channels_readHistory) request;
                return typed.channel == null ? 0 : -typed.channel.channel_id;
            }
        } catch (Throwable ignored) {
        }
        return 0;
    }

    private static long peerId(TLRPC.InputPeer peer) {
        if (peer == null) {
            return 0;
        }
        if (peer.user_id != 0) {
            return peer.user_id;
        }
        if (peer.chat_id != 0) {
            return -peer.chat_id;
        }
        if (peer.channel_id != 0) {
            return -peer.channel_id;
        }
        return 0;
    }

    private static boolean isReading(TLObject request) {
        return request instanceof TLRPC.TL_messages_readHistory
                || request instanceof TLRPC.TL_channels_readHistory
                || request instanceof TLRPC.TL_messages_readDiscussion
                || request instanceof TLRPC.TL_messages_readEncryptedHistory
                || request instanceof TLRPC.TL_channels_readMessageContents
                || request instanceof TLRPC.TL_messages_readMessageContents;
    }

    private static boolean isTyping(TLObject request) {
        return request instanceof TLRPC.TL_messages_setTyping;
    }

    private static boolean isStoryView(TLObject request) {
        return request instanceof TL_stories.TL_stories_readStories
                || request instanceof TL_stories.TL_stories_incrementStoryViews;
    }

    /**
     * Отправка сообщения в любом виде.
     *
     * Список нарочно широкий: след «был в сети» оставляет любая отправка, а не только
     * текст.
     */
    private static boolean isSending(TLObject request) {
        return request instanceof TLRPC.TL_messages_sendMessage
                || request instanceof TLRPC.TL_messages_sendMedia
                || request instanceof TLRPC.TL_messages_sendMultiMedia
                || request instanceof TLRPC.TL_messages_forwardMessages
                || request instanceof TLRPC.TL_messages_sendInlineBotResult
                || request instanceof TLRPC.TL_messages_editMessage
                || request instanceof TLRPC.TL_messages_sendReaction;
    }
}

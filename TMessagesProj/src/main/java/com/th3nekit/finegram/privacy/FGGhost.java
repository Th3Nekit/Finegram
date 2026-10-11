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

public final class FGGhost {

    private static final Set<TLObject> attentionReads = Collections.synchronizedSet(
            Collections.newSetFromMap(new WeakHashMap<>()));

    private FGGhost() {
    }

    public static final int PASS = 0;
    public static final int DROP = 1;

    private static boolean on() {
        return FinegramPrivacyConfig.INSTANCE.getGhostMode();
    }

    public static int decide(TLObject request) {
        if (attentionReads.remove(request)) return PASS;
        if (request == null || !on()) return PASS;

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

    public static void amend(TLObject request) {
        if (request == null || !on()) return;
        if (FinegramPrivacyConfig.INSTANCE.getGhostHideOnline()
                && request instanceof TL_account.updateStatus) {
            ((TL_account.updateStatus) request).offline = true;
        }
    }

    public static TLObject fakeAnswer(TLObject request) {
        if (isStoryView(request)) {
            return new TLRPC.TL_boolTrue();
        }
        return null;
    }

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

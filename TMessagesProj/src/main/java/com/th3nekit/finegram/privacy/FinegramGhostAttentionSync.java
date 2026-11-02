package com.th3nekit.finegram.privacy;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.MessagesController;
import org.telegram.tgnet.ConnectionsManager;
import org.telegram.tgnet.TLObject;
import org.telegram.tgnet.TLRPC;

import java.util.HashSet;
import java.util.Set;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

final class FinegramGhostAttentionSync {
    private final int account;
    private final long dialogId;
    private final long topicId;
    private final boolean reactions;
    private final BooleanSupplier valid;
    private final Consumer<TLRPC.Message> onMessage;
    private final Consumer<Set<Integer>> onComplete;
    private final Set<Integer> ids = new HashSet<>();
    private boolean finished;
    private int requestId;
    private final Runnable timeout = () -> finish(null);

    private FinegramGhostAttentionSync(int account, long dialogId, long topicId, boolean reactions,
            BooleanSupplier valid, Consumer<TLRPC.Message> onMessage, Consumer<Set<Integer>> onComplete) {
        this.account = account;
        this.dialogId = dialogId;
        this.topicId = topicId;
        this.reactions = reactions;
        this.valid = valid;
        this.onMessage = onMessage;
        this.onComplete = onComplete;
    }

    static void load(int account, long dialogId, long topicId, boolean reactions,
            BooleanSupplier valid, Consumer<TLRPC.Message> onMessage, Consumer<Set<Integer>> onComplete) {
        FinegramGhostAttentionSync sync = new FinegramGhostAttentionSync(account, dialogId, topicId,
                reactions, valid, onMessage, onComplete);
        AndroidUtilities.runOnUIThread(sync.timeout, 15000);
        sync.page(0);
    }

    private void page(int offsetId) {
        if (finished || !valid.getAsBoolean()) {
            finish(null);
            return;
        }
        MessagesController controller = MessagesController.getInstance(account);
        TLObject request;
        if (reactions) {
            TLRPC.TL_messages_getUnreadReactions req = new TLRPC.TL_messages_getUnreadReactions();
            req.peer = controller.getInputPeer(dialogId);
            req.limit = 100;
            req.offset_id = offsetId;
            if (topicId != 0 && controller.isMonoForum(dialogId)) {
                req.saved_peer_id = controller.getInputPeer(topicId);
                req.flags |= 2;
            } else if (topicId != 0) {
                req.top_msg_id = (int) topicId;
                req.flags |= 1;
            }
            request = req;
        } else {
            if (topicId != 0 && controller.isMonoForum(dialogId)) {
                finish(null);
                return;
            }
            TLRPC.TL_messages_getUnreadMentions req = new TLRPC.TL_messages_getUnreadMentions();
            req.peer = controller.getInputPeer(dialogId);
            req.limit = 100;
            req.offset_id = offsetId;
            if (topicId != 0) {
                req.top_msg_id = (int) topicId;
                req.flags |= 1;
            }
            request = req;
        }
        requestId = ConnectionsManager.getInstance(account).sendRequest(request, (response, error) ->
                AndroidUtilities.runOnUIThread(() -> {
                    requestId = 0;
                    if (finished || !valid.getAsBoolean()) {
                        finish(null);
                        return;
                    }
                    if (error != null || !(response instanceof TLRPC.messages_Messages)) {
                        finish(null);
                        return;
                    }
                    TLRPC.messages_Messages result = (TLRPC.messages_Messages) response;
                    int nextOffset = offsetId;
                    int previousSize = ids.size();
                    for (TLRPC.Message message : result.messages) {
                        if (message.id <= 0) continue;
                        message.dialog_id = dialogId;
                        ids.add(message.id);
                        if (nextOffset == 0 || message.id < nextOffset) nextOffset = message.id;
                        onMessage.accept(message);
                    }
                    if (result.messages.size() >= 100) {
                        if (ids.size() == previousSize || nextOffset <= 0 || offsetId != 0 && nextOffset >= offsetId) {
                            finish(null);
                        } else {
                            page(nextOffset);
                        }
                    } else if (result.count > ids.size()) {
                        finish(null);
                    } else {
                        finish(ids);
                    }
                }));
    }

    private void finish(Set<Integer> result) {
        if (finished) return;
        finished = true;
        AndroidUtilities.cancelRunOnUIThread(timeout);
        if (requestId != 0) ConnectionsManager.getInstance(account).cancelRequest(requestId, true);
        onComplete.accept(result);
    }
}

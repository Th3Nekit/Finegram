package org.telegram.messenger;

import org.telegram.tgnet.TLObject;
import org.telegram.tgnet.TLRPC;

import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.function.LongSupplier;

final class ProtectedMessageCopyHelper {
    private ProtectedMessageCopyHelper() {
    }

    static boolean canCopy(MessageObject messageObject) {
        if (messageObject == null || messageObject.messageOwner == null) {
            return false;
        }
        TLRPC.Message source = messageObject.messageOwner;
        TLRPC.MessageMedia media = source.media;
        if (media == null || media instanceof TLRPC.TL_messageMediaEmpty || media instanceof TLRPC.TL_messageMediaWebPage) {
            return source.message != null && !source.message.isEmpty();
        }
        if (media.ttl_seconds != 0 || media.live_photo) {
            return false;
        }
        return media instanceof TLRPC.TL_messageMediaPhoto && media.photo instanceof TLRPC.TL_photo ||
                media instanceof TLRPC.TL_messageMediaDocument && media.document instanceof TLRPC.TL_document ||
                media instanceof TLRPC.TL_messageMediaContact && media.phone_number != null ||
                media instanceof TLRPC.TL_messageMediaGeo || media instanceof TLRPC.TL_messageMediaVenue;
    }

    static SendMessagesHelper.SendMessageParams create(MessageObject messageObject, long peer, boolean hideCaption,
            boolean notify, int scheduleDate, int scheduleRepeatPeriod, MessageObject replyToTopMsg, long payStars,
            long monoForumPeerId, MessageSuggestionParams suggestionParams, boolean canSendPreview) {
        if (!canCopy(messageObject) || !messageObject.canForwardMessage()) {
            return null;
        }
        TLRPC.Message source = messageObject.messageOwner;
        TLRPC.MessageMedia media = source.media;
        SendMessagesHelper.SendMessageParams params = new SendMessagesHelper.SendMessageParams();
        params.peer = peer;
        params.notify = notify;
        params.scheduleDate = scheduleDate;
        params.scheduleRepeatPeriod = scheduleRepeatPeriod;
        params.replyToMsg = replyToTopMsg;
        params.replyToTopMsg = replyToTopMsg;
        params.payStars = payStars;
        params.monoForumPeer = monoForumPeerId;
        params.suggestionParams = suggestionParams;
        params.invert_media = source.invert_media;
        params.searchLinks = canSendPreview;
        boolean text = media == null || media instanceof TLRPC.TL_messageMediaEmpty || media instanceof TLRPC.TL_messageMediaWebPage;
        if (text) {
            params.message = source.message;
            if (canSendPreview && media instanceof TLRPC.TL_messageMediaWebPage) {
                params.mediaWebPage = (TLRPC.TL_messageMediaWebPage) copyObject(media);
                if (params.mediaWebPage == null) {
                    return null;
                }
                params.webPage = params.mediaWebPage.webpage;
            }
        } else if (media instanceof TLRPC.TL_messageMediaPhoto) {
            params.photo = (TLRPC.TL_photo) copyObject(media.photo);
            if (params.photo == null) {
                return null;
            }
        } else if (media instanceof TLRPC.TL_messageMediaDocument) {
            params.document = (TLRPC.TL_document) copyObject(media.document);
            if (params.document == null) {
                return null;
            }
            params.path = source.attachPath;
        } else if (media instanceof TLRPC.TL_messageMediaContact) {
            params.user = new TLRPC.TL_userContact_old2();
            params.user.phone = media.phone_number;
            params.user.first_name = media.first_name;
            params.user.last_name = media.last_name;
            params.user.id = media.user_id;
            if (media.vcard != null && !media.vcard.isEmpty()) {
                TLRPC.TL_reastrictionReason reason = new TLRPC.TL_reastrictionReason();
                reason.platform = "";
                reason.reason = "";
                reason.text = media.vcard;
                params.user.restriction_reason.add(reason);
            }
        } else {
            params.location = copyObject(media);
            if (params.location == null) {
                return null;
            }
        }
        if (params.photo != null || params.document != null) {
            params.caption = hideCaption ? "" : source.message;
            params.hasMediaSpoilers = media.spoiler;
            params.parentObject = "sent_" + (source.peer_id == null ? 0 : source.peer_id.channel_id) + "_" + messageObject.getId();
        }
        if ((text || !hideCaption && (params.photo != null || params.document != null)) && source.entities != null && !source.entities.isEmpty()) {
            params.entities = new ArrayList<>();
            for (TLRPC.MessageEntity entity : source.entities) {
                TLRPC.MessageEntity copy = copyObject(entity);
                if (copy == null) {
                    return null;
                }
                params.entities.add(copy);
            }
        }
        return params;
    }

    @SuppressWarnings("unchecked")
    private static <T extends TLObject> T copyObject(T source) {
        try {
            return (T) copyValue(source, new IdentityHashMap<>());
        } catch (ReflectiveOperationException e) {
            throw new IllegalArgumentException("Cannot copy protected message content", e);
        }
    }

    private static Object copyValue(Object source, IdentityHashMap<Object, Object> copies) throws ReflectiveOperationException {
        if (source == null) {
            return null;
        }
        Object existing = copies.get(source);
        if (existing != null) {
            return existing;
        }
        if (source instanceof TLObject) {
            Object copy = source.getClass().getDeclaredConstructor().newInstance();
            copies.put(source, copy);
            for (Field field : source.getClass().getFields()) {
                if (!Modifier.isStatic(field.getModifiers())) {
                    field.set(copy, copyValue(field.get(source), copies));
                }
            }
            return copy;
        }
        if (source instanceof ArrayList) {
            ArrayList<?> list = (ArrayList<?>) source;
            ArrayList<Object> copy = new ArrayList<>(list.size());
            copies.put(source, copy);
            for (Object value : list) {
                copy.add(copyValue(value, copies));
            }
            return copy;
        }
        if (source.getClass().isArray()) {
            int length = Array.getLength(source);
            Object copy = Array.newInstance(source.getClass().getComponentType(), length);
            copies.put(source, copy);
            if (source.getClass().getComponentType().isPrimitive()) {
                System.arraycopy(source, 0, copy, 0, length);
            } else {
                for (int i = 0; i < length; i++) {
                    Array.set(copy, i, copyValue(Array.get(source, i), copies));
                }
            }
            return copy;
        }
        return source;
    }

    static void assignGroups(ArrayList<MessageObject> sources, ArrayList<SendMessagesHelper.SendMessageParams> copies, LongSupplier nextGroupId) {
        for (int start = 0; start < sources.size();) {
            MessageObject source = sources.get(start);
            long originalGroupId = source.getGroupId();
            int end = start + 1;
            if (originalGroupId != 0 && canGroup(source, copies.get(start))) {
                while (end < sources.size() && end - start < 10 && sources.get(end).getGroupId() == originalGroupId &&
                        sourceDialogId(sources.get(end)) == sourceDialogId(source) && canGroup(sources.get(end), copies.get(end))) {
                    end++;
                }
                if (end - start > 1) {
                    long groupId;
                    do {
                        groupId = nextGroupId.getAsLong();
                    } while (groupId == 0);
                    for (int i = start; i < end; i++) {
                        SendMessagesHelper.SendMessageParams params = copies.get(i);
                        params.params = new HashMap<>();
                        params.params.put("groupId", Long.toString(groupId));
                        if (i == end - 1) {
                            params.params.put("final", "1");
                        }
                    }
                }
            }
            start = end;
        }
    }

    private static boolean canGroup(MessageObject source, SendMessagesHelper.SendMessageParams params) {
        return (params.photo != null || params.document != null) && !source.isVoice() && !source.isRoundVideo() &&
                !source.isSticker() && !source.isAnimatedSticker() && !source.isGif();
    }

    private static long sourceDialogId(MessageObject source) {
        return source.messageOwner.peer_id == null ? source.messageOwner.dialog_id : DialogObject.getPeerDialogId(source.messageOwner.peer_id);
    }
}

/*
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.privacy;

import android.content.SharedPreferences;

import com.th3nekit.finegram.core.configs.FinegramPrivacyConfig;

import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.DialogObject;
import org.telegram.messenger.FileLog;
import org.telegram.messenger.MessageObject;
import org.telegram.messenger.MessagesStorage;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.UserConfig;
import org.telegram.messenger.Utilities;
import org.telegram.tgnet.SerializedData;
import org.telegram.tgnet.TLRPC;

import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.WeakHashMap;

public final class FinegramGhostPrefs {
    private static final Map<Integer, FinegramGhostPrefs> instances = new HashMap<>();
    private final long owner;
    private final int account;
    private boolean closed;
    private final Map<String, AttentionSync> attentionSyncs = new HashMap<>();
    private final SharedPreferences prefs;
    private final Map<String, Object> values;
    private final Map<String, Set<String>> attentionKeys = new HashMap<>();
    private final Map<TLRPC.Message, Boolean> mentionSources = new WeakHashMap<>();
    private final Map<TLRPC.TL_messageReactions, Set<String>> reactionSources = new WeakHashMap<>();
    private final Set<TLRPC.Message> serverMessages = Collections.newSetFromMap(new WeakHashMap<>());
    private final Set<TLRPC.TL_messageReactions> serverReactions = Collections.newSetFromMap(new WeakHashMap<>());
    private final Set<Object> appliedCounters = Collections.newSetFromMap(new WeakHashMap<>());
    private final Map<String, Set<String>> visibleReactions = new LinkedHashMap<String, Set<String>>(128, 0.75f, true) {
        @Override
        protected boolean removeEldestEntry(Map.Entry<String, Set<String>> eldest) {
            return size() > 512;
        }
    };

    private FinegramGhostPrefs(int account, long userId) {
        this.account = account;
        owner = userId;
        prefs = ApplicationLoader.applicationContext.getSharedPreferences("fg_ghost_reads_" + account + "_" + userId, 0);
        values = new HashMap<>(prefs.getAll());
        for (Map.Entry<String, Object> entry : values.entrySet()) index(entry.getKey(), entry.getValue(), true);
    }

    public static synchronized FinegramGhostPrefs getInstance(int account) {
        long userId = UserConfig.getInstance(account).getClientUserId();
        FinegramGhostPrefs instance = instances.get(account);
        if (instance == null || instance.owner != userId) {
            instance = new FinegramGhostPrefs(account, userId);
            instances.put(account, instance);
        }
        return instance;
    }

    public static boolean isLocalRead(long dialogId) {
        return dialogId != 0 && !DialogObject.isEncryptedDialog(dialogId)
                && FinegramPrivacyConfig.INSTANCE.getGhostMode()
                && FinegramPrivacyConfig.INSTANCE.getGhostHideReading()
                && !FGGhostExceptions.excluded(dialogId);
    }

    public static synchronized void clear(int account) {
        FinegramGhostPrefs instance = getInstance(account);
        synchronized (instance) {
            instance.closed = true;
            instance.attentionSyncs.clear();
            if (!instance.values.isEmpty()) instance.prefs.edit().clear().apply();
            instance.values.clear();
            instance.attentionKeys.clear();
            instance.mentionSources.clear();
            instance.reactionSources.clear();
            instance.serverMessages.clear();
            instance.serverReactions.clear();
            instance.appliedCounters.clear();
            instance.visibleReactions.clear();
        }
        instances.remove(account);
    }

    private String key(long dialogId, int messageId) {
        return dialogId + ":" + messageId;
    }

    private boolean getBoolean(String name, boolean fallback) {
        Object value = values.get(name);
        return value instanceof Boolean ? (Boolean) value : fallback;
    }

    private long getLong(String name, long fallback) {
        Object value = values.get(name);
        return value instanceof Long ? (Long) value : fallback;
    }

    @SuppressWarnings("unchecked")
    private Set<String> getStringSet(String name, Set<String> fallback) {
        Object value = values.get(name);
        return value instanceof Set ? (Set<String>) value : fallback;
    }

    private void index(String name, Object value, boolean add) {
        if (!Boolean.TRUE.equals(value) || !(name.startsWith("pm:") || name.startsWith("pr:") || name.startsWith("um:") || name.startsWith("ur:"))) return;
        String messageKey = name.substring(3);
        String dialogKey = messageKey.substring(0, messageKey.lastIndexOf(':'));
        long topicId = getLong("t:" + messageKey, 0);
        indexBucket(dialogKey + ":0", name, add);
        if (topicId != 0) indexBucket(dialogKey + ":" + topicId, name, add);
    }

    private void indexBucket(String bucket, String name, boolean add) {
        if (add) {
            attentionKeys.computeIfAbsent(bucket, unused -> new HashSet<>()).add(name);
        } else {
            Set<String> keys = attentionKeys.get(bucket);
            if (keys != null && keys.remove(name) && keys.isEmpty()) attentionKeys.remove(bucket);
        }
    }

    private final class Changes {
        private final Map<String, Object> updates = new HashMap<>();

        Changes putBoolean(String name, boolean value) { updates.put(name, value); return this; }
        Changes putLong(String name, long value) { updates.put(name, value); return this; }
        Changes putStringSet(String name, Set<String> value) { updates.put(name, new HashSet<>(value)); return this; }
        Changes remove(String name) { updates.put(name, null); return this; }

        void apply() {
            SharedPreferences.Editor editor = null;
            for (Map.Entry<String, Object> entry : updates.entrySet()) {
                String name = entry.getKey();
                Object value = entry.getValue();
                if (Objects.equals(values.get(name), value)) continue;
                if (name.startsWith("t:")) {
                    String messageKey = name.substring(2);
                    for (String prefix : new String[] {"pm:", "pr:", "um:", "ur:"}) index(prefix + messageKey, values.get(prefix + messageKey), false);
                } else {
                    index(name, values.get(name), false);
                }
                if (value == null) values.remove(name);
                else values.put(name, value);
                if (name.startsWith("t:")) {
                    String messageKey = name.substring(2);
                    for (String prefix : new String[] {"pm:", "pr:", "um:", "ur:"}) index(prefix + messageKey, values.get(prefix + messageKey), true);
                } else {
                    index(name, value, true);
                }
                if (editor == null) editor = prefs.edit();
                if (value == null) editor.remove(name);
                else if (value instanceof Boolean) editor.putBoolean(name, (Boolean) value);
                else if (value instanceof Long) editor.putLong(name, (Long) value);
                else editor.putStringSet(name, getStringSet(name, Collections.emptySet()));
            }
            if (editor != null) editor.apply();
        }
    }

    public synchronized boolean readMention(long dialogId, long topicId, int messageId) {
        if (owner == 0 || messageId <= 0 || !isLocalRead(dialogId)) return false;
        String key = key(dialogId, messageId);
        boolean firstRead = !getBoolean("m:" + key, false);
        Changes editor = new Changes().putBoolean("m:" + key, true).remove("um:" + key);
        if (firstRead) editor.putBoolean("pm:" + key, true);
        if (topicId != 0) editor.putLong("t:" + key, topicId);
        editor.apply();
        return firstRead;
    }

    public synchronized boolean isMentionRead(long dialogId, int messageId) {
        return owner != 0 && getBoolean("m:" + key(dialogId, messageId), false);
    }

    public synchronized boolean readKnownReactions(long dialogId, long topicId, int messageId) {
        if (owner == 0 || messageId <= 0 || !isLocalRead(dialogId)) return false;
        String key = key(dialogId, messageId);
        Set<String> visible = visibleReactions.get(key);
        if (visible == null) return false;
        saveReactions(key, topicId, visible, !visible.isEmpty());
        visibleReactions.put(key, Collections.emptySet());
        return true;
    }

    public synchronized void readReactions(long dialogId, long topicId, int messageId, TLRPC.TL_messageReactions reactions) {
        if (owner == 0 || messageId <= 0 || !isLocalRead(dialogId) || reactions == null) return;
        Set<String> identities = new HashSet<>();
        Set<String> source = reactionSources.get(reactions);
        boolean unread = false;
        for (TLRPC.MessagePeerReaction reaction : reactions.recent_reactions) {
            String identity = reactionKey(reaction);
            if (identity != null) {
                identities.add(identity);
                unread |= reaction.unread || source != null && source.contains(identity);
            }
        }
        String key = key(dialogId, messageId);
        saveReactions(key, topicId, identities, unread);
        Set<String> visible = visibleReactions.get(key);
        if (visible != null) {
            visible = new HashSet<>(visible);
            visible.removeAll(identities);
            visibleReactions.put(key, visible);
        }
    }

    private void saveReactions(String key, long topicId, Set<String> identities, boolean unread) {
        Set<String> read = new HashSet<>(getStringSet("r:" + key, Collections.emptySet()));
        read.addAll(identities);
        Set<String> remaining = new HashSet<>(visibleReactions.getOrDefault(key, Collections.emptySet()));
        remaining.removeAll(read);
        Changes editor = new Changes();
        if (!read.isEmpty()) editor.putStringSet("r:" + key, read);
        if (!remaining.isEmpty()) {
            editor.putBoolean("pr:" + key, false).putBoolean("ur:" + key, true);
        } else {
            editor.remove("ur:" + key);
            if (unread) editor.putBoolean("pr:" + key, true);
        }
        if (topicId != 0 && (!read.isEmpty() || unread || !remaining.isEmpty())) editor.putLong("t:" + key, topicId);
        editor.apply();
    }

    private String reactionKey(TLRPC.MessagePeerReaction reaction) {
        if (reaction == null || reaction.peer_id == null || reaction.reaction == null) return null;
        SerializedData data = new SerializedData();
        try {
            reaction.peer_id.serializeToStream(data);
            data.writeInt32(reaction.date);
            reaction.reaction.serializeToStream(data);
            return Utilities.bytesToHex(data.toByteArray());
        } catch (Exception e) {
            FileLog.e(e);
            return null;
        } finally {
            data.cleanup();
        }
    }

    public synchronized void applyMessage(int account, TLRPC.Message message, boolean fromServer) {
        if (owner == 0 || message == null || message.id <= 0) return;
        long dialogId = MessageObject.getDialogId(message);
        if (dialogId == 0 || DialogObject.isEncryptedDialog(dialogId)) return;
        String key = key(dialogId, message.id);
        boolean hasLocalRead = getBoolean("m:" + key, false) || values.containsKey("r:" + key);
        if (!message.mentioned && message.reactions == null && !hasLocalRead) return;
        if (!hasLocalRead && !isLocalRead(dialogId) && !attentionKeys.containsKey(dialogId + ":0")) return;
        long topicId = MessageObject.getTopicId(account, message, MessagesStorage.getInstance(account).getForumTypeFlags(dialogId));
        Changes editor = new Changes();
        if (topicId != 0 && (hasLocalRead || message.mentioned && message.media_unread || MessageObject.hasUnreadReactions(message))) editor.putLong("t:" + key, topicId);
        Boolean source = mentionSources.get(message);
        if (source == null) {
            source = message.mentioned && message.media_unread;
            mentionSources.put(message, source);
        }
        boolean firstServer = fromServer && serverMessages.add(message);
        if (getBoolean("m:" + key, false)) {
            if (firstServer) editor.putBoolean("pm:" + key, source);
            if (message.mentioned) message.media_unread = false;
        }
        if (firstServer) {
            if (message.mentioned && message.media_unread) editor.putBoolean("um:" + key, true);
            else editor.remove("um:" + key);
        }
        editor.apply();
        applyReactions(dialogId, message.id, message.reactions, fromServer);
    }

    public synchronized void applyReactions(long dialogId, int messageId, TLRPC.TL_messageReactions reactions, boolean fromServer) {
        if (owner == 0 || dialogId == 0 || messageId <= 0 || DialogObject.isEncryptedDialog(dialogId)) return;
        String key = key(dialogId, messageId);
        if (!values.containsKey("r:" + key) && !isLocalRead(dialogId) && !attentionKeys.containsKey(dialogId + ":0")) return;
        Set<String> read = getStringSet("r:" + key, Collections.emptySet());
        if (reactions == null) {
            if (fromServer) new Changes().remove("pr:" + key).remove("ur:" + key).apply();
            visibleReactions.remove(key);
            return;
        }
        Set<String> source = reactionSources.get(reactions);
        if (source == null) source = new HashSet<>();
        Set<String> visible = new HashSet<>();
        boolean remainingUnread = false;
        for (TLRPC.MessagePeerReaction reaction : reactions.recent_reactions) {
            String identity = reactionKey(reaction);
            if (reaction.unread && identity != null) source.add(identity);
            if (read.contains(identity)) reaction.unread = false;
            if (reaction.unread && identity != null) visible.add(identity);
            remainingUnread |= reaction.unread;
        }
        reactionSources.put(reactions, source);
        visibleReactions.put(key, visible);
        if (fromServer && !reactions.min && serverReactions.add(reactions)) {
            Changes editor = new Changes().putBoolean("pr:" + key, !source.isEmpty() && !remainingUnread);
            if (remainingUnread) editor.putBoolean("ur:" + key, true);
            else editor.remove("ur:" + key);
            editor.apply();
        }
    }

    public synchronized int serverCount(long dialogId, long topicId, int count, boolean reactions) {
        if (owner == 0) return Math.max(0, count);
        if (count == 0) {
            attentionSyncs.remove(dialogId + ":" + topicId + (reactions ? ":r" : ":m"));
        }
        Set<String> keys = attentionKeys.get(dialogId + ":" + topicId);
        if (keys == null) return Math.max(0, count);
        int localReads = 0, visible = 0;
        String prefix = reactions ? "pr:" : "pm:";
        String unreadPrefix = reactions ? "ur:" : "um:";
        Changes editor = count == 0 ? new Changes() : null;
        for (String name : keys) {
            if (!name.startsWith(prefix) && !name.startsWith(unreadPrefix)) continue;
            if (editor != null) editor.remove(name);
            if (name.startsWith(prefix)) localReads++;
            else visible++;
        }
        if (editor != null) editor.apply();
        if (localReads > 0 && count > 0) scheduleAttentionSync(dialogId, topicId, reactions, keys);
        return Math.max(Math.max(0, count - localReads), Math.min(Math.max(0, count), visible));
    }

    private static final class AttentionSync {
        final Set<String> keys;
        boolean running = true;
        long finishedAt;

        AttentionSync(Set<String> keys) {
            this.keys = new HashSet<>(keys);
        }
    }

    private boolean isCurrentSync(String bucket, AttentionSync sync) {
        return !closed && owner != 0 && UserConfig.getInstance(account).getClientUserId() == owner
                && attentionSyncs.get(bucket) == sync && sync.running;
    }

    private void scheduleAttentionSync(long dialogId, long topicId, boolean reactions, Set<String> keys) {
        if (closed || !isLocalRead(dialogId)) return;
        String bucket = dialogId + ":" + topicId + (reactions ? ":r" : ":m");
        AttentionSync previous = attentionSyncs.get(bucket);
        long now = android.os.SystemClock.elapsedRealtime();
        if (previous != null && (previous.running || now - previous.finishedAt < 30000)) return;
        AttentionSync sync = new AttentionSync(keys);
        attentionSyncs.put(bucket, sync);
        AndroidUtilities.runOnUIThread(() -> FinegramGhostAttentionSync.load(account, dialogId, topicId, reactions,
                () -> {
                    synchronized (FinegramGhostPrefs.this) {
                        return isCurrentSync(bucket, sync) && isLocalRead(dialogId);
                    }
                }, message -> {
                    synchronized (FinegramGhostPrefs.this) {
                        if (isCurrentSync(bucket, sync)) applyMessage(account, message, true);
                    }
                }, ids -> finishAttentionSync(dialogId, topicId, reactions, bucket, sync, ids)));
    }

    private void finishAttentionSync(long dialogId, long topicId, boolean reactions,
            String bucket, AttentionSync sync, Set<Integer> ids) {
        int count = 0;
        synchronized (this) {
            if (!isCurrentSync(bucket, sync)) return;
            sync.running = false;
            sync.finishedAt = android.os.SystemClock.elapsedRealtime();
            if (ids == null || !isLocalRead(dialogId)) return;
            String prefix = reactions ? "pr:" : "pm:";
            String unreadPrefix = reactions ? "ur:" : "um:";
            Changes editor = new Changes();
            for (String name : sync.keys) {
                if (!name.startsWith(prefix) && !name.startsWith(unreadPrefix)) continue;
                int messageId = Integer.parseInt(name.substring(name.lastIndexOf(':') + 1));
                if (!ids.contains(messageId)) editor.remove(name);
            }
            editor.apply();
            for (int id : ids) {
                if (!getBoolean(prefix + key(dialogId, id), false)) count++;
            }
        }
        if (reactions) {
            MessagesController controller = MessagesController.getInstance(account);
            if (topicId == 0) {
                TLRPC.Dialog dialog = controller.dialogs_dict.get(dialogId);
                if (dialog != null) dialog.unread_reactions_count = count;
            } else {
                controller.getTopicsController().updateReactionsUnread(dialogId, topicId, count, false);
            }
            MessagesStorage.getInstance(account).updateUnreadReactionsCount(dialogId, topicId, count);
            NotificationCenter.getInstance(account).postNotificationName(
                    NotificationCenter.dialogsUnreadReactionsCounterChanged, dialogId, topicId, count, null);
        } else {
            MessagesStorage.getInstance(account).resetMentionsCount(dialogId, topicId, count);
        }
    }

    public synchronized void applyDialog(TLRPC.Dialog dialog) {
        applyDialog(dialog, false);
    }

    public synchronized void applyDialog(TLRPC.Dialog dialog, boolean fromServer) {
        if (dialog == null || dialog instanceof TLRPC.TL_dialogFolder || !fromServer && appliedCounters.contains(dialog)) return;
        long dialogId = dialog.id != 0 ? dialog.id : DialogObject.getPeerDialogId(dialog.peer);
        dialog.unread_mentions_count = serverCount(dialogId, 0, dialog.unread_mentions_count, false);
        dialog.unread_reactions_count = serverCount(dialogId, 0, dialog.unread_reactions_count, true);
        appliedCounters.add(dialog);
    }

    public synchronized void applyTopic(long dialogId, long topicId, TLRPC.TL_forumTopic topic) {
        applyTopic(dialogId, topicId, topic, false);
    }

    public synchronized void applyTopic(long dialogId, long topicId, TLRPC.TL_forumTopic topic, boolean fromServer) {
        if (topic == null || !fromServer && appliedCounters.contains(topic)) return;
        topic.unread_mentions_count = serverCount(dialogId, topicId, topic.unread_mentions_count, false);
        topic.unread_reactions_count = serverCount(dialogId, topicId, topic.unread_reactions_count, true);
        appliedCounters.add(topic);
    }
}

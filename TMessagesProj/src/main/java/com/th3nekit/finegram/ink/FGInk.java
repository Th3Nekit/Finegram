package com.th3nekit.finegram.ink;

import android.content.Context;
import android.content.SharedPreferences;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.util.Base64;

import com.th3nekit.finegram.core.configs.FinegramChatsConfig;

import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.MessageObject;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.R;
import org.telegram.messenger.UserConfig;
import org.telegram.messenger.UserObject;
import org.telegram.tgnet.ConnectionsManager;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.Components.QuoteSpan;
import org.telegram.ui.Components.TextStyleSpan;

import java.security.GeneralSecurityException;
import java.security.KeyPair;
import java.security.MessageDigest;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.ECParameterSpec;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

public final class FGInk {

    public static final int MAX_MESSAGE = 4096;
    private static final int MAX_PEER_KEYS = 4;
    private static final String PREFS = "fg_ink";
    private static final String[] EMOJI = {
            "🐶", "🐱", "🦊", "🐻", "🐼", "🐨", "🐯", "🦁", "🐮", "🐷", "🐸", "🐵", "🐔", "🐧", "🐦", "🦉",
            "🐝", "🦋", "🐢", "🐙", "🐬", "🐳", "🦄", "🐲", "🌵", "🌲", "🍀", "🍁", "🍄", "🌻", "🌙", "⭐",
            "🔥", "🌈", "❄", "💧", "🍎", "🍋", "🍉", "🍇", "🍓", "🍒", "🍑", "🍍", "🥝", "🍔", "🍕", "🍩",
            "⚽", "🏀", "🎲", "🎸", "🎧", "📷", "🚀", "⛵", "🚲", "🏠", "⏰", "🔑", "🎁", "💎", "🎈", "🔔"
    };

    private static final Object lock = new Object();
    private static final HashMap<Long, Identity> identities = new HashMap<>();
    private static final HashMap<String, byte[]> pairKeys = new HashMap<>();
    private static final HashMap<String, List<byte[]>> peerKeys = new HashMap<>();

    private static final class Identity {
        final PrivateKey privateKey;
        final byte[] publicKey;
        final ECParameterSpec params;

        Identity(PrivateKey privateKey, byte[] publicKey, ECParameterSpec params) {
            this.privateKey = privateKey;
            this.publicKey = publicKey;
            this.params = params;
        }
    }

    public static final class Payload {
        public final FGInkCodec.Frame frame;
        public final String tail;

        Payload(FGInkCodec.Frame frame, String tail) {
            this.frame = frame;
            this.tail = tail;
        }
    }

    private FGInk() {
    }

    public static boolean enabled() {
        return FinegramChatsConfig.INSTANCE.getInvisibleInk();
    }

    private static SharedPreferences prefs() {
        return ApplicationLoader.applicationContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    public static boolean supportsDialog(int account, long dialogId) {
        if (dialogId <= 0 || dialogId == UserConfig.getInstance(account).getClientUserId()) {
            return false;
        }
        TLRPC.User user = MessagesController.getInstance(account).getUser(dialogId);
        return user != null && !user.bot && !UserObject.isDeleted(user) && !UserObject.isReplyUser(user) && !UserObject.isService(user.id);
    }

    public static CharSequence strip(MessageObject message, CharSequence text) {
        message.fgInkPayload = null;
        message.fgInkDecorated = null;
        message.fgInkCoverLength = -1;
        if (text == null || text.length() < 7) {
            return text;
        }
        char last = text.charAt(text.length() - 1);
        if (last < '︀' || last > '️') {
            return text;
        }
        FGInkCodec.Frame frame = FGInkCodec.find(text);
        if (frame == null) {
            return text;
        }
        message.fgInkPayload = new Payload(frame, text.subSequence(frame.tailStart, text.length()).toString());
        if (frame.type == FGInkCodec.TYPE_HELLO && enabled()) {
            try {
                rememberHello(message, frame);
            } catch (Throwable ignored) {
            }
        }
        return text.subSequence(0, frame.tailStart);
    }

    public static CharSequence decorate(MessageObject message, CharSequence text) {
        Payload payload = message.fgInkPayload;
        if (payload == null || text == null || !enabled()) {
            return text;
        }
        if (message.fgInkDecorated != null && text == message.fgInkDecorated) {
            return text;
        }
        long dialogId = message.getDialogId();
        if (dialogId <= 0) {
            return text;
        }
        Block block;
        try {
            block = block(message, payload.frame, dialogId);
        } catch (Throwable e) {
            block = null;
        }
        if (block == null) {
            return text;
        }
        SpannableStringBuilder out = text instanceof SpannableStringBuilder ? (SpannableStringBuilder) text : new SpannableStringBuilder(text);
        int coverEnd = out.length();
        if (coverEnd > 0) {
            out.append('\n');
        }
        int quoteStart = out.length();
        out.append(block.header);
        style(out, quoteStart, out.length(), TextStyleSpan.FLAG_STYLE_BOLD);
        if (block.body != null && block.body.length() > 0) {
            out.append('\n');
            int bodyStart = out.length();
            out.append(block.body);
            if (block.spoiler) {
                style(out, bodyStart, out.length(), TextStyleSpan.FLAG_STYLE_SPOILER);
            }
        }
        QuoteSpan.putQuote(out, quoteStart, out.length(), false);
        message.fgInkCoverLength = coverEnd;
        message.fgInkDecorated = out;
        return out;
    }

    private static void style(SpannableStringBuilder out, int start, int end, int flags) {
        if (end <= start) {
            return;
        }
        TextStyleSpan.TextStyleRun run = new TextStyleSpan.TextStyleRun();
        run.flags = flags;
        run.start = start;
        run.end = end;
        out.setSpan(new TextStyleSpan(run), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
    }

    public static CharSequence cover(MessageObject message) {
        if (message == null) {
            return null;
        }
        CharSequence text = message.messageText;
        if (text != null && message.fgInkCoverLength >= 0 && message.fgInkCoverLength <= text.length() && text == message.fgInkDecorated) {
            return text.subSequence(0, message.fgInkCoverLength);
        }
        return text;
    }

    public static String keepTail(MessageObject message, String edited) {
        if (message == null || edited == null || message.fgInkPayload == null) {
            return edited;
        }
        String tail = message.fgInkPayload.tail;
        if (edited.length() + tail.length() > MAX_MESSAGE || FGInkCodec.find(edited) != null) {
            return edited;
        }
        return edited + tail;
    }

    private static final class Block {
        final String header;
        final String body;
        final boolean spoiler;

        Block(String header, String body, boolean spoiler) {
            this.header = header;
            this.body = body;
            this.spoiler = spoiler;
        }
    }

    private static Block block(MessageObject message, FGInkCodec.Frame frame, long peerId) throws GeneralSecurityException {
        int account = message.currentAccount;
        long selfId = UserConfig.getInstance(account).getClientUserId();
        if (frame.type == FGInkCodec.TYPE_HELLO) {
            boolean ready = exchanged(account, peerId);
            int text = ready ? R.string.FG_Ink_HelloReady : message.isOut() ? R.string.FG_Ink_HelloOutgoing : R.string.FG_Ink_HelloIncoming;
            return new Block("\uD83D\uDD10 " + LocaleController.getString(R.string.FG_Ink_Title), LocaleController.getString(text), false);
        }
        String plain = open(account, peerId, frame);
        if (plain == null) {
            return null;
        }
        if (frame.type == FGInkCodec.TYPE_INK) {
            return new Block("\uD83D\uDD0F " + LocaleController.getString(R.string.FG_Ink_Title), plain, true);
        }
        int unlockAt = frame.unlockAt();
        int now = ConnectionsManager.getInstance(account).getCurrentTime();
        String when = LocaleController.formatDateTime(unlockAt, false);
        if (now < unlockAt) {
            return new Block("\u23F3 " + LocaleController.getString(R.string.FG_Ink_Capsule),
                    LocaleController.formatString(R.string.FG_Ink_CapsuleLocked, when, left(unlockAt - now)), false);
        }
        return new Block("\u231B " + LocaleController.getString(R.string.FG_Ink_Capsule) + " · " + LocaleController.formatString(R.string.FG_Ink_CapsuleOpened, when), plain, true);
    }

    private static String left(int seconds) {
        int days = seconds / 86400;
        int hours = (seconds % 86400) / 3600;
        int minutes = Math.max(1, (seconds % 3600) / 60);
        if (days > 0) {
            return LocaleController.formatPluralString("Days", days) + (hours > 0 ? " " + LocaleController.formatPluralString("Hours", hours) : "");
        }
        if (hours > 0) {
            return LocaleController.formatPluralString("Hours", hours) + " " + LocaleController.formatPluralString("Minutes", minutes);
        }
        return LocaleController.formatPluralString("Minutes", minutes);
    }

    private static String open(int account, long peerId, FGInkCodec.Frame frame) throws GeneralSecurityException {
        long selfId = UserConfig.getInstance(account).getClientUserId();
        Identity own = identity(selfId, false);
        if (own == null) {
            return null;
        }
        for (byte[] peer : peerKeys(selfId, peerId)) {
            byte[] key = pairKey(selfId, peerId, own, peer);
            String plain = key == null ? null : FGInkCodec.open(key, frame);
            if (plain != null) {
                return plain;
            }
        }
        return null;
    }

    private static byte[] pairKey(long selfId, long peerId, Identity own, byte[] peer) {
        String cacheKey = selfId + ":" + peerId + ":" + Base64.encodeToString(peer, Base64.NO_WRAP);
        synchronized (lock) {
            byte[] cached = pairKeys.get(cacheKey);
            if (cached != null) {
                return cached;
            }
        }
        try {
            PublicKey publicKey = FGInkCodec.decompress(peer, own.params);
            byte[] key = FGInkCodec.pairKey(own.privateKey, publicKey, selfId, peerId);
            synchronized (lock) {
                pairKeys.put(cacheKey, key);
            }
            return key;
        } catch (GeneralSecurityException e) {
            return null;
        }
    }

    private static Identity identity(long selfId, boolean create) throws GeneralSecurityException {
        synchronized (lock) {
            Identity cached = identities.get(selfId);
            if (cached != null) {
                return cached;
            }
            SharedPreferences prefs = prefs();
            String stored = prefs.getString("own_" + selfId, null);
            if (stored != null) {
                int split = stored.indexOf(':');
                if (split > 0) {
                    try {
                        PrivateKey privateKey = FGInkCodec.privateKey(Base64.decode(stored.substring(0, split), Base64.NO_WRAP));
                        byte[] publicKey = Base64.decode(stored.substring(split + 1), Base64.NO_WRAP);
                        ECParameterSpec params = ((java.security.interfaces.ECPrivateKey) privateKey).getParams();
                        Identity identity = new Identity(privateKey, publicKey, params);
                        identities.put(selfId, identity);
                        return identity;
                    } catch (GeneralSecurityException | IllegalArgumentException | ClassCastException ignored) {
                    }
                }
            }
            if (!create) {
                return null;
            }
            KeyPair pair = FGInkCodec.generateKeyPair();
            ECPublicKey publicKey = (ECPublicKey) pair.getPublic();
            byte[] compressed = FGInkCodec.compress(publicKey);
            Identity identity = new Identity(pair.getPrivate(), compressed, publicKey.getParams());
            prefs.edit().putString("own_" + selfId,
                    Base64.encodeToString(pair.getPrivate().getEncoded(), Base64.NO_WRAP) + ":" + Base64.encodeToString(compressed, Base64.NO_WRAP)).apply();
            identities.put(selfId, identity);
            return identity;
        }
    }

    private static List<byte[]> peerKeys(long selfId, long peerId) {
        String name = "peer_" + selfId + "_" + peerId;
        synchronized (lock) {
            List<byte[]> cached = peerKeys.get(name);
            if (cached != null) {
                return new ArrayList<>(cached);
            }
            ArrayList<byte[]> keys = new ArrayList<>();
            String stored = prefs().getString(name, "");
            if (!stored.isEmpty()) {
                for (String part : stored.split(",")) {
                    try {
                        byte[] key = Base64.decode(part, Base64.NO_WRAP);
                        if (key.length == 33) {
                            keys.add(key);
                        }
                    } catch (IllegalArgumentException ignored) {
                    }
                }
            }
            peerKeys.put(name, keys);
            return new ArrayList<>(keys);
        }
    }

    public static boolean hasPeerKey(int account, long peerId) {
        return !peerKeys(UserConfig.getInstance(account).getClientUserId(), peerId).isEmpty();
    }

    private static void rememberHello(MessageObject message, FGInkCodec.Frame frame) {
        if (message.isOut()) {
            return;
        }
        long peerId = message.getDialogId();
        if (peerId <= 0 || message.messageOwner == null
                || message.messageOwner.from_id != null && message.messageOwner.from_id.user_id != 0 && message.messageOwner.from_id.user_id != peerId) {
            return;
        }
        long selfId = UserConfig.getInstance(message.currentAccount).getClientUserId();
        if (selfId == 0 || selfId == peerId) {
            return;
        }
        byte[] key = frame.publicKey();
        if (key == null) {
            return;
        }
        String name = "peer_" + selfId + "_" + peerId;
        synchronized (lock) {
            List<byte[]> keys = peerKeys(selfId, peerId);
            if (!keys.isEmpty() && java.util.Arrays.equals(keys.get(0), key)) {
                return;
            }
            if (!keys.isEmpty()) {
                prefs().edit().remove("mine_" + selfId + "_" + peerId).remove("sent_" + selfId + "_" + peerId).apply();
            }
            for (int i = keys.size() - 1; i >= 0; i--) {
                if (java.util.Arrays.equals(keys.get(i), key)) {
                    keys.remove(i);
                }
            }
            keys.add(0, key);
            while (keys.size() > MAX_PEER_KEYS) {
                keys.remove(keys.size() - 1);
            }
            peerKeys.put(name, new ArrayList<>(keys));
            StringBuilder joined = new StringBuilder();
            for (byte[] k : keys) {
                if (joined.length() > 0) {
                    joined.append(',');
                }
                joined.append(Base64.encodeToString(k, Base64.NO_WRAP));
            }
            prefs().edit().putString(name, joined.toString()).apply();
        }
    }

    private static String digest(byte[] key) {
        try {
            byte[] hash = MessageDigest.getInstance("SHA-256").digest(key);
            return Base64.encodeToString(hash, 0, 9, Base64.NO_WRAP);
        } catch (GeneralSecurityException e) {
            return "";
        }
    }

    private static boolean keySent(long selfId, long peerId) {
        SharedPreferences prefs = prefs();
        if (prefs.getBoolean("mine_" + selfId + "_" + peerId, false)) {
            return true;
        }
        List<byte[]> keys = peerKeys(selfId, peerId);
        return !keys.isEmpty() && digest(keys.get(0)).equals(prefs.getString("sent_" + selfId + "_" + peerId, ""));
    }

    private static void markKeySent(long selfId, long peerId) {
        prefs().edit().putBoolean("mine_" + selfId + "_" + peerId, true).apply();
    }

    public static boolean keySent(int account, long peerId) {
        return keySent(UserConfig.getInstance(account).getClientUserId(), peerId);
    }

    public static boolean exchanged(int account, long peerId) {
        return hasPeerKey(account, peerId) && keySent(account, peerId);
    }

    public static String withHello(int account, long dialogId, String text) {
        if (text == null || !enabled() || !supportsDialog(account, dialogId) || FGInkCodec.find(text) != null) {
            return text;
        }
        long selfId = UserConfig.getInstance(account).getClientUserId();
        if (!hasPeerKey(account, dialogId) || keySent(selfId, dialogId)) {
            return text;
        }
        try {
            Identity own = identity(selfId, true);
            String tail = FGInkCodec.tail(FGInkCodec.hello(own.publicKey));
            if (text.length() + tail.length() > MAX_MESSAGE) {
                return text;
            }
            markKeySent(selfId, dialogId);
            return text + tail;
        } catch (GeneralSecurityException e) {
            return text;
        }
    }

    public static String proposal(int account, long dialogId) throws GeneralSecurityException {
        long selfId = UserConfig.getInstance(account).getClientUserId();
        Identity own = identity(selfId, true);
        boolean reply = hasPeerKey(account, dialogId);
        markKeySent(selfId, dialogId);
        return LocaleController.getString(reply ? R.string.FG_Ink_ProposeReply : R.string.FG_Ink_Propose) + FGInkCodec.tail(FGInkCodec.hello(own.publicKey));
    }

    public static int room(String cover, int type, String hidden) {
        int coverLength = cover == null ? 0 : cover.length();
        int size = FGInkCodec.sealedSize(type, hidden == null ? "" : hidden);
        return MAX_MESSAGE - coverLength - FGInkCodec.tailLength(size);
    }

    public static String compose(int account, long dialogId, String cover, String hidden, int type, int unlockAt) throws GeneralSecurityException {
        long selfId = UserConfig.getInstance(account).getClientUserId();
        Identity own = identity(selfId, true);
        List<byte[]> keys = peerKeys(selfId, dialogId);
        if (keys.isEmpty() || !keySent(selfId, dialogId)) {
            throw new GeneralSecurityException("keys not exchanged");
        }
        byte[] key = pairKey(selfId, dialogId, own, keys.get(0));
        if (key == null) {
            throw new GeneralSecurityException("bad peer key");
        }
        String tail = FGInkCodec.tail(FGInkCodec.seal(key, type, unlockAt, hidden));
        String visible = cover == null ? "" : cover;
        if (visible.length() + tail.length() > MAX_MESSAGE) {
            throw new GeneralSecurityException("too long");
        }
        return visible + tail;
    }

    public static String fingerprint(int account, long dialogId) {
        long selfId = UserConfig.getInstance(account).getClientUserId();
        List<byte[]> keys = peerKeys(selfId, dialogId);
        if (keys.isEmpty()) {
            return null;
        }
        try {
            Identity own = identity(selfId, true);
            byte[] hash = FGInkCodec.fingerprint(own.publicKey, keys.get(0));
            StringBuilder out = new StringBuilder();
            for (int i = 0; i < 8; i++) {
                if (i == 4) {
                    out.append("  ");
                }
                out.append(EMOJI[hash[i] & 0x3F]);
            }
            return out.toString();
        } catch (GeneralSecurityException e) {
            return null;
        }
    }
}

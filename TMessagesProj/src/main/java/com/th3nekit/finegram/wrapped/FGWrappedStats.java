package com.th3nekit.finegram.wrapped;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ChatObject;
import org.telegram.messenger.DialogObject;
import org.telegram.messenger.Emoji;
import org.telegram.messenger.MessageObject;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.UserConfig;
import org.telegram.messenger.UserObject;
import org.telegram.messenger.Utilities;
import org.telegram.tgnet.ConnectionsManager;
import org.telegram.tgnet.TLObject;
import org.telegram.tgnet.TLRPC;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Map;

public final class FGWrappedStats {

    public static final int PERIOD_PREVIOUS_MONTH = 0;
    public static final int PERIOD_CURRENT_MONTH = 1;
    public static final int PERIOD_YEAR = 2;

    public static final class Person {
        public long dialogId;
        public TLObject peer;
        public int count;
        boolean channel;
        final ArrayList<TLRPC.Message> pages = new ArrayList<>();
    }

    public interface Listener {
        void onProgress(float progress);

        void onDone(FGWrappedStats stats);
    }

    public final int account;
    public final int period;
    public final int start;
    public final int end;
    public final int month;
    public final int year;

    public int total;
    public int chats;
    public final ArrayList<Person> top = new ArrayList<>();
    public final int[] hours = new int[24];
    public int sampled;
    public long sampledChars;
    public int longest;
    public final ArrayList<String> emoji = new ArrayList<>();
    public final ArrayList<Long> emojiDocs = new ArrayList<>();
    public final ArrayList<Integer> emojiCounts = new ArrayList<>();
    public TLRPC.Document sticker;
    public MessageObject stickerParent;
    public int stickerCount;
    public int stickersSampled;
    public int voice;
    public int rounds;
    public int photos;
    public int calls;
    public int callSeconds;
    public TLObject callPeer;
    public int callPeerSeconds;
    public String word;
    public int wordCount;
    public int activeDays;

    private static final HashMap<String, FGWrappedStats> cache = new HashMap<>();

    private static final HashSet<String> STOP = new HashSet<>();

    static {
        Collections.addAll(STOP,
                "это", "этот", "эта", "эти", "этого", "этом", "этой", "этим", "этих", "что", "чтобы", "чего", "чем", "как",
                "так", "там", "тут", "тоже", "также", "когда", "если", "есть", "нету", "было", "были", "была", "будет",
                "будут", "буду", "будешь", "будем", "быть", "очень", "можно", "нельзя", "надо", "нужно", "нужен", "нужна",
                "сейчас", "потом", "только", "вообще", "короче", "ладно", "вроде", "типа", "почему", "зачем", "который",
                "которые", "которая", "которой", "которых", "просто", "меня", "тебя", "тебе", "себя", "себе", "него",
                "неё", "нее", "них", "нам", "вам", "нас", "вас", "ваш", "наш", "наша", "мой", "моя", "мои", "моё", "твой",
                "твоя", "твои", "свой", "своя", "свои", "того", "даже", "уже", "еще", "ещё", "ведь", "либо", "или", "где",
                "куда", "откуда", "сколько", "какой", "какая", "какие", "какое", "такой", "такая", "такие", "такое", "всех",
                "всё", "все", "весь", "всего", "всем", "тогда", "сегодня", "завтра", "вчера", "блин", "давай", "давайте",
                "хорошо", "окей", "спасибо", "привет", "пока", "может", "могу", "можешь", "могут", "мочь", "хочу",
                "хочешь", "хотел", "хотела", "думаю", "знаю", "знаешь", "понял", "поняла", "понятно", "кстати", "конечно",
                "наверное", "наверно", "реально", "щас", "типо", "вот", "через", "после", "перед", "между", "около",
                "пусть", "почти", "опять", "снова", "совсем", "больше", "меньше", "много", "мало", "лучше", "хуже",
                "потому", "поэтому", "чтоли", "чтото", "ничего", "никто", "нечего", "сразу", "всегда", "никогда", "иногда",
                "здесь", "оттуда", "отсюда", "такого", "этому", "которого", "сказал", "сказала", "говорю", "говорит",
                "делать", "сделать", "делаю", "сделал", "сделала", "смотри", "посмотри", "вижу", "видел", "пишу", "написал",
                "you", "the", "and", "that", "this", "with", "have", "just", "what", "your", "from", "they", "will",
                "there", "about", "would", "like", "yeah", "okay", "when", "then", "them", "were", "been", "here", "dont",
                "http", "https", "www", "com", "html", "null", "true", "false", "hash", "test", "user", "admin");
    }

    private FGWrappedStats(int account, int period) {
        this.account = account;
        this.period = period;
        Calendar calendar = Calendar.getInstance();
        int nowYear = calendar.get(Calendar.YEAR);
        int nowMonth = calendar.get(Calendar.MONTH);
        Calendar from = Calendar.getInstance();
        from.clear();
        Calendar to = Calendar.getInstance();
        to.clear();
        if (period == PERIOD_YEAR) {
            from.set(nowYear, Calendar.JANUARY, 1, 0, 0, 0);
            to.set(nowYear + 1, Calendar.JANUARY, 1, 0, 0, 0);
            year = nowYear;
            month = -1;
        } else if (period == PERIOD_CURRENT_MONTH) {
            from.set(nowYear, nowMonth, 1, 0, 0, 0);
            to.set(nowYear, nowMonth, 1, 0, 0, 0);
            to.add(Calendar.MONTH, 1);
            year = nowYear;
            month = nowMonth;
        } else {
            from.set(nowYear, nowMonth, 1, 0, 0, 0);
            from.add(Calendar.MONTH, -1);
            to.set(nowYear, nowMonth, 1, 0, 0, 0);
            year = from.get(Calendar.YEAR);
            month = from.get(Calendar.MONTH);
        }
        start = (int) (from.getTimeInMillis() / 1000L);
        end = (int) (Math.min(to.getTimeInMillis(), System.currentTimeMillis()) / 1000L);
    }

    public static int[] previousMonth() {
        Calendar calendar = Calendar.getInstance();
        calendar.set(Calendar.DAY_OF_MONTH, 1);
        calendar.add(Calendar.MONTH, -1);
        return new int[]{calendar.get(Calendar.YEAR), calendar.get(Calendar.MONTH)};
    }

    public boolean isYear() {
        return period == PERIOD_YEAR;
    }

    public static Runnable collect(int account, int period, Listener listener) {
        String key = account + "_" + period + "_" + (System.currentTimeMillis() / 3_600_000L);
        FGWrappedStats cached = cache.get(key);
        if (cached != null) {
            listener.onProgress(1f);
            AndroidUtilities.runOnUIThread(() -> listener.onDone(cached));
            return () -> {
            };
        }
        Collector collector = new Collector(new FGWrappedStats(account, period), listener, key);
        collector.start();
        return collector::cancel;
    }

    private static int total(TLRPC.messages_Messages res) {
        return res == null ? 0 : Math.max(res.count, res.messages.size());
    }

    private static final class Collector {
        private static final int CONCURRENCY = 4;
        private static final long REQUEST_TIMEOUT_MS = 15_000;
        private static final long STALL_MS = 30_000;

        private final FGWrappedStats s;
        private final Listener listener;
        private final String key;
        private final ArrayDeque<Runnable> pending = new ArrayDeque<>();
        private final ArrayList<Person> candidates = new ArrayList<>();
        private final ArrayList<TLRPC.Message> sample = new ArrayList<>();
        private final HashSet<Long> sampleIds = new HashSet<>();
        private final HashMap<Long, Integer> callTotals = new HashMap<>();
        private final ArrayList<Integer> requestIds = new ArrayList<>();
        private int inFlight;
        private int stageLeft;
        private Runnable stageDone;
        private int planned;
        private int completed;
        private boolean cancelled;
        private boolean finished;
        private long lastProgress;
        private final boolean openEnd;
        private final int channelPages;

        private final Runnable watchdog = new Runnable() {
            @Override
            public void run() {
                if (cancelled || finished) {
                    return;
                }
                if (System.currentTimeMillis() - lastProgress >= STALL_MS) {
                    pending.clear();
                    stageDone = null;
                    finish();
                    return;
                }
                AndroidUtilities.runOnUIThread(this, 2000);
            }
        };

        Collector(FGWrappedStats stats, Listener listener, String key) {
            this.s = stats;
            this.listener = listener;
            this.key = key;
            this.openEnd = System.currentTimeMillis() / 1000L - stats.end < 120;
            this.channelPages = stats.isYear() ? 8 : 6;
        }

        void cancel() {
            cancelled = true;
            pending.clear();
            AndroidUtilities.cancelRunOnUIThread(watchdog);
            ConnectionsManager connections = ConnectionsManager.getInstance(s.account);
            for (int id : requestIds) {
                connections.cancelRequest(id, true);
            }
            requestIds.clear();
        }

        void start() {
            lastProgress = System.currentTimeMillis();
            AndroidUtilities.runOnUIThread(watchdog, 2000);
            MessagesController controller = MessagesController.getInstance(s.account);
            long self = UserConfig.getInstance(s.account).getClientUserId();
            int limit = s.isYear() ? 110 : 70;
            ArrayList<TLRPC.Dialog> dialogs = new ArrayList<>(controller.getAllDialogs());
            Collections.sort(dialogs, (a, b) -> Integer.compare(b.last_message_date, a.last_message_date));
            for (TLRPC.Dialog dialog : dialogs) {
                if (candidates.size() >= limit) {
                    break;
                }
                if (dialog.last_message_date != 0 && dialog.last_message_date < s.start) {
                    continue;
                }
                long id = dialog.id;
                if (id == self || DialogObject.isEncryptedDialog(id)) {
                    continue;
                }
                Person person = new Person();
                if (DialogObject.isUserDialog(id)) {
                    TLRPC.User user = controller.getUser(id);
                    if (user == null || user.bot || user.deleted || user.self || UserObject.isService(user.id)) {
                        continue;
                    }
                    person.peer = user;
                } else {
                    TLRPC.Chat chat = controller.getChat(-id);
                    if (chat == null || ChatObject.isChannelAndNotMegaGroup(chat)) {
                        continue;
                    }
                    person.peer = chat;
                    person.channel = ChatObject.isChannel(chat);
                }
                person.dialogId = id;
                candidates.add(person);
            }
            planned = candidates.size() * 2 + 60;
            beginStage(this::afterCounts);
            for (Person person : candidates) {
                TLRPC.InputPeer peer = controller.getInputPeer(person.dialogId);
                if (peer == null) {
                    continue;
                }
                if (person.channel) {
                    pageChannel(person, peer, 0, 0);
                } else {
                    countPrivate(peer, new TLRPC.TL_inputMessagesFilterEmpty(), value -> person.count = value);
                }
            }
            endStageSetup();
        }

        private TLRPC.TL_messages_search search(TLRPC.InputPeer peer, TLRPC.MessagesFilter filter, int maxDate, int offsetId, int limit) {
            TLRPC.TL_messages_search req = new TLRPC.TL_messages_search();
            req.peer = peer;
            req.q = "";
            req.from_id = new TLRPC.TL_inputPeerSelf();
            req.filter = filter;
            req.max_date = maxDate;
            req.offset_id = offsetId;
            req.limit = limit;
            return req;
        }

        private static int newerThan(TLRPC.messages_Messages res) {
            if (res == null) {
                return -1;
            }
            if (res.messages.isEmpty()) {
                return total(res);
            }
            return res.offset_id_offset;
        }

        private void countPrivate(TLRPC.InputPeer peer, TLRPC.MessagesFilter filter, Utilities.Callback<Integer> result) {
            final int[] offsets = {openEnd ? 0 : Integer.MIN_VALUE, Integer.MIN_VALUE};
            final Runnable done = () -> {
                if (offsets[0] == Integer.MIN_VALUE || offsets[1] == Integer.MIN_VALUE) {
                    return;
                }
                if (offsets[0] < 0 || offsets[1] < 0) {
                    result.run(0);
                } else {
                    result.run(Math.max(0, offsets[1] - offsets[0]));
                }
            };
            if (!openEnd) {
                enqueue(search(peer, filter, s.end, 0, 1), res -> {
                    offsets[0] = newerThan(res);
                    done.run();
                });
            }
            enqueue(search(peer, filter, s.start, 0, 1), res -> {
                offsets[1] = newerThan(res);
                done.run();
            });
        }

        private void pageChannel(Person person, TLRPC.InputPeer peer, int offsetId, int page) {
            enqueue(search(peer, new TLRPC.TL_inputMessagesFilterEmpty(), openEnd ? 0 : s.end, offsetId, 100), res -> {
                if (res == null) {
                    return;
                }
                int oldest = 0;
                int last = 0;
                boolean reachedStart = false;
                for (TLRPC.Message message : res.messages) {
                    last = message.id;
                    if (message.date < s.start) {
                        reachedStart = true;
                        break;
                    }
                    if (message.date <= s.end) {
                        person.pages.add(message);
                        person.count++;
                        oldest = message.date;
                    }
                }
                boolean more = !reachedStart && res.messages.size() >= 100 && last != 0;
                if (more && page + 1 < channelPages) {
                    pageChannel(person, peer, last, page + 1);
                } else if (more && oldest > s.start) {
                    float covered = Math.max(1f, s.end - oldest);
                    person.count = Math.round(person.count * Math.min(40f, (s.end - s.start) / covered));
                }
            });
        }

        private void afterCounts() {
            ArrayList<Person> active = new ArrayList<>();
            for (Person person : candidates) {
                if (person.count > 0) {
                    s.total += person.count;
                    active.add(person);
                }
            }
            s.chats = active.size();
            Collections.sort(active, (a, b) -> Integer.compare(b.count, a.count));
            for (int i = 0; i < Math.min(5, active.size()); i++) {
                s.top.add(active.get(i));
            }
            MessagesController controller = MessagesController.getInstance(s.account);
            beginStage(this::finish);
            int sampleChats = Math.min(14, active.size());
            int mediaChats = Math.min(10, active.size());
            planned = completed + sampleChats * 3 + mediaChats * 6 + 4;
            int span = Math.max(1, s.end - s.start);
            for (int i = 0; i < sampleChats; i++) {
                Person person = active.get(i);
                if (person.channel) {
                    addSample(person.pages);
                    continue;
                }
                TLRPC.InputPeer peer = controller.getInputPeer(person.dialogId);
                if (peer == null) {
                    continue;
                }
                int slices = s.isYear() ? 4 : 3;
                for (int k = 1; k <= slices; k++) {
                    int maxDate = s.start + (int) ((long) span * k / slices);
                    if (k == slices && openEnd) {
                        maxDate = 0;
                    }
                    enqueue(search(peer, new TLRPC.TL_inputMessagesFilterEmpty(), maxDate, 0, 40), res -> {
                        if (res != null) {
                            addSample(res.messages);
                        }
                    });
                }
            }
            for (int i = 0; i < mediaChats; i++) {
                Person person = active.get(i);
                TLRPC.InputPeer peer = controller.getInputPeer(person.dialogId);
                if (peer == null) {
                    continue;
                }
                if (person.channel) {
                    countChannelMedia(peer, new TLRPC.TL_inputMessagesFilterVoice(), value -> s.voice += value);
                    countChannelMedia(peer, new TLRPC.TL_inputMessagesFilterRoundVideo(), value -> s.rounds += value);
                    countChannelMedia(peer, new TLRPC.TL_inputMessagesFilterPhotos(), value -> s.photos += value);
                } else {
                    countPrivate(peer, new TLRPC.TL_inputMessagesFilterVoice(), value -> s.voice += value);
                    countPrivate(peer, new TLRPC.TL_inputMessagesFilterRoundVideo(), value -> s.rounds += value);
                    countPrivate(peer, new TLRPC.TL_inputMessagesFilterPhotos(), value -> s.photos += value);
                }
            }
            loadCalls(0, 0);
            endStageSetup();
        }

        private void countChannelMedia(TLRPC.InputPeer peer, TLRPC.MessagesFilter filter, Utilities.Callback<Integer> result) {
            enqueue(search(peer, filter, openEnd ? 0 : s.end, 0, 100), res -> {
                if (res == null) {
                    return;
                }
                int count = 0;
                for (TLRPC.Message message : res.messages) {
                    if (message.date >= s.start && message.date <= s.end) {
                        count++;
                    }
                }
                result.run(count);
            });
        }

        private void addSample(ArrayList<TLRPC.Message> messages) {
            for (TLRPC.Message message : messages) {
                if (message == null || message.date < s.start || message.date > s.end) {
                    continue;
                }
                long key = ((long) message.id << 20) ^ (MessageObject.getDialogId(message) & 0xFFFFF);
                if (sampleIds.add(key)) {
                    sample.add(message);
                }
            }
        }

        private void loadCalls(int offsetId, int page) {
            TLRPC.TL_messages_search req = new TLRPC.TL_messages_search();
            req.peer = new TLRPC.TL_inputPeerEmpty();
            req.q = "";
            req.filter = new TLRPC.TL_inputMessagesFilterPhoneCalls();
            req.min_date = s.start;
            req.max_date = s.end;
            req.offset_id = offsetId;
            req.limit = 100;
            enqueue(req, res -> {
                if (res == null) {
                    return;
                }
                int last = 0;
                boolean older = false;
                for (TLRPC.Message message : res.messages) {
                    last = message.id;
                    if (message.date < s.start) {
                        older = true;
                        continue;
                    }
                    if (message.date > s.end || !(message.action instanceof TLRPC.TL_messageActionPhoneCall)) {
                        continue;
                    }
                    int duration = message.action.duration;
                    if (duration <= 0) {
                        continue;
                    }
                    s.calls++;
                    s.callSeconds += duration;
                    long dialogId = MessageObject.getDialogId(message);
                    Integer prev = callTotals.get(dialogId);
                    callTotals.put(dialogId, (prev == null ? 0 : prev) + duration);
                }
                if (!older && res.messages.size() >= 100 && page < 6 && last != 0) {
                    loadCalls(last, page + 1);
                }
            });
        }

        private void finish() {
            if (cancelled || finished) {
                return;
            }
            finished = true;
            AndroidUtilities.cancelRunOnUIThread(watchdog);
            ConnectionsManager connections = ConnectionsManager.getInstance(s.account);
            for (int id : requestIds) {
                connections.cancelRequest(id, true);
            }
            requestIds.clear();
            if (s.top.isEmpty() && s.total == 0) {
                ArrayList<Person> active = new ArrayList<>();
                for (Person person : candidates) {
                    if (person.count > 0) {
                        s.total += person.count;
                        active.add(person);
                    }
                }
                s.chats = active.size();
                Collections.sort(active, (a, b) -> Integer.compare(b.count, a.count));
                for (int i = 0; i < Math.min(5, active.size()); i++) {
                    s.top.add(active.get(i));
                }
            }
            analyzeSample();
            MessagesController controller = MessagesController.getInstance(s.account);
            long best = 0;
            int bestSeconds = 0;
            for (Map.Entry<Long, Integer> entry : callTotals.entrySet()) {
                if (entry.getValue() > bestSeconds) {
                    bestSeconds = entry.getValue();
                    best = entry.getKey();
                }
            }
            if (best != 0) {
                s.callPeer = DialogObject.isUserDialog(best) ? controller.getUser(best) : controller.getChat(-best);
                s.callPeerSeconds = bestSeconds;
            }
            cache.put(key, s);
            listener.onProgress(1f);
            listener.onDone(s);
        }

        private static boolean looksLikeCode(String text) {
            int letters = 0;
            int symbols = 0;
            for (int i = 0; i < text.length(); i++) {
                char c = text.charAt(i);
                if (Character.isLetter(c) || Character.isWhitespace(c)) {
                    letters++;
                } else if ("{}[]<>=_/\\|;:#$%^&*`~".indexOf(c) >= 0 || Character.isDigit(c)) {
                    symbols++;
                }
            }
            return text.length() > 700 || symbols > 6 && symbols * 4 > letters;
        }

        private static boolean isLaugh(String word) {
            return word.matches("(а?х+а+)+х*") || word.matches("(хе)+") || word.matches("(хи)+")
                    || word.matches("(ah|ha)+h*") || word.matches("(lol)+") || word.matches(".*(.)\\1{2,}.*");
        }

        private void analyzeSample() {
            HashMap<String, Integer> emojiMap = new HashMap<>();
            HashMap<String, Long> emojiDocMap = new HashMap<>();
            HashMap<Long, Integer> stickerMap = new HashMap<>();
            HashMap<Long, TLRPC.Message> stickerMessages = new HashMap<>();
            HashMap<String, Integer> stemCounts = new HashMap<>();
            HashMap<String, HashSet<Integer>> stemDays = new HashMap<>();
            HashMap<String, HashSet<Long>> stemChats = new HashMap<>();
            HashMap<String, HashMap<String, Integer>> stemForms = new HashMap<>();
            HashSet<Integer> days = new HashSet<>();
            Calendar calendar = Calendar.getInstance();
            for (TLRPC.Message message : sample) {
                calendar.setTimeInMillis(message.date * 1000L);
                s.hours[calendar.get(Calendar.HOUR_OF_DAY)]++;
                int day = calendar.get(Calendar.YEAR) * 400 + calendar.get(Calendar.DAY_OF_YEAR);
                days.add(day);
                s.sampled++;
                long chat = MessageObject.getDialogId(message);
                if (message.media instanceof TLRPC.TL_messageMediaDocument && message.media.document != null) {
                    TLRPC.Document document = message.media.document;
                    if (MessageObject.isStickerDocument(document) || MessageObject.isAnimatedStickerDocument(document, true) || MessageObject.isVideoStickerDocument(document)) {
                        s.stickersSampled++;
                        Integer prev = stickerMap.get(document.id);
                        stickerMap.put(document.id, (prev == null ? 0 : prev) + 1);
                        stickerMessages.put(document.id, message);
                    }
                }
                String text = message.message;
                if (text == null || text.isEmpty() || message.fwd_from != null) {
                    continue;
                }
                s.sampledChars += text.length();
                s.longest = Math.max(s.longest, text.length());
                boolean[] covered = new boolean[text.length()];
                if (message.entities != null) {
                    for (TLRPC.MessageEntity entity : message.entities) {
                        boolean customEmoji = entity instanceof TLRPC.TL_messageEntityCustomEmoji;
                        if (customEmoji) {
                            long documentId = ((TLRPC.TL_messageEntityCustomEmoji) entity).document_id;
                            String codeKey = "\u0000" + documentId;
                            Integer prev = emojiMap.get(codeKey);
                            emojiMap.put(codeKey, (prev == null ? 0 : prev) + 1);
                            emojiDocMap.put(codeKey, documentId);
                        }
                        if (customEmoji || entity instanceof TLRPC.TL_messageEntityCode || entity instanceof TLRPC.TL_messageEntityPre
                                || entity instanceof TLRPC.TL_messageEntityUrl || entity instanceof TLRPC.TL_messageEntityTextUrl
                                || entity instanceof TLRPC.TL_messageEntityMention || entity instanceof TLRPC.TL_messageEntityHashtag
                                || entity instanceof TLRPC.TL_messageEntityBotCommand || entity instanceof TLRPC.TL_messageEntityEmail) {
                            for (int i = Math.max(0, entity.offset); i < Math.min(text.length(), entity.offset + entity.length); i++) {
                                covered[i] = true;
                            }
                        }
                    }
                }
                try {
                    ArrayList<Emoji.EmojiSpanRange> ranges = Emoji.parseEmojis(text);
                    if (ranges != null) {
                        for (Emoji.EmojiSpanRange range : ranges) {
                            if (range.code == null || range.start >= 0 && range.start < covered.length && covered[range.start]) {
                                continue;
                            }
                            String code = range.code.toString();
                            Integer prev = emojiMap.get(code);
                            emojiMap.put(code, (prev == null ? 0 : prev) + 1);
                        }
                    }
                } catch (Throwable ignored) {
                }
                if (looksLikeCode(text)) {
                    continue;
                }
                StringBuilder current = new StringBuilder();
                for (int i = 0; i <= text.length(); i++) {
                    char c = i < text.length() ? text.charAt(i) : ' ';
                    boolean inside = i < text.length() && covered[i];
                    if (!inside && Character.isLetter(c)) {
                        current.append(c);
                        continue;
                    }
                    if (current.length() >= 4 && current.length() <= 18) {
                        String lower = current.toString().toLowerCase(Locale.ROOT).replace('ё', 'е');
                        if (!STOP.contains(lower) && !isLaugh(lower)) {
                            char first = lower.charAt(0);
                            String stem = lower.length() >= 6 && first >= 'а' && first <= 'я' ? lower.substring(0, 5) : lower;
                            Integer prev = stemCounts.get(stem);
                            stemCounts.put(stem, (prev == null ? 0 : prev) + 1);
                            HashSet<Integer> wordDays = stemDays.get(stem);
                            if (wordDays == null) {
                                stemDays.put(stem, wordDays = new HashSet<>());
                            }
                            wordDays.add(day);
                            HashSet<Long> wordChats = stemChats.get(stem);
                            if (wordChats == null) {
                                stemChats.put(stem, wordChats = new HashSet<>());
                            }
                            wordChats.add(chat);
                            HashMap<String, Integer> forms = stemForms.get(stem);
                            if (forms == null) {
                                stemForms.put(stem, forms = new HashMap<>());
                            }
                            Integer formPrev = forms.get(lower);
                            forms.put(lower, (formPrev == null ? 0 : formPrev) + 1);
                        }
                    }
                    current.setLength(0);
                }
            }
            s.activeDays = days.size();
            ArrayList<Map.Entry<String, Integer>> emojiList = new ArrayList<>(emojiMap.entrySet());
            Collections.sort(emojiList, (a, b) -> Integer.compare(b.getValue(), a.getValue()));
            for (int i = 0; i < Math.min(5, emojiList.size()); i++) {
                String code = emojiList.get(i).getKey();
                Long documentId = emojiDocMap.get(code);
                s.emoji.add(documentId != null ? "" : code);
                s.emojiDocs.add(documentId != null ? documentId : 0L);
                s.emojiCounts.add(emojiList.get(i).getValue());
            }
            long bestSticker = 0;
            int bestStickerCount = 0;
            for (Map.Entry<Long, Integer> entry : stickerMap.entrySet()) {
                if (entry.getValue() > bestStickerCount) {
                    bestStickerCount = entry.getValue();
                    bestSticker = entry.getKey();
                }
            }
            if (bestSticker != 0 && bestStickerCount >= 2) {
                TLRPC.Message message = stickerMessages.get(bestSticker);
                s.sticker = message.media.document;
                s.stickerCount = bestStickerCount;
                try {
                    s.stickerParent = new MessageObject(s.account, message, false, false);
                } catch (Throwable ignored) {
                }
            }
            int minDays = s.isYear() ? 6 : Math.max(3, Math.min(5, days.size() / 6));
            String bestStem = null;
            double bestScore = 0;
            for (Map.Entry<String, Integer> entry : stemCounts.entrySet()) {
                int count = entry.getValue();
                int spread = stemDays.get(entry.getKey()).size();
                int chats = stemChats.get(entry.getKey()).size();
                if (count < 5 || spread < minDays) {
                    continue;
                }
                double score = count * Math.sqrt(spread) * (chats > 1 ? 1.25 : 1.0);
                if (score > bestScore) {
                    bestScore = score;
                    bestStem = entry.getKey();
                }
            }
            if (bestStem != null) {
                String bestForm = null;
                int bestFormCount = 0;
                for (Map.Entry<String, Integer> form : stemForms.get(bestStem).entrySet()) {
                    if (form.getValue() > bestFormCount) {
                        bestFormCount = form.getValue();
                        bestForm = form.getKey();
                    }
                }
                s.word = bestForm;
                s.wordCount = stemCounts.get(bestStem);
            }
        }

        private void beginStage(Runnable done) {
            stageDone = done;
            stageLeft = 1;
        }

        private void endStageSetup() {
            stageLeft--;
            pump();
        }

        private void enqueue(TLRPC.TL_messages_search req, Utilities.Callback<TLRPC.messages_Messages> callback) {
            stageLeft++;
            pending.add(() -> send(req, callback, 0));
        }

        private void send(TLRPC.TL_messages_search req, Utilities.Callback<TLRPC.messages_Messages> callback, int attempt) {
            if (cancelled || finished) {
                return;
            }
            final boolean[] answered = new boolean[1];
            final int[] id = new int[1];
            final Utilities.Callback<TLObject> complete = response -> {
                if (answered[0]) {
                    return;
                }
                answered[0] = true;
                requestIds.remove((Integer) id[0]);
                if (cancelled || finished) {
                    return;
                }
                TLRPC.messages_Messages res = response instanceof TLRPC.messages_Messages ? (TLRPC.messages_Messages) response : null;
                if (res != null) {
                    MessagesController controller = MessagesController.getInstance(s.account);
                    controller.putUsers(res.users, false);
                    controller.putChats(res.chats, false);
                }
                try {
                    callback.run(res);
                } catch (Throwable ignored) {
                }
                inFlight--;
                completed++;
                stageLeft--;
                lastProgress = System.currentTimeMillis();
                listener.onProgress(Math.min(0.98f, completed / (float) Math.max(planned, completed + 1)));
                pump();
            };
            id[0] = ConnectionsManager.getInstance(s.account).sendRequest(req, (response, error) -> AndroidUtilities.runOnUIThread(() -> {
                if (answered[0] || cancelled || finished) {
                    return;
                }
                if (error != null && error.text != null && error.text.startsWith("FLOOD_WAIT_") && attempt < 2) {
                    int seconds = Utilities.parseInt(error.text);
                    if (seconds > 0 && seconds <= 12) {
                        answered[0] = true;
                        requestIds.remove((Integer) id[0]);
                        lastProgress = System.currentTimeMillis();
                        AndroidUtilities.runOnUIThread(() -> send(req, callback, attempt + 1), seconds * 1000L + 300);
                        return;
                    }
                }
                complete.run(response);
            }));
            requestIds.add(id[0]);
            AndroidUtilities.runOnUIThread(() -> {
                if (!answered[0]) {
                    ConnectionsManager.getInstance(s.account).cancelRequest(id[0], true);
                    complete.run(null);
                }
            }, REQUEST_TIMEOUT_MS);
        }

        private void pump() {
            if (cancelled || finished) {
                return;
            }
            while (inFlight < CONCURRENCY && !pending.isEmpty()) {
                inFlight++;
                pending.poll().run();
            }
            if (stageLeft <= 0 && inFlight == 0 && pending.isEmpty() && stageDone != null) {
                Runnable done = stageDone;
                stageDone = null;
                done.run();
            }
        }
    }
}

package com.th3nekit.finegram.chats.helpers;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;

public final class OwnMessageHistorySearch {
    public static final class Entry {
        public final int id, date;
        public final boolean outgoing, post;

        public Entry(int id, int date, boolean outgoing, boolean post) {
            this.id = id;
            this.date = date;
            this.outgoing = outgoing;
            this.post = post;
        }
    }

    public interface PageCallback {
        void complete(List<Entry> entries, String error);
    }

    public interface Backend {
        void search(long dialogId, int topicId, int before, int offset, PageCallback callback);
    }

    public interface Completion {
        void complete(Map<Long, ArrayList<Integer>> messages, String error);
    }

    private final Backend backend;
    private final Completion completion;
    private final long dialogId, mergeDialogId;
    private final int topicId, before;
    private final Map<Long, ArrayList<Integer>> result = new LinkedHashMap<>();
    private LinkedHashSet<Integer> ids = new LinkedHashSet<>();
    private long currentDialog;
    private int offset = Integer.MAX_VALUE;
    private int generation, pages;
    private boolean stopped;

    public OwnMessageHistorySearch(Backend backend, Completion completion, long dialogId,
                                   long mergeDialogId, int topicId, int before) {
        this.backend = backend;
        this.completion = completion;
        this.dialogId = currentDialog = dialogId;
        this.mergeDialogId = mergeDialogId == dialogId ? 0 : mergeDialogId;
        this.topicId = topicId;
        this.before = before;
    }

    public void start() {
        if (!stopped && generation == 0) next();
    }

    public void cancel() {
        stopped = true;
        generation++;
        ids.clear();
        result.clear();
    }

    private void next() {
        if (stopped) return;
        if (++pages > 10000) {
            finish("SEARCH_LIMIT");
            return;
        }
        int request = ++generation;
        try {
            backend.search(currentDialog, currentDialog == dialogId ? topicId : 0, before, offset,
                    (entries, error) -> accept(request, entries, error));
        } catch (RuntimeException error) {
            if (!stopped && generation == request) finish("SEARCH_FAILED");
        }
    }

    private void accept(int request, List<Entry> entries, String error) {
        if (stopped || request != generation) return;
        generation++;
        if (error != null || entries == null) {
            finish(error != null ? error : "SEARCH_FAILED");
            return;
        }
        if (entries.isEmpty()) {
            result.put(currentDialog, new ArrayList<>(ids));
            if (currentDialog == dialogId && mergeDialogId != 0) {
                currentDialog = mergeDialogId;
                offset = Integer.MAX_VALUE;
                ids = new LinkedHashSet<>();
                next();
            } else {
                finish(null);
            }
            return;
        }
        int nextOffset = offset;
        for (Entry entry : entries) {
            if (entry == null || entry.id <= 0) continue;
            nextOffset = Math.min(nextOffset, entry.id);
            if (entry.id < offset && entry.outgoing && !entry.post && entry.date < before) ids.add(entry.id);
        }
        if (nextOffset >= offset) {
            finish("SEARCH_NO_PROGRESS");
            return;
        }
        offset = nextOffset;
        next();
    }

    private void finish(String error) {
        if (stopped) return;
        stopped = true;
        generation++;
        completion.complete(error == null ? new LinkedHashMap<>(result) : new LinkedHashMap<>(), error);
    }

    public static int cutoff(int currentTime, int olderThanDays) {
        if (olderThanDays <= 0) return currentTime == Integer.MAX_VALUE ? currentTime : currentTime + 1;
        return (int) Math.max(0L, (long) currentTime - (long) olderThanDays * 86400L);
    }
}

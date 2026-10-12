package org.telegram.ui.iv;

import org.telegram.tgnet.tl.TL_iv;

public final class RichContainer {

    public static final int LIST = 0;
    public static final int QUOTE = 1;
    public static final int DETAILS = 2;

    private static long ID_GEN = 1;
    public static long newId() { return ID_GEN++; }

    public final int type;

    public long id;

    public boolean ordered;

    public boolean checklist;

    public long itemId;

    public int itemNum;

    public boolean itemChecked;

    public TL_iv.RichText author;

    public boolean open;

    public RichContainer(int type, long id) {
        this.type = type;
        this.id = id;
    }

    public static RichContainer list(long id, long itemId, boolean ordered, boolean checklist, boolean checked) {
        final RichContainer c = new RichContainer(LIST, id);
        c.itemId = itemId;
        c.ordered = ordered;
        c.checklist = checklist;
        c.itemChecked = checked;
        return c;
    }

    public static RichContainer quote(long id) {
        return new RichContainer(QUOTE, id);
    }

    public static RichContainer details(long id, boolean open) {
        final RichContainer c = new RichContainer(DETAILS, id);
        c.open = open;
        return c;
    }

    public boolean isList() { return type == LIST; }
    public boolean isQuote() { return type == QUOTE; }
    public boolean isDetails() { return type == DETAILS; }

    public boolean sameInstance(RichContainer o) {
        return o != null && o.type == type && o.id == id;
    }

    public boolean sameItem(RichContainer o) {
        return sameInstance(o) && o.itemId == itemId;
    }

    public RichContainer copy() {
        final RichContainer c = new RichContainer(type, id);
        c.ordered = ordered;
        c.checklist = checklist;
        c.itemId = itemId;
        c.itemChecked = itemChecked;
        c.author = author;
        c.open = open;
        return c;
    }
}

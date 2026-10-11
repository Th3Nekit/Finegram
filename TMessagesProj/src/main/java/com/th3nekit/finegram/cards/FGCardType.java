/* Licensed under GNU GPL v. 2 or later. */

package com.th3nekit.finegram.cards;

public enum FGCardType {
    WEATHER(1),
    TON(2),
    BTC(3),
    USD(4),
    CACHE(5),
    PROXY(6);

    public final int id;

    FGCardType(int id) {
        this.id = id;
    }

    public static FGCardType byId(int id) {
        for (FGCardType t : values()) {
            if (t.id == id) return t;
        }
        return null;
    }
}

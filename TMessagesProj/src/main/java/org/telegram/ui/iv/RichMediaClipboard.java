package org.telegram.ui.iv;

import org.telegram.tgnet.TLRPC;

import java.util.HashMap;
import java.util.List;

public final class RichMediaClipboard {

    private RichMediaClipboard() {}

    private static final HashMap<Long, TLRPC.Photo> photos = new HashMap<>();
    private static final HashMap<Long, TLRPC.Document> documents = new HashMap<>();

    public static synchronized void set(List<TLRPC.Photo> newPhotos, List<TLRPC.Document> newDocuments) {
        photos.clear();
        documents.clear();
        if (newPhotos != null) {
            for (TLRPC.Photo p : newPhotos) if (p != null) photos.put(p.id, p);
        }
        if (newDocuments != null) {
            for (TLRPC.Document d : newDocuments) if (d != null) documents.put(d.id, d);
        }
    }

    public static synchronized TLRPC.Photo photo(long id) {
        return id == 0 ? null : photos.get(id);
    }

    public static synchronized TLRPC.Document document(long id) {
        return id == 0 ? null : documents.get(id);
    }
}

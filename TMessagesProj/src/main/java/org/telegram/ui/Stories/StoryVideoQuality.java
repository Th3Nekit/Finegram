package org.telegram.ui.Stories;

import org.telegram.messenger.MessagesController;
import org.telegram.messenger.UserConfig;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.Components.VideoPlayer;

public final class StoryVideoQuality {
    private StoryVideoQuality() {
    }

    public static TLRPC.Document getDocument(int account, TLRPC.MessageMedia media) {
        if (media == null) {
            return null;
        }
        boolean highest = UserConfig.getInstance(account).isPremium()
                && MessagesController.getInstance(account).storyQualityFull;
        TLRPC.Document selected = getVariant(media, highest);
        if (selected != null) {
            return selected;
        }
        if (media.document != null) {
            return media.document;
        }
        for (int i = 0; i < media.alt_documents.size(); i++) {
            if (media.alt_documents.get(i) != null) {
                return media.alt_documents.get(i);
            }
        }
        return null;
    }

    public static boolean hasQualityChoice(TLRPC.MessageMedia media) {
        TLRPC.Document low = getVariant(media, false);
        TLRPC.Document high = getVariant(media, true);
        return low != null && high != null && getArea(high) > getArea(low);
    }

    public static String getResolution(TLRPC.MessageMedia media, boolean highest) {
        TLRPC.DocumentAttribute video = getVideoAttribute(getVariant(media, highest));
        return video == null ? "" : video.w + " × " + video.h;
    }

    private static TLRPC.Document getVariant(TLRPC.MessageMedia media, boolean highest) {
        if (media == null) {
            return null;
        }
        TLRPC.Document selected = null;
        long selectedArea = 0;
        for (int i = -1; i < media.alt_documents.size(); i++) {
            TLRPC.Document candidate = i < 0 ? media.document : media.alt_documents.get(i);
            long area = getArea(candidate);
            if (area > 0 && (selected == null || (highest ? area > selectedArea : area < selectedArea))) {
                selected = candidate;
                selectedArea = area;
            }
        }
        return selected;
    }

    private static long getArea(TLRPC.Document document) {
        TLRPC.DocumentAttribute video = getVideoAttribute(document);
        return video == null ? 0 : (long) video.w * video.h;
    }

    private static TLRPC.DocumentAttribute getVideoAttribute(TLRPC.Document document) {
        if (document == null || "application/x-mpegurl".equalsIgnoreCase(document.mime_type)
                || "application/x-tgstoryboard".equalsIgnoreCase(document.mime_type)
                || "application/x-tgstoryboardmap".equalsIgnoreCase(document.mime_type)) {
            return null;
        }
        for (int i = 0; i < document.attributes.size(); i++) {
            TLRPC.DocumentAttribute attribute = document.attributes.get(i);
            if (attribute instanceof TLRPC.TL_documentAttributeVideo && attribute.w > 0 && attribute.h > 0) {
                String codec = attribute.video_codec;
                if (("av1".equals(codec) || "av01".equals(codec) || "hevc".equals(codec)
                        || "h265".equals(codec) || "vp9".equals(codec)) && !VideoPlayer.supportsHardwareDecoder(codec)) {
                    return null;
                }
                return attribute;
            }
        }
        return null;
    }
}

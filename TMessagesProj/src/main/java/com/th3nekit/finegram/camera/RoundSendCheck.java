package com.th3nekit.finegram.camera;

import com.th3nekit.finegram.core.crash.CameraDiagnostics;
import com.th3nekit.finegram.core.crash.CameraLogUpload;

import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.MessageObject;
import org.telegram.messenger.VideoEditedInfo;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.Components.InstantCameraViewBase;

import java.io.File;

public final class RoundSendCheck {

    public static void sending(String path, VideoEditedInfo info, TLRPC.Document document) {
        CameraDiagnostics.record("round.send", describe(path, info, document));
    }

    public static void sent(MessageObject local, TLRPC.Document sentDocument) {
        if (local == null || local.messageOwner == null || local.messageOwner.media == null) return;
        TLRPC.Document localDocument = local.messageOwner.media.document;
        if (!MessageObject.isRoundVideoDocument(localDocument) || MessageObject.isRoundVideoDocument(sentDocument)) {
            return;
        }
        CameraLogUpload.incident("round.sent_as_video",
                describe(local.messageOwner.attachPath, local.videoEditedInfo, localDocument)
                        + " | server " + document(sentDocument));
    }

    private static String describe(String path, VideoEditedInfo info, TLRPC.Document document) {
        StringBuilder out = new StringBuilder("engine=")
                .append(InstantCameraViewBase.isUsingCamera2Implementation() ? "telegram" : "finegram")
                .append(" paused=").append(ApplicationLoader.mainInterfacePaused)
                .append(" local ").append(document(document));
        if (path != null) {
            File file = new File(path);
            out.append(" file=").append(file.exists() ? file.length() : -1);
        }
        if (info != null) {
            out.append(" info=").append(info.resultWidth).append('x').append(info.resultHeight)
                    .append(" est_ms=").append(info.estimatedDuration)
                    .append(" est_size=").append(info.estimatedSize)
                    .append(" trim=").append(info.startTime).append("..").append(info.endTime)
                    .append(" convert=").append(info.needConvert())
                    .append(" not_ready=").append(info.notReadyYet)
                    .append(" muted=").append(info.muted)
                    .append(" uploaded=").append(info.file != null || info.encryptedFile != null)
                    .append(" bitrate=").append(info.bitrate)
                    .append(" fps=").append(info.framerate);
        }
        return out.toString();
    }

    private static String document(TLRPC.Document document) {
        if (document == null) return "none";
        StringBuilder out = new StringBuilder("mime=").append(document.mime_type)
                .append(" size=").append(document.size);
        for (TLRPC.DocumentAttribute attribute : document.attributes) {
            if (attribute instanceof TLRPC.TL_documentAttributeVideo) {
                out.append(" video round=").append(attribute.round_message)
                        .append(' ').append(attribute.w).append('x').append(attribute.h)
                        .append(" duration=").append(attribute.duration)
                        .append(" codec=").append(attribute.video_codec);
            } else if (attribute instanceof TLRPC.TL_documentAttributeAnimated) {
                out.append(" animated");
            } else if (attribute instanceof TLRPC.TL_documentAttributeFilename) {
                out.append(" named");
            }
        }
        return out.toString();
    }

    private RoundSendCheck() {}
}

package com.th3nekit.finegram.camera;

import android.media.AudioRecord;
import java.nio.ByteBuffer;
import org.telegram.messenger.FileLog;

public final class AudioCaptureLifecycle {
    private AudioCaptureLifecycle() {}

    public static int read(AudioRecord recorder, ByteBuffer buffer, int bytes) {
        return recorder.read(buffer, bytes, AudioRecord.READ_NON_BLOCKING);
    }

    public static void release(AudioRecord recorder) {
        if (recorder == null) return;
        try {
            if (recorder.getRecordingState() != AudioRecord.RECORDSTATE_STOPPED) {
                recorder.stop();
            }
        } catch (RuntimeException e) {
            FileLog.e(e);
        } finally {
            try {
                recorder.release();
            } catch (RuntimeException e) {
                FileLog.e(e);
            }
        }
    }
}

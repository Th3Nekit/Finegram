package org.telegram.messenger.video.remix;

import androidx.annotation.NonNull;

import java.nio.Buffer;
import java.nio.ShortBuffer;

public interface AudioRemixer {

    void remix(@NonNull final ShortBuffer inputBuffer, int inputChannelCount,
               @NonNull final ShortBuffer outputBuffer, int outputChannelCount);

    int getRemixedSize(int inputSize, int inputChannelCount, int outputChannelCount);

    AudioRemixer DOWNMIX = new DownMixAudioRemixer();

    AudioRemixer UPMIX = new UpMixAudioRemixer();

    AudioRemixer PASSTHROUGH = new PassThroughAudioRemixer();

    AudioRemixer SURROUND = new SurroundAudioRemixer();
}

package org.telegram.messenger.video.resample;

import androidx.annotation.NonNull;

import java.nio.ShortBuffer;

public class PassThroughAudioResampler implements AudioResampler {

    @Override
    public void resample(@NonNull ShortBuffer inputBuffer, int inputSampleRate,
                         @NonNull ShortBuffer outputBuffer, int outputSampleRate, int channels) {
        if (inputSampleRate != outputSampleRate) {
            throw new IllegalArgumentException("Illegal use of PassThroughAudioResampler");
        }
        outputBuffer.put(inputBuffer);
    }
}

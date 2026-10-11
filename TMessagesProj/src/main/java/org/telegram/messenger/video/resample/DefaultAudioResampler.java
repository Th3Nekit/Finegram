package org.telegram.messenger.video.resample;

import androidx.annotation.NonNull;

import java.nio.ShortBuffer;

public class DefaultAudioResampler implements AudioResampler {

    @Override
    public void resample(@NonNull ShortBuffer inputBuffer, int inputSampleRate, @NonNull ShortBuffer outputBuffer, int outputSampleRate, int channels) {
        if (inputSampleRate < outputSampleRate) {
            UPSAMPLE.resample(inputBuffer, inputSampleRate, outputBuffer, outputSampleRate, channels);
        } else if (inputSampleRate > outputSampleRate) {
            DOWNSAMPLE.resample(inputBuffer, inputSampleRate, outputBuffer, outputSampleRate, channels);
        } else {
            PASSTHROUGH.resample(inputBuffer, inputSampleRate, outputBuffer, outputSampleRate, channels);
        }
    }
}

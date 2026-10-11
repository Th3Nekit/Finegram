package org.telegram.messenger.video.resample;

import androidx.annotation.NonNull;

import java.nio.ShortBuffer;

public class DownsampleAudioResampler implements AudioResampler {

    private static float ratio(int remaining, int all) {
        return (float) remaining / all;
    }

    @Override
    public void resample(@NonNull ShortBuffer inputBuffer, int inputSampleRate, @NonNull ShortBuffer outputBuffer, int outputSampleRate, int channels) {
        if (inputSampleRate < outputSampleRate) {
            throw new IllegalArgumentException("Illegal use of DownsampleAudioResampler");
        }
        if (channels != 1 && channels != 2) {
            throw new IllegalArgumentException("Illegal use of DownsampleAudioResampler. Channels:" + channels);
        }
        final int inputSamples = inputBuffer.remaining() / channels;
        final int outputSamples = (int) Math.ceil(inputSamples * ((double) outputSampleRate / inputSampleRate));
        final int dropSamples = inputSamples - outputSamples;
        int remainingOutputSamples = outputSamples;
        int remainingDropSamples = dropSamples;
        float remainingOutputSamplesRatio = ratio(remainingOutputSamples, outputSamples);
        float remainingDropSamplesRatio = ratio(remainingDropSamples, dropSamples);
        while (remainingOutputSamples > 0 && remainingDropSamples > 0) {

            if (remainingOutputSamplesRatio >= remainingDropSamplesRatio) {
                outputBuffer.put(inputBuffer.get());
                if (channels == 2) outputBuffer.put(inputBuffer.get());
                remainingOutputSamples--;
                remainingOutputSamplesRatio = ratio(remainingOutputSamples, outputSamples);
            } else {

                inputBuffer.position(inputBuffer.position() + channels);
                remainingDropSamples--;
                remainingDropSamplesRatio = ratio(remainingDropSamples, dropSamples);
            }
        }
    }
}
